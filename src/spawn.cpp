/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "spawn.h"

#include "report.h"
#include "utility.h"
#include <report.h>
#include <inject.h>
#include <appconfig.h>
#include <windows_error.h>

#include <QApplication>
#include <QMessageBox>

#include <Shellapi.h>

#include <boost/scoped_array.hpp>

using namespace MOBase;
using namespace MOShared;


static const int BUFSIZE = 4096;

static bool spawn(LPCWSTR binary, LPCWSTR arguments, LPCWSTR currentDirectory, bool suspended,
                  HANDLE stdOut, HANDLE stdErr,
                  HANDLE& processHandle, HANDLE& threadHandle)
{
  BOOL inheritHandles = FALSE;
  STARTUPINFO si;
  ::ZeroMemory(&si, sizeof(si));
  if (stdOut != INVALID_HANDLE_VALUE) {
    si.hStdOutput = stdOut;
    inheritHandles = TRUE;
    si.dwFlags |= STARTF_USESTDHANDLES;
  }
  if (stdErr != INVALID_HANDLE_VALUE) {
    si.hStdError = stdErr;
    inheritHandles = TRUE;
    si.dwFlags |= STARTF_USESTDHANDLES;
  }
  si.cb = sizeof(si);
  int length = wcslen(binary) + wcslen(arguments) + 4;
  wchar_t *commandLine = nullptr;
  if (arguments[0] != L'\0') {
    commandLine = new wchar_t[length];
    _snwprintf(commandLine, length, L"\"%ls\" %ls", binary, arguments);
  } else {
    commandLine = new wchar_t[length];
    _snwprintf_s(commandLine, length, _TRUNCATE, L"\"%ls\"", binary);
  }

  QString moPath = QCoreApplication::applicationDirPath();

  boost::scoped_array<TCHAR> oldPath(new TCHAR[BUFSIZE]);
  DWORD offset = ::GetEnvironmentVariable(TEXT("PATH"), oldPath.get(), BUFSIZE);
  if (offset > BUFSIZE) {
    oldPath.reset(new TCHAR[offset]);
    ::GetEnvironmentVariable(TEXT("PATH"), oldPath.get(), offset);
  }

  {
    boost::scoped_array<TCHAR> newPath(new TCHAR[offset + moPath.length() + 2]);
    _tcsncpy(newPath.get(), oldPath.get(), offset);
    newPath.get()[offset] = '\0';
    _tcsncat(newPath.get(), TEXT(";"), 1);
    _tcsncat(newPath.get(), ToWString(QDir::toNativeSeparators(moPath)).c_str(), moPath.length());

    ::SetEnvironmentVariable(TEXT("PATH"), newPath.get());
  }

  PROCESS_INFORMATION pi;
  BOOL success = ::CreateProcess(nullptr,
                                 commandLine,
                                 nullptr, nullptr,       // no special process or thread attributes
                                 inheritHandles,   // inherit handles if we plan to use stdout or stderr reroute
                                 CREATE_BREAKAWAY_FROM_JOB | (suspended ? CREATE_SUSPENDED : 0), // create suspended so I have time to inject the DLL
                                 nullptr,             // same environment as parent
                                 currentDirectory, // current directory
                                 &si, &pi          // startup and process information
                                 );

  ::SetEnvironmentVariable(TEXT("PATH"), oldPath.get());

  delete [] commandLine;

  if (!success) {
    throw windows_error("failed to start process");
  }

  processHandle = pi.hProcess;
  threadHandle = pi.hThread;
  return true;
}


HANDLE startBinary(const QFileInfo &binary,
                   const QString &arguments,
                   const QString& profileName,
                   int logLevel,
                   const QDir &currentDirectory,
                   bool hooked,
                   HANDLE stdOut,
                   HANDLE stdErr)
{
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo;

  ::QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation, &jobInfo, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION), nullptr);
  jobInfo.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_BREAKAWAY_OK;

  HANDLE jobObject = ::CreateJobObject(nullptr, nullptr);

  if (jobObject == nullptr) {
    qWarning("failed to create job object: %lu", ::GetLastError());
  } else {
    ::SetInformationJobObject(jobObject, JobObjectExtendedLimitInformation, &jobInfo, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
  }

  HANDLE processHandle, threadHandle;
  std::wstring binaryName = ToWString(QDir::toNativeSeparators(binary.absoluteFilePath()));
  std::wstring currentDirectoryName = ToWString(QDir::toNativeSeparators(currentDirectory.absolutePath()));

  try {
    if (!spawn(binaryName.c_str(), ToWString(arguments).c_str(), currentDirectoryName.c_str(), true,
               stdOut, stdErr, processHandle, threadHandle)) {
      reportError(QObject::tr("failed to spawn \"%1\"").arg(binary.fileName()));
      return INVALID_HANDLE_VALUE;
    }
  } catch (const windows_error &e) {
    if (e.getErrorCode() == ERROR_ELEVATION_REQUIRED) {
      // TODO: check if this is really correct. Are all settings updated that the secondary instance may use?

      if (QMessageBox::question(nullptr, QObject::tr("Elevation required"),
                                QObject::tr("This process requires elevation to run.\n"
                                    "This is a potential security risk so I highly advice you to investigate if\n"
                                    "\"%1\"\n"
                                    "can be installed to work without elevation.\n\n"
                                    "Start elevated anyway? "
                                    "(you will be asked if you want to allow ModOrganizer.exe to make changes to the system)").arg(
                                        QDir::toNativeSeparators(binary.absoluteFilePath())),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        ::ShellExecuteW(nullptr, L"runas", ToWString(QCoreApplication::applicationFilePath()).c_str(),
                        (std::wstring(L"\"") + binaryName + L"\" " + ToWString(arguments)).c_str(), currentDirectoryName.c_str(), SW_SHOWNORMAL);
        return INVALID_HANDLE_VALUE;
      } else {
        return INVALID_HANDLE_VALUE;
      }
    } else {
      reportError(QObject::tr("failed to spawn \"%1\": %2").arg(binary.fileName()).arg(e.what()));
      return INVALID_HANDLE_VALUE;
    }
  }

  if (hooked) {
    try {
      QFileInfo dllInfo(QApplication::applicationDirPath() + "/" + ToQString(AppConfig::hookDLLName()));
      if (!dllInfo.exists()) {
        reportError(QObject::tr("\"%1\" doesn't exist").arg(dllInfo.fileName()));
        return INVALID_HANDLE_VALUE;
      }
      injectDLL(processHandle, threadHandle,
                QDir::toNativeSeparators(dllInfo.canonicalFilePath()).toLocal8Bit().constData(),
                ToWString(profileName).c_str(), logLevel);
    } catch (const windows_error& e) {
      reportError(QObject::tr("failed to inject dll into \"%1\": %2").arg(binary.fileName()).arg(e.what()));
      ::TerminateProcess(processHandle, 1);
      return INVALID_HANDLE_VALUE;
    }
#ifdef _DEBUG
    reportError("ready?");
#endif // DEBUG
  }

  if (::AssignProcessToJobObject(jobObject, processHandle) == 0) {
    qWarning("failed to assign to job object: %lu", ::GetLastError());
    ::CloseHandle(jobObject);
    jobObject = processHandle;
  } else {
    ::CloseHandle(processHandle);
  }

  if (::ResumeThread(threadHandle) == (DWORD)-1) {
    reportError(QObject::tr("failed to run \"%1\"").arg(binary.fileName()));
    return INVALID_HANDLE_VALUE;
  }
  ::CloseHandle(threadHandle);
  return jobObject;
}
