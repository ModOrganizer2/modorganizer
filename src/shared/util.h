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

#ifndef UTIL_H
#define UTIL_H


#include <string>
#include <optional>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <versioninfo.h>
#include <QUuid>

class Executable;

namespace MOShared {

/// Test if a file (or directory) by the specified name exists
bool FileExists(const std::string &filename);
bool FileExists(const std::wstring &filename);

bool FileExists(const std::wstring &searchPath, const std::wstring &filename);

std::string ToString(const std::wstring &source, bool utf8);
std::wstring ToWString(const std::string &source, bool utf8);

std::string &ToLower(std::string &text);
std::string ToLower(const std::string &text);

std::wstring &ToLower(std::wstring &text);
std::wstring ToLower(const std::wstring &text);

bool CaseInsensitiveEqual(const std::wstring &lhs, const std::wstring &rhs);


namespace env
{

class Console
{
public:
  Console();
  ~Console();

private:
  bool m_hasConsole;
  FILE* m_in;
  FILE* m_out;
  FILE* m_err;
};


// an application shortcut that can be either on the desktop or the start menu
//
class Shortcut
{
public:
  // location of a shortcut
  //
  enum Locations
  {
    None = 0,

    // on the desktop
    Desktop,

    // in the start menu
    StartMenu
  };


  // empty shortcut
  //
  Shortcut();

  // shortcut from an executable
  //
  explicit Shortcut(const Executable& exe);

  // sets the name of the shortcut, shown on icons and start menu entries
  //
  Shortcut& name(const QString& s);

  // the program to start
  //
  Shortcut& target(const QString& s);

  // arguments to pass
  //
  Shortcut& arguments(const QString& s);

  // shows in the status bar of explorer, for example
  //
  Shortcut& description(const QString& s);

  // path to a binary that contains the icon and its index
  //
  Shortcut& icon(const QString& s, int index=0);

  // "start in" option for this shortcut
  //
  Shortcut& workingDirectory(const QString& s);


  // returns whether this shortcut already exists at the given location; this
  // does not check whether the shortcut parameters are different, it merely if
  // the .lnk file exists
  //
  bool exists(Locations loc) const;

  // calls remove() if exists(), or add()
  //
  bool toggle(Locations loc);

  // adds the shortcut to the given location
  //
  bool add(Locations loc);

  // removes the shortcut from the given location
  //
  bool remove(Locations loc);

private:
  QString m_name;
  QString m_target;
  QString m_arguments;
  QString m_description;
  QString m_icon;
  int m_iconIndex;
  QString m_workingDirectory;

  // returns a qCritical() logger with a prefix already logged
  //
  QDebug critical() const;

  // returns a qDebug() logger with a prefix already logged
  //
  QDebug debug() const;


  // returns the path where the shortcut file should be saved
  //
  QString shortcutPath(Locations loc) const;

  // returns the directory where the shortcut file should be saved
  //
  QString shortcutDirectory(Locations loc) const;

  // returns the filename of the shortcut file that should be used when saving
  //
  QString shortcutFilename() const;
};


// returns a string representation of the given location
//
QString toString(Shortcut::Locations loc);


// represents one module
//
class Module
{
public:
  explicit Module(QString path, std::size_t fileSize);

  // returns the module's path
  //
  const QString& path() const;

  // returns the module's path in lowercase and using forward slashes
  //
  QString displayPath() const;

  // returns the size in bytes, may be 0
  //
  std::size_t fileSize() const;

  // returns the x.x.x.x version embedded from the version info, may be empty
  //
  const QString& version() const;

  // returns the FileVersion entry from the resource file, returns
  // "(no version)" if not available
  //
  const QString& versionString() const;

  // returns the build date from the version info, or the creation time of the
  // file on the filesystem, may be empty
  //
  const QDateTime& timestamp() const;

  // returns the md5 of the file, may be empty for system files
  //
  const QString& md5() const;

  // converts timestamp() to a string for display, returns "(no timestamp)" if
  // not available
  //
  QString timestampString() const;

  // returns a string with all the above information on one line
  //
  QString toString() const;

private:
  // contains the information from the version resource
  //
  struct FileInfo
  {
    VS_FIXEDFILEINFO ffi;
    QString fileDescription;
  };

  QString m_path;
  std::size_t m_fileSize;
  QString m_version;
  QDateTime m_timestamp;
  QString m_versionString;
  QString m_md5;

  // returns information from the version resource
  //
  FileInfo getFileInfo() const;

  // uses VS_FIXEDFILEINFO to build the version string
  //
  QString getVersion(const VS_FIXEDFILEINFO& fi) const;

  // uses the file date from VS_FIXEDFILEINFO if available, or gets the
  // creation date on the file
  //
  QDateTime getTimestamp(const VS_FIXEDFILEINFO& fi) const;

