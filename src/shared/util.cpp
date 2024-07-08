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
#include "../env.h"
#include "../mainwindow.h"
#include "windows_error.h"
#include <uibase/log.h>
#include <usvfs/usvfs.h>
#include <usvfs/usvfs_version.h>

using namespace MOBase;

namespace MOShared
{

bool FileExists(const std::string& filename)
{
  DWORD dwAttrib = ::GetFileAttributesA(filename.c_str());

  return (dwAttrib != INVALID_FILE_ATTRIBUTES);
}

bool FileExists(const std::wstring& filename)
{
  DWORD dwAttrib = ::GetFileAttributesW(filename.c_str());

  return (dwAttrib != INVALID_FILE_ATTRIBUTES);
}

bool FileExists(const std::wstring& searchPath, const std::wstring& filename)
{
  std::wstringstream stream;
  stream << searchPath << "\\" << filename;
  return FileExists(stream.str());
}

std::string ToString(const std::wstring& source, bool utf8)
{
  std::string result;
  if (source.length() > 0) {
    UINT codepage = CP_UTF8;
    if (!utf8) {
      codepage = AreFileApisANSI() ? GetACP() : GetOEMCP();
    }
    int sizeRequired = ::WideCharToMultiByte(codepage, 0, &source[0], -1, nullptr, 0,
                                             nullptr, nullptr);
    if (sizeRequired == 0) {
      throw windows_error("failed to convert string to multibyte");
    }
    // the size returned by WideCharToMultiByte contains zero termination IF -1 is
    // specified for the length. we don't want that \0 in the string because then the
    // length field would be wrong. Because madness
    result.resize(sizeRequired - 1, '\0');
    ::WideCharToMultiByte(codepage, 0, &source[0], (int)source.size(), &result[0],
                          sizeRequired, nullptr, nullptr);
  }

  return result;
}

std::wstring ToWString(const std::string& source, bool utf8)
{
  std::wstring result;
  if (source.length() > 0) {
    UINT codepage = CP_UTF8;
    if (!utf8) {
      codepage = AreFileApisANSI() ? GetACP() : GetOEMCP();
    }
    int sizeRequired = ::MultiByteToWideChar(
        codepage, 0, source.c_str(), static_cast<int>(source.length()), nullptr, 0);
    if (sizeRequired == 0) {
      throw windows_error("failed to convert string to wide character");
    }
    result.resize(sizeRequired, L'\0');
    ::MultiByteToWideChar(codepage, 0, source.c_str(),
                          static_cast<int>(source.length()), &result[0], sizeRequired);
  }

  return result;
}

static std::locale loc("");
static auto locToLowerW = [](wchar_t in) -> wchar_t {
  return std::tolower(in, loc);
};

static auto locToLower = [](char in) -> char {
  return std::tolower(in, loc);
};

std::string& ToLowerInPlace(std::string& text)
{
  CharLowerBuffA(const_cast<CHAR*>(text.c_str()), static_cast<DWORD>(text.size()));
  return text;
}

std::string ToLowerCopy(const std::string& text)
{
  std::string result(text);
  CharLowerBuffA(const_cast<CHAR*>(result.c_str()), static_cast<DWORD>(result.size()));
  return result;
}

std::wstring& ToLowerInPlace(std::wstring& text)
{
  CharLowerBuffW(const_cast<WCHAR*>(text.c_str()), static_cast<DWORD>(text.size()));
  return text;
}

std::wstring ToLowerCopy(const std::wstring& text)
{
  std::wstring result(text);
  CharLowerBuffW(const_cast<WCHAR*>(result.c_str()), static_cast<DWORD>(result.size()));
  return result;
}

std::wstring ToLowerCopy(std::wstring_view text)
{
  std::wstring result(text.begin(), text.end());
  ToLowerInPlace(result);
  return result;
}

bool CaseInsenstiveComparePred(wchar_t lhs, wchar_t rhs)
{
  return std::tolower(lhs, loc) == std::tolower(rhs, loc);
}

bool CaseInsensitiveEqual(const std::wstring& lhs, const std::wstring& rhs)
{
  return (lhs.length() == rhs.length()) &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                    [](wchar_t lhs, wchar_t rhs) -> bool {
                      return std::tolower(lhs, loc) == std::tolower(rhs, loc);
                    });
}

