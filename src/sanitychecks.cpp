#include "env.h"
#include "envmodule.h"
#include <log.h>

using namespace MOBase;

enum class SecurityZone
{
  NoZone = -1,
  MyComputer = 0,
  Intranet = 1,
  Trusted = 2,
  Internet = 3,
  Untrusted = 4,
};

QString toCodeName(SecurityZone z)
{
  switch (z)
  {
    case SecurityZone::NoZone: return "NoZone";
    case SecurityZone::MyComputer: return "MyComputer";
    case SecurityZone::Intranet: return "Intranet";
    case SecurityZone::Trusted: return "Trusted";
    case SecurityZone::Internet: return "Internet";
    case SecurityZone::Untrusted: return "Untrusted";
    default: return "Unknown zone";
  }
}

QString toString(SecurityZone z)
{
  return QString("%1 (%2)")
    .arg(toCodeName(z))
    .arg(static_cast<int>(z));
}

// whether the given zone is considered blocked
//
bool isZoneBlocked(SecurityZone z)
{
  switch (z)
  {
    case SecurityZone::Internet:
    case SecurityZone::Untrusted:
      return true;

    case SecurityZone::NoZone:
    case SecurityZone::MyComputer:
    case SecurityZone::Intranet:
    case SecurityZone::Trusted:
    default:
      return false;
  }
}

// whether the given file is blocked
//
bool isFileBlocked(const QFileInfo& fi)
{
  // name of the alternate data stream containing the zone identifier ini
  const QString ads = "Zone.Identifier";

  // key in the ini
  const auto key = "ZoneTransfer/ZoneId";

  // the path to the ADS is always `filename:Zone.Identifier`
  const auto path = fi.absoluteFilePath();
  const auto adsPath = path + ":" + ads;

  QFile f(adsPath);
  if (!f.exists()) {
    // no ADS for this file
    return false;
  }

  log::debug("'{}' has an ADS for {}", path, adsPath);

  const QSettings qs(adsPath, QSettings::IniFormat);

  // looking for key
  if (!qs.contains(key)) {
    log::debug("'{}': key '{}' not found", adsPath, key);
    return false;
  }

  // getting value
  const auto v = qs.value(key);
  if (v.isNull()) {
    log::debug("'{}': key '{}' is null", adsPath, key);
    return false;
  }

  // should be an int
  bool ok = false;
  const auto z = static_cast<SecurityZone>(v.toInt(&ok));

  if (!ok) {
    log::debug("'{}': key '{}' is not an int (value is '{}')", adsPath, key, v);
    return false;
  }

  if (!isZoneBlocked(z)) {
    // that zone is not a blocked zone
    log::debug("'{}': zone id is {}, which is fine", adsPath, toString(z));
    return false;
  }

  // file is blocked
  log::warn("'{}': file is blocked, zone id is {}", path, toString(z));
  return true;
}

int checkBlockedFiles(const QDir& dir)
{
  // executables file types
  const QStringList FileTypes = {"*.dll", "*.exe"};

  if (!dir.exists()) {
    // shouldn't happen
    log::error(
      "while checking for blocked files, directory '{}' not found",
      dir.absolutePath());

    return 1;
  }

  const auto files = dir.entryInfoList(FileTypes, QDir::Files);
  if (files.empty()) {
    // shouldn't happen
    log::error(
      "while checking for blocked files, directory '{}' is empty",
      dir.absolutePath());

    return 1;
  }

  int n = 0;

  // checking each file in this directory
  for (auto&& fi : files) {
    if (isFileBlocked(fi)) {
      ++n;
    }
  }

  return n;
}

int checkBlocked()
{
  // directories that contain executables; these need to be explicit because
  // portable instances might add billions of files in MO's directory
  const QString dirs[] = {
    ".",
    "/dlls",
    "/loot",
    "/NCC",
    "/platforms",
    "/plugins"
  };

  log::debug("  . blocked files");
  const QString appDir = QCoreApplication::applicationDirPath();

  int n = 0;

  for (const auto& d : dirs) {
    const auto path = QDir(appDir + "/" + d).canonicalPath();
    n += checkBlockedFiles(path);
  }

  return n;
}

int checkMissingFiles()
{
  // files that are likely to be eaten
  static const QStringList files({
    "helper.exe", "nxmhandler.exe",
    "usvfs_proxy_x64.exe", "usvfs_proxy_x86.exe",
    "usvfs_x64.dll", "usvfs_x86.dll"
    });

  log::debug("  . missing files");
  const auto dir = QCoreApplication::applicationDirPath();

  int n = 0;

  for (const auto& name : files) {
    const QFileInfo file(dir + "/" + name);

    if (!file.exists()) {
      log::warn(
        "'{}' seems to be missing, an antivirus may have deleted it",
        file.absoluteFilePath());

      ++n;
    }
  }

  return n;
}

bool checkNahimic(const env::Environment& e)
{
  // Nahimic seems to interfere mostly with dialogs, like the mod info dialog:
  // it renders dialogs fully white and makes it impossible to interact with
  // them
  //
  // NahimicOSD.dll is usually loaded on startup, but there has been some
  // reports where it got loaded later, so this check is not entirely accurate

  for (auto&& m : e.loadedModules()) {
    const QFileInfo file(m.path());

    if (file.fileName().compare("NahimicOSD.dll", Qt::CaseInsensitive) == 0) {
      log::warn(
        "NahimicOSD.dll is loaded. Nahimic is known to cause issues with "
        "Mod Organizer, such as freezing or blank windows. Consider "
        "uninstalling it.");

      return true;
    }
  }

  return false;
}

int checkIncompatibilities(const env::Environment& e)
{
  log::debug("  . incompatibilities");

  int n = 0;

  if (checkNahimic(e)) {
    ++n;
  }

  return n;
}

void sanityChecks(const env::Environment& e)
{
  log::debug("running sanity checks...");

  int n = 0;

  n += checkBlocked();
  n += checkMissingFiles();
  n += checkIncompatibilities(e);

  log::debug(
    "sanity checks done, {}",
    (n > 0 ? "problems were found" : "everything looks okay"));
}
