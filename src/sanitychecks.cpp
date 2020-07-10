#include "env.h"
#include "envmodule.h"
#include "settings.h"
#include <iplugingame.h>
#include <log.h>
#include <utility.h>

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
  log::warn("{}", QObject::tr(
    "'%1': file is blocked (%2)")
    .arg(path)
    .arg(toString(z)));

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
    "helper.exe",
    "nxmhandler.exe",
    "usvfs_proxy_x64.exe",
    "usvfs_proxy_x86.exe",
    "usvfs_x64.dll",
    "usvfs_x86.dll",
    "loot/loot.dll",
    "loot/lootcli.exe"
    });

  log::debug("  . missing files");
  const auto dir = QCoreApplication::applicationDirPath();

  int n = 0;

  for (const auto& name : files) {
    const QFileInfo file(dir + "/" + name);

    if (!file.exists()) {
      log::warn("{}", QObject::tr(
        "'%1' seems to be missing, an antivirus may have deleted it")
        .arg(file.absoluteFilePath()));

      ++n;
    }
  }

  return n;
}

int checkBadOSDs(const env::Module& m)
{
  // these dlls seems to interfere mostly with dialogs, like the mod info
  // dialog: it renders dialogs fully white and makes it impossible to interact
  // with them
  //
  // the dlls is usually loaded on startup, but there has been some  reports
  // where it got loaded later, so this is also called every time a new module
  // is loaded into this process

  const char* nahimic =
    "Nahimic (also known as SonicSuite, SonicRadar, SteelSeries, A-Volute, etc.)";

  static const std::map<QString, QString> names = {
    {"NahimicOSD.dll",         nahimic},
    {"nahimicmsiosd.dll",      nahimic},
    {"cassinimlkosd.dll",      nahimic},
    {"SS3DevProps.dll",        nahimic},
    {"ss2devprops.dll",        nahimic},
    {"ss2osd.dll",             nahimic},
    {"RTSSHooks64.dll",        "RivaTuner Statistics Server"},
    {"SSAudioOSD.dll",         "SteelSeries Audio"},
    {"specialk64.dll",         "SpecialK"},
    {"corsairosdhook.x64.dll", "Corsair Utility Engine"},
    {"gtii-osd64-vk.dll",      "ASUS GPU Tweak 2"},
    {"easyhook64.dll",         "Razer Cortex"},
    {"k_fps64.dll",            "Razer Cortex"}
  };

  const QFileInfo file(m.path());
  int n = 0;

  for (auto&& p : names) {
    if (file.fileName().compare(p.first, Qt::CaseInsensitive) == 0) {
      log::warn("{}", QObject::tr(
        "%1 is loaded.\nThis program is known to cause issues with "
        "Mod Organizer, such as freezing or blank windows. Consider "
        "uninstalling it.")
        .arg(p.second));

      log::warn("{}", file.absoluteFilePath());

      ++n;
    }
  }

  return n;
}

int checkUsvfsIncompatibilites(const env::Module& m)
{
  // these dlls seems to interfere with usvfs

  static const std::map<QString, QString> names = {
    {"mactype64.dll", "Mactype"}
  };

  const QFileInfo file(m.path());
  int n = 0;

  for (auto&& p : names) {
    if (file.fileName().compare(p.first, Qt::CaseInsensitive) == 0) {
      log::warn("{}", QObject::tr(
        "%1 is loaded. This program is known to cause issues with "
        "Mod Organizer and its virtual filesystem, such script extenders "
        "refusing to run. Consider uninstalling it.")
        .arg(p.second));

      log::warn("{}", file.absoluteFilePath());

      ++n;
    }
  }

  return n;
}

int checkIncompatibleModule(const env::Module& m)
{
  int n = 0;

  n += checkBadOSDs(m);
  n += checkUsvfsIncompatibilites(m);

  return n;
}

int checkIncompatibilities(const env::Environment& e)
{
  log::debug("  . incompatibilities");

  int n = 0;

  for (auto&& m : e.loadedModules()) {
    n += checkIncompatibleModule(m);
  }

  return n;
}

std::vector<std::pair<QString, QString>> getSystemDirectories()
{
  // folder ids and display names for logging
  const std::vector<std::pair<GUID, QString>> systemFolderIDs = {
    {FOLDERID_ProgramFiles, "in Program Files"},
    {FOLDERID_ProgramFilesX86, "in Program Files"},
    {FOLDERID_Desktop, "on the desktop"},
    {FOLDERID_OneDrive, "in OneDrive"},
    {FOLDERID_Documents, "in Documents"},
    {FOLDERID_Downloads, "in Downloads"}
  };

  std::vector<std::pair<QString, QString>> systemDirs;

  for (auto&& p : systemFolderIDs) {
    const auto dir = MOBase::getOptionalKnownFolder(p.first);

    if (!dir.isEmpty()) {
      auto path = QDir::toNativeSeparators(dir).toLower();
      if (!path.endsWith("\\")) {
        path += "\\";
      }

      systemDirs.push_back({path, p.second});
    }
  }

  return systemDirs;
}

int checkProtected(const QDir& d, const QString& what)
{
  static const auto systemDirs = getSystemDirectories();

  const auto path = QDir::toNativeSeparators(d.absolutePath()).toLower();

  log::debug("  . {}: {}", what, path);

  for (auto&& sd : systemDirs) {
    if (path.startsWith(sd.first)) {
      log::warn(
        "{} is {}; this may cause issues because it's a special "
        "system folder",
        what, sd.second);

      log::debug("path '{}' starts with '{}'", path, sd.first);

      return 1;
    }
  }

  return 0;
}

int checkPathsForSanity(IPluginGame& game, const Settings& s)
{
  log::debug("checking paths");

  int n = 0;

  n += checkProtected(game.gameDirectory(), "the game");
  n += checkProtected(QApplication::applicationDirPath(), "Mod Organizer");

  if (checkProtected(s.paths().base(), "the instance base directory")) {
    ++n;
  } else {
    n += checkProtected(s.paths().downloads(), "the downloads directory");
    n += checkProtected(s.paths().mods(), "the mods directory");
    n += checkProtected(s.paths().cache(), "the cache directory");
    n += checkProtected(s.paths().profiles(), "the profiles directory");
    n += checkProtected(s.paths().overwrite(), "the overwrite directory");
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