VS_FIXEDFILEINFO GetFileVersion(const std::wstring& fileName)
{
  DWORD handle = 0UL;
  DWORD size   = ::GetFileVersionInfoSizeW(fileName.c_str(), &handle);
  if (size == 0) {
    throw windows_error("failed to determine file version info size");
  }

  boost::scoped_array<char> buffer(new char[size]);
  try {
    handle = 0UL;
    if (!::GetFileVersionInfoW(fileName.c_str(), handle, size, buffer.get())) {
      throw windows_error("failed to determine file version info");
    }

    void* versionInfoPtr   = nullptr;
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

std::wstring GetFileVersionString(const std::wstring& fileName)
{
  DWORD handle = 0UL;
  DWORD size   = ::GetFileVersionInfoSizeW(fileName.c_str(), &handle);
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
    UINT strLength   = 0;
    if (!::VerQueryValue(buffer.get(), L"\\StringFileInfo\\040904B0\\ProductVersion",
                         &strBuffer, &strLength)) {
      throw windows_error("failed to determine file version");
    }

    return std::wstring((LPCTSTR)strBuffer);
  } catch (...) {
    throw;
  }
}

VersionInfo createVersionInfo()
{
  VS_FIXEDFILEINFO version = GetFileVersion(env::thisProcessPath().native());

  if (version.dwFileFlags & VS_FF_PRERELEASE) {
    // Pre-release builds need annotating
    QString versionString =
        QString::fromStdWString(GetFileVersionString(env::thisProcessPath().native()));

    // The pre-release flag can be set without the string specifying what type of
    // pre-release
    bool noLetters = true;
    for (QChar character : versionString) {
      if (character.isLetter()) {
        noLetters = false;
        break;
      }
    }

    if (noLetters) {
      // Default to pre-alpha when release type is unspecified
      return VersionInfo(
          version.dwFileVersionMS >> 16, version.dwFileVersionMS & 0xFFFF,
          version.dwFileVersionLS >> 16, version.dwFileVersionLS & 0xFFFF,
          VersionInfo::RELEASE_PREALPHA);
    } else {
      // Trust the string to make sense
      return VersionInfo(versionString);
    }
  } else {
    // Non-pre-release builds just need their version numbers reading
    return VersionInfo(version.dwFileVersionMS >> 16, version.dwFileVersionMS & 0xFFFF,
                       version.dwFileVersionLS >> 16, version.dwFileVersionLS & 0xFFFF);
  }
}

QString getUsvfsDLLVersion()
{
  QString s = usvfsVersionString();
  if (s.isEmpty()) {
    s = "?";
  }
  return s;
}

QString getUsvfsVersionString()
{
  const QString dll    = getUsvfsDLLVersion();
  const QString header = USVFS_VERSION_STRING;

  QString usvfsVersion;

  if (dll == header) {
    return dll;
  } else {
    return "dll is " + dll + ", compiled against " + header;
  }
}

void SetThisThreadName(const QString& s)
{
  using SetThreadDescriptionType = HRESULT(HANDLE hThread, PCWSTR lpThreadDescription);

  static SetThreadDescriptionType* SetThreadDescription = [] {
    SetThreadDescriptionType* p = nullptr;

    env::LibraryPtr kernel32(LoadLibraryW(L"kernel32.dll"));
    if (!kernel32) {
      return p;
    }

    p = reinterpret_cast<SetThreadDescriptionType*>(
        GetProcAddress(kernel32.get(), "SetThreadDescription"));

    return p;
  }();

  if (SetThreadDescription) {
    SetThreadDescription(GetCurrentThread(), s.toStdWString().c_str());
  }
}

char shortcutChar(const QAction* a)
{
  const auto text = a->text();
  char shortcut   = 0;

  for (int i = 0; i < text.size(); ++i) {
    const auto c = text[i];
    if (c == '&') {
      if (i >= (text.size() - 1)) {
        log::error("ampersand at the end");
        return 0;
      }

      return text[i + 1].toLatin1();
    }
  }

  log::error("action {} has no shortcut", text);
  return 0;
}

void checkDuplicateShortcuts(const QMenu& m)
{
  const auto actions = m.actions();

  for (int i = 0; i < actions.size(); ++i) {
    const auto* action1 = actions[i];
    if (action1->isSeparator()) {
      continue;
    }

    const char shortcut1 = shortcutChar(action1);
    if (shortcut1 == 0) {
      continue;
    }

    for (int j = i + 1; j < actions.size(); ++j) {
      const auto* action2 = actions[j];
      if (action2->isSeparator()) {
        continue;
      }

      const char shortcut2 = shortcutChar(action2);

      if (shortcut1 == shortcut2) {
        log::error("duplicate shortcut {} for {} and {}", shortcut1, action1->text(),
                   action2->text());

        break;
      }
    }
  }
}

}  // namespace MOShared

static bool g_exiting  = false;
static bool g_canClose = false;

MainWindow* findMainWindow()
{
  for (auto* tl : qApp->topLevelWidgets()) {
    if (auto* mw = dynamic_cast<MainWindow*>(tl)) {
      return mw;
    }
  }

  return nullptr;
}

bool ExitModOrganizer(ExitFlags e)
{
  if (g_exiting) {
    return true;
  }

  g_exiting = true;
  Guard g([&] {
    g_exiting = false;
  });

  if (!e.testFlag(Exit::Force)) {
    if (auto* mw = findMainWindow()) {
      if (!mw->canExit()) {
        return false;
      }
    }
  }

  g_canClose = true;

  const int code = (e.testFlag(Exit::Restart) ? RestartExitCode : 0);
  qApp->exit(code);

  return true;
}

bool ModOrganizerCanCloseNow()
{
  return g_canClose;
}

bool ModOrganizerExiting()
{
  return g_exiting;
}

void ResetExitFlag()
{
  g_exiting = false;
}

bool isNxmLink(const QString& link)
{
  return link.startsWith("nxm://", Qt::CaseInsensitive);
}
