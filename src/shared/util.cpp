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
#include "executableslist.h"
#include "instancemanager.h"
#include <utility.h>
#include <log.h>

#include <sstream>
#include <locale>
#include <algorithm>
#include <set>
#include <filesystem>

#include <DbgHelp.h>
#include <boost/scoped_array.hpp>
#include <QApplication>

#include <comdef.h>
#include <Wbemidl.h>
#include <wscapi.h>
#include <netfw.h>
#include <shellscalingapi.h>

#pragma comment(lib, "Wbemuuid.lib")

using namespace MOBase;
namespace fs = std::filesystem;

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

using HandlePtr = std::unique_ptr<HANDLE, HandleCloser>;


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


template <class T>
using COMPtr = std::unique_ptr<T, COMReleaser>;


class ShellLinkException
{
public:
  ShellLinkException(QString s)
    : m_what(std::move(s))
  {
  }

  const QString& what() const
  {
    return m_what;
  }

private:
  QString m_what;
};

// just a wrapper around IShellLink operations that throws ShellLinkException
// on errors
//
class ShellLinkWrapper
{
public:
  ShellLinkWrapper()
  {
    m_link = createShellLink();
    m_file = createPersistFile();
  }

  void setPath(const QString& s)
  {
    if (s.isEmpty()) {
      throw ShellLinkException("path cannot be empty");
    }

    const auto r = m_link->SetPath(s.toStdWString().c_str());
    throwOnFail(r, QString("failed to set target path '%1'").arg(s));
  }

  void setArguments(const QString& s)
  {
    const auto r = m_link->SetArguments(s.toStdWString().c_str());
    throwOnFail(r, QString("failed to set arguments '%1'").arg(s));
  }

  void setDescription(const QString& s)
  {
    if (s.isEmpty()) {
      return;
    }

    const auto r = m_link->SetDescription(s.toStdWString().c_str());
    throwOnFail(r, QString("failed to set description '%1'").arg(s));
  }

  void setIcon(const QString& file, int i)
  {
    if (file.isEmpty()) {
      return;
    }

    const auto r = m_link->SetIconLocation(file.toStdWString().c_str(), i);
    throwOnFail(r, QString("failed to set icon '%1' @ %2").arg(file).arg(i));
  }

  void setWorkingDirectory(const QString& s)
  {
    if (s.isEmpty()) {
      return;
    }

    const auto r = m_link->SetWorkingDirectory(s.toStdWString().c_str());
    throwOnFail(r, QString("failed to set working directory '%1'").arg(s));
  }

  void save(const QString& path)
  {
    const auto r = m_file->Save(path.toStdWString().c_str(), TRUE);
    throwOnFail(r, QString("failed to save link '%1'").arg(path));
  }

private:
  COMPtr<IShellLink> m_link;
  COMPtr<IPersistFile> m_file;

  void throwOnFail(HRESULT r, const QString& s)
  {
    if (FAILED(r)) {
      throw ShellLinkException(QString("%1, %2")
        .arg(s)
        .arg(formatSystemMessageQ(r)));
    }
  }

  COMPtr<IShellLink> createShellLink()
  {
    void* link = nullptr;

    const auto r = CoCreateInstance(
      CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
      IID_IShellLink, &link);

    throwOnFail(r, "failed to create IShellLink instance");

    if (!link) {
      throw ShellLinkException("creating IShellLink worked, pointer is null");
    }

    return COMPtr<IShellLink>(static_cast<IShellLink*>(link));
  }

  COMPtr<IPersistFile> createPersistFile()
  {
    void* file = nullptr;

    const auto r = m_link->QueryInterface(IID_IPersistFile, &file);
    throwOnFail(r, "failed to get IPersistFile interface");

    if (!file) {
      throw ShellLinkException("querying IPersistFile worked, pointer is null");
    }

    return COMPtr<IPersistFile>(static_cast<IPersistFile*>(file));
  }
};


Console::Console()
  : m_hasConsole(false), m_in(nullptr), m_out(nullptr), m_err(nullptr)
{
  // open a console
  if (!AllocConsole()) {
    // failed, ignore
  }

  m_hasConsole = true;

  // redirect stdin, stdout and stderr to it
  freopen_s(&m_in, "CONIN$", "r", stdin);
  freopen_s(&m_out, "CONOUT$", "w", stdout);
  freopen_s(&m_err, "CONOUT$", "w", stderr);
}

