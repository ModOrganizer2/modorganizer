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

#include <comdef.h>
#include <Wbemidl.h>
#include <wscapi.h>
#pragma comment(lib, "Wbemuuid.lib")

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

struct COMReleaser
{
  void operator()(IUnknown* p)
  {
    if (p) {
      p->Release();
    }
  }
};


class WMI
{
public:
  class failed {};

  WMI(const std::string& ns)
  {
    try
    {
      createLocator();
      createService(ns);
      setSecurity();
    }
    catch(failed&)
    {
    }
  }

  template <class F>
  void query(const std::string& q, F&& f)
  {
    if (!m_locator || !m_service) {
      return;
    }

    auto enumerator = getEnumerator(q);

    for (;;)
    {
      std::unique_ptr<IWbemClassObject, COMReleaser> object;

      {
        IWbemClassObject* rawObject = nullptr;
        ULONG count = 0;
        auto ret = enumerator->Next(WBEM_INFINITE, 1, &rawObject, &count);

        if (count == 0) {
          break;
        }

        object.reset(rawObject);
      }

      f(object.get());
    }
  }

  std::unique_ptr<IEnumWbemClassObject, COMReleaser> getEnumerator(
    const std::string& query)
  {
    IEnumWbemClassObject* rawEnumerator = NULL;

    auto ret = m_service->ExecQuery(
      bstr_t("WQL"),
      bstr_t(query.c_str()),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
      NULL,
      &rawEnumerator);

    if (FAILED(ret))
    {
      qCritical()
        << "query '" << QString::fromStdString(query) << "' failed, "
        << formatSystemMessage(ret);

      return {};
    }

    return std::unique_ptr<IEnumWbemClassObject, COMReleaser>(rawEnumerator);
  }

private:
  std::unique_ptr<IWbemLocator, COMReleaser> m_locator;
  std::unique_ptr<IWbemServices, COMReleaser> m_service;

  void createLocator()
  {
    void* rawLocator = nullptr;

    const auto ret = CoCreateInstance(
      CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
      IID_IWbemLocator, &rawLocator);

    if (FAILED(ret)) {
      qCritical()
        << "CoCreateInstance for WbemLocator failed, "
        << formatSystemMessage(ret);

      throw failed();
    }

    m_locator.reset(static_cast<IWbemLocator*>(rawLocator));
  }

  void createService(const std::string& ns)
  {
    IWbemServices* rawService = nullptr;

    const auto res = m_locator->ConnectServer(
      _bstr_t(ns.c_str()),
      nullptr, nullptr, nullptr, 0, nullptr, nullptr,
      &rawService);

    if (FAILED(res)) {
      qCritical()
        << "locator->ConnectServer() failed for namespace "
        << "'" << QString::fromStdString(ns) << "', "
        << formatSystemMessage(res);

      throw failed();
    }

    m_service.reset(rawService);
  }

  void setSecurity()
  {
    auto ret = CoSetProxyBlanket(
      m_service.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE);

    if (FAILED(ret))
    {
      qCritical()
        << "CoSetProxyBlanket() failed, " << formatSystemMessage(ret);

      throw failed();
    }
  }
};


Environment::Environment()
{
  getLoadedModules();
  getSecurityFeatures();
}

const std::vector<Module>& Environment::loadedModules()
{
  return m_modules;
}

const WindowsInfo& Environment::windowsInfo() const
{
  return m_windows;
}

const std::vector<SecurityFeature>& Environment::securityFeatures() const
{
  return m_security;
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

  MODULEENTRY32 me = {};
  me.dwSize = sizeof(me);

  // first module, this shouldn't fail because there's at least the executable
  if (!Module32First(snapshot.get(), &me))
  {
    const auto e = GetLastError();

    qCritical().nospace().noquote()
      << "Module32First() failed, " << formatSystemMessage(e);

    return;
  }

  for (;;)
  {
    const auto path = QString::fromWCharArray(me.szExePath);

    m_modules.push_back(Module(path, me.modBaseSize));

    // next module
    if (!Module32Next(snapshot.get(), &me)) {
      const auto e = GetLastError();

      if (e == ERROR_NO_MORE_FILES) {
        // not an error
        break;
      }

       qCritical().nospace().noquote()
        << "Module32Next() failed, " << formatSystemMessage(e);

      break;
    }
  }

  // sorting by display name
  std::sort(m_modules.begin(), m_modules.end(), [](auto&& a, auto&& b) {
    return (a.displayPath().compare(b.displayPath(), Qt::CaseInsensitive) < 0);
  });
}