  // returns the md5 hash unless the path contains "\windows\"
  //
  QString getMD5() const;

  // gets VS_FIXEDFILEINFO from the file version info buffer
  //
  VS_FIXEDFILEINFO getFixedFileInfo(std::byte* buffer) const;

  // gets FileVersion from the file version info buffer
  //
  QString getFileDescription(std::byte* buffer) const;
};


// a variety of information on windows
//
class WindowsInfo
{
public:
  struct Version
  {
    DWORD major=0, minor=0, build=0;

    QString toString() const
    {
      return QString("%1.%2.%3").arg(major).arg(minor).arg(build);
    }

    friend bool operator==(const Version& a, const Version& b)
    {
      return
        a.major == b.major &&
        a.minor == b.minor &&
        a.build == b.build;
    }

    friend bool operator!=(const Version& a, const Version& b)
    {
      return !(a == b);
    }
  };

  struct Release
  {
    // the BuildLab entry from the registry, may be empty
    QString buildLab;

    // product name such as "Windows 10 Pro", may not be in English, may be
    // empty
    QString productName;

    // release ID such as 1809, may be mepty
    QString ID;

    // some sub-build number, undocumented, may be empty
    DWORD UBR;

    Release()
      : UBR(0)
    {
    }
  };


  WindowsInfo();

  // tries to guess whether this process is running in compatibility mode
  //
  bool compatibilityMode() const;

  // returns the Windows version, may not correspond to the actual version
  // if the process is running in compatibility mode
  //
  const Version& reportedVersion() const;

  // tries to guess the real Windows version that's running, can be empty
  //
  const Version& realVersion() const;

  // various information about the current release
  //
  const Release& release() const;

  // whether this process is running as administrator, may be empty if the
  // information is not available
  std::optional<bool> isElevated() const;

  // returns a string with all the above information on one line
  //
  QString toString() const;

private:
  Version m_reported, m_real;
  Release m_release;
  std::optional<bool> m_elevated;

  // uses RtlGetVersion() to get the version number as reported by Windows
  //
  Version getReportedVersion(HINSTANCE ntdll) const;

  // uses RtlGetNtVersionNumbers() to get the real version number
  //
  Version getRealVersion(HINSTANCE ntdll) const;

  // gets various information from the registry
  //
  Release getRelease() const;

  // gets whether the process is elevated
  //
  std::optional<bool> getElevated() const;
};


// represents a security product, such as an antivirus or a firewall
//
class SecurityProduct
{
public:
  SecurityProduct(
    QUuid guid, QString name, int provider,
    bool active, bool upToDate);

  // display name of the product
  //
  const QString& name() const;

  // a bunch of _WSC_SECURITY_PROVIDER flags
  //
  int provider() const;

  // whether the product is active
  //
  bool active() const;

  // whether its definitions are up-to-date
  //
  bool upToDate() const;

  // string representation of the above
  //
  QString toString() const;

private:
  QUuid m_guid;
  QString m_name;
  int m_provider;
  bool m_active;
  bool m_upToDate;

  QString providerToString() const;
};


class Metrics
{
public:
  struct Display
  {
    int resX=0, resY=0, dpi=0;
    bool primary=false;
    int refreshRate = 0;
    QString monitor, adapter;

    QString toString() const;
  };

  Metrics();

  const std::vector<Display>& displays() const;

private:
  std::vector<Display> m_displays;
};


// represents the process's environment
//
class Environment
{
public:
  Environment();

  // list of loaded modules in the current process
  //
  const std::vector<Module>& loadedModules() const;

  // information about the operating system
  //
  const WindowsInfo& windowsInfo() const;

  // information about the installed security products
  //
  const std::vector<SecurityProduct>& securityProducts() const;

  // information about displays
  //
  const Metrics& metrics() const;

  // logs the environment
  //
  void dump() const;

private:
  std::vector<Module> m_modules;
  WindowsInfo m_windows;
  std::vector<SecurityProduct> m_security;
  Metrics m_metrics;

  std::vector<Module> getLoadedModules() const;
  std::vector<SecurityProduct> getSecurityProducts() const;

  std::vector<SecurityProduct> getSecurityProductsFromWMI() const;
  std::optional<SecurityProduct> getWindowsFirewall() const;
};


enum class CoreDumpTypes
{
  Mini = 1,
  Data,
  Full
};

// creates a minidump file for the given process
//
bool coredump(CoreDumpTypes type);

// finds another process with the same name as this one and creates a minidump
// file for it
//
bool coredumpOther(CoreDumpTypes type);

} // namespace env


MOBase::VersionInfo createVersionInfo();

} // namespace MOShared

#endif // UTIL_H
