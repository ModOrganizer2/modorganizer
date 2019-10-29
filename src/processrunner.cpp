#include "processrunner.h"
#include "organizercore.h"
#include "instancemanager.h"
#include "lockeddialog.h"
#include "iuserinterface.h"
#include "envmodule.h"
#include <log.h>

using namespace MOBase;

enum class WaitResults
{
  Completed = 1,
  Error,
  Cancelled,
  StillRunning
};


WaitResults singleWait(HANDLE handle, DWORD* exitCode)
{
  if (handle == INVALID_HANDLE_VALUE) {
    return WaitResults::Error;
  }

  const DWORD WAIT_EVENT = WAIT_OBJECT_0 + 1;

  // Wait for a an event on the handle, a key press, mouse click or timeout
  const auto res = MsgWaitForMultipleObjects(
    1, &handle, FALSE, 50, QS_KEY | QS_MOUSEBUTTON);

  switch (res)
  {
    case WAIT_OBJECT_0:
    {
      // completed
      if (exitCode) {
        if (!::GetExitCodeProcess(handle, exitCode)) {
          const auto e = ::GetLastError();
          log::warn(
            "failed to get exit code of process, {}",
            formatSystemMessage(e));
        }
      }

      return WaitResults::Completed;
    }

    case WAIT_TIMEOUT:
    case WAIT_EVENT:
    {
      return WaitResults::StillRunning;
    }

    case WAIT_FAILED:  // fall-through
    default:
    {
      // error
      const auto e = ::GetLastError();

      log::error(
        "failed waiting for process completion, {}", formatSystemMessage(e));

      return WaitResults::Error;
    }
  }
}

enum class Interest
{
  None = 0,
  Weak,
  Strong
};

QString toString(Interest i)
{
  switch (i)
  {
    case Interest::Weak:
      return "weak";

    case Interest::Strong:
      return "strong";

    case Interest::None:  // fall-through
    default:
      return "no";
  }
}


std::pair<env::Process, Interest> findInterestingProcessInTrees(
  std::vector<env::Process>& processes)
{
  if (processes.empty()) {
    return {{}, Interest::None};
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
      return {root, Interest::Strong};
    }

    for (auto&& child : root.children()) {
      if (!isHidden(child)) {
        return {child, Interest::Strong};
      }
    }
  }

  // everything is hidden, just pick the first one
  return {processes[0], Interest::Weak};
}

std::pair<env::Process, Interest> getInterestingProcess(
  const std::vector<HANDLE>& initialProcesses)
{
  std::vector<env::Process> processes;

  log::debug("getting process tree for {} processes", initialProcesses.size());
  for (auto&& h : initialProcesses) {
    auto tree = env::getProcessTree(h);
    if (tree.isValid()) {
      processes.push_back(tree);
    }
  }

  if (processes.empty()) {
    log::debug("nothing to wait for");
    return {{}, Interest::None};
  }

  const auto interest = findInterestingProcessInTrees(processes);
  if (!interest.first.isValid()) {
    log::debug("no interesting process to wait for");
    return {{}, Interest::None};
  }

  return interest;
}

const std::chrono::milliseconds Infinite(-1);

WaitResults timedWait(
  HANDLE handle, DWORD* exitCode, ILockedWaitingForProcess* uilock,
  std::chrono::milliseconds wait)
{
  using namespace std::chrono;

  high_resolution_clock::time_point start;
  if (wait != Infinite) {
    start = high_resolution_clock::now();
  }

  for (;;) {
    const auto r = singleWait(handle, exitCode);

    if (r != WaitResults::StillRunning) {
      return r;
    }

    // keep processing events so the app doesn't appear dead
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();

    if (uilock && uilock->unlockForced()) {
      return WaitResults::Cancelled;
    }

    if (wait != Infinite) {
      const auto now = high_resolution_clock::now();
      if (duration_cast<milliseconds>(now - start) >= wait) {
        return WaitResults::StillRunning;
      }
    }
  }
}

WaitResults waitForProcesses(
  const std::vector<HANDLE>& initialProcesses,
  LPDWORD exitCode, ILockedWaitingForProcess* uilock)
{
  using namespace std::chrono;

  if (initialProcesses.empty()) {
    return WaitResults::Completed;
  }

  DWORD currentPID = 0;
  milliseconds wait(50);

  for (;;) {
    auto [p, interest] = getInterestingProcess(initialProcesses);

    if (uilock) {
      uilock->setProcessInformation(p.pid(), p.name());
    }

    auto interestingHandle = p.openHandleForWait();
    if (!interestingHandle) {
      return WaitResults::Error;
    }

    if (p.pid() != currentPID) {
      currentPID = p.pid();

      log::debug(
        "waiting for completion on {} ({}), {} interest",
        p.name(), p.pid(), toString(interest));
    }

    if (interest == Interest::Strong) {
      wait = Infinite;
    }

    const auto r = timedWait(interestingHandle.get(), exitCode, uilock, wait);
    if (r != WaitResults::StillRunning) {
      return r;
    }

    wait = std::min(wait * 2, milliseconds(2000));

    log::debug(
      "looking for a more interesting process (next check in {}ms)",
      wait.count());
  }
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
  } else {
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
    log::debug("not waiting for process because user has disabled locking");
    return true;
  }

  bool r = false;

  withLock([&](auto* uilock) {
    r = waitForProcessCompletion(handle, exitCode, uilock);
  });

  return r;
}

bool ProcessRunner::waitForApplication(HANDLE handle, LPDWORD exitCode)
{
  // don't check for lockGUI() setting; this _always_ locks the ui
  //
  // this is typically called only from OrganizerProxy, which allows plugins
  // to wait on applications until they're finished
  //
  // the check_fnis plugin for example will start FNIS, wait for it to complete,
  // and then check the exit code; this has to work regardless of the locking
  // setting

  bool r = false;

  withLock([&](auto* uilock) {
    r = waitForProcessCompletion(handle, exitCode, uilock);
  });

  return r;
}

bool ProcessRunner::waitForProcessCompletion(
  HANDLE handle, LPDWORD exitCode, ILockedWaitingForProcess* uilock)
{
  std::vector<HANDLE> processes = {handle};
  const auto r = waitForProcesses(processes, exitCode, uilock);

  return (r != WaitResults::Error);
}

bool ProcessRunner::waitForAllUSVFSProcessesWithLock()
{
  if (!Settings::instance().interface().lockGUI()) {
    log::debug("not waiting for usvfs processes because user has disabled locking");
    return true;
  }

  bool r = false;

  withLock([&](auto* uilock) {
    r = waitForAllUSVFSProcesses(uilock);
  });

  return r;
}

bool ProcessRunner::waitForAllUSVFSProcesses(ILockedWaitingForProcess* uilock)
{
  for (;;) {
    const auto processes = getRunningUSVFSProcesses();
    if (processes.empty()) {
      break;
    }

    const auto r = waitForProcesses(processes, nullptr, uilock);

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