void Environment::getSecurityFeatures()
{
  WMI wmi("root\\SecurityCenter2");
  std::map<QUuid, SecurityFeature> map;

  auto handleProduct = [&](auto* o) {
    VARIANT prop;

    auto ret = o->Get(L"displayName", 0, &prop, 0, 0);
    if (FAILED(ret)) {
      qCritical() << "failed to get displayName, " << formatSystemMessage(ret);
      return;
    }

    const std::wstring name = prop.bstrVal;
    VariantClear(&prop);

    ret = o->Get(L"productState", 0, &prop, 0, 0);
    if (FAILED(ret)) {
      qCritical() << "failed to get productState, " << formatSystemMessage(ret);
      return;
    }

    const DWORD state = prop.ulVal;
    VariantClear(&prop);

    ret = o->Get(L"instanceGuid", 0, &prop, 0, 0);
    if (FAILED(ret)) {
      qCritical() << "failed to get instanceGuid, " << formatSystemMessage(ret);
      return;
    }

    const QUuid guid(QString::fromWCharArray(prop.bstrVal));
    VariantClear(&prop);


    auto itor = map.find(guid);

    if (itor == map.end()) {
      map.insert({
        guid, SecurityFeature(QString::fromStdWString(name), state)});
    }
  };

  wmi.query("select * from AntivirusProduct", handleProduct);
  wmi.query("select * from FirewallProduct", handleProduct);
  wmi.query("select * from AntiSpywareProduct", handleProduct);

  for (auto&& p : map) {
    m_security.push_back(p.second);
  }
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

const QDateTime& Module::timestamp() const
{
  return m_timestamp;
}

const QString& Module::md5() const
{
  return m_md5;
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

  // file size
  sl.push_back(displayPath());
  sl.push_back(QString("%1 B").arg(m_fileSize));

  // version
  if (m_version.isEmpty() && m_versionString.isEmpty()) {
    sl.push_back("(no version)");
  } else {
    if (!m_version.isEmpty()) {
      sl.push_back(m_version);
    }

    if (!m_versionString.isEmpty() && m_versionString != m_version) {
      sl.push_back(versionString());
    }
  }

  // timestamp
  if (m_timestamp.isValid()) {
    sl.push_back(m_timestamp.toString(Qt::DateFormat::ISODate));
  } else {
    sl.push_back("(no timestamp)");
  }

  // md5
  if (!m_md5.isEmpty()) {
    sl.push_back(m_md5);
  }

  return sl.join(", ");
}

Module::FileInfo Module::getFileInfo() const
{
  const auto wspath = m_path.toStdWString();

  // getting version info size
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

  // getting version info
  auto buffer = std::make_unique<std::byte[]>(size);

  if (!GetFileVersionInfoW(wspath.c_str(), 0, size, buffer.get())) {
    const auto e = GetLastError();

    qCritical().nospace().noquote()
      << "GetFileVersionInfoW() failed on '" << m_path << "', "
      << formatSystemMessage(e);

    return {};
  }

  // the version info has two major parts: a fixed version and a localizable
  // set of strings

  FileInfo fi;
  fi.ffi = getFixedFileInfo(buffer.get());
  fi.fileDescription = getFileDescription(buffer.get());

  return fi;
}

VS_FIXEDFILEINFO Module::getFixedFileInfo(std::byte* buffer) const
{
  void* valuePointer = nullptr;
  unsigned int valueSize = 0;

  // the fixed version info is in the root
  const auto ret = VerQueryValueW(buffer, L"\\", &valuePointer, &valueSize);

  if (!ret || !valuePointer || valueSize == 0) {
    // not an error, no fixed file info
    return {};
  }

  const auto* fi = static_cast<VS_FIXEDFILEINFO*>(valuePointer);

  // signature is always 0xfeef04bd
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

  // getting list of available languages
  auto ret = VerQueryValueW(
    buffer, L"\\VarFileInfo\\Translation", &valuePointer, &valueSize);

  if (!ret || !valuePointer || valueSize == 0) {
    qCritical().nospace().noquote()
      << "VerQueryValueW() for translations failed on '" << m_path << "'";

    return {};
  }

  // number of languages
  const auto count = valueSize / sizeof(LANGANDCODEPAGE);
  if (count == 0) {
    return {};
  }

  // using the first language in the list to get FileVersion
  const auto* lcp = static_cast<LANGANDCODEPAGE*>(valuePointer);

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
    static_cast<wchar_t*>(valuePointer), valueSize - 1);
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
    // if the file info is invalid or doesn't have a date, use the creation
    // time on the file

    // opening the file
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

    // getting the file time
    if (!GetFileTime(h.get(), &ft, nullptr, nullptr)) {
      const auto e = GetLastError();
      qCritical().nospace().noquote()
        << "can't get file time for '" << m_path << "', "
        << formatSystemMessage(e);

      return {};
    }
  } else {
    // use the time from the file info
    ft.dwHighDateTime = fi.dwFileDateMS;
    ft.dwLowDateTime = fi.dwFileDateLS;
  }


  // converting to SYSTEMTIME
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

  // opening the file
  QFile f(m_path);

  if (!f.open(QFile::ReadOnly)) {
    qCritical().nospace().noquote()
      << "failed to open file '" << m_path << "' for md5";

    return {};
  }

  // hashing
  QCryptographicHash hash(QCryptographicHash::Md5);
  if (!hash.addData(&f)) {
    qCritical().nospace().noquote()
      << "failed to calculate md5 for '" << m_path << "'";

    return {};
  }

  return hash.result().toHex();
}


