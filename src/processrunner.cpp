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


// returns a process that's in the hidden list, or the top-level process if
// they're all hidden; returns an invalid process if the list is empty
//
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

// gets the most interesting process in the list
//
std::pair<env::Process, Interest> getInterestingProcess(
  const std::vector<HANDLE>& initialProcesses)
{
  if (initialProcesses.empty()) {
    log::debug("nothing to wait for");
    return {{}, Interest::None};
  }

  std::vector<env::Process> processes;

  // getting process trees for all processes
  for (auto&& h : initialProcesses) {
    auto tree = env::getProcessTree(h);
    if (tree.isValid()) {
      processes.push_back(tree);
    }
  }

  if (processes.empty()) {
    // if the initial list wasn't empty but this one is, it means all the
    // processes were already completed
    log::debug("processes are already completed");
    return {{}, Interest::None};
  }

  const auto interest = findInterestingProcessInTrees(processes);
  if (!interest.first.isValid()) {
    // this shouldn't happen
    log::debug("no interesting process to wait for");
    return {{}, Interest::None};
  }

  return interest;
}

const std::chrono::milliseconds Infinite(-1);

// waits for completion, times out after `wait` if not Infinite
//
std::optional<ProcessRunner::Results> timedWait(
  HANDLE handle, DWORD pid, LockWidget& lock, std::chrono::milliseconds wait)
{
  using namespace std::chrono;

  high_resolution_clock::time_point start;
  if (wait != Infinite) {
    start = high_resolution_clock::now();
  }

  for (;;) {
    // wait for a very short while, allows for processing events below
    const auto r = singleWait(handle, pid);

    if (r) {
      // the process has either completed or an error was returned
      return *r;
    }

    // the process is still running

    // keep processing events so the app doesn't appear dead
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();

    // check the lock widget
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
      // check if enough time has elapsed
      const auto now = high_resolution_clock::now();
      if (duration_cast<milliseconds>(now - start) >= wait) {
        // if so, return an empty result
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

  // if the interesting process that was found is weak (such as ModOrganizer.exe
  // when starting a program from within the Data directory), start with a short
  // wait and check for more interesting children
  milliseconds wait(50);

  for (;;) {
    auto [p, interest] = getInterestingProcess(initialProcesses);
    if (!p.isValid()) {
      // nothing to wait on
      return ProcessRunner::Completed;
    }

    // update the lock widget
    lock.setInfo(p.pid(), p.name());

    // open the process
    auto interestingHandle = p.openHandleForWait();
    if (!interestingHandle) {
      return ProcessRunner::Error;
    }

    if (p.pid() != currentPID) {
      // log any change in the process being waited for
      currentPID = p.pid();

      log::debug(
        "waiting for completion on {} ({}), {} interest",
        p.name(), p.pid(), toString(interest));
    }

    if (interest == Interest::Strong) {
      // don't bother with short wait, this is a good process to wait for
      wait = Infinite;
    }

    const auto r = timedWait(interestingHandle.get(), p.pid(), lock, wait);
    if (r) {
      // the process has completed or returned an error
      return *r;
    }

    // exponentially increase the wait time between checks for interesting
    // processes
    wait = std::min(wait * 2, milliseconds(2000));
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


ProcessRunner::ProcessRunner(OrganizerCore& core, IUserInterface* ui) :
  m_core(core), m_ui(ui), m_lockReason(LockWidget::NoReason),
  m_waitFlags(NoFlags), m_handle(INVALID_HANDLE_VALUE), m_exitCode(-1)
{
  // all processes started in ProcessRunner are hooked
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
  WaitFlags flags, LockWidget::Reasons reason)
{
  m_waitFlags = flags;
  m_lockReason = reason;
  return *this;
}

ProcessRunner& ProcessRunner::setFromFile(QWidget* parent, const QFileInfo& targetInfo)
{
  if (!parent && m_ui) {
    parent = m_ui->qtWidget();
  }

  // if the file is a .exe, start it directory; if it's anything else, ask the
  // shell to start it

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

  setBinary(executable);
  setArguments(args.join(" "));
  setCurrentDirectory(cwd);
  setProfileName(profileOverride);

  if (executable.contains('\\') || executable.contains('/')) {
    // file path

    if (m_sp.binary.isRelative()) {
      // relative path, should be relative to game directory
      setBinary(m_core.managedGame()->gameDirectory().absoluteFilePath(executable));
    }

    if (cwd == "") {
      setCurrentDirectory(m_sp.binary.absolutePath());
    }

    try {
      const Executable& exe = m_core.executablesList()->getByBinary(m_sp.binary);

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
      }

      setBinary(exe.binaryInfo());

      if (cwd == "") {
        setCurrentDirectory(exe.workingDirectory());
      }
    } catch (const std::runtime_error &) {
      log::warn("\"{}\" not set up as executable", executable);
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
  std::optional<Results> r;

  if (!m_shellOpen.isEmpty()) {
    r = runShell();
  } else {
    r = runBinary();
  }

  if (r) {
    // early result: something went wrong and the process cannot be waited for
    return *r;
  }

  return postRun();
}

std::optional<ProcessRunner::Results> ProcessRunner::runShell()
{
  log::debug("executing from shell: '{}'", m_shellOpen);

  auto r = shell::Open(m_shellOpen);
  if (!r.success()) {
    return Error;
  }

  m_handle.reset(r.stealProcessHandle());

  // not all files will return a valid handle even if opening them was
  // successful, such as inproc handlers (like the photo viewer); in this
  // case it's impossible to determine the status, so just say it's still
  // running
  if (m_handle.get() == INVALID_HANDLE_VALUE) {
    log::debug("shell didn't report an error, but no handle is available");
    return Running;
  }

  return {};
}

std::optional<ProcessRunner::Results> ProcessRunner::runBinary()
{
  if (m_profileName.isEmpty()) {
    // get the current profile name if it wasn't overridden
    const auto* profile = m_core.currentProfile();
    if (!profile) {
      throw MyException(QObject::tr("No profile set"));
    }

    m_profileName = profile->name();
  }

  // saves profile, sets up usvfs, notifies plugins, etc.; can return false if
  // a plugin doesn't want the program to run (such as when checkFNIS fails to
  // run FNIS and the user clicks cancel)
  if (!m_core.beforeRun(m_sp.binary, m_profileName, m_customOverwrite, m_forcedLibraries)) {
    return Error;
  }

  // parent widget used for any dialog popped up while checking for things
  QWidget* parent = (m_ui ? m_ui->qtWidget() : nullptr);

  const auto* game = m_core.managedGame();
  auto& settings = m_core.settings();

  // start steam if needed
  if (!checkSteam(parent, m_sp, game->gameDirectory(), m_sp.steamAppID, settings)) {
    return Error;
  }

  // warn if the executable is on the blacklist
  if (!checkBlacklist(parent, m_sp, settings)) {
    return Error;
  }

  // if the executable is inside the mods folder another instance of
  // ModOrganizer.exe is spawned instead to launch it
  adjustForVirtualized(game, m_sp, settings);

  // run the binary
  m_handle.reset(startBinary(parent, m_sp));
  if (m_handle.get() == INVALID_HANDLE_VALUE) {
    return Error;
  }

  return {};
}

ProcessRunner::Results ProcessRunner::postRun()
{
  const bool mustWait = (m_waitFlags & ForceWait);

  if (mustWait && m_lockReason == LockWidget::NoReason) {
    // never lock the ui without an escape hatch for the user
    log::debug(
      "the ForceWait flag is set but the lock reason wasn't, "
      "defaulting to LockUI");

    m_lockReason = LockWidget::LockUI;
  }

  if (mustWait) {
    if (!m_core.settings().interface().lockGUI()) {
      // at least tell the user what's going on
      log::debug(
        "locking is disabled, but the output of the application is required; "
        "overriding this setting and locking the ui");
    }
  } else {
    // no force wait

    if (m_lockReason == LockWidget::NoReason) {
      // no locking requested
      return Running;
    }

    if (!m_core.settings().interface().lockGUI()) {
      // disabling locking is like clicking on unlock immediately
      log::debug("not waiting for process because locking is disabled");
      return ForceUnlocked;
    }
  }

  auto r = Error;

  withLock([&](auto& lock) {
    r = waitForProcess(m_handle.get(), &m_exitCode, lock);
  });

  if (r == Completed && (m_waitFlags & Refresh)) {
    // afterRun() is only called with the Refresh flag; it refreshes the
    // directory structure and notifies plugins
    //
    // refreshing is not always required and can actually cause problems:
    //
    //  1) running shortcuts doesn't need refreshing because MO closes right
    //     after
    //
    //  2) the mod info dialog is not set up to deal with refreshes, so that
    //     it will crash because the old DirectoryEntry's are still being used
    //     in the list
    //
    m_core.afterRun(m_sp.binary, m_exitCode);
  }

  return r;
}

ProcessRunner::Results ProcessRunner::attachToProcess(HANDLE h)
{
  m_handle.reset(h);
  return postRun();
}

DWORD ProcessRunner::exitCode() const
{
  return m_exitCode;
}

HANDLE ProcessRunner::getProcessHandle() const
{
  return m_handle.get();
}

env::HandlePtr ProcessRunner::stealProcessHandle()
{
  auto h = m_handle.release();
  m_handle.reset(INVALID_HANDLE_VALUE);
  return env::HandlePtr(h);
}

ProcessRunner::Results ProcessRunner::waitForAllUSVFSProcessesWithLock(
  LockWidget::Reasons reason)
{
  m_lockReason = reason;

  if (!m_core.settings().interface().lockGUI()) {
    // disabling locking is like clicking on unlock immediately
    return ForceUnlocked;
  }

  auto r = Error;

  withLock([&](auto& lock) {
    for (;;) {
      const auto processes = getRunningUSVFSProcesses();
      if (processes.empty()) {
        break;
      }

      r = waitForProcesses(processes, lock);

      if (r != Completed) {
        // error, cancelled, or unlocked
        return;
      }

      // this process is completed, check for others
    }

    r = Completed;
  });

  return r;
}

void ProcessRunner::withLock(std::function<void (LockWidget&)> f)
{
  auto lk = std::make_unique<LockWidget>(
    m_ui ? m_ui->qtWidget() : nullptr, m_lockReason);

  f(*lk);
}
