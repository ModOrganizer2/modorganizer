#include "processrunner.h"
#include "organizercore.h"
#include "instancemanager.h"
#include "lockeddialog.h"
#include "iuserinterface.h"
#include "envmodule.h"
#include <log.h>

using namespace MOBase;

void adjustForVirtualized(
  const IPluginGame* game, spawn::SpawnParameters& sp, const Settings& settings)
{
  const QString modsPath = settings.paths().mods();

  // Check if this a request with either an executable or a working directory
  // under our mods folder then will start the process in a virtualized
  // "environment" with the appropriate paths fixed:
  // (i.e. mods\FNIS\path\exe => game\data\path\exe)
  QString cwdPath = sp.currentDirectory.absolutePath();
  bool virtualizedCwd = cwdPath.startsWith(modsPath, Qt::CaseInsensitive);
  QString binPath = sp.binary.absoluteFilePath();
  bool virtualizedBin = binPath.startsWith(modsPath, Qt::CaseInsensitive);
  if (virtualizedCwd || virtualizedBin) {
    if (virtualizedCwd) {
      int cwdOffset = cwdPath.indexOf('/', modsPath.length() + 1);
      QString adjustedCwd = cwdPath.mid(cwdOffset, -1);
      cwdPath = game->dataDirectory().absolutePath();
      if (cwdOffset >= 0)
        cwdPath += adjustedCwd;

    }

    if (virtualizedBin) {
      int binOffset = binPath.indexOf('/', modsPath.length() + 1);
      QString adjustedBin = binPath.mid(binOffset, -1);
      binPath = game->dataDirectory().absolutePath();
      if (binOffset >= 0)
        binPath += adjustedBin;
    }

    QString cmdline
      = QString("launch \"%1\" \"%2\" %3")
      .arg(QDir::toNativeSeparators(cwdPath),
        QDir::toNativeSeparators(binPath), sp.arguments);

    sp.binary = QFileInfo(QCoreApplication::applicationFilePath());
    sp.arguments = cmdline;
    sp.currentDirectory.setPath(QCoreApplication::applicationDirPath());
  }
}

env::Process* getInterestingProcess(std::vector<env::Process>& processes)
{
  if (processes.empty()) {
    return nullptr;
  }

  // Certain process names we wish to "hide" for aesthetic reason:
  const std::vector<QString> hiddenList = {
    QFileInfo(QCoreApplication::applicationFilePath()).fileName()
  };

  auto isHidden = [&](auto&& p) {
    for (auto h : hiddenList) {
      if (p.name().contains(h, Qt::CaseInsensitive)) {
        return true;
      }
    }

    return false;
  };


  for (auto&& root : processes) {
    if (!isHidden(root)) {
      return &root;
    }

    for (auto&& child : root.children()) {
      if (!isHidden(child)) {
        return &child;
      }
    }
  }


  // everything is hidden, just pick the first one
  return &processes[0];
}


SpawnedProcess::SpawnedProcess(HANDLE handle, spawn::SpawnParameters sp)
  : m_handle(handle), m_parameters(std::move(sp))
{
}

SpawnedProcess::SpawnedProcess(SpawnedProcess&& other)
  : m_handle(other.m_handle), m_parameters(std::move(other.m_parameters))
{
  other.m_handle = INVALID_HANDLE_VALUE;
}

SpawnedProcess& SpawnedProcess::operator=(SpawnedProcess&& other)
{
  if (this != &other) {
    destroy();

    m_handle = other.m_handle;
    other.m_handle = INVALID_HANDLE_VALUE;

    m_parameters = std::move(other.m_parameters);
  }

  return *this;
}

SpawnedProcess::~SpawnedProcess()
{
  destroy();
}

HANDLE SpawnedProcess::releaseHandle()
{
  const auto h = m_handle;
  m_handle = INVALID_HANDLE_VALUE;
  return h;
}

void SpawnedProcess::destroy()
{
  if (m_handle != INVALID_HANDLE_VALUE) {
    ::CloseHandle(m_handle);
    m_handle = INVALID_HANDLE_VALUE;
  }
}


ProcessRunner::ProcessRunner(OrganizerCore& core)
  : m_core(core), m_ui(nullptr)
{
}

void ProcessRunner::setUserInterface(IUserInterface* ui)
{
  m_ui = ui;
}

