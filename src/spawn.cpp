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

#include "report.h"
#include "utility.h"
#include "env.h"
#include "envwindows.h"
#include "envsecurity.h"
#include "envmodule.h"
#include "settings.h"
#include <errorcodes.h>
#include <report.h>
#include <log.h>
#include <usvfs.h>
#include <Shellapi.h>
#include <appconfig.h>
#include <windows_error.h>
#include "helper.h"
#include <QApplication>
#include <QMessageBox>
#include <QtDebug>
#include <Shellapi.h>
#include <fmt/format.h>

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

QString makeDetails(const SpawnParameters& sp, DWORD code, const QString& more={})
{
  std::wstring owner, rights;

  if (sp.binary.isFile()) {
    const auto fs = env::getFileSecurity(sp.binary.absoluteFilePath());

    if (fs.error.isEmpty()) {
      owner = fs.owner.toStdWString();
      rights = makeRightsDetails(fs);
    } else {
      owner = fs.error.toStdWString();
      rights = fs.error.toStdWString();
    }
  } else {
    owner = L"(file not found)";
    rights = L"(file not found)";
  }

  const bool cwdExists = (sp.currentDirectory.isEmpty() ?
    true : sp.currentDirectory.exists());

  const auto appDir = QCoreApplication::applicationDirPath();
  const auto sep = QDir::separator();

  const std::wstring usvfs_x86_dll =
    QFileInfo(appDir + sep + "usvfs_x86.dll").isFile() ? L"ok" : L"not found";

  const std::wstring usvfs_x64_dll =
    QFileInfo(appDir + sep + "usvfs_x64.dll").isFile() ? L"ok" : L"not found";

  const std::wstring usvfs_x86_proxy =
    QFileInfo(appDir + sep + "usvfs_proxy_x86.exe").isFile() ? L"ok" : L"not found";

  const std::wstring usvfs_x64_proxy =
    QFileInfo(appDir + sep + "usvfs_proxy_x64.exe").isFile() ? L"ok" : L"not found";

  std::wstring elevated;
  if (auto b=env::Environment().windowsInfo().isElevated()) {
    elevated = (*b ? L"yes" : L"no");
  } else {
    elevated = L"(not available)";
  }

  std::wstring f =
    L"Error {code} {codename}{more}: {error}\n"
    L" . binary: '{bin}'\n"
    L" . owner: {owner}\n"
    L" . rights: {rights}\n"
    L" . arguments: '{args}'\n"
    L" . cwd: '{cwd}'{cwdexists}\n"
    L" . stdout: {stdout}, stderr: {stderr}, hooked: {hooked}\n"
    L" . MO elevated: {elevated}";

  if (sp.hooked) {
    f += L"\n . usvfs x86:{x86_dll} x64:{x64_dll} proxy_x86:{x86_proxy} proxy_x64:{x64_proxy}";
  }

  const std::wstring wmore = (more.isEmpty() ? L"" : (", " + more).toStdWString());

  const auto s = fmt::format(f,
    fmt::arg(L"code", code),
    fmt::arg(L"codename", errorCodeName(code)),
    fmt::arg(L"more", wmore),
    fmt::arg(L"bin", QDir::toNativeSeparators(sp.binary.absoluteFilePath()).toStdWString()),
    fmt::arg(L"owner", owner),
    fmt::arg(L"rights", rights),
    fmt::arg(L"error", formatSystemMessage(code)),
    fmt::arg(L"args", sp.arguments.toStdWString()),
    fmt::arg(L"cwd", QDir::toNativeSeparators(sp.currentDirectory.absolutePath()).toStdWString()),
    fmt::arg(L"cwdexists", (cwdExists ? L"" : L" (not found)")),
    fmt::arg(L"stdout", (sp.stdOut == INVALID_HANDLE_VALUE ? L"no" : L"yes")),
    fmt::arg(L"stderr", (sp.stdErr == INVALID_HANDLE_VALUE ? L"no" : L"yes")),
    fmt::arg(L"hooked", (sp.hooked ? L"yes" : L"no")),
    fmt::arg(L"x86_dll", usvfs_x86_dll),
    fmt::arg(L"x64_dll", usvfs_x64_dll),
    fmt::arg(L"x86_proxy", usvfs_x86_proxy),
    fmt::arg(L"x64_proxy", usvfs_x64_proxy),
    fmt::arg(L"elevated", elevated));

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
  } else {
    return QString::fromStdWString(formatSystemMessage(code));
  }
}

