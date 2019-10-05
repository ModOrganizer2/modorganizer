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
    default: return "Unknown";
  }
}

QString toString(SecurityZone z)
{
  return QString("%1 (%2)")
    .arg(toCodeName(z))
    .arg(static_cast<int>(z));
}

bool isZoneBlocked(SecurityZone z)
{
  return (z == SecurityZone::Internet || z == SecurityZone::Untrusted);
}

bool isFileBlocked(const QFileInfo& fi)
{
  const QString ads = "Zone.Identifier";
  const auto key = "ZoneTransfer/ZoneId";

  const auto path = fi.absoluteFilePath();
  const auto adsPath = path + ":" + ads;

  QFile f(adsPath);
  if (!f.exists()) {
    return false;
  }

  log::debug("'{}' has an ADS for {}", path, adsPath);

  const QSettings qs(adsPath, QSettings::IniFormat);

  if (!qs.contains(key)) {
    log::debug("'{}': key '{}' not found", adsPath, key);
    return false;
  }

  const auto v = qs.value(key);
  if (v.isNull()) {
    log::debug("'{}': key '{}' is null", adsPath, key);
    return false;
  }

  bool ok = false;
  const auto z = static_cast<SecurityZone>(v.toInt(&ok));

  if (!ok) {
    log::debug("'{}': key '{}' is not an int (value is '{}')", adsPath, key, v);
    return false;
  }

  if (!isZoneBlocked(z)) {
    log::debug("'{}': zone id is {}, which is fine", adsPath, toString(z));
    return false;
  }

  log::warn("'{}': file is blocked (zone id is {})", path, toString(z));
  return true;
}

void checkBlockedFiles(const QDir& dir)
{
  if (!dir.exists()) {
    log::error(
      "while checking for blocked files, directory '{}' not found",
      dir.absolutePath());

    return;
  }

  const auto files = dir.entryInfoList({"*.dll", "*.exe"}, QDir::Files);
  if (files.empty()) {
    log::error(
      "while checking for blocked files, directory '{}' is empty",
      dir.absolutePath());

    return;
  }

  for (auto&& fi : files) {
    isFileBlocked(fi);
  }
}

void checkBlocked()
{
  const QString appDir = QCoreApplication::applicationDirPath();

  const QDir dirs[] = {
    appDir,
    appDir + "/dlls",
    appDir + "/loot",
    appDir + "/NCC",
    appDir + "/platforms",
    appDir + "/plugins"
  };

  for (const auto& d : dirs) {
    checkBlockedFiles(d);
  }
}

void checkMissingFiles()
{
  // files that are likely to be eaten
  static const QStringList files({
    "helper.exe", "nxmhandler.exe",
    "usvfs_proxy_x64.exe", "usvfs_proxy_x86.exe",
    "usvfs_x64.dll", "usvfs_x86.dll"
    });

  const auto dir = QCoreApplication::applicationDirPath();

  for (const auto& name : files) {
    const QFileInfo file(dir + "/" + name);

    if (!file.exists()) {
      log::warn(
        "'{}' seems to be missing, an antivirus may have deleted it",
        file.absoluteFilePath());
    }
  }
}

void checkNahimic(const env::Environment& e)
{
  for (auto&& m : e.loadedModules()) {
    const QFileInfo file(m.path());

    if (file.fileName().compare("NahimicOSD.dll", Qt::CaseInsensitive) == 0) {
      log::warn(
        "NahimicOSD.dll is loaded. Nahimic is known to cause issues with "
        "Mod Organizer, such as freezing or blank windows. Consider "
        "uninstalling it.");

      break;
    }
  }
}

void sanityChecks(const env::Environment& e)
{
  log::debug("running sanity checks");

  checkBlocked();
  checkMissingFiles();
  checkNahimic(e);
}
