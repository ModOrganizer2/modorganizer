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

namespace spawn
{

// details
namespace
{


std::wstring pathEnv()
{
  std::wstring s(4000, L' ');

  DWORD realSize = ::GetEnvironmentVariableW(
    L"PATH", s.data(), static_cast<DWORD>(s.size()));

  if (realSize > s.size()) {
    s.resize(realSize);

    ::GetEnvironmentVariableW(
      TEXT("PATH"), s.data(), static_cast<DWORD>(s.size()));
  }

  return s;
}

void setPathEnv(const std::wstring& s)
{
  ::SetEnvironmentVariableW(L"PATH", s.c_str());
}

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

  std::wstring commandLine = L"\"" + bin + L"\"";
  if (sp.arguments[0] != L'\0') {
    commandLine +=  L" " + sp.arguments.toStdWString();
  }

  QString moPath = QCoreApplication::applicationDirPath();

  const auto oldPath = pathEnv();
  setPathEnv(oldPath + L";" + QDir::toNativeSeparators(moPath).toStdWString());

  PROCESS_INFORMATION pi;
  BOOL success = FALSE;

  const auto cwd = QDir::toNativeSeparators(sp.currentDirectory.absolutePath()).toStdWString();

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
  setPathEnv(oldPath);

  if (!success) {
    return e;
  }

  processHandle = pi.hProcess;
  threadHandle = pi.hThread;

  return ERROR_SUCCESS;
}

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

std::wstring makeDetails(const SpawnParameters& sp, DWORD code)
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

  return fmt::format(
    L"Error {code} {codename}: {error}\n"
    L" . binary: '{bin}'\n"
    L" . owner: {owner}\n"
    L" . rights: {rights}\n"
    L" . arguments: '{args}'\n"
    L" . cwd: '{cwd}'{cwdexists}\n"
    L" . stdout: {stdout}, stderr: {stderr}, hooked: {hooked}\n"
    L" . usvfs x86:{x86_dll} x64:{x64_dll} proxy_x86:{x86_proxy} proxy_x64:{x64_proxy}\n"
    L" . MO elevated: {elevated}",
    fmt::arg(L"code", code),
    fmt::arg(L"codename", errorCodeName(code)),
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
}

void spawnFailed(const SpawnParameters& sp, DWORD code)
{
  const auto details = QString::fromStdWString(makeDetails(sp, code));
  log::error("{}", details);

  const auto title = QObject::tr("Cannot launch program");

  const auto mainText = QObject::tr("Cannot start %1")
    .arg(sp.binary.fileName());

  QString content;

  if (code == ERROR_INVALID_PARAMETER) {
    content = QObject::tr(
      "This error typically happens because an antivirus has deleted critical "
      "files from Mod Organizer's installation folder or has made them "
      "generally inaccessible. Add an exclusion for Mod Organizer's "
      "installation folder in your antivirus, reinstall Mod Organizer and try "
      "again.");
  } else if (code == ERROR_ACCESS_DENIED) {
    content = QObject::tr(
      "This error typically happens because an antivirus is preventing Mod "
      "Organizer from starting programs. Add an exclusion for Mod Organizer's "
      "installation folder in your antivirus and try again.");
  } else {
    content = QString::fromStdWString(formatSystemMessage(code));
  }

  QWidget *window = qApp->activeWindow();
  if ((window != nullptr) && (!window->isVisible())) {
    window = nullptr;
  }

  MOBase::TaskDialog(window, title)
    .main(mainText)
    .content(content)
    .details(details)
    .exec();
}

bool confirmRestartAsAdmin(const SpawnParameters& sp)
{
  const auto details = QString::fromStdWString(
    makeDetails(sp, ERROR_ELEVATION_REQUIRED));

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

void startBinaryAdmin(const SpawnParameters& sp)
{
  if (!confirmRestartAsAdmin(sp)) {
    log::debug("user declined");
    return;
  }

  log::info("restarting MO as administrator");

  WCHAR cwd[MAX_PATH] = {};
  if (!GetCurrentDirectory(MAX_PATH, cwd)) {
    cwd[0] = L'\0';
  }

  if (Helper::adminLaunch(
    qApp->applicationDirPath().toStdWString(),
    qApp->applicationFilePath().toStdWString(),
    std::wstring(cwd))) {
    qApp->exit(0);
  }
}

} // namespace


bool checkBinary(QWidget* parent, const SpawnParameters& sp)
{
  if (!sp.binary.exists()) {
    reportError(
      QObject::tr("Executable not found: %1")
      .arg(sp.binary.absoluteFilePath()));

    return false;
  }

  return true;
}

bool testForSteam(bool *found, bool *access)
{
  HANDLE hProcessSnap;
  HANDLE hProcess;
  PROCESSENTRY32 pe32;
  DWORD lastError;

  if (found == nullptr || access == nullptr) {
    return false;
  }

  // Take a snapshot of all processes in the system.
  hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hProcessSnap == INVALID_HANDLE_VALUE) {
    lastError = GetLastError();
    log::error("unable to get snapshot of processes (error {})", lastError);
    return false;
  }

