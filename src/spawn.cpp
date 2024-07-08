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

#include "spawn.h"

#include "env.h"
#include "envmodule.h"
#include "envsecurity.h"
#include "envwindows.h"
#include "settings.h"
#include "settingsdialogworkarounds.h"
#include "shared/appconfig.h"
#include "shared/windows_error.h"
#include <QApplication>
#include <QMessageBox>
#include <QtDebug>
#include <Shellapi.h>
#include <uibase/errorcodes.h>
#include <uibase/log.h>
#include <uibase/report.h>
#include <uibase/utility.h>
#include <usvfs/usvfs.h>

using namespace MOBase;
using namespace MOShared;

namespace spawn::dialogs
{

std::wstring makeRightsDetails(const env::FileSecurity& fs)
{
  if (fs.rights.normalRights) {
    return L"(normal rights)";
  }

  if (fs.rights.list.isEmpty()) {
    return L"(none)";
  }

  std::wstring s = fs.rights.list.join("|").toStdWString();
  if (!fs.rights.hasExecute) {
    s += L" (execute is missing)";
  }

  return s;
}

QString makeDetails(const SpawnParameters& sp, DWORD code, const QString& more = {})
{
  std::wstring owner, rights;

  if (sp.binary.isFile()) {
    const auto fs = env::getFileSecurity(sp.binary.absoluteFilePath());

    if (fs.error.isEmpty()) {
      owner  = fs.owner.toStdWString();
      rights = makeRightsDetails(fs);
    } else {
      owner  = fs.error.toStdWString();
      rights = fs.error.toStdWString();
    }
  } else {
    owner  = L"(file not found)";
    rights = L"(file not found)";
  }

  const bool cwdExists =
      (sp.currentDirectory.isEmpty() ? true : sp.currentDirectory.exists());

  const auto appDir = QCoreApplication::applicationDirPath();
  const auto sep    = QDir::separator();

  const std::wstring usvfs_x86_dll =
      QFileInfo(appDir + sep + "usvfs_x86.dll").isFile() ? L"ok" : L"not found";

  const std::wstring usvfs_x64_dll =
      QFileInfo(appDir + sep + "usvfs_x64.dll").isFile() ? L"ok" : L"not found";

  const std::wstring usvfs_x86_proxy =
      QFileInfo(appDir + sep + "usvfs_proxy_x86.exe").isFile() ? L"ok" : L"not found";

  const std::wstring usvfs_x64_proxy =
      QFileInfo(appDir + sep + "usvfs_proxy_x64.exe").isFile() ? L"ok" : L"not found";

  std::wstring elevated;
  if (auto b = env::Environment().windowsInfo().isElevated()) {
    elevated = (*b ? L"yes" : L"no");
  } else {
    elevated = L"(not available)";
  }

  auto s = std::format(L"Error {} {}{}: {}\n"
                       L" . binary: '{}'\n"
                       L" . owner: {}\n"
                       L" . rights: {}\n"
                       L" . arguments: '{}'\n"
                       L" . cwd: '{}'{}\n"
                       L" . stdout: {}, stderr: {}, hooked: {}\n"
                       L" . MO elevated: {}",
                       code, errorCodeName(code), (more.isEmpty() ? more : ", " + more),
                       formatSystemMessage(code),
                       QDir::toNativeSeparators(sp.binary.absoluteFilePath()), owner,
                       rights, sp.arguments,
                       QDir::toNativeSeparators(sp.currentDirectory.absolutePath()),
                       (cwdExists ? L"" : L" (not found)"),
                       (sp.stdOut == INVALID_HANDLE_VALUE ? L"no" : L"yes"),
                       (sp.stdErr == INVALID_HANDLE_VALUE ? L"no" : L"yes"),
                       (sp.hooked ? L"yes" : L"no"), elevated);

  if (sp.hooked) {
    s += std::format(L"\n . usvfs x86:{} x64:{} proxy_x86:{} proxy_x64:{}",
                     usvfs_x86_dll, usvfs_x64_dll, usvfs_x86_proxy, usvfs_x64_proxy);
  }

  return QString::fromStdWString(s);
}

QString makeContent(const SpawnParameters& sp, DWORD code)
{
  if (code == ERROR_INVALID_PARAMETER) {
    return QObject::tr(
        "This error typically happens because an antivirus has deleted critical "
        "files from Mod Organizer's installation folder or has made them "
        "generally inaccessible. Add an exclusion for Mod Organizer's "
        "installation folder in your antivirus, reinstall Mod Organizer and try "
        "again.");
  } else if (code == ERROR_ACCESS_DENIED) {
    return QObject::tr(
        "This error typically happens because an antivirus is preventing Mod "
        "Organizer from starting programs. Add an exclusion for Mod Organizer's "
        "installation folder in your antivirus and try again.");
  } else if (code == ERROR_FILE_NOT_FOUND) {
    return QObject::tr("The file '%1' does not exist.")
        .arg(QDir::toNativeSeparators(sp.binary.absoluteFilePath()));
  } else if (code == ERROR_DIRECTORY) {
    if (!sp.currentDirectory.exists()) {
      return QObject::tr("The working directory '%1' does not exist.")
          .arg(QDir::toNativeSeparators(sp.currentDirectory.absolutePath()));
    }
  }

  return QString::fromStdWString(formatSystemMessage(code));
}

QMessageBox::StandardButton badSteamReg(QWidget* parent, const QString& keyName,
                                        const QString& valueName)
{
  const auto details =
      QString("can't start steam, registry value at '%1' is empty or doesn't exist")
          .arg(keyName + "\\" + valueName);

  log::error("{}", details);

  return MOBase::TaskDialog(parent, QObject::tr("Cannot start Steam"))
      .main(QObject::tr("Cannot start Steam"))
      .content(
          QObject::tr("The path to the Steam executable cannot be found. You might try "
                      "reinstalling Steam."))
      .details(details)
      .icon(QMessageBox::Critical)
      .button({QObject::tr("Continue without starting Steam"),
               QObject::tr("The program may fail to launch."), QMessageBox::Yes})
      .button({QObject::tr("Cancel"), QMessageBox::Cancel})
      .exec();
}

QMessageBox::StandardButton startSteamFailed(QWidget* parent, const QString& keyName,
                                             const QString& valueName,
                                             const QString& exe,
                                             const SpawnParameters& sp, DWORD e)
{
  auto details = QString("a steam install was found in the registry at '%1': '%2'\n\n")
                     .arg(keyName + "\\" + valueName)
                     .arg(exe);

  details += makeDetails(sp, e);

  log::error("{}", details);

  return MOBase::TaskDialog(parent, QObject::tr("Cannot start Steam"))
      .main(QObject::tr("Cannot start Steam"))
      .content(makeContent(sp, e))
      .details(details)
      .icon(QMessageBox::Critical)
      .button({QObject::tr("Continue without starting Steam"),
               QObject::tr("The program may fail to launch."), QMessageBox::Yes})
      .button({QObject::tr("Cancel"), QMessageBox::Cancel})
      .exec();
}

void spawnFailed(QWidget* parent, const SpawnParameters& sp, DWORD code)
{
  const auto details = makeDetails(sp, code);
  log::error("{}", details);

  const auto title = QObject::tr("Cannot launch program");

  const auto mainText = QObject::tr("Cannot start %1").arg(sp.binary.fileName());

  MOBase::TaskDialog(parent, title)
      .main(mainText)
      .content(makeContent(sp, code))
      .details(details)
      .icon(QMessageBox::Critical)
      .exec();
}

void helperFailed(QWidget* parent, DWORD code, const QString& why,
                  const std::wstring& binary, const std::wstring& cwd,
                  const std::wstring& args)
{
  SpawnParameters sp;
  sp.binary = QFileInfo(QString::fromStdWString(binary));
  sp.currentDirectory.setPath(QString::fromStdWString(cwd));
  sp.arguments = QString::fromStdWString(args);

  const auto details = makeDetails(sp, code, "in " + why);
  log::error("{}", details);

  const auto title = QObject::tr("Cannot launch helper");

  const auto mainText = QObject::tr("Cannot start %1").arg(sp.binary.fileName());

  MOBase::TaskDialog(parent, title)
      .main(mainText)
      .content(makeContent(sp, code))
      .details(details)
      .icon(QMessageBox::Critical)
      .exec();
}

bool confirmRestartAsAdmin(QWidget* parent, const SpawnParameters& sp)
{
  const auto details = makeDetails(sp, ERROR_ELEVATION_REQUIRED);

  log::error("{}", details);

  const auto title = QObject::tr("Elevation required");

  const auto mainText = QObject::tr("Cannot start %1").arg(sp.binary.fileName());

  const auto content = QObject::tr(
      "This program is requesting to run as administrator but Mod Organizer "
      "itself is not running as administrator. Running programs as administrator "
      "is typically unnecessary as long as the game and Mod Organizer have been "
      "installed outside \"Program Files\".\r\n\r\n"
      "You can restart Mod Organizer as administrator and try launching the "
      "program again.");

  log::debug("asking user to restart MO as administrator");

  const auto r =
      MOBase::TaskDialog(parent, title)
          .main(mainText)
          .content(content)
          .details(details)
          .icon(QMessageBox::Question)
          .button({QObject::tr("Restart Mod Organizer as administrator"),
                   QObject::tr(
                       "You must allow \"helper.exe\" to make changes to the system."),
                   QMessageBox::Yes})
          .button({QObject::tr("Cancel"), QMessageBox::Cancel})
          .exec();

  return (r == QMessageBox::Yes);
}

QMessageBox::StandardButton
confirmStartSteam(QWidget* parent, const SpawnParameters& sp, const QString& details)
{
  const auto title    = QObject::tr("Launch Steam");
  const auto mainText = QObject::tr("This program requires Steam");
  const auto content  = QObject::tr(
      "Mod Organizer has detected that this program likely requires Steam to be "
       "running to function properly.");

  return MOBase::TaskDialog(parent, title)
      .main(mainText)
      .content(content)
      .details(details)
      .icon(QMessageBox::Question)
      .button({QObject::tr("Start Steam"), QMessageBox::Yes})
      .button({QObject::tr("Continue without starting Steam"),
               QObject::tr("The program might fail to run."), QMessageBox::No})
      .button({QObject::tr("Cancel"), QMessageBox::Cancel})
      .remember("steamQuery", sp.binary.fileName())
      .exec();
}

QMessageBox::StandardButton confirmRestartAsAdminForSteam(QWidget* parent,
                                                          const SpawnParameters& sp)
{
  const auto title    = QObject::tr("Elevation required");
  const auto mainText = QObject::tr("Steam is running as administrator");
  const auto content  = QObject::tr(
      "Running Steam as administrator is typically unnecessary and can cause "
       "problems when Mod Organizer itself is not running as administrator."
       "\r\n\r\n"
       "You can restart Mod Organizer as administrator and try launching the "
       "program again.");

  return MOBase::TaskDialog(parent, title)
      .main(mainText)
      .content(content)
      .icon(QMessageBox::Question)
      .button(
          {QObject::tr("Restart Mod Organizer as administrator"),
           QObject::tr("You must allow \"helper.exe\" to make changes to the system."),
           QMessageBox::Yes})
      .button({QObject::tr("Continue"), QObject::tr("The program might fail to run."),
               QMessageBox::No})
      .button({QObject::tr("Cancel"), QMessageBox::Cancel})
      .remember("steamAdminQuery", sp.binary.fileName())
      .exec();
}

bool eventLogNotRunning(QWidget* parent, const env::Service& s,
                        const SpawnParameters& sp)
{
  const auto title    = QObject::tr("Event Log not running");
  const auto mainText = QObject::tr("The Event Log service is not running");
  const auto content  = QObject::tr(
      "The Windows Event Log service is not running. This can prevent USVFS from "
       "running properly and your mods may not be recognized by the program being "
       "launched.");

  const auto r =
      MOBase::TaskDialog(parent, title)
          .main(mainText)
          .content(content)
          .details(s.toString())
          .icon(QMessageBox::Question)
          .remember("eventLogService", sp.binary.fileName())
          .button({QObject::tr("Continue"), QObject::tr("Your mods might not work."),
                   QMessageBox::Yes})
          .button({QObject::tr("Cancel"), QMessageBox::Cancel})
          .exec();

  return (r == QMessageBox::Yes);
}

QMessageBox::StandardButton
confirmBlacklisted(QWidget* parent, const SpawnParameters& sp, Settings& settings)
{
  const auto title = QObject::tr("Blacklisted program");
  const auto mainText =
      QObject::tr("The program %1 is blacklisted").arg(sp.binary.fileName());
  const auto content = QObject::tr(
      "The program you are attempting to launch is blacklisted in the virtual "
      "filesystem. This will likely prevent it from seeing any mods, INI files "
      "or any other virtualized files.");

  const auto details = "Executable: " + sp.binary.fileName() +
                       "\n"
                       "Current blacklist: " +
                       settings.executablesBlacklist();

  auto r = MOBase::TaskDialog(parent, title)
               .main(mainText)
               .content(content)
               .details(details)
               .icon(QMessageBox::Question)
               .remember("blacklistedExecutable", sp.binary.fileName())
               .button({QObject::tr("Continue"),
                        QObject::tr("Your mods might not work."), QMessageBox::Yes})
               .button({QObject::tr("Change the blacklist"), QMessageBox::Retry})
               .button({QObject::tr("Cancel"), QMessageBox::Cancel})
               .exec();

  if (r == QMessageBox::Retry) {
    if (!WorkaroundsSettingsTab::changeBlacklistNow(parent, settings)) {
      r = QMessageBox::Cancel;
    }
  }

  return r;
}

}  // namespace spawn::dialogs

