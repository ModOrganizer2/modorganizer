#include "envmodule.h"
#include "env.h"
#include <utility.h>
#include <log.h>

namespace env
{

using namespace MOBase;

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

    log::error(
      "GetFileVersionInfoSizeW() failed on '{}', {}",
      m_path, formatSystemMessage(e));

    return {};
  }

  // getting version info
  auto buffer = std::make_unique<std::byte[]>(size);

  if (!GetFileVersionInfoW(wspath.c_str(), 0, size, buffer.get())) {
    const auto e = GetLastError();

    log::error(
      "GetFileVersionInfoW() failed on '{}', {}",
      m_path, formatSystemMessage(e));

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
    log::error(
      "bad file info signature {:#x} for '{}'",
      fi->dwSignature, m_path);

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
    log::error("VerQueryValueW() for translations failed on '{}'", m_path);
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
    HandlePtr h(CreateFileW(
      m_path.toStdWString().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));

    if (h.get() == INVALID_HANDLE_VALUE) {
      const auto e = GetLastError();

      log::error(
        "can't open file '{}' for timestamp, {}",
        m_path, formatSystemMessage(e));

      return {};
    }

    // getting the file time
    if (!GetFileTime(h.get(), &ft, nullptr, nullptr)) {
      const auto e = GetLastError();

      log::error(
        "can't get file time for '{}', {}",
        m_path, formatSystemMessage(e));

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
    log::error(
      "FileTimeToSystemTime() failed on timestamp high={:#x} low={:#x} for '{}'",
      ft.dwHighDateTime, ft.dwLowDateTime, m_path);

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
    log::error("failed to open file '{}' for md5", m_path);
    return {};
  }

  // hashing
  QCryptographicHash hash(QCryptographicHash::Md5);
  if (!hash.addData(&f)) {
    log::error("failed to calculate md5 for '{}'", m_path);
    return {};
  }

  return hash.result().toHex();
}


std::vector<Module> getLoadedModules()
{
  HandlePtr snapshot(CreateToolhelp32Snapshot(
    TH32CS_SNAPMODULE32 | TH32CS_SNAPMODULE, GetCurrentProcessId()));

  if (snapshot.get() == INVALID_HANDLE_VALUE)
  {
    const auto e = GetLastError();
    log::error("CreateToolhelp32Snapshot() failed, {}", formatSystemMessage(e));
    return {};
  }

  MODULEENTRY32 me = {};
  me.dwSize = sizeof(me);

  // first module, this shouldn't fail because there's at least the executable
  if (!Module32First(snapshot.get(), &me))
  {
    const auto e = GetLastError();
    log::error("Module32First() failed, {}", formatSystemMessage(e));
    return {};
  }

  std::vector<Module> v;

  for (;;)
  {
    const auto path = QString::fromWCharArray(me.szExePath);
    if (!path.isEmpty()) {
      v.push_back(Module(path, me.modBaseSize));
    }

    // next module
    if (!Module32Next(snapshot.get(), &me)) {
      const auto e = GetLastError();

      // no more modules is not an error
      if (e != ERROR_NO_MORE_FILES) {
        log::error("Module32Next() failed, {}", formatSystemMessage(e));
      }

      break;
    }
  }

  // sorting by display name
  std::sort(v.begin(), v.end(), [](auto&& a, auto&& b) {
    return (a.displayPath().compare(b.displayPath(), Qt::CaseInsensitive) < 0);
    });

  return v;
}

} // namespace