bool ProcessRunner::runFile(QWidget* parent, const QFileInfo& targetInfo)
{
  if (!parent && m_ui) {
    parent = m_ui->qtWidget();
  }

  const auto fec = spawn::getFileExecutionContext(parent, targetInfo);

  switch (fec.type)
  {
    case spawn::FileExecutionTypes::Executable:
    {
      runExecutableFile(fec.binary, fec.arguments, targetInfo.absoluteDir());
      return true;
    }

    case spawn::FileExecutionTypes::Other:  // fall-through
    default:
    {
      auto r = shell::Open(targetInfo.absoluteFilePath());
      if (!r.success()) {
        return false;
      }

      // not all files will return a valid handle even if opening them was
      // successful, such as inproc handlers (like the photo viewer)
      if (r.processHandle() != INVALID_HANDLE_VALUE) {
        // steal because it gets closed after the wait
        return waitForProcessCompletionWithLock(r.stealProcessHandle(), nullptr);
      }

      return true;
    }
  }
}

bool ProcessRunner::runExecutableFile(
  const QFileInfo &binary, const QString &arguments,
  const QDir &currentDirectory, const QString &steamAppID,
  const QString &customOverwrite,
  const QList<MOBase::ExecutableForcedLoadSetting> &forcedLibraries,
  bool refresh)
{
  DWORD processExitCode = 0;
  HANDLE processHandle = spawnAndWait(
    binary, arguments, m_core.currentProfile()->name(),
    currentDirectory, steamAppID, customOverwrite, forcedLibraries,
    &processExitCode);

  if (processHandle == INVALID_HANDLE_VALUE) {
    // failed
    return false;
  }

  if (refresh) {
    m_core.afterRun(binary, processExitCode);
  }

  return true;
}

bool ProcessRunner::runExecutable(const Executable& exe, bool refresh)
{
  const auto* profile = m_core.currentProfile();
  if (!profile) {
    throw MyException(QObject::tr("No profile set"));
  }

  const QString customOverwrite = profile->setting(
    "custom_overwrites", exe.title()).toString();

  QList<MOBase::ExecutableForcedLoadSetting> forcedLibraries;

  if (profile->forcedLibrariesEnabled(exe.title())) {
    forcedLibraries = profile->determineForcedLibraries(exe.title());
  }

  return runExecutableFile(
    exe.binaryInfo(),
    exe.arguments(),
    exe.workingDirectory().length() != 0 ? exe.workingDirectory() : exe.binaryInfo().absolutePath(),
    exe.steamAppID(),
    customOverwrite,
    forcedLibraries,
    refresh);
}

bool ProcessRunner::runShortcut(const MOShortcut& shortcut)
{
  if (shortcut.hasInstance() && shortcut.instance() != InstanceManager::instance().currentInstance()) {
    throw std::runtime_error(
      QString("Refusing to run executable from different instance %1:%2")
      .arg(shortcut.instance(),shortcut.executable())
      .toLocal8Bit().constData());
  }

  const Executable& exe = m_core.executablesList()->get(shortcut.executable());
  return runExecutable(exe, false);
}

HANDLE ProcessRunner::runExecutableOrExecutableFile(
  const QString& executable, const QStringList &args, const QString &cwd,
  const QString& profileOverride, const QString &forcedCustomOverwrite,
  bool ignoreCustomOverwrite)
{
  const auto* profile = m_core.currentProfile();
  if (!profile) {
    throw MyException(QObject::tr("No profile set"));
  }

  QString profileName = profileOverride;
  if (profileName == "") {
      profileName = profile->name();
  }

  QFileInfo binary;
  QString arguments = args.join(" ");
  QString currentDirectory = cwd;
  QString steamAppID;
  QString customOverwrite;
  QList<ExecutableForcedLoadSetting> forcedLibraries;

  if (executable.contains('\\') || executable.contains('/')) {
    // file path

    binary = QFileInfo(executable);
    if (binary.isRelative()) {
      // relative path, should be relative to game directory
      binary = m_core.managedGame()->gameDirectory().absoluteFilePath(executable);
    }

    if (currentDirectory == "") {
      currentDirectory = binary.absolutePath();
    }

    try {
      const Executable& exe = m_core.executablesList()->getByBinary(binary);
      steamAppID = exe.steamAppID();
      customOverwrite = profile->setting("custom_overwrites", exe.title()).toString();
      if (profile->forcedLibrariesEnabled(exe.title())) {
        forcedLibraries = profile->determineForcedLibraries(exe.title());
      }
    } catch (const std::runtime_error &) {
      // nop
    }
  } else {
    // only a file name, search executables list
    try {
      const Executable &exe = m_core.executablesList()->get(executable);
      steamAppID = exe.steamAppID();
      customOverwrite = profile->setting("custom_overwrites", exe.title()).toString();
      if (profile->forcedLibrariesEnabled(exe.title())) {
        forcedLibraries = profile->determineForcedLibraries(exe.title());
      }
      if (arguments == "") {
        arguments = exe.arguments();
      }
      binary = exe.binaryInfo();
      if (currentDirectory == "") {
        currentDirectory = exe.workingDirectory();
      }
    } catch (const std::runtime_error &) {
      log::warn("\"{}\" not set up as executable", executable);
      binary = QFileInfo(executable);
    }
  }

  if (!forcedCustomOverwrite.isEmpty())
    customOverwrite = forcedCustomOverwrite;

  if (ignoreCustomOverwrite)
    customOverwrite.clear();

  return spawnAndWait(
    binary,
    arguments,
    profileName,
    currentDirectory,
    steamAppID,
    customOverwrite,
    forcedLibraries);
}