namespace spawn
{

void logSpawning(const SpawnParameters& sp, const QString& realCmd)
{
  log::debug("spawning binary:\n"
             " . exe: '{}'\n"
             " . args: '{}'\n"
             " . cwd: '{}'\n"
             " . steam id: '{}'\n"
             " . hooked: {}\n"
             " . stdout: {}\n"
             " . stderr: {}\n"
             " . real cmd: '{}'",
             sp.binary.absoluteFilePath(), sp.arguments,
             sp.currentDirectory.absolutePath(), sp.steamAppID, sp.hooked,
             (sp.stdOut == INVALID_HANDLE_VALUE ? "no" : "yes"),
             (sp.stdErr == INVALID_HANDLE_VALUE ? "no" : "yes"), realCmd);
}

DWORD spawn(const SpawnParameters& sp, HANDLE& processHandle)
{
  BOOL inheritHandles = FALSE;

  STARTUPINFO si = {};
  si.cb          = sizeof(si);

  // inherit handles if we plan to use stdout or stderr reroute
  if (sp.stdOut != INVALID_HANDLE_VALUE) {
    si.hStdOutput  = sp.stdOut;
    inheritHandles = TRUE;
    si.dwFlags |= STARTF_USESTDHANDLES;
  }

  if (sp.stdErr != INVALID_HANDLE_VALUE) {
    si.hStdError   = sp.stdErr;
    inheritHandles = TRUE;
    si.dwFlags |= STARTF_USESTDHANDLES;
  }

  const auto bin = QDir::toNativeSeparators(sp.binary.absoluteFilePath());
  const auto cwd = QDir::toNativeSeparators(sp.currentDirectory.absolutePath());

  QString commandLine = "\"" + bin + "\"";
  if (!sp.arguments.isEmpty()) {
    commandLine += " " + sp.arguments;
  }

  const QString moPath = QCoreApplication::applicationDirPath();
  const auto oldPath   = env::appendToPath(QDir::toNativeSeparators(moPath));

  PROCESS_INFORMATION pi = {};
  BOOL success           = FALSE;

  logSpawning(sp, commandLine);

  const auto wcommandLine = commandLine.toStdWString();
  const auto wcwd         = cwd.toStdWString();

  const DWORD flags = CREATE_BREAKAWAY_FROM_JOB;

  if (sp.hooked) {
    success = ::usvfsCreateProcessHooked(
        nullptr, const_cast<wchar_t*>(wcommandLine.c_str()), nullptr, nullptr,
        inheritHandles, flags, nullptr, wcwd.c_str(), &si, &pi);
  } else {
    success = ::CreateProcess(nullptr, const_cast<wchar_t*>(wcommandLine.c_str()),
                              nullptr, nullptr, inheritHandles, flags, nullptr,
                              wcwd.c_str(), &si, &pi);
  }

  const auto e = GetLastError();
  env::setPath(oldPath);

  if (!success) {
    return e;
  }

  processHandle = pi.hProcess;
  ::CloseHandle(pi.hThread);

  return ERROR_SUCCESS;
}

bool restartAsAdmin(QWidget* parent)
{
  WCHAR cwd[MAX_PATH] = {};
  if (!GetCurrentDirectory(MAX_PATH, cwd)) {
    cwd[0] = L'\0';
  }

  if (!helper::adminLaunch(parent, qApp->applicationDirPath().toStdWString(),
                           qApp->applicationFilePath().toStdWString(),
                           std::wstring(cwd))) {
    log::error("admin launch failed");
    return false;
  }

  log::debug("exiting MO");
  ExitModOrganizer(Exit::Force);

  return true;
}

void startBinaryAdmin(QWidget* parent, const SpawnParameters& sp)
{
  if (!dialogs::confirmRestartAsAdmin(parent, sp)) {
    log::debug("user declined");
    return;
  }

  log::info("restarting MO as administrator");
  restartAsAdmin(parent);
}

struct SteamStatus
{
  bool running    = false;
  bool accessible = false;
};

SteamStatus getSteamStatus()
{
  SteamStatus ss;

  const auto ps = env::Environment().runningProcesses();

  for (const auto& p : ps) {
    if ((p.name().compare("Steam.exe", Qt::CaseInsensitive) == 0) ||
        (p.name().compare("SteamService.exe", Qt::CaseInsensitive) == 0)) {
      ss.running    = true;
      ss.accessible = p.canAccess();

      log::debug("'{}' is running, accessible={}", p.name(),
                 (ss.accessible ? "yes" : "no"));

      break;
    }
  }

  return ss;
}

QString makeSteamArguments(const QString& username, const QString& password)
{
  QString args;

  if (username != "") {
    args += "-login " + username;

    if (password != "") {
      args += " " + password;
    }
  }

  return args;
}

bool startSteam(QWidget* parent)
{
  const QString keyName   = "HKEY_CURRENT_USER\\Software\\Valve\\Steam";
  const QString valueName = "SteamExe";

  const QSettings steamSettings(keyName, QSettings::NativeFormat);
  const QString exe = steamSettings.value(valueName, "").toString();

  if (exe.isEmpty()) {
    return (dialogs::badSteamReg(parent, keyName, valueName) == QMessageBox::Yes);
  }

  SpawnParameters sp;
  sp.binary = QFileInfo(exe);

  // See if username and password supplied. If so, pass them into steam.
  QString username, password;
  if (Settings::instance().steam().login(username, password)) {
    if (username.length() > 0)
      MOBase::log::getDefault().addToBlacklist(username.toStdString(),
                                               "STEAM_USERNAME");
    if (password.length() > 0)
      MOBase::log::getDefault().addToBlacklist(password.toStdString(),
                                               "STEAM_PASSWORD");
    sp.arguments = makeSteamArguments(username, password);
  }

  log::debug("starting steam process:\n"
             " . program: '{}'\n"
             " . username={}, password={}",
             sp.binary.filePath().toStdString(), (username.isEmpty() ? "no" : "yes"),
             (password.isEmpty() ? "no" : "yes"));

  HANDLE ph    = INVALID_HANDLE_VALUE;
  const auto e = spawn(sp, ph);
  ::CloseHandle(ph);

  if (e != ERROR_SUCCESS) {
    // make sure username and passwords are not shown
    sp.arguments = makeSteamArguments((username.isEmpty() ? "" : "USERNAME"),
                                      (password.isEmpty() ? "" : "PASSWORD"));

    const auto r = dialogs::startSteamFailed(parent, keyName, valueName, exe, sp, e);

    return (r == QMessageBox::Yes);
  }

  QMessageBox::information(
      parent, QObject::tr("Waiting"),
      QObject::tr("Please press OK once you're logged into steam."));

  return true;
}

bool checkSteam(QWidget* parent, const SpawnParameters& sp, const QDir& gameDirectory,
                const QString& steamAppID, const Settings& settings)
{
  static const std::vector<QString> steamFiles = {"steam_api.dll", "steam_api64.dll"};

  log::debug("checking steam");

  if (!steamAppID.isEmpty()) {
    env::set("SteamAPPId", steamAppID);
  } else {
    env::set("SteamAPPId", settings.steam().appID());
  }

  bool steamRequired = false;
  QString details;

  for (const auto& file : steamFiles) {
    const QFileInfo fi(gameDirectory.absoluteFilePath(file));
    if (fi.exists()) {
      details = QString("managed game is located at '%1' and file '%2' exists")
                    .arg(gameDirectory.absolutePath())
                    .arg(fi.absoluteFilePath());

      log::debug("{}", details);
      steamRequired = true;

      break;
    }
  }

  if (!steamRequired) {
    log::debug("program doesn't seem to require steam");
    return true;
  }

  auto ss = getSteamStatus();

  if (!ss.running) {
    log::debug("steam isn't running, asking to start steam");

    const auto c = dialogs::confirmStartSteam(parent, sp, details);

    if (c == QDialogButtonBox::Yes) {
      log::debug("user wants to start steam");

      if (!startSteam(parent)) {
        // cancel
        return false;
      }

      // double-check that Steam is started
      ss = getSteamStatus();
      if (!ss.running) {
        log::error("steam is still not running, hoping for the best");
        return true;
      }
    } else if (c == QDialogButtonBox::No) {
      log::debug("user declined to start steam");
      return true;
    } else {
      log::debug("user cancelled");
      return false;
    }
  }

  if (ss.running && !ss.accessible) {
    log::debug("steam is running but is not accessible, asking to restart MO");
    const auto c = dialogs::confirmRestartAsAdminForSteam(parent, sp);

    if (c == QDialogButtonBox::Yes) {
      restartAsAdmin(parent);
      return false;
    } else if (c == QDialogButtonBox::No) {
      log::debug("user declined to restart MO, continuing");
      return true;
    } else {
      log::debug("user cancelled");
      return false;
    }
  }

  return true;
}

bool checkBlacklist(QWidget* parent, const SpawnParameters& sp, Settings& settings)
{
  for (;;) {
    if (!settings.isExecutableBlacklisted(sp.binary.fileName())) {
      return true;
    }

    const auto r = dialogs::confirmBlacklisted(parent, sp, settings);

    if (r != QMessageBox::Retry) {
      return (r == QMessageBox::Yes);
    }
  }
}

HANDLE startBinary(QWidget* parent, const SpawnParameters& sp)
{
  HANDLE handle = INVALID_HANDLE_VALUE;
  const auto e  = spawn::spawn(sp, handle);

  switch (e) {
  case ERROR_SUCCESS: {
    return handle;
  }

  case ERROR_ELEVATION_REQUIRED: {
    startBinaryAdmin(parent, sp);
    return INVALID_HANDLE_VALUE;
  }

  default: {
    dialogs::spawnFailed(parent, sp, e);
    return INVALID_HANDLE_VALUE;
  }
  }
}

QString getExecutableForJarFile(const QString& jarFile)
{
  const std::wstring jarFileW = jarFile.toStdWString();

  WCHAR buffer[MAX_PATH];

  const auto hinst = ::FindExecutableW(jarFileW.c_str(), nullptr, buffer);
  const auto r     = static_cast<int>(reinterpret_cast<std::uintptr_t>(hinst));

  // anything <= 32 signals failure
  if (r <= 32) {
    log::warn("failed to find executable associated with file '{}', {}", jarFile,
              shell::formatError(r));

    return {};
  }

  DWORD binaryType = 0;

  if (!::GetBinaryTypeW(buffer, &binaryType)) {
    const auto e = ::GetLastError();

    log::warn("failed to determine binary type of '{}', {}",
              QString::fromWCharArray(buffer), formatSystemMessage(e));

    return {};
  }

  if (binaryType != SCS_32BIT_BINARY && binaryType != SCS_64BIT_BINARY) {
    log::warn("unexpected binary type {} for file '{}'", binaryType,
              QString::fromWCharArray(buffer));

    return {};
  }

  return QString::fromWCharArray(buffer);
}

QString getJavaHome()
{
  const QString key =
      "HKEY_LOCAL_MACHINE\\Software\\JavaSoft\\Java Runtime Environment";
  const QString value = "CurrentVersion";

  QSettings reg(key, QSettings::NativeFormat);

  if (!reg.contains(value)) {
    log::warn("key '{}\\{}' doesn't exist", key, value);
    return {};
  }

  const QString currentVersion = reg.value("CurrentVersion").toString();
  const QString javaHome       = QString("%1/JavaHome").arg(currentVersion);

  if (!reg.contains(javaHome)) {
    log::warn("java version '{}' was found at '{}\\{}', but '{}\\{}' doesn't exist",
              currentVersion, key, value, key, javaHome);

    return {};
  }

  const auto path = reg.value(javaHome).toString();
  return path + "\\bin\\javaw.exe";
}

QString findJavaInstallation(const QString& jarFile)
{
  // try to find java automatically based on the given jar file
  if (!jarFile.isEmpty()) {
    const auto s = getExecutableForJarFile(jarFile);
    if (!s.isEmpty()) {
      return s;
    }
  }

  // second attempt: look to the registry
  const auto s = getJavaHome();
  if (!s.isEmpty()) {
    return s;
  }

  // not found
  return {};
}

bool isBatchFile(const QFileInfo& target)
{
  const auto batchExtensions = {"cmd", "bat"};

  const QString extension = target.suffix();
  for (auto&& e : batchExtensions) {
    if (extension.compare(e, Qt::CaseInsensitive) == 0) {
      return true;
    }
  }

  return false;
}

bool isExeFile(const QFileInfo& target)
{
  return (target.suffix().compare("exe", Qt::CaseInsensitive) == 0);
}

bool isJavaFile(const QFileInfo& target)
{
  return (target.suffix().compare("jar", Qt::CaseInsensitive) == 0);
}

QFileInfo getCmdPath()
{
  const auto p = env::get("COMSPEC");
  if (!p.isEmpty()) {
    return QFileInfo(p);
  }

  QString systemDirectory;

  const std::size_t buffer_size   = 1000;
  wchar_t buffer[buffer_size + 1] = {};

  const auto length = ::GetSystemDirectoryW(buffer, buffer_size);
  if (length != 0) {
    systemDirectory = QString::fromWCharArray(buffer, length);

    if (!systemDirectory.endsWith("\\")) {
      systemDirectory += "\\";
    }
  } else {
    systemDirectory = "C:\\Windows\\System32\\";
  }

  return QFileInfo(systemDirectory + "cmd.exe");
}

FileExecutionTypes getFileExecutionType(const QFileInfo& target)
{
  if (isExeFile(target) || isBatchFile(target) || isJavaFile(target)) {
    return FileExecutionTypes::Executable;
  }

  return FileExecutionTypes::Other;
}

FileExecutionContext getFileExecutionContext(QWidget* parent, const QFileInfo& target)
{
  if (isExeFile(target)) {
    return {target, "", FileExecutionTypes::Executable};
  }

  if (isBatchFile(target)) {
    return {
        getCmdPath(),
        QString("/C \"%1\"").arg(QDir::toNativeSeparators(target.absoluteFilePath())),
        FileExecutionTypes::Executable};
  }

  if (isJavaFile(target)) {
    auto java = findJavaInstallation(target.absoluteFilePath());

    if (java.isEmpty()) {
      java =
          QFileDialog::getOpenFileName(parent, QObject::tr("Select binary"), QString(),
                                       QObject::tr("Binary") + " (*.exe)");
    }

    if (!java.isEmpty()) {
      return {QFileInfo(java),
              QString("-jar \"%1\"")
                  .arg(QDir::toNativeSeparators(target.absoluteFilePath())),
              FileExecutionTypes::Executable};
    }
  }

  return {{}, {}, FileExecutionTypes::Other};
}

}  // namespace spawn

