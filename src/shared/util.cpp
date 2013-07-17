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

#include "util.h"
#include "windows_error.h"
#include "error_report.h"

#include <sstream>
#include <algorithm>
#include <DbgHelp.h>
#include <set>

namespace MOShared {


bool FileExists(const std::string &filename)
{
  WIN32_FIND_DATAA findData;
  ZeroMemory(&findData, sizeof(WIN32_FIND_DATAA));
  HANDLE search = ::FindFirstFileExA(filename.c_str(), FindExInfoStandard, &findData, FindExSearchNameMatch, NULL, 0);
  if (search == INVALID_HANDLE_VALUE) {
    return false;
  } else {
    FindClose(search);
    return true;
  }
}

bool FileExists(const std::wstring &filename)
{
  WIN32_FIND_DATAW findData;
  ZeroMemory(&findData, sizeof(WIN32_FIND_DATAW));
  HANDLE search = ::FindFirstFileExW(filename.c_str(), FindExInfoStandard, &findData, FindExSearchNameMatch, NULL, 0);
  if (search == INVALID_HANDLE_VALUE) {
    return false;
  } else {
    FindClose(search);
    return true;
  }
}

bool FileExists(const std::wstring &searchPath, const std::wstring &filename)
{
  std::wstringstream stream;
  stream << searchPath << "\\" << filename;
  return FileExists(stream.str());
}

std::string ToString(const std::wstring &source, bool utf8)
{
  char buffer[MAX_PATH];
  if (utf8) {
    ::WideCharToMultiByte(CP_UTF8, 0, source.c_str(), -1, buffer, MAX_PATH, NULL, NULL);
  } else {
    ::WideCharToMultiByte(GetACP(), 0, source.c_str(), -1, buffer, MAX_PATH, NULL, NULL);
//    wcstombs(buffer, source.c_str(), MAX_PATH);
  }
  return std::string(buffer);
}

std::wstring ToWString(const std::string &source, bool utf8)
{
  wchar_t buffer[MAX_PATH];
  if (utf8) {
    ::MultiByteToWideChar(CP_UTF8, 0, source.c_str(), -1, buffer, MAX_PATH);
  } else {
    // codepage encoding
    ::MultiByteToWideChar(GetACP(), 0, source.c_str(), -1, buffer, MAX_PATH);
//    mbstowcs(buffer, source.c_str(), MAX_PATH);
  }
  return std::wstring(buffer);
}

std::string &ToLower(std::string &text)
{
  std::transform(text.begin(), text.end(), text.begin(), tolower);
  return text;
}

std::string ToLower(const std::string &text)
{
  std::string temp = text;

  std::transform(temp.begin(), temp.end(), temp.begin(), tolower);
  return temp;
}

std::wstring &ToLower(std::wstring &text)
{
  std::transform(text.begin(), text.end(), text.begin(), towlower);
  return text;
}

std::wstring ToLower(const std::wstring &text)
{
  std::wstring temp = text;

  std::transform(temp.begin(), temp.end(), temp.begin(), towlower);
  return temp;
}

VS_FIXEDFILEINFO GetFileVersion(const std::wstring &fileName)
{
  DWORD size = ::GetFileVersionInfoSizeW(fileName.c_str(), NULL);
  if (size == 0) {
    throw windows_error("failed to determine file version info size");
  }

  void *buffer = new char[size];
  try {
    if (!::GetFileVersionInfoW(fileName.c_str(), 0UL, size, buffer)) {
      throw windows_error("failed to determine file version info");
    }

    void *versionInfoPtr = NULL;
    UINT versionInfoLength = 0;
    if (!::VerQueryValue(buffer, L"\\", &versionInfoPtr, &versionInfoLength)) {
      throw windows_error("failed to determine file version");
    }

    VS_FIXEDFILEINFO result = *(VS_FIXEDFILEINFO*)versionInfoPtr;
    delete [] buffer;
    return result;
  } catch (...) {
    delete [] buffer;
    throw;
  }
}




std::string GetStack()
{
#ifdef _DEBUG
  HANDLE process = ::GetCurrentProcess();
  static std::set<DWORD> initialized;
  if (initialized.find(::GetCurrentProcessId()) == initialized.end()) {
    static bool firstCall = true;
    if (firstCall) {
      ::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
      firstCall = false;
    }
    if (!::SymInitialize(process, NULL, TRUE)) {
      log("failed to initialize symbols: %d", ::GetLastError());
    }
    initialized.insert(::GetCurrentProcessId());
  }

  LPVOID stack[32];
  WORD frames = ::CaptureStackBackTrace(0, 100, stack, NULL);

  char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
  PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;

  std::ostringstream stackStream;
  for(unsigned int i = 0; i < frames; ++i) {
    DWORD64 addr = (DWORD64)stack[i];
    DWORD64 displacement = 0;
    if (!::SymFromAddr(::GetCurrentProcess(), addr, &displacement, symbol)) {
      stackStream << frames - i - 1 << ": " << stack[i] << " - " << ::GetLastError() << " (error)\n";
    } else {
      stackStream << frames - i - 1 << ": " << symbol->Name << "\n";
    }
  }
  return stackStream.str();
#else
  return "";
#endif
}


} // namespace MOShared