Console::~Console()
{
  // close redirected handles and redirect standard stream to NUL in case
  // they're used after this

  if (m_err) {
    std::fclose(m_err);
    freopen_s(&m_err, "NUL", "w", stderr);
  }

  if (m_out) {
    std::fclose(m_out);
    freopen_s(&m_out, "NUL", "w", stdout);
  }

  if (m_in) {
    std::fclose(m_in);
    freopen_s(&m_in, "NUL", "r", stdin);
  }

  // close console
  if (m_hasConsole) {
    FreeConsole();
  }
}


Shortcut::Shortcut()
  : m_iconIndex(0)
{
}

Shortcut::Shortcut(const Executable& exe)
  : Shortcut()
{
  m_name = exe.title();
  m_target = QFileInfo(qApp->applicationFilePath()).absoluteFilePath();

  m_arguments = QString("\"moshortcut://%1:%2\"")
      .arg(InstanceManager::instance().currentInstance())
      .arg(exe.title());

  m_description = QString("Run %1 with ModOrganizer").arg(exe.title());

  if (exe.usesOwnIcon()) {
    m_icon = exe.binaryInfo().absoluteFilePath();
  }

  m_workingDirectory = qApp->applicationDirPath();
}

Shortcut& Shortcut::name(const QString& s)
{
  m_name = s;
  return *this;
}

Shortcut& Shortcut::target(const QString& s)
{
  m_target = s;
  return *this;
}

Shortcut& Shortcut::arguments(const QString& s)
{
  m_arguments = s;
  return *this;
}

Shortcut& Shortcut::description(const QString& s)
{
  m_description = s;
  return *this;
}

Shortcut& Shortcut::icon(const QString& s, int index)
{
  m_icon = s;
  m_iconIndex = index;
  return *this;
}

Shortcut& Shortcut::workingDirectory(const QString& s)
{
  m_workingDirectory = s;
  return *this;
}

bool Shortcut::exists(Locations loc) const
{
  const auto path = shortcutPath(loc);
  if (path.isEmpty()) {
    return false;
  }

  return QFileInfo(path).exists();
}

bool Shortcut::toggle(Locations loc)
{
  if (exists(loc)) {
    return remove(loc);
  } else {
    return add(loc);
  }
}

bool Shortcut::add(Locations loc)
{
  debug()
    << "adding shortcut to " << toString(loc) << ":\n"
    << "  . name: '" << m_name << "'\n"
    << "  . target: '" << m_target << "'\n"
    << "  . arguments: '" << m_arguments << "'\n"
    << "  . description: '" << m_description << "'\n"
    << "  . icon: '" << m_icon << "' @ " << m_iconIndex << "\n"
    << "  . working directory: '" << m_workingDirectory << "'";

  if (m_target.isEmpty()) {
    critical() << "target is empty";
    return false;
  }

  const auto path = shortcutPath(loc);
  if (path.isEmpty()) {
    return false;
  }

  debug() << "shorcut file will be saved at '" << path << "'";

  try
  {
    ShellLinkWrapper link;

    link.setPath(m_target);
    link.setArguments(m_arguments);
    link.setDescription(m_description);
    link.setIcon(m_icon, m_iconIndex);
    link.setWorkingDirectory(m_workingDirectory);

    link.save(path);

    return true;
  }
  catch(ShellLinkException& e)
  {
    critical() << e.what() << "\nshortcut file was not saved";
  }

  return false;
}

bool Shortcut::remove(Locations loc)
{
  debug() << "removing shortcut for '" << m_name << "' from " << toString(loc);

  const auto path = shortcutPath(loc);
  if (path.isEmpty()) {
    return false;
  }

  debug() << "path to shortcut file is '" << path << "'";

  if (!QFile::exists(path)) {
    critical() << "can't remove '" << path << "', file not found";
    return false;
  }

  if (!MOBase::shellDelete({path})) {
    const auto e = ::GetLastError();

    critical()
      << "failed to remove '" << path << "', "
      << formatSystemMessageQ(e);

    return false;
  }

  return true;
}

QString Shortcut::shortcutPath(Locations loc) const
{
  const auto dir = shortcutDirectory(loc);
  if (dir.isEmpty()) {
    return {};
  }

  const auto file = shortcutFilename();
  if (file.isEmpty()) {
    return {};
  }

  return dir + QDir::separator() + file;
}

QString Shortcut::shortcutDirectory(Locations loc) const
{
  QString dir;

  try
  {
    switch (loc)
    {
      case Desktop:
        dir = MOBase::getDesktopDirectory();
        break;

      case StartMenu:
        dir = MOBase::getStartMenuDirectory();
        break;

      case None:
      default:
        critical() << "bad location " << loc;
        break;
    }
  }
  catch(std::exception&)
  {
  }

  return QDir::toNativeSeparators(dir);
}

QString Shortcut::shortcutFilename() const
{
  if (m_name.isEmpty()) {
    critical() << "name is empty";
    return {};
  }

  return m_name + ".lnk";
}