HANDLE ProcessRunner::spawnAndWait(
  const QFileInfo &binary, const QString &arguments, const QString &profileName,
  const QDir &currentDirectory, const QString &steamAppID,
  const QString &customOverwrite,
  const QList<MOBase::ExecutableForcedLoadSetting> &forcedLibraries,
  LPDWORD exitCode)
{
  spawn::SpawnParameters sp;
  sp.binary = binary;
  sp.arguments = arguments;
  sp.currentDirectory = currentDirectory;
  sp.steamAppID = steamAppID;
  sp.hooked = true;

  if (!m_core.beforeRun(binary, profileName, customOverwrite, forcedLibraries)) {
    return INVALID_HANDLE_VALUE;
  }

  HANDLE handle = spawn(sp).releaseHandle();

  if (handle == INVALID_HANDLE_VALUE) {
    // failed
    return INVALID_HANDLE_VALUE;
  }

  waitForProcessCompletionWithLock(handle, exitCode);
  return handle;
}

SpawnedProcess ProcessRunner::spawn(spawn::SpawnParameters sp)
{
  QWidget* parent = nullptr;
  if (m_ui) {
    parent = m_ui->qtWidget();
  }

  if (!checkBinary(parent, sp)) {
    return {INVALID_HANDLE_VALUE, sp};
  }

  const auto* game = m_core.managedGame();
  auto& settings = m_core.settings();

  if (!checkSteam(parent, sp, game->gameDirectory(), sp.steamAppID, settings)) {
    return {INVALID_HANDLE_VALUE, sp};
  }

  if (!checkEnvironment(parent, sp)) {
    return {INVALID_HANDLE_VALUE, sp};
  }

  if (!checkBlacklist(parent, sp, settings)) {
    return {INVALID_HANDLE_VALUE, sp};
  }

  adjustForVirtualized(game, sp, settings);

  return {startBinary(parent, sp), sp};
}

void ProcessRunner::withLock(std::function<void (ILockedWaitingForProcess*)> f)
{
  std::unique_ptr<LockedDialog> dlg;
  ILockedWaitingForProcess* uilock = nullptr;

  if (m_ui != nullptr) {
    uilock = m_ui->lock();
  }
  else {
    // i.e. when running command line shortcuts there is no user interface
    dlg.reset(new LockedDialog);
    dlg->show();
    dlg->setEnabled(true);
    uilock = dlg.get();
  }

  Guard g([&]() {
    if (m_ui != nullptr) {
      m_ui->unlock();
    }
  });

  f(uilock);
}

bool ProcessRunner::waitForProcessCompletionWithLock(
  HANDLE handle, LPDWORD exitCode)
{
  if (!Settings::instance().interface().lockGUI()) {
    return true;
  }

  bool r = false;

  withLock([&](auto* uilock) {
    DWORD ignoreExitCode;
    r = waitForProcessCompletion(handle, exitCode ? exitCode : &ignoreExitCode, uilock);
  });

  return r;
}

bool ProcessRunner::waitForApplication(HANDLE handle, LPDWORD exitCode)
{
  if (!Settings::instance().interface().lockGUI())
    return true;

  bool r = false;

  withLock([&](auto* uilock) {
    r = waitForProcessCompletion(handle, exitCode, uilock);
  });

  return r;
}

