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

#include "helper.h"
#include "report.h"
#include "utility.h"
#include <LMCons.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <QDir>


namespace Helper {


static bool helperExec(LPCWSTR moDirectory, LPCWSTR commandLine)
{
  wchar_t fileName[MAX_PATH];
  _snwprintf(fileName, MAX_PATH, L"%ls\\helper.exe", moDirectory);

  SHELLEXECUTEINFOW execInfo = {0};

  execInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
  execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
  execInfo.hwnd = nullptr;
  execInfo.lpVerb = L"runas";
  execInfo.lpFile = fileName;
  execInfo.lpParameters = commandLine;
  execInfo.lpDirectory = moDirectory;
  execInfo.nShow = SW_SHOW;

  ::ShellExecuteExW(&execInfo);

  if (::WaitForSingleObject(execInfo.hProcess, INFINITE) != WAIT_OBJECT_0) {
    reportError(QObject::tr("helper failed"));
    return false;
  }

  DWORD exitCode;
  GetExitCodeProcess(execInfo.hProcess, &exitCode);
  return exitCode == NOERROR;
}


bool init(const std::wstring &moDirectory)
{
  DWORD userNameLen = UNLEN + 1;
  wchar_t userName[UNLEN + 1];

  if (!GetUserName(userName, &userNameLen)) {
    reportError(QObject::tr("failed to determine account name"));
    return false;
  }
  wchar_t *commandLine = new wchar_t[32768];

  _snwprintf(commandLine, 32768, L"init \"%ls\" \"%ls\"",
               moDirectory.c_str(), userName);

  bool res = helperExec(moDirectory.c_str(), commandLine);
  delete [] commandLine;

  return res;
}


bool backdateBSAs(const std::wstring &moDirectory, const std::wstring &dataPath)
{
  wchar_t *commandLine = new wchar_t[32768];
  _snwprintf(commandLine, 32768, L"backdateBSA \"%ls\"",
             dataPath.c_str());

  bool res = helperExec(moDirectory.c_str(), commandLine);
  delete [] commandLine;

  return res;
}


} // namespace