QMessageBox::StandardButton badSteamReg(
  QWidget* parent, const QString& keyName, const QString& valueName)
{
  const auto details = QString(
    "can't start steam, registry value at '%1' is empty or doesn't exist")
    .arg(keyName + "\\" + valueName);

  log::error("{}", details);

  return MOBase::TaskDialog(parent, QObject::tr("Cannot start Steam"))
    .main(QObject::tr("Cannot start Steam"))
    .content(QObject::tr(
      "The path to the Steam executable cannot be found. You might try "
      "reinstalling Steam."))
    .details(details)
    .button({
    QObject::tr("Continue without starting Steam"),
    QObject::tr("The program may fail to launch."),
    QMessageBox::Yes})
    .button({
    QObject::tr("Cancel"),
    QMessageBox::Cancel})
    .exec();
}

QMessageBox::StandardButton startSteamFailed(
  QWidget* parent, const SpawnParameters& sp, DWORD e)
{
  const auto details = makeDetails(sp, e);
  log::error("{}", details);

  return MOBase::TaskDialog(parent, QObject::tr("Cannot start Steam"))
    .main(QObject::tr("Cannot start Steam"))
    .content(makeContent(sp, e))
    .details(details)
    .button({
    QObject::tr("Continue without starting Steam"),
    QObject::tr("The program may fail to launch."),
    QMessageBox::Yes})
    .button({
    QObject::tr("Cancel"),
    QMessageBox::Cancel})
    .exec();
}

void spawnFailed(const SpawnParameters& sp, DWORD code)
{
  const auto details = makeDetails(sp, code);
  log::error("{}", details);

  const auto title = QObject::tr("Cannot launch program");

  const auto mainText = QObject::tr("Cannot start %1")
    .arg(sp.binary.fileName());

  QWidget *window = qApp->activeWindow();
  if ((window != nullptr) && (!window->isVisible())) {
    window = nullptr;
  }

  MOBase::TaskDialog(window, title)
    .main(mainText)
    .content(makeContent(sp, code))
    .details(details)
    .exec();
}

void helperFailed(
  DWORD code, const QString& why, const std::wstring& binary,
  const std::wstring& cwd, const std::wstring& args)
{
  SpawnParameters sp;
  sp.binary = QString::fromStdWString(binary);
  sp.currentDirectory.setPath(QString::fromStdWString(cwd));
  sp.arguments = QString::fromStdWString(args);

  const auto details = makeDetails(sp, code, "in " + why);
  log::error("{}", details);

  const auto title = QObject::tr("Cannot launch helper");

  const auto mainText = QObject::tr("Cannot start %1")
    .arg(sp.binary.fileName());

  QWidget *window = qApp->activeWindow();
  if ((window != nullptr) && (!window->isVisible())) {
    window = nullptr;
  }

  MOBase::TaskDialog(window, title)
    .main(mainText)
    .content(makeContent(sp, code))
    .details(details)
    .exec();
}

