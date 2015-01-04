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

#include "error_report.h"
#include <sstream>
#include <stdio.h>

namespace MOShared {


void reportError(LPCSTR format, ...)
{
  char buffer[1025];
  memset(buffer, 0, sizeof(char) * 1025);

  va_list argList;
  va_start(argList, format);

  vsnprintf(buffer, 1024, format, argList);
  va_end(argList);

  MessageBoxA(nullptr, buffer, "Error", MB_OK | MB_ICONERROR);
}

void reportError(LPCWSTR format, ...)
{
  WCHAR buffer[1025];
  memset(buffer, 0, sizeof(WCHAR) * 1025);

  va_list argList;
  va_start(argList, format);


  _vsnwprintf(buffer, 1024, format, argList);
  va_end(argList);

  MessageBoxW(nullptr, buffer, L"Error", MB_OK | MB_ICONERROR);
}


std::string getCurrentErrorStringA()
{
  LPSTR buffer = nullptr;

  DWORD errorCode = ::GetLastError();

  if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, nullptr) == 0) {
    ::SetLastError(errorCode);
    return std::string();
  } else {
    LPSTR lastChar = buffer + strlen(buffer) - 2;
	  *lastChar = '\0';

    std::string result(buffer);

    LocalFree(buffer);
    ::SetLastError(errorCode);
    return result;
  }
}

std::wstring getCurrentErrorStringW()
{
  LPWSTR buffer = nullptr;

  DWORD errorCode = ::GetLastError();

  if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&buffer, 0, nullptr) == 0) {
    ::SetLastError(errorCode);
    return std::wstring();
  } else {
    LPWSTR lastChar = buffer + wcslen(buffer) - 2;
	  *lastChar = '\0';

    std::wstring result(buffer);

    LocalFree(buffer);
    ::SetLastError(errorCode);
    return result;
  }
}
} // namespace MOShared
