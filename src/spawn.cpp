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

namespace
{

struct SpawnParameters
{
  std::wstring binary;
  std::wstring arguments;
  std::wstring currentDirectory;
  bool suspended = false;
  bool hooked = false;
  HANDLE stdOut = INVALID_HANDLE_VALUE;
  HANDLE stdErr = INVALID_HANDLE_VALUE;
};


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

  std::wstring commandLine;

  if (sp.arguments[0] != L'\0') {
    commandLine = L"\"" + sp.binary + L"\" " + sp.arguments;
  } else {
    commandLine = L"\"" + sp.binary + L"\"";
  }

  QString moPath = QCoreApplication::applicationDirPath();

  const auto oldPath = pathEnv();
  setPathEnv(oldPath + L";" + QDir::toNativeSeparators(moPath).toStdWString());

  PROCESS_INFORMATION pi;
  BOOL success = FALSE;

  if (sp.hooked) {
    success = ::CreateProcessHooked(
      nullptr, const_cast<wchar_t*>(commandLine.c_str()), nullptr, nullptr,
      inheritHandles, CREATE_BREAKAWAY_FROM_JOB, nullptr,
      sp.currentDirectory.c_str(), &si, &pi);
  } else {
    success = ::CreateProcess(
      nullptr, const_cast<wchar_t*>(commandLine.c_str()), nullptr, nullptr,
      inheritHandles, CREATE_BREAKAWAY_FROM_JOB, nullptr,
      sp.currentDirectory.c_str(), &si, &pi);
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
  const QFileInfo bin(QString::fromStdWString(sp.binary));
  std::wstring owner, rights;

  if (bin.isFile()) {
    const auto fs = env::getFileSecurity(bin.absoluteFilePath());

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

  const bool cwdExists = (sp.currentDirectory.empty() ?
    true : QFileInfo(QString::fromStdWString(sp.currentDirectory)).isDir());

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
    L" . stdout: {stdout}, stderr: {stderr}, suspended: {susp}, hooked: {hooked}\n"
    L" . usvfs x86:{x86_dll} x64:{x64_dll} proxy_x86:{x86_proxy} proxy_x64:{x64_proxy}\n"
    L" . MO elevated: {elevated}",
    fmt::arg(L"code", code),
    fmt::arg(L"codename", errorCodeName(code)),
    fmt::arg(L"bin", sp.binary),
    fmt::arg(L"owner", owner),
    fmt::arg(L"rights", rights),
    fmt::arg(L"error", formatSystemMessage(code)),
    fmt::arg(L"args", sp.arguments),
    fmt::arg(L"cwd", sp.currentDirectory),
    fmt::arg(L"cwdexists", (cwdExists ? L"" : L" (not found)")),
    fmt::arg(L"stdout", (sp.stdOut == INVALID_HANDLE_VALUE ? L"no" : L"yes")),
    fmt::arg(L"stderr", (sp.stdErr == INVALID_HANDLE_VALUE ? L"no" : L"yes")),
    fmt::arg(L"susp", (sp.suspended ? L"yes" : L"no")),
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

  const auto binary = QFileInfo(QString::fromStdWString(sp.binary));

  const auto title = QObject::tr("Cannot launch program");

  const auto mainText = QObject::tr("Cannot start %1")
    .arg(binary.fileName());

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

  const auto binary = QFileInfo(QString::fromStdWString(sp.binary));

  const auto title = QObject::tr("Elevation required");

  const auto mainText = QObject::tr("Cannot start %1")
    .arg(binary.fileName());

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

} // namespace

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

HANDLE startBinary(
  const QFileInfo &binary, const QString &arguments, const QDir &currentDirectory,
  bool hooked, HANDLE stdOut, HANDLE stdErr)
{
  SpawnParameters sp;

  sp.binary = QDir::toNativeSeparators(binary.absoluteFilePath()).toStdWString();
  sp.arguments = arguments.toStdWString();
  sp.currentDirectory = QDir::toNativeSeparators(currentDirectory.absolutePath()).toStdWString();
  sp.suspended = true;
  sp.hooked = hooked;
  sp.stdOut = stdOut;
  sp.stdErr = stdErr;

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