WindowsInfo::WindowsInfo()
{
  // loading ntdll.dll, the functions will be found with GetProcAddress()
  std::unique_ptr<HINSTANCE, LibraryFreer> ntdll(LoadLibraryW(L"ntdll.dll"));

  if (!ntdll) {
    qCritical() << "failed to load ntdll.dll while getting version";
    return;
  } else {
    m_reported = getReportedVersion(ntdll.get());
    m_real = getRealVersion(ntdll.get());
  }

  m_release = getRelease();
  m_elevated = getElevated();
}

bool WindowsInfo::compatibilityMode() const
{
  if (m_real == Version()) {
    // don't know the real version, can't guess compatibility mode
    return false;
  }

  return (m_real != m_reported);
}

const WindowsInfo::Version& WindowsInfo::reportedVersion() const
{
  return m_reported;
}

const WindowsInfo::Version& WindowsInfo::realVersion() const
{
  return m_real;
}

const WindowsInfo::Release& WindowsInfo::release() const
{
  return m_release;
}

std::optional<bool> WindowsInfo::isElevated() const
{
  return m_elevated;
}

QString WindowsInfo::toString() const
{
  QStringList sl;

  const QString reported = m_reported.toString();
  const QString real = m_real.toString();

  // version
  sl.push_back("version: " + reported);

  // real version if different
  if (compatibilityMode()) {
    sl.push_back("real version: " + real);
  }

  // build.UBR, such as 17763.557
  if (m_release.UBR != 0) {
    DWORD build = 0;

    if (compatibilityMode()) {
      build = m_real.build;
    } else {
      build = m_reported.build;
    }

    sl.push_back(QString("%1.%2").arg(build).arg(m_release.UBR));
  }

  // release ID
  if (!m_release.ID.isEmpty()) {
    sl.push_back("release " + m_release.ID);
  }

  // buildlab string
  if (!m_release.buildLab.isEmpty()) {
    sl.push_back(m_release.buildLab);
  }

  // product name
  if (!m_release.productName.isEmpty()) {
    sl.push_back(m_release.productName);
  }

  // elevated
  QString elevated = "?";
  if (m_elevated.has_value()) {
    elevated = (*m_elevated ? "yes" : "no");
  }

  sl.push_back("elevated: " + elevated);

  return sl.join(", ");
}

WindowsInfo::Version WindowsInfo::getReportedVersion(HINSTANCE ntdll) const
{
  // windows has been deprecating pretty much all the functions having to do
  // with getting version information because apparently, people keep misusing
  // them for feature detection
  //
  // there's still RtlGetVersion() though

  using RtlGetVersionType = NTSTATUS (NTAPI)(PRTL_OSVERSIONINFOW);

  auto* RtlGetVersion = reinterpret_cast<RtlGetVersionType*>(
    GetProcAddress(ntdll, "RtlGetVersion"));

  if (!RtlGetVersion) {
    qCritical() << "RtlGetVersion() not found in ntdll.dll";
    return {};
  }

  OSVERSIONINFOEX vi = {};
  vi.dwOSVersionInfoSize = sizeof(vi);

  // this apparently never fails
  RtlGetVersion((RTL_OSVERSIONINFOW*)&vi);

  return {vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber};
}