QDebug Shortcut::debug() const
{
  return qDebug().noquote().nospace() << "system shortcut: ";
}

QDebug Shortcut::critical() const
{
  return qCritical().noquote().nospace() << "system shortcut: ";
}


QString toString(Shortcut::Locations loc)
{
  switch (loc)
  {
    case Shortcut::None:
      return "none";

    case Shortcut::Desktop:
      return "desktop";

    case Shortcut::StartMenu:
      return "start menu";

    default:
      return QString("? (%1)").arg(static_cast<int>(loc));
  }
}



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
    if (!enumerator) {
      return;
    }

    for (;;)
    {
      COMPtr<IWbemClassObject> object;

      {
        IWbemClassObject* rawObject = nullptr;
        ULONG count = 0;
        auto ret = enumerator->Next(WBEM_INFINITE, 1, &rawObject, &count);

        if (count == 0 || !rawObject) {
          break;
        }

        if (FAILED(ret)) {
          qCritical()
            << "enumerator->next() failed, " << formatSystemMessageQ(ret);
          break;
        }

        object.reset(rawObject);
      }

      f(object.get());
    }
  }

private:
  COMPtr<IWbemLocator> m_locator;
  COMPtr<IWbemServices> m_service;

  void createLocator()
  {
    void* rawLocator = nullptr;

    const auto ret = CoCreateInstance(
      CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
      IID_IWbemLocator, &rawLocator);

    if (FAILED(ret) || !rawLocator) {
      qCritical()
        << "CoCreateInstance for WbemLocator failed, "
        << formatSystemMessageQ(ret);

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

    if (FAILED(res) || !rawService) {
      qCritical()
        << "locator->ConnectServer() failed for namespace "
        << "'" << QString::fromStdString(ns) << "', "
        << formatSystemMessageQ(res);

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
        << "CoSetProxyBlanket() failed, " << formatSystemMessageQ(ret);

      throw failed();
    }
  }

  COMPtr<IEnumWbemClassObject> getEnumerator(
    const std::string& query)
  {
    IEnumWbemClassObject* rawEnumerator = NULL;

    auto ret = m_service->ExecQuery(
      bstr_t("WQL"),
      bstr_t(query.c_str()),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
      NULL,
      &rawEnumerator);

    if (FAILED(ret) || !rawEnumerator)
    {
      qCritical()
        << "query '" << QString::fromStdString(query) << "' failed, "
        << formatSystemMessageQ(ret);

      return {};
    }

    return COMPtr<IEnumWbemClassObject>(rawEnumerator);
  }
};


class DisplayEnumerator
{
public:
  DisplayEnumerator()
    : m_GetDpiForMonitor(nullptr)
  {
    m_shcore.reset(LoadLibraryW(L"Shcore.dll"));

    if (m_shcore) {
      // windows 8.1+ only
      m_GetDpiForMonitor = reinterpret_cast<GetDpiForMonitorFunction*>(
        GetProcAddress(m_shcore.get(), "GetDpiForMonitor"));
    }

    // gets all monitors and the device they're running on
    getDisplayDevices();
  }

  std::vector<Metrics::Display>&& displays() &&
  {
    return std::move(m_displays);
  }

