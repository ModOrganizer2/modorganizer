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
#include <boost/scoped_array.hpp>
#include <gameinfo.h>
#include <inject.h>
#include <appconfig.h>
#include <windows_error.h>
#include <QApplication>


using namespace MOBase;
using namespace MOShared;


static const int BUFSIZE = 4096;

bool spawn(LPCWSTR binary, LPCWSTR arguments, LPCWSTR currentDirectory, bool suspended, HANDLE& processHandle, HANDLE& threadHandle)
{
  STARTUPINFO si;
  ::ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  int length = wcslen(binary) + wcslen(arguments) + 4;
  wchar_t *commandLine = NULL;
  if (arguments[0] != L'\0') {
    commandLine = new wchar_t[length];
    _snwprintf(commandLine, length, L"\"%ls\" %ls", binary, arguments);
  } else {
    commandLine = new wchar_t[length];
    _snwprintf(commandLine, length, L"\"%ls\"", binary);
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
    _tcsncpy(newPath.get(), oldPath.get(), offset - 1);
    newPath.get()[offset - 1] = L'\0';
    _tcsncat(newPath.get(), TEXT(";"), 1);
    _tcsncat(newPath.get(), ToWString(QDir::toNativeSeparators(moPath)).c_str(), moPath.length());

    ::SetEnvironmentVariable(TEXT("PATH"), newPath.get());
  }

  PROCESS_INFORMATION pi;
  BOOL success = ::CreateProcess(NULL,
                                 commandLine,
                                 NULL, NULL,       // no special process or thread attributes
                                 FALSE,            // don't inherit handle
                                 suspended ? CREATE_SUSPENDED : 0, // create suspended so I have time to inject the DLL
                                 NULL,             // same environment as parent
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


HANDLE startBinary(const QFileInfo &binary, const QString &arguments, const QString& profileName, int logLevel, const QDir &currentDirectory, bool hooked)
{
  HANDLE processHandle, threadHandle;
  std::wstring binaryName = ToWString(QDir::toNativeSeparators(binary.absoluteFilePath()));
  std::wstring currentDirectoryName = ToWString(QDir::toNativeSeparators(currentDirectory.absolutePath()));

  try {
    if (!spawn(binaryName.c_str(), ToWString(arguments).c_str(), currentDirectoryName.c_str(), hooked, processHandle, threadHandle)) {
      reportError(QObject::tr("failed to spawn \"%1\"").arg(binary.fileName()));
      return INVALID_HANDLE_VALUE;
    }
  } catch (const windows_error &e) {
    reportError(QObject::tr("failed to spawn \"%1\": %2").arg(binary.fileName()).arg(e.what()));
    return INVALID_HANDLE_VALUE;
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
    if (::ResumeThread(threadHandle) == (DWORD)-1) {
      reportError(QObject::tr("failed to run \"%1\"").arg(binary.fileName()));
      return INVALID_HANDLE_VALUE;
    }
  }
  return processHandle;
}
