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
#include <utility.h>

#include <sstream>
#include <locale>
#include <algorithm>
#include <DbgHelp.h>
#include <set>
#include <boost/scoped_array.hpp>
#include <QApplication>

using MOBase::formatSystemMessage;

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
  //std::transform(text.begin(), text.end(), text.begin(), locToLower);
  CharLowerBuffA(const_cast<CHAR *>(text.c_str()), static_cast<DWORD>(text.size()));
  return text;
}

std::string ToLower(const std::string &text)
{
  std::string result(text);
  //std::transform(result.begin(), result.end(), result.begin(), locToLower);
  CharLowerBuffA(const_cast<CHAR *>(result.c_str()), static_cast<DWORD>(result.size()));
  return result;
}

std::wstring &ToLower(std::wstring &text)
{
  //std::transform(text.begin(), text.end(), text.begin(), locToLowerW);
  CharLowerBuffW(const_cast<WCHAR *>(text.c_str()), static_cast<DWORD>(text.size()));
  return text;
}

std::wstring ToLower(const std::wstring &text)
{
  std::wstring result(text);
  //std::transform(result.begin(), result.end(), result.begin(), locToLowerW);
  CharLowerBuffW(const_cast<WCHAR *>(result.c_str()), static_cast<DWORD>(result.size()));
  return result;
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

  if (version.dwFileFlags & VS_FF_PRERELEASE)
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


namespace env
{

struct HandleCloser
{
  using pointer = HANDLE;

  void operator()(HANDLE h)
  {
    if (h != INVALID_HANDLE_VALUE) {
      ::CloseHandle(h);
    }
  }
};

struct LibraryFreer
{
  using pointer = HINSTANCE;

  void operator()(HINSTANCE h)
  {
    if (h != 0) {
      ::FreeLibrary(h);
    }
  }
};


Environment::Environment()
{
  getLoadedModules();
}

const std::vector<Module>& Environment::loadedModules()
{
  return m_modules;
}

const WindowsVersion& Environment::windowsVersion() const
{
  return m_windows;
}

void Environment::getLoadedModules()
{
  std::unique_ptr<HANDLE, HandleCloser> snapshot(CreateToolhelp32Snapshot(
    TH32CS_SNAPMODULE32 | TH32CS_SNAPMODULE, GetCurrentProcessId()));

  if (snapshot.get() == INVALID_HANDLE_VALUE)
  {
    const auto e = GetLastError();

    qCritical().nospace().noquote()
      << "CreateToolhelp32Snapshot() failed, "
      << formatSystemMessage(e);

    return;
  }

  //  Set the size of the structure before using it.
  MODULEENTRY32 me = {};
  me.dwSize = sizeof(me);

  //  Retrieve information about the first module,
  //  and exit if unsuccessful
  if (!Module32First(snapshot.get(), &me))
  {
    const auto e = GetLastError();

    qCritical().nospace().noquote()
      << "Module32First() failed, " << formatSystemMessage(e);

    return;
  }

  //  Now walk the module list of the process,
  //  and display information about each module

  for (;;)
  {
    const auto path = QString::fromWCharArray(me.szExePath);

    m_modules.push_back(Module(path, me.modBaseSize));

    if (!Module32Next(snapshot.get(), &me)) {
      const auto e = GetLastError();

      if (e != ERROR_NO_MORE_FILES) {
        qCritical().nospace().noquote()
          << "Module32Next() failed, " << formatSystemMessage(e);
      }

      break;
    }
  }

  std::sort(m_modules.begin(), m_modules.end(), [](auto&& a, auto&& b) {
    return (a.displayPath().compare(b.displayPath(), Qt::CaseInsensitive) < 0);
  });
}


Module::Module(QString path, std::size_t fileSize)
  : m_path(std::move(path)), m_fileSize(fileSize)
{
  const auto fi = getFileInfo();

  m_version = getVersion(fi.ffi);
  m_timestamp = getTimestamp(fi.ffi);
  m_versionString = fi.fileDescription;
  m_md5 = getMD5();
}

const QString& Module::path() const
{
  return m_path;
}

QString Module::displayPath() const
{
  return QDir::fromNativeSeparators(m_path.toLower());
}

std::size_t Module::fileSize() const
{
  return m_fileSize;
}

const QString& Module::version() const
{
  return m_version;
}

const QString& Module::versionString() const
{
  return m_versionString;
}

QString Module::timestampString() const
{
  if (!m_timestamp.isValid()) {
    return "(no timestamp)";
  }

  return m_timestamp.toString(Qt::DateFormat::ISODate);
}

QString Module::toString() const
{
  QStringList sl;

  sl.push_back(displayPath());
  sl.push_back(QString("%1 B").arg(m_fileSize));

  if (m_version.isEmpty() && m_versionString.isEmpty()) {
    sl.push_back("(no version)");
  } else {
    if (!m_version.isEmpty()) {
      sl.push_back(m_version);
    }

    if (m_versionString != m_version) {
      sl.push_back(versionString());
    }
  }

  if (m_timestamp.isValid()) {
    sl.push_back(m_timestamp.toString(Qt::DateFormat::ISODate));
  } else {
    sl.push_back("(no timestamp)");
  }

  if (!m_md5.isEmpty()) {
    sl.push_back(m_md5);
  }

  return sl.join(", ");
}

Module::FileInfo Module::getFileInfo() const
{
  const auto wspath = m_path.toStdWString();

  DWORD dummy = 0;
  const DWORD size = GetFileVersionInfoSizeW(wspath.c_str(), &dummy);

  if (size == 0) {
    const auto e = GetLastError();

    if (e == ERROR_RESOURCE_TYPE_NOT_FOUND) {
      // not an error, no version information built into that module
      return {};
    }

    qCritical().nospace().noquote()
      << "GetFileVersionInfoSizeW() failed on '" << m_path << "', "
      << formatSystemMessage(e);

    return {};
  }

  auto buffer = std::make_unique<std::byte[]>(size);

  if (!GetFileVersionInfoW(wspath.c_str(), 0, size, buffer.get())) {
    const auto e = GetLastError();

    qCritical().nospace().noquote()
      << "GetFileVersionInfoW() failed on '" << m_path << "', "
      << formatSystemMessage(e);

    return {};
  }


  FileInfo fi;
  fi.ffi = getFixedFileInfo(buffer.get());
  fi.fileDescription = getFileDescription(buffer.get());

  return fi;
}

VS_FIXEDFILEINFO Module::getFixedFileInfo(std::byte* buffer) const
{
  void* valuePointer = nullptr;
  unsigned int valueSize = 0;

  const auto ret = VerQueryValueW(buffer, L"\\", &valuePointer, &valueSize);

  if (!ret || !valuePointer || valueSize == 0) {
    // not an error, no fixed file info
    return {};
  }

  const auto* fi = reinterpret_cast<VS_FIXEDFILEINFO*>(valuePointer);

  if (fi->dwSignature != 0xfeef04bd) {
    qCritical().nospace().noquote()
      << "bad file info signature 0x" << hex << fi->dwSignature << " for "
      << "'" << m_path << "'";

    return {};
  }

  return *fi;
}

QString Module::getFileDescription(std::byte* buffer) const
{
  struct LANGANDCODEPAGE
  {
    WORD wLanguage;
    WORD wCodePage;
  };

  void* valuePointer = nullptr;
  unsigned int valueSize = 0;

  auto ret = VerQueryValueW(
    buffer, L"\\VarFileInfo\\Translation", &valuePointer, &valueSize);

  if (!ret || !valuePointer || valueSize == 0) {
    qCritical().nospace().noquote()
      << "VerQueryValueW() for translations failed on '" << m_path << "'";

    return {};
  }

  const auto count = valueSize / sizeof(LANGANDCODEPAGE);
  if (count == 0) {
    return {};
  }

  const auto* lcp = reinterpret_cast<LANGANDCODEPAGE*>(valuePointer);

  const auto subBlock = QString("\\StringFileInfo\\%1%2\\FileVersion")
    .arg(lcp->wLanguage, 4, 16, QChar('0'))
    .arg(lcp->wCodePage, 4, 16, QChar('0'));

  ret = VerQueryValueW(
    buffer, subBlock.toStdWString().c_str(), &valuePointer, &valueSize);

  if (!ret || !valuePointer || valueSize == 0) {
    // not an error, no file version
    return {};
  }

  // valueSize includes the null terminator
  return QString::fromWCharArray(
    reinterpret_cast<wchar_t*>(valuePointer), valueSize - 1);
}

QString Module::getVersion(const VS_FIXEDFILEINFO& fi) const
{
  if (fi.dwSignature == 0) {
    return {};
  }

  const DWORD major = (fi.dwFileVersionMS >> 16 ) & 0xffff;
  const DWORD minor = (fi.dwFileVersionMS >>  0 ) & 0xffff;
  const DWORD maintenance = (fi.dwFileVersionLS >> 16 ) & 0xffff;
  const DWORD build = (fi.dwFileVersionLS >>  0 ) & 0xffff;

  if (major == 0 && minor == 0 && maintenance == 0 && build == 0) {
    return {};
  }

  return QString("%1.%2.%3.%4")
    .arg(major).arg(minor).arg(maintenance).arg(build);
}

QDateTime Module::getTimestamp(const VS_FIXEDFILEINFO& fi) const
{
  FILETIME ft = {};

  if (fi.dwSignature == 0 || (fi.dwFileDateMS == 0 && fi.dwFileDateLS == 0)) {
    std::unique_ptr<HANDLE, HandleCloser> h(CreateFileW(
      m_path.toStdWString().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));

    if (h.get() == INVALID_HANDLE_VALUE) {
      const auto e = GetLastError();

      qCritical().nospace().noquote()
        << "can't open file '" << m_path << "' for timestamp, "
        << formatSystemMessage(e);

      return {};
    }

    if (!GetFileTime(h.get(), &ft, nullptr, nullptr)) {
      const auto e = GetLastError();
      qCritical().nospace().noquote()
        << "can't get file time for '" << m_path << "', "
        << formatSystemMessage(e);

      return {};
    }
  } else {
    ft.dwHighDateTime = fi.dwFileDateMS;
    ft.dwLowDateTime = fi.dwFileDateLS;
  }


  SYSTEMTIME utc = {};
  if (!FileTimeToSystemTime(&ft, &utc)) {
    qCritical().nospace().noquote()
      << "FileTimeToSystemTime() failed on timestamp "
      << "high=0x" << hex << ft.dwHighDateTime << " "
      << "low=0x" << hex << ft.dwLowDateTime << " for "
      << "'" << m_path << "'";

    return {};
  }

  return QDateTime(
    QDate(utc.wYear, utc.wMonth, utc.wDay),
    QTime(utc.wHour, utc.wMinute, utc.wSecond, utc.wMilliseconds));
}

QString Module::getMD5() const
{
  if (m_path.contains("\\windows\\", Qt::CaseInsensitive)) {
    // don't calculate md5 for system files, it's not really relevant and
    // it takes a while
    return {};
  }

  QFile f(m_path);

  if (!f.open(QFile::ReadOnly)) {
    qCritical().nospace().noquote()
      << "failed to open file '" << m_path << "' for md5";

    return {};
  }

  QCryptographicHash hash(QCryptographicHash::Md5);
  if (!hash.addData(&f)) {
    qCritical().nospace().noquote()
      << "failed to calculate md5 for '" << m_path << "'";

    return {};
  }

  return hash.result().toHex();
}


WindowsVersion::WindowsVersion() :
  m_realMajor(0), m_realMinor(0), m_realBuild(0),
  m_major(0), m_minor(0), m_build(0), m_UBR(0)
{
  getVersion();
  getRelease();
  getElevated();
}

QString WindowsVersion::toString() const
{
  QStringList sl;

  const QString version = QString("%1.%2.%3")
    .arg(m_major).arg(m_minor).arg(m_build);

  const QString realVersion = QString("%1.%2.%3")
    .arg(m_realMajor).arg(m_realMinor).arg(m_realBuild);

  sl.push_back("version: " + version);

  if (m_realMajor != m_major || m_realMinor != m_minor || m_realBuild != m_build) {
    sl.push_back("real version: " + realVersion);
  }

  if (!m_buildLab.isEmpty()) {
    sl.push_back(m_buildLab);
  }

  if (!m_productName.isEmpty()) {
    sl.push_back(m_productName);
  }

  if (!m_releaseID.isEmpty()) {
    sl.push_back("build " + m_releaseID);
  }

  if (m_UBR != 0) {
    sl.push_back(QString("%1").arg(m_UBR));
  }

  QString elevated = "?";
  if (m_elevated.has_value()) {
    elevated = (*m_elevated ? "yes" : "no");
  }

  sl.push_back("elevated: " + elevated);

  return sl.join(", ");
}

void WindowsVersion::getVersion()
{
  std::unique_ptr<HINSTANCE, LibraryFreer> ntdll(LoadLibraryW(L"ntdll.dll"));

  if (!ntdll) {
    qCritical() << "failed to load ntdll.dll while getting version";
    return;
  }

  getRealVersion(ntdll.get());
  getReportedVersion(ntdll.get());
}

void WindowsVersion::getRealVersion(HINSTANCE ntdll)
{
  using RtlGetNtVersionNumbersType = void (NTAPI)(DWORD*, DWORD*, DWORD*);

  auto* RtlGetNtVersionNumbers = reinterpret_cast<RtlGetNtVersionNumbersType*>(
    GetProcAddress(ntdll, "RtlGetNtVersionNumbers"));

  if (RtlGetNtVersionNumbers) {
    DWORD build = 0;
    RtlGetNtVersionNumbers(&m_realMajor, &m_realMinor, &build);

    m_realBuild = 0x0fffffff & build;
  }
}

void WindowsVersion::getReportedVersion(HINSTANCE ntdll)
{
  using RtlGetVersionType = NTSTATUS (NTAPI)(PRTL_OSVERSIONINFOW);

  auto* RtlGetVersion = reinterpret_cast<RtlGetVersionType*>(
    GetProcAddress(ntdll, "RtlGetVersion"));

  if (!RtlGetVersion) {
    qCritical() << "RtlGetVersion() not found in ntdll.dll";
    return;
  }

  OSVERSIONINFOEX vi = {};
  vi.dwOSVersionInfoSize = sizeof(vi);

  RtlGetVersion((RTL_OSVERSIONINFOW*)&vi);

  m_major = vi.dwMajorVersion;
  m_minor = vi.dwMinorVersion;
  m_build = vi.dwBuildNumber;
}

void WindowsVersion::getRelease()
{
  QSettings settings(
    R"(HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion)",
    QSettings::NativeFormat);

  m_buildLab = settings.value("BuildLabEx", "").toString();
  if (m_buildLab.isEmpty()) {
    m_buildLab = settings.value("BuildLab", "").toString();
    if (m_buildLab.isEmpty()) {
      m_buildLab = settings.value("BuildBranch", "").toString();
    }
  }

  m_productName = settings.value("ProductName", "").toString();
  m_releaseID = settings.value("ReleaseId", "").toString();
  m_UBR = settings.value("UBR", 0).toUInt();
}

void WindowsVersion::getElevated()
{
  std::unique_ptr<HANDLE, HandleCloser> token;

  {
    HANDLE rawToken = 0;

    if (!OpenProcessToken(GetCurrentProcess( ), TOKEN_QUERY, &rawToken)) {
      const auto e = GetLastError();

      qCritical()
        << "while trying to check if process is elevated, "
        << "OpenProcessToken() failed: " << formatSystemMessage(e);

      return;
    }

    token.reset(rawToken);
  }

  TOKEN_ELEVATION e = {};
  DWORD size = sizeof(TOKEN_ELEVATION);

  if (!GetTokenInformation(token.get(), TokenElevation, &e, sizeof(e), &size)) {
    const auto e = GetLastError();

    qCritical()
      << "while trying to check if process is elevated, "
      << "GetTokenInformation() failed: " << formatSystemMessage(e);

    return;
  }

  m_elevated = (e.TokenIsElevated != 0);
}

} // namespace env

} // namespace MOShared