  // Retrieve information about the first process,
  // and exit if unsuccessful
  pe32.dwSize = sizeof(PROCESSENTRY32);
  if (!Process32First(hProcessSnap, &pe32)) {
    lastError = GetLastError();
    log::error("unable to get first process (error {})", lastError);
    CloseHandle(hProcessSnap);
    return false;
  }

  *found = false;
  *access = true;

  // Now walk the snapshot of processes, and
  // display information about each process in turn
  do {
    if ((_tcsicmp(pe32.szExeFile, L"Steam.exe") == 0) ||
      (_tcsicmp(pe32.szExeFile, L"SteamService.exe") == 0)) {

      *found = true;

      // Try to open the process to determine if MO has the proper access
      hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE, pe32.th32ProcessID);
      if (hProcess == NULL) {
        lastError = GetLastError();
        if (lastError == ERROR_ACCESS_DENIED) {
          *access = false;
        }
      } else {
        CloseHandle(hProcess);
      }
      break;
    }

  } while(Process32Next(hProcessSnap, &pe32));

  CloseHandle(hProcessSnap);
  return true;
}

void startSteam(QWidget *widget)
{
  QSettings steamSettings("HKEY_CURRENT_USER\\Software\\Valve\\Steam",
    QSettings::NativeFormat);
  QString exe = steamSettings.value("SteamExe", "").toString();
  if (!exe.isEmpty()) {
    exe = QString("\"%1\"").arg(exe);
    // See if username and password supplied. If so, pass them into steam.
    QStringList args;
    QString username;
    QString password;
    if (Settings::instance().steam().login(username, password)) {
      args << "-login";
      args << username;
      if (password != "") {
        args << password;
      }
    }
    if (!QProcess::startDetached(exe, args)) {
      reportError(QObject::tr("Failed to start \"%1\"").arg(exe));
    } else {
      QMessageBox::information(
        widget, QObject::tr("Waiting"),
        QObject::tr("Please press OK once you're logged into steam."));
    }
  }
}

bool checkSteam(
  QWidget* parent, const SpawnParameters& sp,
  const QDir& gameDirectory, const QString &steamAppID, const Settings& settings)
{
  if (!steamAppID.isEmpty()) {
    ::SetEnvironmentVariableW(L"SteamAPPId", ToWString(steamAppID).c_str());
  } else {
    ::SetEnvironmentVariableW(L"SteamAPPId",
      ToWString(settings.steam().appID()).c_str());
  }

  if ((QFileInfo(gameDirectory.absoluteFilePath("steam_api.dll")).exists() ||
       QFileInfo(gameDirectory.absoluteFilePath("steam_api64.dll")).exists())
    && (settings.game().loadMechanismType() == LoadMechanism::LOAD_MODORGANIZER)) {

    bool steamFound = true;
    bool steamAccess = true;
    if (!testForSteam(&steamFound, &steamAccess)) {
      log::error("unable to determine state of Steam");
    }

    if (!steamFound) {
      QDialogButtonBox::StandardButton result;
      result = QuestionBoxMemory::query(parent, "steamQuery", sp.binary.fileName(),
        QObject::tr("Start Steam?"),
        QObject::tr("Steam is required to be running already to correctly start the game. "
          "Should MO try to start steam now?"),
        QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
      if (result == QDialogButtonBox::Yes) {
        startSteam(parent);

        // double-check that Steam is started and MO has access
        steamFound = true;
        steamAccess = true;
        if (!testForSteam(&steamFound, &steamAccess)) {
          log::error("unable to determine state of Steam");
        } else if (!steamFound) {
          log::error("could not find Steam");
        }

      } else if (result == QDialogButtonBox::Cancel) {
        return false;
      }
    }

    if (!steamAccess) {
      QDialogButtonBox::StandardButton result;
      result = QuestionBoxMemory::query(parent, "steamAdminQuery", sp.binary.fileName(),
        QObject::tr("Steam: Access Denied"),
        QObject::tr("MO was denied access to the Steam process.  This normally indicates that "
          "Steam is being run as administrator while MO is not.  This can cause issues "
          "launching the game.  It is recommended to not run Steam as administrator unless "
          "absolutely necessary.\n\n"
          "Restart MO as administrator?"),
        QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
      if (result == QDialogButtonBox::Yes) {
        WCHAR cwd[MAX_PATH];
        if (!GetCurrentDirectory(MAX_PATH, cwd)) {
          log::error("unable to get current directory (error {})", GetLastError());
          cwd[0] = L'\0';
        }
        if (!Helper::adminLaunch(
          qApp->applicationDirPath().toStdWString(),
          qApp->applicationFilePath().toStdWString(),
          std::wstring(cwd))) {
          log::error("unable to relaunch MO as admin");
          return false;
        }
        qApp->exit(0);
        return false;
      } else if (result == QDialogButtonBox::Cancel) {
        return false;
      }
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
      spawnFailed(sp, e);
      return INVALID_HANDLE_VALUE;
    }
  }
}

} // namespace