bool ProcessRunner::waitForProcessCompletion(
  HANDLE handle, LPDWORD exitCode, ILockedWaitingForProcess* uilock)
{
  const auto tree = env::getProcessTree(handle);
  std::vector<env::Process> processes = {tree};

  const auto* interesting = getInterestingProcess(processes);
  if (!interesting) {
    return true;
  }

  if (uilock) {
    uilock->setProcessInformation(interesting->pid(), interesting->name());
  }

  auto interestingHandle = interesting->openHandleForWait();
  if (!interestingHandle) {
    return true;
  }

  auto progress = [&]{ return uilock->unlockForced(); };
  const auto r = waitForProcess(
    interestingHandle.get(), exitCode, progress);

  switch (r)
  {
    case WaitResults::Completed:  // fall-through
    case WaitResults::Cancelled:
      return true;

    case WaitResults::Error:  // fall-through
    default:
      return false;
  }
}

bool ProcessRunner::waitForAllUSVFSProcessesWithLock()
{
  if (!Settings::instance().interface().lockGUI())
    return true;

  bool r = false;

  withLock([&](auto* uilock) {
    r = waitForAllUSVFSProcesses(uilock);
  });

  return r;
}

bool ProcessRunner::waitForAllUSVFSProcesses(ILockedWaitingForProcess* uilock)
{
  for (;;) {
    const auto handles = getRunningUSVFSProcesses();
    if (handles.empty()) {
      break;
    }

    std::vector<env::Process> processes;
    for (auto&& h : handles) {
      auto p = env::getProcessTree(h);
      if (p.isValid()) {
        processes.emplace_back(std::move(p));
      }
    }

    const auto* interesting = getInterestingProcess(processes);
    if (!interesting) {
      break;
    }

    if (uilock) {
      uilock->setProcessInformation(interesting->pid(), interesting->name());
    }

    auto interestingHandle = interesting->openHandleForWait();
    if (!interestingHandle) {
      break;
    }

    auto progress = [&]{ return uilock->unlockForced(); };
    const auto r = waitForProcess(
      interestingHandle.get(), nullptr, progress);

    switch (r)
    {
      case WaitResults::Completed:
        // this process is completed, check for others
        break;

      case WaitResults::Cancelled:
        // force unlocked
        log::debug("waiting for process completion aborted by UI");
        return true;

      case WaitResults::Error:  // fall-through
      default:
        log::debug("waiting for process completion not successful");
        return false;
    }
  }

  log::debug("waiting for process completion successful");
  return true;
}



WaitResults waitForProcess(
  HANDLE handle, DWORD* exitCode, std::function<bool ()> progress)
{
  if (handle == INVALID_HANDLE_VALUE) {
    return WaitResults::Error;
  }

  log::debug("waiting for completion on pid {}", ::GetProcessId(handle));

  std::vector<HANDLE> handles;
  handles.push_back(handle);

  std::vector<DWORD> exitCodes;

  const auto r = waitForProcesses(handles, exitCodes, progress);

  if (r == WaitResults::Completed) {
    if (exitCode && !exitCodes.empty()) {
      *exitCode = exitCodes[0];
    }
  }

  return r;
}

WaitResults waitForProcesses(
  const std::vector<HANDLE>& handles, std::vector<DWORD>& exitCodes,
  std::function<bool ()> progress)
{
  if (handles.empty()) {
    return WaitResults::Completed;
  }

  const auto WAIT_OBJECT_N = static_cast<DWORD>(WAIT_OBJECT_0 + handles.size());

  for (;;) {
    // Wait for a an event on the handle, a key press, mouse click or timeout
    const auto res = MsgWaitForMultipleObjects(
      static_cast<DWORD>(handles.size()), &handles[0],
      TRUE, 50, QS_KEY | QS_MOUSEBUTTON);

    if (res == WAIT_FAILED) {
      // error
      const auto e = ::GetLastError();

      log::error(
        "failed waiting for process completion, {}", formatSystemMessage(e));

      return WaitResults::Error;
    } else if (res >= WAIT_OBJECT_0 && res < WAIT_OBJECT_N) {
      // completed
      exitCodes.resize(handles.size());
      std::fill(exitCodes.begin(), exitCodes.end(), 0);

      for (std::size_t i=0; i<handles.size(); ++i) {
        DWORD exitCode = 0;

        if (::GetExitCodeProcess(handles[i], &exitCode)) {
          exitCodes[i] = exitCode;
        } else {
          const auto e = ::GetLastError();
          log::warn(
            "failed to get exit code of process, {}",
            formatSystemMessage(e));
        }
      }

      return WaitResults::Completed;
    }

    // keep processing events so the app doesn't appear dead
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();

    if (progress && progress()) {
      return WaitResults::Cancelled;
    }
  }
}