bool confirmRestartAsAdmin(const SpawnParameters& sp)
{
  const auto details = makeDetails(sp, ERROR_ELEVATION_REQUIRED);

  log::error("{}", details);

  const auto title = QObject::tr("Elevation required");

  const auto mainText = QObject::tr("Cannot start %1")
    .arg(sp.binary.fileName());

  const auto content = QObject::tr(
    "This program is requesting to run as administrator but Mod Organizer "
    "itself is not running as administrator. Running programs as administrator "
    "is typically unnecessary as long as the game and Mod Organizer have been "
    "installed outside \"Program Files\".\r\n\r\n"
    "You can restart Mod Organizer as administrator and try launching the "
    "program again.");


  QWidget *window = qApp->activeWindow();
  if ((window != nullptr) && (!window->isVisible())) {
    window = nullptr;
  }

  log::debug("asking user to restart MO as administrator");

  const auto r = MOBase::TaskDialog(window, title)
    .main(mainText)
    .content(content)
    .details(details)
    .button({
    QObject::tr("Restart Mod Organizer as administrator"),
    QObject::tr("You must allow \"helper.exe\" to make changes to the system."),
    QMessageBox::Yes})
    .button({
    QObject::tr("Cancel"),
    QMessageBox::Cancel})
    .exec();

  return (r == QMessageBox::Yes);
}

