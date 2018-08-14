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
#include <locale>
#include <algorithm>
#include <DbgHelp.h>
#include <set>
#include <boost/scoped_array.hpp>
#include <QApplication>

#define USE_CONST_CAST_IN_STD_STRING_C_STR

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
    UINT codepage = CP_UTF8;
    if (!utf8) {
      codepage = AreFileApisANSI() ? GetACP() : GetOEMCP();
    }
    int sizeRequired = ::WideCharToMultiByte(codepage, 0, &source[0], -1, nullptr, 0, nullptr, nullptr);
    if (sizeRequired == 0) {
      throw windows_error("failed to convert string to multibyte");
    }
    // the size returned by WideCharToMultiByte contains zero termination IF -1 is specified for the length.
    // we don't want that \0 in the string because then the length field would be wrong. Because madness
    result.resize(sizeRequired - 1, '\0');
    ::WideCharToMultiByte(codepage, 0, &source[0], (int)source.size(), &result[0], sizeRequired, nullptr, nullptr);
  }

  return result;
}

std::wstring ToWString(const std::string &source, bool utf8)
{
  std::wstring result;
  if (source.length() > 0) {
    UINT codepage = CP_UTF8;
    if (!utf8) {
      codepage = AreFileApisANSI() ? GetACP() : GetOEMCP();
    }
    int sizeRequired
        = ::MultiByteToWideChar(codepage, 0, source.c_str(),
                                static_cast<int>(source.length()), nullptr, 0);
    if (sizeRequired == 0) {
      throw windows_error("failed to convert string to wide character");
    }
    result.resize(sizeRequired, L'\0');
    ::MultiByteToWideChar(codepage, 0, source.c_str(),
                          static_cast<int>(source.length()), &result[0],
                          sizeRequired);
  }

  return result;
}

static std::locale loc("");
static auto locToLowerW = [] (wchar_t in) -> wchar_t {
  return std::tolower(in, loc);
};

static auto locToLower = [] (char in) -> char {
  return std::tolower(in, loc);
};

std::string &ToLower(std::string &text)
{
  std::transform(text.begin(), text.end(), text.begin(), locToLower);
  return text;
}

std::string &ToLower(const std::string &Src, std::string &Dst)
{
#ifndef USE_CONST_CAST_IN_STD_STRING_C_STR
  if (Src.empty())
  {
    Dst.clear();
    return;
  }

  std::vector<char> Buff(Src.begin(), Src.end());
  CharLowerBuffA(&Buff.front(), static_cast<DWORD>(Buff.size()));
  Dst.assign(Buff.begin(), Buff.end());

  return Dst;
#else
  Dst = Src;
  CharLowerBuffA(const_cast<CHAR *>(Dst.c_str()), static_cast<DWORD>(Dst.size()));
  return Dst;
#endif
}


std::string ToLower(const std::string &text)
{
  std::string result(text);
  
  return ToLower(text,result);
}

std::wstring &ToLower(std::wstring &text)
{
  std::transform(text.begin(), text.end(), text.begin(), locToLowerW);
  return text;
}

std::wstring &ToLower(const std::wstring &Src, std::wstring &Dst)
{
  Dst = Src;
  CharLowerBuffW(const_cast<WCHAR *>(Dst.c_str()), static_cast<DWORD>(Dst.size()));
  return Dst;
}

std::wstring ToLower(const std::wstring &text)
{
  std::wstring result(text);
  CharLowerBuffW(&result[0], static_cast<DWORD>(result.size()));
  return result;
  // return ToLower(text, result);
} 

bool CaseInsenstiveComparePred(wchar_t lhs, wchar_t rhs)
{
  return std::tolower(lhs, loc) == std::tolower(rhs, loc);
}

bool CaseInsensitiveEqual(const std::wstring &lhs, const std::wstring &rhs)
{
  return (lhs.length() == rhs.length())
      && std::equal(lhs.begin(), lhs.end(),
                    rhs.begin(),
                    [] (wchar_t lhs, wchar_t rhs) -> bool {
                      return std::tolower(lhs, loc) == std::tolower(rhs, loc);
                    });
}

VS_FIXEDFILEINFO GetFileVersion(const std::wstring &fileName)
{
  DWORD handle = 0UL;
  DWORD size = ::GetFileVersionInfoSizeW(fileName.c_str(), &handle);
  if (size == 0) {
    throw windows_error("failed to determine file version info size");
  }

  boost::scoped_array<char> buffer(new char[size]);
  try {
    handle = 0UL;
    if (!::GetFileVersionInfoW(fileName.c_str(), handle, size, buffer.get())) {
      throw windows_error("failed to determine file version info");
    }

    void *versionInfoPtr = nullptr;
    UINT versionInfoLength = 0;
    if (!::VerQueryValue(buffer.get(), L"\\", &versionInfoPtr, &versionInfoLength)) {
      throw windows_error("failed to determine file version");
    }

    VS_FIXEDFILEINFO result = *(VS_FIXEDFILEINFO*)versionInfoPtr;
    return result;
  } catch (...) {
    throw;
  }
}

std::wstring GetFileVersionString(const std::wstring &fileName)
{
  DWORD handle = 0UL;
  DWORD size = ::GetFileVersionInfoSizeW(fileName.c_str(), &handle);
  if (size == 0) {
    throw windows_error("failed to determine file version info size");
  }

  boost::scoped_array<char> buffer(new char[size]);
  try {
    handle = 0UL;
    if (!::GetFileVersionInfoW(fileName.c_str(), handle, size, buffer.get())) {
      throw windows_error("failed to determine file version info");
    }

    LPVOID strBuffer = nullptr;
    UINT strLength = 0;
    if (!::VerQueryValue(buffer.get(), L"\\StringFileInfo\\040904B0\\ProductVersion", &strBuffer, &strLength)) {
      throw windows_error("failed to determine file version");
    }

    return std::wstring((LPCTSTR)strBuffer);
  }
  catch (...) {
    throw;
  }
}

MOBase::VersionInfo createVersionInfo()
{
  VS_FIXEDFILEINFO version = GetFileVersion(QApplication::applicationFilePath().toStdWString());

  if (version.dwFileFlags | VS_FF_PRERELEASE)
  {
    // Pre-release builds need annotating
    QString versionString = QString::fromStdWString(GetFileVersionString(QApplication::applicationFilePath().toStdWString()));

    // The pre-release flag can be set without the string specifying what type of pre-release
    bool noLetters = true;
    for (QChar character : versionString)
    {
      if (character.isLetter())
      {
        noLetters = false;
        break;
      }
    }

    if (noLetters)
    {
      // Default to pre-alpha when release type is unspecified
      return MOBase::VersionInfo(version.dwFileVersionMS >> 16,
                                 version.dwFileVersionMS & 0xFFFF,
                                 version.dwFileVersionLS >> 16,
                                 version.dwFileVersionLS & 0xFFFF,
                                 MOBase::VersionInfo::RELEASE_PREALPHA);
    }
    else
    {
      // Trust the string to make sense
      return MOBase::VersionInfo(versionString);
    }
  }
  else
  {
    // Non-pre-release builds just need their version numbers reading
    return MOBase::VersionInfo(version.dwFileVersionMS >> 16,
                               version.dwFileVersionMS & 0xFFFF,
                               version.dwFileVersionLS >> 16,
                               version.dwFileVersionLS & 0xFFFF);
  }
}


} // namespace MOShared