namespace helper
{

bool helperExec(QWidget* parent, const std::wstring& moDirectory,
                const std::wstring& commandLine, BOOL async)
{
  const std::wstring fileName = moDirectory + L"\\helper.exe";

  env::HandlePtr process;

  {
    SHELLEXECUTEINFOW execInfo = {};

    ULONG flags = SEE_MASK_FLAG_NO_UI;
    if (!async)
      flags |= SEE_MASK_NOCLOSEPROCESS;

    execInfo.cbSize       = sizeof(SHELLEXECUTEINFOW);
    execInfo.fMask        = flags;
    execInfo.hwnd         = 0;
    execInfo.lpVerb       = L"runas";
    execInfo.lpFile       = fileName.c_str();
    execInfo.lpParameters = commandLine.c_str();
    execInfo.lpDirectory  = moDirectory.c_str();
    execInfo.nShow        = SW_SHOW;

    if (!::ShellExecuteExW(&execInfo) && execInfo.hProcess == 0) {
      const auto e = GetLastError();

      spawn::dialogs::helperFailed(parent, e, "ShellExecuteExW()", fileName,
                                   moDirectory, commandLine);

      return false;
    }

    if (async) {
      return true;
    }

    process.reset(execInfo.hProcess);
  }

  const auto r = ::WaitForSingleObject(process.get(), INFINITE);

  if (r != WAIT_OBJECT_0) {
    // for WAIT_ABANDONED, the documentation doesn't mention that GetLastError()
    // returns something meaningful, but code ERROR_ABANDONED_WAIT_0 exists, so
    // use that instead
    const auto code = (r == WAIT_ABANDONED ? ERROR_ABANDONED_WAIT_0 : GetLastError());

    spawn::dialogs::helperFailed(parent, code, "WaitForSingleObject()", fileName,
                                 moDirectory, commandLine);

    return false;
  }

  DWORD exitCode = 0;
  if (!GetExitCodeProcess(process.get(), &exitCode)) {
    const auto e = GetLastError();

    spawn::dialogs::helperFailed(parent, e, "GetExitCodeProcess()", fileName,
                                 moDirectory, commandLine);

    return false;
  }

  return (exitCode == 0);
}

bool backdateBSAs(QWidget* parent, const std::wstring& moPath,
                  const std::wstring& dataPath)
{
  const std::wstring commandLine = std::format(L"backdateBSA \"{}\"", dataPath);

  return helperExec(parent, moPath, commandLine, FALSE);
}

bool adminLaunch(QWidget* parent, const std::wstring& moPath,
                 const std::wstring& moFile, const std::wstring& workingDir)
{
  const std::wstring commandLine = std::format(
      L"adminLaunch {} \"{}\" \"{}\"", ::GetCurrentProcessId(), moFile, workingDir);

  return helperExec(parent, moPath, commandLine, true);
}

}  // namespace helper
