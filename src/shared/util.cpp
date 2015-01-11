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
#include <boost/scoped_array.hpp>

namespace MOShared {


bool FileExists(const std::string &filename)
{
  DWORD dwAttrib = ::GetFileAttributesA(filename.c_str());

  return (dwAttrib != INVALID_FILE_ATTRIBUTES);
}

bool FileExists(const std::wstring &filename)
{
  DWORD dwAttrib = ::GetFileAttributesW(filename.c_str());

  return (dwAttrib != INVALID_FILE_ATTRIBUTES);
}

bool FileExists(const std::wstring &searchPath, const std::wstring &filename)
{
  std::wstringstream stream;
  stream << searchPath << "\\" << filename;
  return FileExists(stream.str());
}

std::string ToString(const std::wstring &source, bool utf8)
{
  std::string result;
  if (source.length() > 0) {
    UINT codepage = utf8 ? CP_UTF8 : GetACP();
    int sizeRequired = ::WideCharToMultiByte(codepage, 0, &source[0], -1, NULL, 0, NULL, NULL);
    if (sizeRequired == 0) {
      throw windows_error("failed to convert string to multibyte");
    }
    // the size returned by WideCharToMultiByte contains zero termination IF -1 is specified for the length.
    // we don't want that \0 in the string because then the length field would be wrong. Because madness
    result.resize(sizeRequired - 1, '\0');
    ::WideCharToMultiByte(codepage, 0, &source[0], (int)source.size(), &result[0], sizeRequired, NULL, NULL);
  }

  return result;
}

std::wstring ToWString(const std::string &source, bool utf8)
{
  std::wstring result;
  if (source.length() > 0) {
    UINT codepage = utf8 ? CP_UTF8 : GetACP();
    int sizeRequired = ::MultiByteToWideChar(codepage, 0, source.c_str(), source.length(), NULL, 0);
    if (sizeRequired == 0) {
      throw windows_error("failed to convert string to wide character");
    }
    result.resize(sizeRequired, L'\0');
    ::MultiByteToWideChar(codepage, 0, source.c_str(), source.length(), &result[0], sizeRequired);
  }

  return result;
}

std::string &ToLower(std::string &text)
{
  std::transform(text.begin(), text.end(), text.begin(), ::tolower);
  return text;
}

std::string ToLower(const std::string &text)
{
  std::string result = text;
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

std::wstring &ToLower(std::wstring &text)
{
  std::transform(text.begin(), text.end(), text.begin(), ::towlower);
  return text;
}

std::wstring ToLower(const std::wstring &text)
{
  std::wstring result = text;
  std::transform(result.begin(), result.end(), result.begin(), ::towlower);
  return result;
}

VS_FIXEDFILEINFO GetFileVersion(const std::wstring &fileName)
{
  DWORD handle;
  DWORD size = ::GetFileVersionInfoSizeW(fileName.c_str(), &handle);
  if (size == 0) {
    throw windows_error("failed to determine file version info size");
  }

  void *buffer = new char[size];
  try {
    if (!::GetFileVersionInfoW(fileName.c_str(), handle, size, buffer)) {
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
