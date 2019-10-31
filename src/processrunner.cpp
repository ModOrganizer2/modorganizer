#include "processrunner.h"
#include "organizercore.h"
#include "instancemanager.h"
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

std::optional<ProcessRunner::Results> singleWait(HANDLE handle, DWORD pid)
{
  if (handle == INVALID_HANDLE_VALUE) {
    return ProcessRunner::Error;
  }

  const DWORD WAIT_EVENT = WAIT_OBJECT_0 + 1;

  // Wait for a an event on the handle, a key press, mouse click or timeout
  const auto res = MsgWaitForMultipleObjects(
    1, &handle, FALSE, 50, QS_KEY | QS_MOUSEBUTTON);

  switch (res)
  {
    case WAIT_OBJECT_0:
    {
      log::debug("process {} completed", pid);
      return ProcessRunner::Completed;
    }

    case WAIT_TIMEOUT:
    case WAIT_EVENT:
    {
      // still running
      return {};
    }

    case WAIT_FAILED:  // fall-through
    default:
    {
      // error
      const auto e = ::GetLastError();
      log::error("failed waiting for {}, {}", pid, formatSystemMessage(e));
      return ProcessRunner::Error;
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
  if (initialProcesses.empty()) {
    log::debug("nothing to wait for");
    return {{}, Interest::None};
  }

  std::vector<env::Process> processes;

  log::debug("getting process tree for {} processes", initialProcesses.size());
  for (auto&& h : initialProcesses) {
    auto tree = env::getProcessTree(h);
    if (tree.isValid()) {
      processes.push_back(tree);
    }
  }

  if (processes.empty()) {
    log::debug("processes are already completed");
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

std::optional<ProcessRunner::Results> timedWait(
  HANDLE handle, DWORD pid, LockWidget& lock, std::chrono::milliseconds wait)
{
  using namespace std::chrono;

  high_resolution_clock::time_point start;
  if (wait != Infinite) {
    start = high_resolution_clock::now();
  }

  for (;;) {
    const auto r = singleWait(handle, pid);

    if (r) {
      return *r;
    }

    // still running

    // keep processing events so the app doesn't appear dead
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();

    switch (lock.result())
    {
      case LockWidget::StillLocked:
      {
        break;
      }

      case LockWidget::ForceUnlocked:
      {
        log::debug("waiting for {} force unlocked by user", pid);
        return ProcessRunner::ForceUnlocked;
      }

      case LockWidget::Cancelled:
      {
        log::debug("waiting for {} cancelled by user", pid);
        return ProcessRunner::Cancelled;
      }

      case LockWidget::NoResult:  // fall-through
      default:
      {
        // shouldn't happen
        log::debug(
          "unexpected result {} while waiting for {}",
          static_cast<int>(lock.result()), pid);

        return ProcessRunner::Error;
      }
    }

    if (wait != Infinite) {
      const auto now = high_resolution_clock::now();
      if (duration_cast<milliseconds>(now - start) >= wait) {
        return {};
      }
    }
  }
}

ProcessRunner::Results waitForProcesses(
  const std::vector<HANDLE>& initialProcesses, LockWidget& lock)
{
  using namespace std::chrono;

  if (initialProcesses.empty()) {
    // shouldn't happen
    return ProcessRunner::Completed;
  }

  DWORD currentPID = 0;
  milliseconds wait(50);

  for (;;) {
    auto [p, interest] = getInterestingProcess(initialProcesses);
    if (!p.isValid()) {
      // nothing to wait on
      return ProcessRunner::Completed;
    }

    lock.setInfo(p.pid(), p.name());

    auto interestingHandle = p.openHandleForWait();
    if (!interestingHandle) {
      return ProcessRunner::Error;
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

    const auto r = timedWait(interestingHandle.get(), p.pid(), lock, wait);
    if (r) {
      return *r;
    }

    wait = std::min(wait * 2, milliseconds(2000));

    log::debug(
      "looking for a more interesting process (next check in {}ms)",
      wait.count());
  }
}

ProcessRunner::Results waitForProcess(
  HANDLE initialProcess, LPDWORD exitCode, LockWidget& lock)
{
  std::vector<HANDLE> processes = {initialProcess};

  const auto r = waitForProcesses(processes, lock);

  // as long as it's not running anymore, try to get the exit code
  if (exitCode && r != ProcessRunner::Running) {
    if (!::GetExitCodeProcess(initialProcess, exitCode)) {
      const auto e = ::GetLastError();
      log::warn(
        "failed to get exit code of process, {}",
        formatSystemMessage(e));
    }
  }

  return r;
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


ProcessRunner::ProcessRunner(OrganizerCore& core, IUserInterface* ui) :
  m_core(core), m_ui(ui), m_lock(LockWidget::NoReason), m_refresh(NoRefresh),
  m_handle(INVALID_HANDLE_VALUE), m_exitCode(-1)
{
  m_sp.hooked = true;
}

ProcessRunner& ProcessRunner::setBinary(const QFileInfo &binary)
{
  m_sp.binary = binary;
  return *this;
}

ProcessRunner& ProcessRunner::setArguments(const QString& arguments)
{
  m_sp.arguments = arguments;
  return *this;
}

ProcessRunner& ProcessRunner::setCurrentDirectory(const QDir& directory)
{
  m_sp.currentDirectory = directory;
  return *this;
}

ProcessRunner& ProcessRunner::setSteamID(const QString& steamID)
{
  m_sp.steamAppID = steamID;
  return *this;
}

ProcessRunner& ProcessRunner::setCustomOverwrite(const QString& customOverwrite)
{
  m_customOverwrite = customOverwrite;
  return *this;
}

ProcessRunner& ProcessRunner::setForcedLibraries(const ForcedLibraries& forcedLibraries)
{
  m_forcedLibraries = forcedLibraries;
  return *this;
}

ProcessRunner& ProcessRunner::setProfileName(const QString& profileName)
{
  m_profileName = profileName;
  return *this;
}

ProcessRunner& ProcessRunner::setWaitForCompletion(
  RefreshModes refresh, LockWidget::Reasons reason)
{
  m_refresh = refresh;
  m_lock = reason;
  return *this;
}

ProcessRunner& ProcessRunner::setFromFile(QWidget* parent, const QFileInfo& targetInfo)
{
  if (!parent && m_ui) {
    parent = m_ui->qtWidget();
  }

  const auto fec = spawn::getFileExecutionContext(parent, targetInfo);

  switch (fec.type)
  {
    case spawn::FileExecutionTypes::Executable:
    {
      setBinary(fec.binary);
      setArguments(fec.arguments);
      setCurrentDirectory(targetInfo.absoluteDir());
      break;
    }

    case spawn::FileExecutionTypes::Other:  // fall-through
    default:
    {
      m_shellOpen = targetInfo.absoluteFilePath();
      break;
    }
  }

  return *this;
}

ProcessRunner& ProcessRunner::setFromExecutable(const Executable& exe)
{
  const auto* profile = m_core.currentProfile();
  if (!profile) {
    throw MyException(QObject::tr("No profile set"));
  }

  const QString customOverwrite = profile->setting(
    "custom_overwrites", exe.title()).toString();

  ForcedLibraries forcedLibraries;
  if (profile->forcedLibrariesEnabled(exe.title())) {
    forcedLibraries = profile->determineForcedLibraries(exe.title());
  }

  QDir currentDirectory = exe.workingDirectory();
  if (currentDirectory.isEmpty()) {
    currentDirectory.setPath(exe.binaryInfo().absolutePath());
  }

  setBinary(exe.binaryInfo());
  setArguments(exe.arguments());
  setCurrentDirectory(currentDirectory);
  setSteamID(exe.steamAppID());
  setCustomOverwrite(customOverwrite);
  setForcedLibraries(forcedLibraries);

  return *this;
}

ProcessRunner& ProcessRunner::setFromShortcut(const MOShortcut& shortcut)
{
  const auto currentInstance = InstanceManager::instance().currentInstance();

  if (shortcut.hasInstance() && shortcut.instance() != currentInstance) {
    throw std::runtime_error(
      QString("Refusing to run executable from different instance %1:%2")
      .arg(shortcut.instance(),shortcut.executable())
      .toLocal8Bit().constData());
  }

  const Executable& exe = m_core.executablesList()->get(shortcut.executable());
  setFromExecutable(exe);

  return *this;
}

ProcessRunner& ProcessRunner::setFromFileOrExecutable(
  const QString &executable,
  const QStringList &args,
  const QString &cwd,
  const QString &profileOverride,
  const QString &forcedCustomOverwrite,
  bool ignoreCustomOverwrite)
{
  const auto* profile = m_core.currentProfile();
  if (!profile) {
    throw MyException(QObject::tr("No profile set"));
  }

  setProfileName(profileOverride);

  if (executable.contains('\\') || executable.contains('/')) {
    // file path

    auto binary = QFileInfo(executable);

    if (binary.isRelative()) {
      // relative path, should be relative to game directory
      binary = m_core.managedGame()->gameDirectory().absoluteFilePath(executable);
    }

    setBinary(binary);

    if (cwd == "") {
      setCurrentDirectory(binary.absolutePath());
    } else {
      setCurrentDirectory(cwd);
    }

    try {
      const Executable& exe = m_core.executablesList()->getByBinary(binary);

      setSteamID(exe.steamAppID());
      setCustomOverwrite(profile->setting("custom_overwrites", exe.title()).toString());

      if (profile->forcedLibrariesEnabled(exe.title())) {
        setForcedLibraries(profile->determineForcedLibraries(exe.title()));
      }
    } catch (const std::runtime_error &) {
      // nop
    }
  } else {
    // only a file name, search executables list
    try {
      const Executable &exe = m_core.executablesList()->get(executable);

      setSteamID(exe.steamAppID());
      setCustomOverwrite(profile->setting("custom_overwrites", exe.title()).toString());

      if (profile->forcedLibrariesEnabled(exe.title())) {
        setForcedLibraries(profile->determineForcedLibraries(exe.title()));
      }

      if (args.isEmpty()) {
        setArguments(exe.arguments());
      } else {
        setArguments(args.join(" "));
      }

      setBinary(exe.binaryInfo());

      if (cwd == "") {
        setCurrentDirectory(exe.workingDirectory());
      } else {
        setCurrentDirectory(cwd);
      }
    } catch (const std::runtime_error &) {
      log::warn("\"{}\" not set up as executable", executable);
      setBinary(QFileInfo(executable));
    }
  }

  if (ignoreCustomOverwrite) {
    setCustomOverwrite("");
  } else if (!forcedCustomOverwrite.isEmpty()) {
    setCustomOverwrite(forcedCustomOverwrite);
  }

  return *this;
}

ProcessRunner::Results ProcessRunner::run()
{
  if (!m_shellOpen.isEmpty()) {
    auto r = shell::Open(m_shellOpen);
    if (!r.success()) {
      return Error;
    }

    // not all files will return a valid handle even if opening them was
    // successful, such as inproc handlers (like the photo viewer)
    m_handle = r.stealProcessHandle();
  } else {
    if (m_profileName.isEmpty()) {
      const auto* profile = m_core.currentProfile();
      if (!profile) {
        throw MyException(QObject::tr("No profile set"));
      }

      m_profileName = profile->name();
    }

    if (!m_core.beforeRun(m_sp.binary, m_profileName, m_customOverwrite, m_forcedLibraries)) {
      return Error;
    }

    QWidget* parent = nullptr;
    if (m_ui) {
      parent = m_ui->qtWidget();
    }

    if (!checkBinary(parent, m_sp)) {
      return Error;
    }

    const auto* game = m_core.managedGame();
    auto& settings = m_core.settings();

    if (!checkSteam(parent, m_sp, game->gameDirectory(), m_sp.steamAppID, settings)) {
      return Error;
    }

    if (!checkEnvironment(parent, m_sp)) {
      return Error;
    }

    if (!checkBlacklist(parent, m_sp, settings)) {
      return Error;
    }

    adjustForVirtualized(game, m_sp, settings);

    m_handle = startBinary(parent, m_sp);
    if (m_handle == INVALID_HANDLE_VALUE) {
      return Error;
    }
  }

  if (m_handle == INVALID_HANDLE_VALUE || m_lock == LockWidget::NoReason) {
    return Running;
  } else {
    const auto r = waitForProcessCompletionWithLock(
      m_handle, &m_exitCode, m_lock);

    if (r == Completed && m_refresh == Refresh) {
      m_core.afterRun(m_sp.binary, m_exitCode);
    }

    return r;
  }
}

DWORD ProcessRunner::exitCode()
{
  return m_exitCode;
}


bool ProcessRunner::runShortcut(const MOShortcut& shortcut)
{
  setFromShortcut(shortcut);
  setWaitForCompletion(NoRefresh);

  const auto r = run();
  return (r != Error);
}

HANDLE ProcessRunner::runExecutableOrExecutableFile(
  const QString& executable, const QStringList &args, const QString &cwd,
  const QString& profileOverride, const QString &forcedCustomOverwrite,
  bool ignoreCustomOverwrite)
{
  setFromFileOrExecutable(
    executable, args, cwd, profileOverride, forcedCustomOverwrite,
    ignoreCustomOverwrite);

  setWaitForCompletion(Refresh);

  run();
  return m_handle;
}

void ProcessRunner::withLock(
  LockWidget::Reasons reason, std::function<void (LockWidget&)> f)
{
  auto lock = std::make_unique<LockWidget>(
    m_ui ? m_ui->qtWidget() : nullptr, reason);

  f(*lock);
}

ProcessRunner::Results ProcessRunner::waitForProcessCompletionWithLock(
  HANDLE handle, LPDWORD exitCode, LockWidget::Reasons reason)
{
  if (!Settings::instance().interface().lockGUI()) {
    log::debug("not waiting for process because user has disabled locking");
    return ForceUnlocked;
  }

  return waitForApplication(handle, exitCode, reason);
}

ProcessRunner::Results ProcessRunner::waitForApplication(
  HANDLE handle, LPDWORD exitCode, LockWidget::Reasons reason)
{
  // don't check for lockGUI() setting; this _always_ locks the ui and waits
  // for completion
  //
  // this is typically called only from:
  //  1) OrganizerProxy, which allows plugins to wait on applications until
  //     they're finished
  //
  //     the check_fnis plugin for example will start FNIS, wait for it to
  //     complete, and then check the exit code; this has to work regardless of
  //     the locking setting;
  //
  // 2) waitForProcessCompletionWithLock() above, which has already checked the
  //    lock setting

  auto r = Error;

  withLock(reason, [&](auto& lock) {
    r = waitForProcess(handle, exitCode, lock);
  });

  return r;
}

ProcessRunner::Results ProcessRunner::waitForAllUSVFSProcessesWithLock(
  LockWidget::Reasons reason)
{
  if (!Settings::instance().interface().lockGUI()) {
    log::debug("not waiting for usvfs processes because user has disabled locking");
    return ForceUnlocked;
  }

  auto r = Error;

  withLock(reason, [&](auto& lock) {
    r = waitForAllUSVFSProcesses(lock);
  });

  return r;
}

ProcessRunner::Results ProcessRunner::waitForAllUSVFSProcesses(LockWidget& lock)
{
  for (;;) {
    const auto processes = getRunningUSVFSProcesses();
    if (processes.empty()) {
      break;
    }

    const auto r = waitForProcesses(processes, lock);

    if (r != Completed) {
      // error, cancelled, or unlocked
      return r;
    }

    // this process is completed, check for others
  }

  return Completed;
}