WindowsInfo::Version WindowsInfo::getRealVersion(HINSTANCE ntdll) const
{
  // getting the actual windows version is more difficult because all the
  // functions are lying when running in compatibility mode
  //
  // RtlGetNtVersionNumbers() is an undocumented function that seems to work
  // fine, but it might not in the future

  using RtlGetNtVersionNumbersType = void (NTAPI)(DWORD*, DWORD*, DWORD*);

  auto* RtlGetNtVersionNumbers = reinterpret_cast<RtlGetNtVersionNumbersType*>(
    GetProcAddress(ntdll, "RtlGetNtVersionNumbers"));

  if (!RtlGetNtVersionNumbers) {
    qCritical() << "RtlGetNtVersionNumbers not found in ntdll.dll";
    return {};
  }

  DWORD major=0, minor=0, build=0;
  RtlGetNtVersionNumbers(&major, &minor, &build);

  // for whatever reason, the build number has 0xf0000000 set
  build = 0x0fffffff & build;

  return {major, minor, build};
}

WindowsInfo::Release WindowsInfo::getRelease() const
{
  // there are several interesting items in the registry, but most of them
  // are undocumented, not always available, and localizable
  //
  // most of them are used to provide as much information as possible in case
  // any of the other versions fail to work

  QSettings settings(
    R"(HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion)",
    QSettings::NativeFormat);

  Release r;

  // buildlab seems to be an internal name from the build system
  r.buildLab = settings.value("BuildLabEx", "").toString();
  if (r.buildLab.isEmpty()) {
    r.buildLab = settings.value("BuildLab", "").toString();
    if (r.buildLab.isEmpty()) {
      r.buildLab = settings.value("BuildBranch", "").toString();
    }
  }

  // localized name of windows, such as "Windows 10 Pro"
  r.productName = settings.value("ProductName", "").toString();

  // release ID, such as 1803
  r.ID = settings.value("ReleaseId", "").toString();

  // some other build number, shown in winver.exe
  r.UBR = settings.value("UBR", 0).toUInt();

  return r;
}

std::optional<bool> WindowsInfo::getElevated() const
{
  std::unique_ptr<HANDLE, HandleCloser> token;

  {
    HANDLE rawToken = 0;

    if (!OpenProcessToken(GetCurrentProcess( ), TOKEN_QUERY, &rawToken)) {
      const auto e = GetLastError();

      qCritical()
        << "while trying to check if process is elevated, "
        << "OpenProcessToken() failed: " << formatSystemMessage(e);

      return {};
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

    return {};
  }

  return (e.TokenIsElevated != 0);
}


SecurityFeature::SecurityFeature(QString name, DWORD state)
  : m_name(std::move(name)), m_state(state)
{
}

const QString& SecurityFeature::name() const
{
  return m_name;
}

QString SecurityFeature::toString() const
{
  QString s;

  s += m_name + " ";

  const auto provider = (m_state >> 16) & 0xff;
  const auto scanner = (m_state >> 8) & 0xff;
  const auto definitions = m_state & 0xff;

  QStringList ps;
  if (provider & WSC_SECURITY_PROVIDER_FIREWALL) {
    ps.push_back("firewall");
  }

  if (provider & WSC_SECURITY_PROVIDER_AUTOUPDATE_SETTINGS) {
    ps.push_back("autoupdate");
  }

  if (provider & WSC_SECURITY_PROVIDER_ANTIVIRUS) {
    ps.push_back("antivirus");
  }

  if (provider & WSC_SECURITY_PROVIDER_ANTISPYWARE) {
    ps.push_back("antispyware");
  }

  if (provider & WSC_SECURITY_PROVIDER_INTERNET_SETTINGS) {
    ps.push_back("settings");
  }

  if (provider & WSC_SECURITY_PROVIDER_USER_ACCOUNT_CONTROL) {
    ps.push_back("uac");
  }

  if (provider & WSC_SECURITY_PROVIDER_SERVICE) {
    ps.push_back("service");
  }

  if (ps.empty()) {
    s += "(doesn't provide anything)";
  } else {
    s += "(" + ps.join("|") + ")";
  }

  if (scanner & 0x10) {
    s += ", active";
  } else {
    s += ", inactive";
  }

  if (definitions == 0) {
    s += ", definitions up to date";
  } else {
    s += ", definitions outdated";
  }

  return s;
}

} // namespace env

} // namespace MOShared