  const std::vector<Metrics::Display>& displays() const &
  {
    return m_displays;
  }

private:
  using GetDpiForMonitorFunction =
    HRESULT WINAPI (HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

  std::unique_ptr<HINSTANCE, LibraryFreer> m_shcore;
  GetDpiForMonitorFunction* m_GetDpiForMonitor;
  std::vector<Metrics::Display> m_displays;

  void getDisplayDevices()
  {
    // don't bother if it goes over 100
    for (int i=0; i<100; ++i) {
      DISPLAY_DEVICEW device = {};
      device.cb = sizeof(device);

      if (!EnumDisplayDevicesW(nullptr, i, &device, 0)) {
        // no more
        break;
      }

      // EnumDisplayDevices() seems to be returning a lot of devices that are
      // not actually monitors, but those don't have the
      // DISPLAY_DEVICE_ATTACHED_TO_DESKTOP bit set
      if ((device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0) {
        continue;
      }

      m_displays.push_back(createDisplay(device));
    }
  }

  Metrics::Display createDisplay(const DISPLAY_DEVICEW& device)
  {
    Metrics::Display d;

    d.adapter = QString::fromWCharArray(device.DeviceString);
    d.monitor = QString::fromWCharArray(device.DeviceName);
    d.primary = (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE);

    getDisplaySettings(device.DeviceName, d);
    getDpi(d);

    return d;
  }

  void getDisplaySettings(const wchar_t* monitorName, Metrics::Display& d)
  {
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);

    if (!EnumDisplaySettingsW(monitorName, ENUM_CURRENT_SETTINGS, &dm)) {
      log::error("EnumDisplaySettings() failed for '{}'", d.monitor);
      return;
    }

    // all these fields should be available

    if (dm.dmFields & DM_DISPLAYFREQUENCY) {
      d.refreshRate = dm.dmDisplayFrequency;
    }

    if (dm.dmFields & DM_PELSWIDTH) {
      d.resX = dm.dmPelsWidth;
    }

    if (dm.dmFields & DM_PELSHEIGHT) {
      d.resY = dm.dmPelsHeight;
    }
  }

  void getDpi(Metrics::Display& d)
  {
    if (!m_GetDpiForMonitor) {
      // this happens on windows 7, get the desktop dpi instead
      getDesktopDpi(d);
      return;
    }

    // there's no way to get an HMONITOR from a device name, so all monitors
    // will have to be enumerated and their name checked
    HMONITOR hm = findMonitor(d.monitor);
    if (!hm) {
      log::error("can't get dpi for monitor '{}', not found", d.monitor);
      return;
    }

    UINT dpiX=0, dpiY=0;
    const auto r = m_GetDpiForMonitor(hm, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);

    if (FAILED(r)) {
      log::error(
        "GetDpiForMonitor() failed for '{}', {}",
        d.monitor, formatSystemMessageQ(r));

      return;
    }

    // dpiX and dpiY are always identical, as per the documentation
    d.dpi = dpiX;
  }

  void getDesktopDpi(Metrics::Display& d)
  {
    // desktop dc
    HDC dc = GetDC(0);

    if (!dc) {
      const auto e = GetLastError();
      log::error("can't get desktop DC, {}", formatSystemMessageQ(e));
      return;
    }

    d.dpi = GetDeviceCaps(dc, LOGPIXELSX);

    ReleaseDC(0, dc);
  }

  HMONITOR findMonitor(const QString& name)
  {
    // passed to the enumeration callback
    struct Data
    {
      DisplayEnumerator* self;
      QString name;
      HMONITOR hm;
    };

    Data data = {this, name, 0};

    // for each monitor
    EnumDisplayMonitors(0, nullptr, [](HMONITOR hm, HDC, RECT*, LPARAM lp) {
      auto& data = *reinterpret_cast<Data*>(lp);

      MONITORINFOEX mi = {};
      mi.cbSize = sizeof(mi);

      // monitor info will include the name
      if (!GetMonitorInfoW(hm, &mi)) {
        const auto e = GetLastError();
        log::error(
          "GetMonitorInfo() failed for '{}', {}",
          data.name, formatSystemMessageQ(e));

        // error for this monitor, but continue
        return TRUE;
      }

      if (QString::fromWCharArray(mi.szDevice) == data.name) {
        // found, stop
        data.hm = hm;
        return FALSE;
      }

      // not found, continue to the next monitor
      return TRUE;
    }, reinterpret_cast<LPARAM>(&data));

    return data.hm;
  }
};


Environment::Environment()
{
  m_modules = getLoadedModules();
  m_security = getSecurityProducts();
}

const std::vector<Module>& Environment::loadedModules() const
{
  return m_modules;
}

const WindowsInfo& Environment::windowsInfo() const
{
  return m_windows;
}

const std::vector<SecurityProduct>& Environment::securityProducts() const
{
  return m_security;
}

const Metrics& Environment::metrics() const
{
  return m_metrics;
}

void Environment::dump() const
{
  log::debug("windows: {}", windowsInfo().toString());

  if (windowsInfo().compatibilityMode()) {
    log::warn("MO seems to be running in compatibility mode");
  }

  log::debug("security products:");
  for (const auto& sp : securityProducts()) {
    log::debug("  . {}", sp.toString());
  }

  log::debug("modules loaded in process:");
  for (const auto& m : loadedModules()) {
    log::debug(" . {}", m.toString());
  }

  log::debug("displays:");
  for (const auto& d : m_metrics.displays()) {
    log::debug(" . {}", d.toString());
  }
}

std::vector<Module> Environment::getLoadedModules() const
{
  HandlePtr snapshot(CreateToolhelp32Snapshot(
    TH32CS_SNAPMODULE32 | TH32CS_SNAPMODULE, GetCurrentProcessId()));

  if (snapshot.get() == INVALID_HANDLE_VALUE)
  {
    const auto e = GetLastError();

    qCritical().nospace().noquote()
      << "CreateToolhelp32Snapshot() failed, "
      << formatSystemMessageQ(e);

    return {};
  }

  MODULEENTRY32 me = {};
  me.dwSize = sizeof(me);

  // first module, this shouldn't fail because there's at least the executable
  if (!Module32First(snapshot.get(), &me))
  {
    const auto e = GetLastError();

    qCritical().nospace().noquote()
      << "Module32First() failed, " << formatSystemMessageQ(e);

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
       qCritical().nospace().noquote()
        << "Module32Next() failed, " << formatSystemMessageQ(e);
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

std::vector<SecurityProduct> Environment::getSecurityProducts() const
{
  std::vector<SecurityProduct> v;

  {
    auto fromWMI = getSecurityProductsFromWMI();
    v.insert(
      v.end(),
      std::make_move_iterator(fromWMI.begin()),
      std::make_move_iterator(fromWMI.end()));
  }

  if (auto p=getWindowsFirewall()) {
    v.push_back(std::move(*p));
  }

  return v;
}

std::vector<SecurityProduct> Environment::getSecurityProductsFromWMI() const
{
  // some products may be present in multiple queries, such as a product marked
  // as both antivirus and antispyware, but they'll have the same GUID, so use
  // that to avoid duplicating entries
  std::map<QUuid, SecurityProduct> map;

  auto handleProduct = [&](auto* o) {
    VARIANT prop;

    // display name
    auto ret = o->Get(L"displayName", 0, &prop, 0, 0);
    if (FAILED(ret)) {
      qCritical()
        << "failed to get displayName, "
        << formatSystemMessageQ(ret);

      return;
    }

    if (prop.vt != VT_BSTR) {
      qCritical() << "displayName is a " << prop.vt << ", not a bstr";
      return;
    }

    const std::wstring name = prop.bstrVal;
    VariantClear(&prop);

    // product state
    ret = o->Get(L"productState", 0, &prop, 0, 0);
    if (FAILED(ret)) {
      qCritical()
        << "failed to get productState, "
        << formatSystemMessageQ(ret);

      return;
    }

    if (prop.vt != VT_UI4 && prop.vt != VT_I4) {
      qCritical() << "productState is a " << prop.vt << ", is not a VT_UI4";
      return;
    }

    DWORD state = 0;
    if (prop.vt == VT_I4) {
      state = prop.lVal;
    } else {
      state = prop.ulVal;
    }

    VariantClear(&prop);

    // guid
    ret = o->Get(L"instanceGuid", 0, &prop, 0, 0);
    if (FAILED(ret)) {
      qCritical()
        << "failed to get instanceGuid, "
        << formatSystemMessageQ(ret);

      return;
    }

    if (prop.vt != VT_BSTR) {
      qCritical() << "instanceGuid is a " << prop.vt << ", is not a bstr";
      return;
    }

    const QUuid guid(QString::fromWCharArray(prop.bstrVal));
    VariantClear(&prop);

    const auto provider = static_cast<int>((state >> 16) & 0xff);
    const auto scanner = (state >> 8) & 0xff;
    const auto definitions = state & 0xff;

    const bool active = ((scanner & 0x10) != 0);
    const bool upToDate = (definitions == 0);

    map.insert({
      guid,
      {QString::fromStdWString(name), provider, active, upToDate}});
  };

  {
    WMI wmi("root\\SecurityCenter2");
    wmi.query("select * from AntivirusProduct", handleProduct);
    wmi.query("select * from FirewallProduct", handleProduct);
    wmi.query("select * from AntiSpywareProduct", handleProduct);
  }

  {
    WMI wmi("root\\SecurityCenter");
    wmi.query("select * from AntivirusProduct", handleProduct);
    wmi.query("select * from FirewallProduct", handleProduct);
    wmi.query("select * from AntiSpywareProduct", handleProduct);
  }

  std::vector<SecurityProduct> v;

  for (auto&& p : map) {
    v.push_back(p.second);
  }

  return v;
}

std::optional<SecurityProduct> Environment::getWindowsFirewall() const
{
  HRESULT hr = 0;

  COMPtr<INetFwPolicy2> policy;

  {
    void* rawPolicy = nullptr;

    hr = CoCreateInstance(
      __uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER,
      __uuidof(INetFwPolicy2), &rawPolicy);

    if (FAILED(hr) || !rawPolicy) {
      qCritical()
        << "CoCreateInstance for NetFwPolicy2 failed, "
        << formatSystemMessageQ(hr);

      return {};
    }

    policy.reset(static_cast<INetFwPolicy2*>(rawPolicy));
  }

  VARIANT_BOOL enabledVariant;

  if (policy) {
    hr = policy->get_FirewallEnabled(NET_FW_PROFILE2_PUBLIC, &enabledVariant);
    if (FAILED(hr))
    {
      qCritical()
        << "get_FirewallEnabled failed, "
        << formatSystemMessageQ(hr);

      return {};
    }
  }

  const auto enabled = (enabledVariant != VARIANT_FALSE);
  if (!enabled) {
    return {};
  }

  return SecurityProduct(
    "Windows Firewall", WSC_SECURITY_PROVIDER_FIREWALL, true, true);
}


Metrics::Metrics()
{
  m_displays = DisplayEnumerator().displays();
}

const std::vector<Metrics::Display>& Metrics::displays() const
{
  return m_displays;
}

QString Metrics::Display::toString() const
{
  return QString("%1*%2 %3hz dpi=%4 on %5%6")
    .arg(resX)
    .arg(resY)
    .arg(refreshRate)
    .arg(dpi)
    .arg(adapter)
    .arg(primary ? " (primary)" : "");
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
      << formatSystemMessageQ(e);

    return {};
  }

  // getting version info
  auto buffer = std::make_unique<std::byte[]>(size);

  if (!GetFileVersionInfoW(wspath.c_str(), 0, size, buffer.get())) {
    const auto e = GetLastError();

    qCritical().nospace().noquote()
      << "GetFileVersionInfoW() failed on '" << m_path << "', "
      << formatSystemMessageQ(e);

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
    HandlePtr h(CreateFileW(
      m_path.toStdWString().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));

    if (h.get() == INVALID_HANDLE_VALUE) {
      const auto e = GetLastError();

      qCritical().nospace().noquote()
        << "can't open file '" << m_path << "' for timestamp, "
        << formatSystemMessageQ(e);

      return {};
    }

    // getting the file time
    if (!GetFileTime(h.get(), &ft, nullptr, nullptr)) {
      const auto e = GetLastError();
      qCritical().nospace().noquote()
        << "can't get file time for '" << m_path << "', "
        << formatSystemMessageQ(e);

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
  sl.push_back("version " + reported);

  // real version if different
  if (compatibilityMode()) {
    sl.push_back("real version " + real);
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
  HandlePtr token;

  {
    HANDLE rawToken = 0;

    if (!OpenProcessToken(GetCurrentProcess( ), TOKEN_QUERY, &rawToken)) {
      const auto e = GetLastError();

      qCritical()
        << "while trying to check if process is elevated, "
        << "OpenProcessToken() failed: " << formatSystemMessageQ(e);

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
      << "GetTokenInformation() failed: " << formatSystemMessageQ(e);

    return {};
  }

  return (e.TokenIsElevated != 0);
}


SecurityProduct::SecurityProduct(
  QString name, int provider,
  bool active, bool upToDate) :
    m_name(std::move(name)), m_provider(provider),
    m_active(active), m_upToDate(upToDate)
{
}

const QString& SecurityProduct::name() const
{
  return m_name;
}

int SecurityProduct::provider() const
{
  return m_provider;
}

bool SecurityProduct::active() const
{
  return m_active;
}

bool SecurityProduct::upToDate() const
{
  return m_upToDate;
}

QString SecurityProduct::toString() const
{
  QString s;

  s += m_name + " ";


  QStringList ps;
  if (m_provider & WSC_SECURITY_PROVIDER_FIREWALL) {
    ps.push_back("firewall");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_AUTOUPDATE_SETTINGS) {
    ps.push_back("autoupdate");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_ANTIVIRUS) {
    ps.push_back("antivirus");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_ANTISPYWARE) {
    ps.push_back("antispyware");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_INTERNET_SETTINGS) {
    ps.push_back("settings");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_USER_ACCOUNT_CONTROL) {
    ps.push_back("uac");
  }

  if (m_provider & WSC_SECURITY_PROVIDER_SERVICE) {
    ps.push_back("service");
  }

  if (ps.empty()) {
    s += "(doesn't provide anything)";
  } else {
    s += "(" + ps.join("|") + ")";
  }

  if (!m_active) {
    s += ", inactive";
  }

  if (!m_upToDate) {
    s += ", definitions outdated";
  }

  return s;
}


struct Process
{
  std::wstring filename;
  DWORD pid;

  Process(std::wstring f, DWORD id)
    : filename(std::move(f)), pid(id)
  {
  }
};

// returns the filename of the given process or the current one
//
std::wstring processFilename(HANDLE process=INVALID_HANDLE_VALUE)
{
  // double the buffer size 10 times
  const int MaxTries = 10;

  DWORD bufferSize = MAX_PATH;

  for (int tries=0; tries<MaxTries; ++tries)
  {
    auto buffer = std::make_unique<wchar_t[]>(bufferSize + 1);
    std::fill(buffer.get(), buffer.get() + bufferSize + 1, 0);

    DWORD writtenSize = 0;

    if (process == INVALID_HANDLE_VALUE) {
      // query this process
      writtenSize = GetModuleFileNameW(0, buffer.get(), bufferSize);
    } else {
      // query another process
      writtenSize = GetModuleBaseNameW(process, 0, buffer.get(), bufferSize);
    }

    if (writtenSize == 0) {
      // hard failure
      const auto e = GetLastError();
      std::wcerr << formatSystemMessage(e) << L"\n";
      break;
    } else if (writtenSize >= bufferSize) {
      // buffer is too small, try again
      bufferSize *= 2;
    } else {
      // if GetModuleFileName() works, `writtenSize` does not include the null
      // terminator
      const std::wstring s(buffer.get(), writtenSize);
      const fs::path path(s);

      return path.filename().native();
    }
  }

  // something failed or the path is way too long to make sense

  std::wstring what;
  if (process == INVALID_HANDLE_VALUE) {
    what = L"the current process";
  } else {
    what = L"pid " + std::to_wstring(reinterpret_cast<std::uintptr_t>(process));
  }

  std::wcerr << L"failed to get filename for " << what << L"\n";
  return {};
}

std::vector<DWORD> runningProcessesIds()
{
  // double the buffer size 10 times
  const int MaxTries = 10;

  // initial size of 300 processes, unlikely to be more than that
  std::size_t size = 300;

  for (int tries=0; tries<MaxTries; ++tries) {
    auto ids = std::make_unique<DWORD[]>(size);
    std::fill(ids.get(), ids.get() + size, 0);

    DWORD bytesGiven = static_cast<DWORD>(size * sizeof(ids[0]));
    DWORD bytesWritten = 0;

    if (!EnumProcesses(ids.get(), bytesGiven, &bytesWritten))
    {
      const auto e = GetLastError();

      std::wcerr
        << L"failed to enumerate processes, "
        << formatSystemMessage(e) << L"\n";

      return {};
    }

    if (bytesWritten == bytesGiven) {
      // no way to distinguish between an exact fit and not enough space,
      // just try again
      size *= 2;
      continue;
    }

    const auto count = bytesWritten / sizeof(ids[0]);
    return std::vector<DWORD>(ids.get(), ids.get() + count);
  }

  std::cerr << L"too many processes to enumerate";
  return {};
}

std::vector<Process> runningProcesses()
{
  const auto pids = runningProcessesIds();
  std::vector<Process> v;

  for (const auto& pid : pids) {
    if (pid == 0) {
      // the idle process has pid 0 and seems to be picked up by EnumProcesses()
      continue;
    }

    HandlePtr h(OpenProcess(
      PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));

    if (!h) {
      const auto e = GetLastError();

      if (e != ERROR_ACCESS_DENIED) {
        // don't log access denied, will happen a lot for system processes, even
        // when elevated
        std::wcerr
          << L"failed to open process " << pid << L", "
          << formatSystemMessage(e) << L"\n";
      }

      continue;
    }

    auto filename = processFilename(h.get());
    if (!filename.empty()) {
      v.emplace_back(std::move(filename), pid);
    }
  }

  return v;
}

DWORD findOtherPid()
{
  const std::wstring defaultName = L"ModOrganizer.exe";

  std::wclog << L"looking for the other process...\n";

  // used to skip the current process below
  const auto thisPid = GetCurrentProcessId();
  std::wclog << L"this process id is " << thisPid << L"\n";

  // getting the filename for this process, assumes the other process has the
  // smae one
  auto filename = processFilename();
  if (filename.empty()) {
    std::wcerr
      << L"can't get current process filename, defaulting to "
      << defaultName << L"\n";

    filename = defaultName;
  } else {
    std::wclog << L"this process filename is " << filename << L"\n";
  }

  // getting all running processes
  const auto processes = runningProcesses();
  std::wclog << L"there are " << processes.size() << L" processes running\n";

  // going through processes, trying to find one with the same name and a
  // different pid than this process has
  for (const auto& p : processes) {
    if (p.filename == filename) {
      if (p.pid != thisPid) {
        return p.pid;
      }
    }
  }

  std::wclog
    << L"no process with this filename\n"
    << L"MO may not be running, or it may be running as administrator\n"
    << L"you can try running this again as administrator\n";

  return 0;
}

std::wstring tempDir()
{
  const DWORD bufferSize = MAX_PATH + 1;
  wchar_t buffer[bufferSize + 1] = {};

  const auto written = GetTempPathW(bufferSize, buffer);
  if (written == 0) {
    const auto e = GetLastError();

    std::wcerr
      << L"failed to get temp path, " << formatSystemMessage(e) << L"\n";

    return {};
  }

  // `written` does not include the null terminator
  return std::wstring(buffer, buffer + written);
}

HandlePtr tempFile(const std::wstring dir)
{
  // maximum tries of incrementing the counter
  const int MaxTries = 100;

  // UTC time and date will be in the filename
  const auto now = std::time(0);
  const auto tm = std::gmtime(&now);

  // "ModOrganizer-YYYYMMDDThhmmss.dmp", with a possible "-i" appended, where
  // i can go until MaxTries
  std::wostringstream oss;
  oss
    << L"ModOrganizer-"
    << std::setw(4) << (1900 + tm->tm_year)
    << std::setw(2) << std::setfill(L'0') << (tm->tm_mon + 1)
    << std::setw(2) << std::setfill(L'0') << tm->tm_mday << "T"
    << std::setw(2) << std::setfill(L'0') << tm->tm_hour
    << std::setw(2) << std::setfill(L'0') << tm->tm_min
    << std::setw(2) << std::setfill(L'0') << tm->tm_sec;

  const std::wstring prefix = oss.str();
  const std::wstring ext = L".dmp";

  // first path to try, without counter in it
  std::wstring path = dir + L"\\" + prefix + ext;

  for (int i=0; i<MaxTries; ++i) {
    std::wclog << L"trying file '" << path << L"'\n";

    HandlePtr h (CreateFileW(
      path.c_str(), GENERIC_WRITE, 0, nullptr,
      CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));

    if (h.get() != INVALID_HANDLE_VALUE) {
      // worked
      return h;
    }

    const auto e = GetLastError();

    if (e != ERROR_FILE_EXISTS) {
      // probably no write access
      std::wcerr
        << L"failed to create dump file, " << formatSystemMessage(e) << L"\n";

      return {};
    }

    // try again with "-i"
    path = dir + L"\\" + prefix + L"-" + std::to_wstring(i + 1) + ext;
  }

  std::wcerr << L"can't create dump file, ran out of filenames\n";
  return {};
}

HandlePtr dumpFile()
{
  // try the current directory
  HandlePtr h = tempFile(L".");
  if (h.get() != INVALID_HANDLE_VALUE) {
    return h;
  }

  std::wclog << L"cannot write dump file in current directory\n";

  // try the temp directory
  const auto dir = tempDir();

  if (!dir.empty()) {
    h = tempFile(dir.c_str());
    if (h.get() != INVALID_HANDLE_VALUE) {
      return h;
    }
  }

  return {};
}

bool createMiniDump(HANDLE process, CoreDumpTypes type)
{
  const DWORD pid = GetProcessId(process);

  const HandlePtr file = dumpFile();
  if (!file) {
    std::wcerr << L"nowhere to write the dump file\n";
    return false;
  }

  auto flags = _MINIDUMP_TYPE(
    MiniDumpNormal |
    MiniDumpWithHandleData |
    MiniDumpWithUnloadedModules |
    MiniDumpWithProcessThreadData);

  if (type == CoreDumpTypes::Data) {
    std::wclog << L"writing minidump with data\n";
    flags = _MINIDUMP_TYPE(flags | MiniDumpWithDataSegs);
  } else if (type ==  CoreDumpTypes::Full) {
    std::wclog << L"writing full minidump\n";
    flags = _MINIDUMP_TYPE(flags | MiniDumpWithFullMemory);
  } else {
    std::wclog << L"writing mini minidump\n";
  }

  const auto ret = MiniDumpWriteDump(
    process, pid, file.get(), flags, nullptr, nullptr, nullptr);

  if (!ret) {
    const auto e = GetLastError();

    std::wcerr
      << L"failed to write mini dump, " << formatSystemMessage(e) << L"\n";

    return false;
  }

  std::wclog << L"minidump written correctly\n";
  return true;
}


bool coredump(CoreDumpTypes type)
{
  std::wclog << L"creating minidump for the current process\n";
  return createMiniDump(GetCurrentProcess(), type);
}

bool coredumpOther(CoreDumpTypes type)
{
  std::wclog << L"creating minidump for an running process\n";

  const auto pid = findOtherPid();
  if (pid == 0) {
    std::wcerr << L"no other process found\n";
    return false;
  }

  std::wclog << L"found other process with pid " << pid << L"\n";

  HandlePtr handle(OpenProcess(
    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));

  if (!handle) {
    const auto e = GetLastError();

    std::wcerr
      << L"failed to open process " << pid << L", "
      << formatSystemMessage(e) << L"\n";

    return false;
  }

  return createMiniDump(handle.get(), type);
}

} // namespace env

} // namespace MOShared