QuestionBoxMemory::Button confirmStartSteam(QWidget* parent, const SpawnParameters& sp)
{
  return QuestionBoxMemory::query(
    parent, "steamQuery", sp.binary.fileName(),
    QObject::tr("Start Steam?"),
    QObject::tr("Steam is required to be running already to correctly start the game. "
      "Should MO try to start steam now?"),
    QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
}

QuestionBoxMemory::Button confirmRestartAsAdminForSteam(QWidget* parent, const SpawnParameters& sp)
{
  return QuestionBoxMemory::query(
    parent, "steamAdminQuery", sp.binary.fileName(),
    QObject::tr("Steam: Access Denied"),
    QObject::tr("MO was denied access to the Steam process.  This normally indicates that "
      "Steam is being run as administrator while MO is not.  This can cause issues "
      "launching the game.  It is recommended to not run Steam as administrator unless "
      "absolutely necessary.\n\n"
      "Restart MO as administrator?"),
    QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
}

} // namepsace


namespace spawn
{

DWORD spawn(const SpawnParameters& sp, HANDLE& processHandle, HANDLE& threadHandle)
{
  BOOL inheritHandles = FALSE;

  STARTUPINFO si = {};
  si.cb = sizeof(si);

  // inherit handles if we plan to use stdout or stderr reroute
  if (sp.stdOut != INVALID_HANDLE_VALUE) {
    si.hStdOutput = sp.stdOut;
    inheritHandles = TRUE;
    si.dwFlags |= STARTF_USESTDHANDLES;
  }

  if (sp.stdErr != INVALID_HANDLE_VALUE) {
    si.hStdError = sp.stdErr;
    inheritHandles = TRUE;
    si.dwFlags |= STARTF_USESTDHANDLES;
  }

  const auto bin = QDir::toNativeSeparators(sp.binary.absoluteFilePath()).toStdWString();
  const auto cwd = QDir::toNativeSeparators(sp.currentDirectory.absolutePath()).toStdWString();

  std::wstring commandLine = L"\"" + bin + L"\"";
  if (sp.arguments[0] != L'\0') {
    commandLine +=  L" " + sp.arguments.toStdWString();
  }

  QString moPath = QCoreApplication::applicationDirPath();
  const auto oldPath = env::addPath(QDir::toNativeSeparators(moPath));

  PROCESS_INFORMATION pi;
  BOOL success = FALSE;

  if (sp.hooked) {
    success = ::CreateProcessHooked(
      nullptr, const_cast<wchar_t*>(commandLine.c_str()), nullptr, nullptr,
      inheritHandles, CREATE_BREAKAWAY_FROM_JOB, nullptr,
      cwd.c_str(), &si, &pi);
  } else {
    success = ::CreateProcess(
      nullptr, const_cast<wchar_t*>(commandLine.c_str()), nullptr, nullptr,
      inheritHandles, CREATE_BREAKAWAY_FROM_JOB, nullptr,
      cwd.c_str(), &si, &pi);
  }

  const auto e = GetLastError();
  env::setPath(oldPath);

  if (!success) {
    return e;
  }

  processHandle = pi.hProcess;
  threadHandle = pi.hThread;

  return ERROR_SUCCESS;
}

bool restartAsAdmin()
{
  WCHAR cwd[MAX_PATH] = {};
  if (!GetCurrentDirectory(MAX_PATH, cwd)) {
    cwd[0] = L'\0';
  }

  if (!helper::adminLaunch(
    qApp->applicationDirPath().toStdWString(),
    qApp->applicationFilePath().toStdWString(),
    std::wstring(cwd)))
  {
    log::error("admin launch failed");
    return false;
  }

  log::debug("exiting MO");
  qApp->exit(0);

  return true;
}

void startBinaryAdmin(const SpawnParameters& sp)
{
  if (!dialogs::confirmRestartAsAdmin(sp)) {
    log::debug("user declined");
    return;
  }

  log::info("restarting MO as administrator");
  restartAsAdmin();
}


bool checkBinary(QWidget* parent, const SpawnParameters& sp)
{
  if (!sp.binary.exists()) {
    dialogs::spawnFailed(sp, ERROR_FILE_NOT_FOUND);
    return false;
  }

  return true;
}

struct SteamStatus
{
  bool running=false;
  bool accessible=false;
};

SteamStatus getSteamStatus()
{
  SteamStatus ss;

  const auto ps = env::Environment().runningProcesses();

  for (const auto& p : ps) {
    if ((p.name().compare("Steam.exe", Qt::CaseInsensitive) == 0) ||
        (p.name().compare("SteamService.exe", Qt::CaseInsensitive) == 0))
    {
      ss.running = true;
      ss.accessible = p.canAccess();

      log::debug(
        "'{}' is running, accessible={}",
        p.name(), (ss.accessible ? "yes" : "no"));

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
  const QString keyName = "HKEY_CURRENT_USER\\Software\\Valve\\Steam";
  const QString valueName = "SteamExe";

  const QSettings steamSettings(keyName, QSettings::NativeFormat);
  const QString exe = steamSettings.value(valueName, "").toString();

  if (exe.isEmpty()) {
    return (dialogs::badSteamReg(parent, keyName, valueName) == QMessageBox::Yes);
  }

  SpawnParameters sp;
  sp.binary = exe;

  // See if username and password supplied. If so, pass them into steam.
  QString username, password;
  if (Settings::instance().steam().login(username, password)) {
    sp.arguments = makeSteamArguments(username, password);
  }

  log::debug(
    "starting steam process:\n"
    " . program: '{}'\n"
    " . username={}, password={}",
    sp.binary.filePath().toStdString(),
    (username.isEmpty() ? "no" : "yes"),
    (password.isEmpty() ? "no" : "yes"));

  HANDLE ph = INVALID_HANDLE_VALUE;
  HANDLE th = INVALID_HANDLE_VALUE;
  const auto e = spawn(sp, ph, th);

  if (e != ERROR_SUCCESS) {
    // make sure username and passwords are not shown
    sp.arguments = makeSteamArguments(
      (username.isEmpty() ? "" : "USERNAME"),
      (password.isEmpty() ? "" : "PASSWORD"));

    return (dialogs::startSteamFailed(parent, sp, e) == QMessageBox::Yes);
  }

  QMessageBox::information(
    parent, QObject::tr("Waiting"),
    QObject::tr("Please press OK once you're logged into steam."));

  return true;
}

bool gameRequiresSteam(const QDir& gameDirectory, const Settings& settings)
{
  static const std::vector<QString> files = {
    "steam_api.dll", "steam_api64.dll"
  };

  for (const auto& file : files) {
    const QFileInfo fi(gameDirectory.absoluteFilePath(file));
    if (fi.exists()) {
      log::debug("found '{}'", fi.absoluteFilePath());
      return true;
    }
  }

  return false;
}

bool checkSteam(
  QWidget* parent, const SpawnParameters& sp,
  const QDir& gameDirectory, const QString &steamAppID, const Settings& settings)
{
  log::debug("checking steam");

  if (!steamAppID.isEmpty()) {
    ::SetEnvironmentVariableW(L"SteamAPPId", steamAppID.toStdWString().c_str());
  } else {
    ::SetEnvironmentVariableW(L"SteamAPPId", settings.steam().appID().toStdWString().c_str());
  }

  if (!gameRequiresSteam(gameDirectory, settings)) {
    log::debug("games doesn't seem to require steam");
    return true;
  }

  auto ss = getSteamStatus();

  if (!ss.running) {
    log::debug("steam isn't running, asking to start steam");
    const auto c = dialogs::confirmStartSteam(parent, sp);

    if (c == QDialogButtonBox::Yes) {
      log::debug("user wants to start steam");

      if (!startSteam(parent)) {
        // cancel
        return false;
      }

      // double-check that Steam is started
      ss = getSteamStatus();
      if (!ss.running) {
        log::error("steam is still not running, continuing and hoping for the best");
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
      restartAsAdmin();
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

bool checkService()
{
  SC_HANDLE serviceManagerHandle = NULL;
  SC_HANDLE serviceHandle = NULL;
  LPSERVICE_STATUS_PROCESS serviceStatus = NULL;
  LPQUERY_SERVICE_CONFIG serviceConfig = NULL;
  bool serviceRunning = true;

  DWORD bytesNeeded;

  try {
    serviceManagerHandle = OpenSCManager(NULL, NULL, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!serviceManagerHandle) {
      log::warn("failed to open service manager (query status) (error {})", GetLastError());
      throw 1;
    }

    serviceHandle = OpenService(serviceManagerHandle, L"EventLog", SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!serviceHandle) {
      log::warn("failed to open EventLog service (query status) (error {})", GetLastError());
      throw 2;
    }

    if (QueryServiceConfig(serviceHandle, NULL, 0, &bytesNeeded)
      || (GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
      log::warn("failed to get size of service config (error {})", GetLastError());
      throw 3;
    }

    DWORD serviceConfigSize = bytesNeeded;
    serviceConfig = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LMEM_FIXED, serviceConfigSize);
    if (!QueryServiceConfig(serviceHandle, serviceConfig, serviceConfigSize, &bytesNeeded)) {
      log::warn("failed to query service config (error {})", GetLastError());
      throw 4;
    }

    if (serviceConfig->dwStartType == SERVICE_DISABLED) {
      log::error("Windows Event Log service is disabled!");
      serviceRunning = false;
    }

    if (QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, NULL, 0, &bytesNeeded)
      || (GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
      log::warn("failed to get size of service status (error {})", GetLastError());
      throw 5;
    }

    DWORD serviceStatusSize = bytesNeeded;
    serviceStatus = (LPSERVICE_STATUS_PROCESS)LocalAlloc(LMEM_FIXED, serviceStatusSize);
    if (!QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)serviceStatus, serviceStatusSize, &bytesNeeded)) {
      log::warn("failed to query service status (error {})", GetLastError());
      throw 6;
    }

    if (serviceStatus->dwCurrentState != SERVICE_RUNNING) {
      log::error("Windows Event Log service is not running");
      serviceRunning = false;
    }
  }
  catch (int) {
    serviceRunning = false;
  }

  if (serviceStatus) {
    LocalFree(serviceStatus);
  }
  if (serviceConfig) {
    LocalFree(serviceConfig);
  }
  if (serviceHandle) {
    CloseServiceHandle(serviceHandle);
  }
  if (serviceManagerHandle) {
    CloseServiceHandle(serviceManagerHandle);
  }

  return serviceRunning;
}

bool checkEnvironment(QWidget* parent, const SpawnParameters& sp)
{
  // Check if the Windows Event Logging service is running.  For some reason, this seems to be
  // critical to the successful running of usvfs.
  if (!checkService()) {
    if (QuestionBoxMemory::query(parent, QString("eventLogService"), sp.binary.fileName(),
      QObject::tr("Windows Event Log Error"),
      QObject::tr("The Windows Event Log service is disabled and/or not running.  This prevents"
        " USVFS from running properly.  Your mods may not be working in the executable"
        " that you are launching.  Note that you may have to restart MO and/or your PC"
        " after the service is fixed.\n\nContinue launching %1?").arg(sp.binary.fileName()),
      QDialogButtonBox::Yes | QDialogButtonBox::No) == QDialogButtonBox::No) {
      return false;
    }
  }

  return true;
}

bool checkBlacklist(QWidget* parent, const SpawnParameters& sp, const Settings& settings)
{
  for (auto exec : settings.executablesBlacklist().split(";")) {
    if (exec.compare(sp.binary.fileName(), Qt::CaseInsensitive) == 0) {
      if (QuestionBoxMemory::query(parent, QString("blacklistedExecutable"), sp.binary.fileName(),
        QObject::tr("Blacklisted Executable"),
        QObject::tr("The executable you are attempted to launch is blacklisted in the virtual file"
          " system.  This will likely prevent the executable, and any executables that are"
          " launched by this one, from seeing any mods.  This could extend to INI files, save"
          " games and any other virtualized files.\n\nContinue launching %1?").arg(sp.binary.fileName()),
        QDialogButtonBox::Yes | QDialogButtonBox::No) == QDialogButtonBox::No) {
        return false;
      }
    }
  }

  return true;
}


HANDLE startBinary(QWidget* parent, const SpawnParameters& sp)
{
  HANDLE processHandle, threadHandle;
  const auto e = spawn(sp, processHandle, threadHandle);

  switch (e)
  {
    case ERROR_SUCCESS:
    {
      ::CloseHandle(threadHandle);
      return processHandle;
    }

    case ERROR_ELEVATION_REQUIRED:
    {
      startBinaryAdmin(sp);
      return INVALID_HANDLE_VALUE;
    }

    default:
    {
      dialogs::spawnFailed(sp, e);
      return INVALID_HANDLE_VALUE;
    }
  }
}

} // namespace



namespace helper
{

bool helperExec(
  const std::wstring& moDirectory, const std::wstring& commandLine, BOOL async)
{
  const std::wstring fileName = moDirectory + L"\\helper.exe";

  env::HandlePtr process;

  {
    SHELLEXECUTEINFOW execInfo = {};

    ULONG flags = SEE_MASK_FLAG_NO_UI ;
    if (!async)
      flags |= SEE_MASK_NOCLOSEPROCESS;

    execInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
    execInfo.fMask = flags;
    execInfo.hwnd = 0;
    execInfo.lpVerb = L"runas";
    execInfo.lpFile = fileName.c_str();
    execInfo.lpParameters = commandLine.c_str();
    execInfo.lpDirectory = moDirectory.c_str();
    execInfo.nShow = SW_SHOW;

    if (!::ShellExecuteExW(&execInfo) && execInfo.hProcess == 0) {
      const auto e = GetLastError();

      spawn::dialogs::helperFailed(
        e, "ShellExecuteExW()", fileName, moDirectory, commandLine);

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
    const auto code = (r == WAIT_ABANDONED ?
      ERROR_ABANDONED_WAIT_0 : GetLastError());

    spawn::dialogs::helperFailed(
      code, "WaitForSingleObject()", fileName, moDirectory, commandLine);

    return false;
  }

  DWORD exitCode = 0;
  if (!GetExitCodeProcess(process.get(), &exitCode)) {
    const auto e = GetLastError();

    spawn::dialogs::helperFailed(
      e, "GetExitCodeProcess()", fileName, moDirectory, commandLine);

    return false;
  }

  return (exitCode == 0);
}

bool backdateBSAs(const std::wstring &moPath, const std::wstring &dataPath)
{
  const std::wstring commandLine = fmt::format(
    L"backdateBSA \"{}\"", dataPath);

  return helperExec(moPath, commandLine, FALSE);
}


bool adminLaunch(const std::wstring &moPath, const std::wstring &moFile, const std::wstring &workingDir)
{
  const std::wstring commandLine = fmt::format(
    L"adminLaunch {} \"{}\" \"{}\"",
    ::GetCurrentProcessId(), moFile, workingDir);

  return helperExec(moPath, commandLine, true);
}

} // namespace
