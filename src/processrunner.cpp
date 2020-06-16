#include "processrunner.h"
#include "organizercore.h"
#include "instancemanager.h"
#include "iuserinterface.h"
#include "envmodule.h"
#include "env.h"
#include <iplugingame.h>
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

  const auto res = WaitForSingleObject(handle, 50);

  switch (res)
  {
    case WAIT_OBJECT_0:
    {
      log::debug("process {} completed", pid);
      return ProcessRunner::Completed;
    }

    case WAIT_TIMEOUT:
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


struct InterestingProcess
{
  env::Process p;
  Interest interest = Interest::None;
  env::HandlePtr handle;
};


InterestingProcess findRandomProcess(const env::Process& root)
{
  for (auto&& c : root.children()) {
    env::HandlePtr h = c.openHandleForWait();
    if (h) {
      return {c, Interest::Weak, std::move(h)};
    }

    auto r = findRandomProcess(c);
    if (r.handle) {
      return r;
    }
  }

  return {};
}

// returns a process that's in the hidden list, or the top-level process if
// they're all hidden; returns an invalid process if the list is empty
//
InterestingProcess findInterestingProcessInTrees(const env::Process& root)
{
  // Certain process names we wish to "hide" for aesthetic reason:
  static const std::vector<QString> hiddenList = {
    QFileInfo(QCoreApplication::applicationFilePath()).fileName(),
    "conhost.exe"
  };

  if (root.children().empty()) {
    return {};
  }

  auto isHidden = [&](auto&& p) {
    for (auto h : hiddenList) {
      if (p.name().contains(h, Qt::CaseInsensitive)) {
        return true;
      }
    }

    return false;
  };


  for (auto&& p : root.children()) {
    if (!isHidden(p)) {
      env::HandlePtr h = p.openHandleForWait();
      if (h) {
        return {p, Interest::Strong, std::move(h)};
      }
    }

    auto r = findInterestingProcessInTrees(p);
    if (r.interest == Interest::Strong) {
      return r;
    }
  }

  // everything is hidden, just pick the first one that can be used
  return findRandomProcess(root);
}

void dump(const env::Process& p, int indent)
{
  log::debug(
    "{}{}, pid={}, ppid={}",
    std::string(indent * 4, ' '), p.name(), p.pid(), p.ppid());

  for (auto&& c : p.children()) {
    dump(c, indent + 1);
  }
}

void dump(const env::Process& root)
{
  log::debug("process tree:");

  for (auto&& p : root.children()) {
    dump(p, 1);
  }
}

// gets the most interesting process in the list
//
InterestingProcess getInterestingProcess(HANDLE job)
{
  env::Process root = env::getProcessTree(job);
  if (root.children().empty()) {
    log::debug("nothing to wait for");
    return {};
  }

  dump(root);

  auto interest = findInterestingProcessInTrees(root);
  if (!interest.handle) {
    // this can happen if none of the processes can be opened
    log::debug("no interesting process to wait for");
    return {};
  }

  return interest;
}

const std::chrono::milliseconds Infinite(-1);

// waits for completion, times out after `wait` if not Infinite
//
std::optional<ProcessRunner::Results> timedWait(
  HANDLE handle, DWORD pid, UILocker::Session& ls,
  std::chrono::milliseconds wait, std::atomic<bool>& interrupt)
{
  using namespace std::chrono;

  high_resolution_clock::time_point start;
  if (wait != Infinite) {
    start = high_resolution_clock::now();
  }

  while (!interrupt) {
    // wait for a very short while, allows for processing events below
    const auto r = singleWait(handle, pid);

    if (r) {
      // the process has either completed or an error was returned
      return *r;
    }

    // the process is still running

    // check the lock widget
    switch (ls.result())
    {
      case UILocker::StillLocked:
      {
        break;
      }

      case UILocker::ForceUnlocked:
      {
        log::debug("waiting for {} force unlocked by user", pid);
        return ProcessRunner::ForceUnlocked;
      }

      case UILocker::Cancelled:
      {
        log::debug("waiting for {} cancelled by user", pid);
        return ProcessRunner::Cancelled;
      }

      case UILocker::NoResult:  // fall-through
      default:
      {
        // shouldn't happen
        log::debug(
          "unexpected result {} while waiting for {}",
          static_cast<int>(ls.result()), pid);

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

  log::debug("waiting for {} interrupted", pid);
  return ProcessRunner::ForceUnlocked;
}

ProcessRunner::Results waitForProcessesThreadImpl(
  HANDLE job, UILocker::Session& ls, std::atomic<bool>& interrupt)
{
  using namespace std::chrono;

  DWORD currentPID = 0;

  // if the interesting process that was found is weak (such as ModOrganizer.exe
  // when starting a program from within the Data directory), start with a short
  // wait and check for more interesting children
  const milliseconds defaultWait(50);
  auto wait = defaultWait;

  while (!interrupt) {
    auto ip = getInterestingProcess(job);
    if (!ip.handle) {
      // nothing to wait on
      return ProcessRunner::Completed;
    }

    // update the lock widget
    ls.setInfo(ip.p.pid(), ip.p.name());

    if (ip.p.pid() != currentPID) {
      // log any change in the process being waited for
      currentPID = ip.p.pid();

      log::debug(
        "waiting for completion on {} ({}), {} interest",
        ip.p.name(), ip.p.pid(), toString(ip.interest));
    }

    if (ip.interest == Interest::Strong) {
      // don't bother with short wait, this is a good process to wait for
      wait = Infinite;
    }

    const auto r = timedWait(ip.handle.get(), ip.p.pid(), ls, wait, interrupt);
    if (r) {
      if (*r == ProcessRunner::Results::Completed) {
        // process completed, check another one, reset the wait time to find
        // interesting processes
        wait = defaultWait;
      } else if (*r != ProcessRunner::Results::Running) {
        // something's wrong, or the user unlocked the ui
        return *r;
      }
    }

    // exponentially increase the wait time between checks for interesting
    // processes
    wait = std::min(wait * 2, milliseconds(2000));
  }

  log::debug("waiting for processes interrupted");
  return ProcessRunner::ForceUnlocked;
}

void waitForProcessesThread(
  ProcessRunner::Results& result, HANDLE job, UILocker::Session& ls,
  std::atomic<bool>& interrupt)
{
  result = waitForProcessesThreadImpl(job, ls, interrupt);
  ls.unlock();
}

ProcessRunner::Results waitForProcesses(
  const std::vector<HANDLE>& initialProcesses, UILocker::Session& ls)
{
  if (initialProcesses.empty()) {
    // nothing to wait for
    return ProcessRunner::Completed;
  }

  // using a job so any child process started by any of those processes can also
  // be captured and monitored
  env::HandlePtr job(CreateJobObjectW(nullptr, nullptr));
  if (!job) {
    const auto e = GetLastError();

    log::error(
      "failed to create job to wait for processes, {}",
      formatSystemMessage(e));

    return ProcessRunner::Error;
  }

  bool oneWorked = false;

  for (auto&& h : initialProcesses) {
    if (::AssignProcessToJobObject(job.get(), h)) {
      oneWorked = true;
    } else {
      const auto e = GetLastError();

      // this happens when closing MO while multiple processes are running,
      // so the logging is disabled until it gets fixed

      //log::error(
      //  "can't assign process to job to wait for processes, {}",
      //  formatSystemMessage(e));

      // keep going
    }
  }

  HANDLE monitor = INVALID_HANDLE_VALUE;

  if (oneWorked) {
    monitor = job.get();
  } else {
    // none of the handles could be added to the job, just monitor the first one
    monitor = initialProcesses[0];
  }

  auto results = ProcessRunner::Running;
  std::atomic<bool> interrupt(false);

  auto* t = QThread::create(
    waitForProcessesThread,
    std::ref(results), monitor, std::ref(ls), std::ref(interrupt));

  QEventLoop events;
  QObject::connect(t, &QThread::finished, [&]{
    events.quit();
  });

  t->start();
  events.exec();

  if (t->isRunning()) {
    interrupt = true;
    t->wait();
  }

  delete t;

  return results;
}

ProcessRunner::Results waitForProcess(
  HANDLE initialProcess, LPDWORD exitCode, UILocker::Session& ls)
{
  std::vector<HANDLE> processes = {initialProcess};

  const auto r = waitForProcesses(processes, ls);

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
  m_core(core), m_ui(ui), m_lockReason(UILocker::NoReason),
  m_waitFlags(NoFlags), m_handle(INVALID_HANDLE_VALUE), m_exitCode(-1)
{
  // all processes started in ProcessRunner are hooked by default
  setHooked(true);
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
  WaitFlags flags, UILocker::Reasons reason)
{
  m_waitFlags = flags;
  m_lockReason = reason;
  return *this;
}

ProcessRunner& ProcessRunner::setHooked(bool b)
{
  m_sp.hooked = b;
  return *this;
}

ProcessRunner& ProcessRunner::setFromFile(
  QWidget* parent, const QFileInfo& targetInfo)
{
  if (!parent && m_ui) {
    parent = m_ui->mainWindow();
  }

  // if the file is a .exe, start it directly; if it's anything else, ask the
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
      m_shellOpen = targetInfo;
      setHooked(false);
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

  QString currentDirectory = exe.workingDirectory();
  if (currentDirectory.isEmpty()) {
    currentDirectory = exe.binaryInfo().absolutePath();
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

bool ProcessRunner::shouldRunShell() const
{
  return !m_shellOpen.filePath().isEmpty();
}

ProcessRunner::Results ProcessRunner::run()
{
  // check if setHooked() was called after setFromFile(); this needs to
  // modify the settings to run the associated executable instead of using
  // shell::Open()

  if (shouldRunShell() && m_sp.hooked) {
    // this is a non-executable file, but it should be hooked; the associated
    // executable needs to be retrieved and run instead
    auto assoc = env::getAssociation(m_shellOpen);
    if (!assoc.executable.filePath().isEmpty()) {
      setBinary(assoc.executable);
      setArguments(assoc.formattedCommandLine);
      setCurrentDirectory(assoc.executable.absoluteDir());
      m_shellOpen = {};
    } else {
      // if it fails, just use the regular shell open
      log::error("failed to get the associated executable, running unhooked");
      m_sp.hooked = false;
    }
  } else if (!shouldRunShell() && !m_sp.hooked) {
    // this is an executable that should not be hooked; just run it through
    // the shell
    m_shellOpen = m_sp.binary;
  }


  std::optional<Results> r;

  if (shouldRunShell()) {
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
  const auto file = m_shellOpen.absoluteFilePath();

  log::debug("executing from shell: '{}'", file);

  auto r = shell::Open(file);
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
  QWidget* parent = (m_ui ? m_ui->mainWindow() : nullptr);

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

bool ProcessRunner::shouldRefresh(Results r) const
{
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
  if (!m_waitFlags.testFlag(Refresh)) {
    log::debug("not refreshing because the flag isn't set");
    return false;
  }

  switch (r)
  {
    case Completed:
    {
      log::debug("refreshing because the process completed");
      return true;
    }

    case ForceUnlocked:
    {
      log::debug("refreshing because the ui was force unlocked");
      return true;
    }

    case Error:          // fall-through
    case Cancelled:
    case Running:
    default:
    {
      return false;
    }
  }
}

ProcessRunner::Results ProcessRunner::postRun()
{
  const bool mustWait = (m_waitFlags & ForceWait);

  if (!m_sp.hooked && !mustWait) {
    // the process wasn't hooked and there's no force wait, don't lock
    return Running;
  }

  if (mustWait && m_lockReason == UILocker::NoReason) {
    // never lock the ui without an escape hatch for the user
    log::debug(
      "the ForceWait flag is set but the lock reason wasn't, "
      "defaulting to LockUI");

    m_lockReason = UILocker::LockUI;
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

    if (m_lockReason == UILocker::NoReason) {
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

  withLock([&](auto& ls) {
    r = waitForProcess(m_handle.get(), &m_exitCode, ls);
  });

  if (shouldRefresh(r)) {
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
  UILocker::Reasons reason)
{
  m_lockReason = reason;

  if (!m_core.settings().interface().lockGUI()) {
    // disabling locking is like clicking on unlock immediately
    return ForceUnlocked;
  }

  auto r = Error;

  for (;;) {
    withLock([&](auto& ls) {
      const auto processes = getRunningUSVFSProcesses();
      if (processes.empty()) {
        r = Completed;
        return;
      }

      r = waitForProcesses(processes, ls);

      if (r != Completed) {
        // error, cancelled, or unlocked
        return;
      }

      // this process is completed, check for others
      r = Running;
    });

    if (r != Running) {
      break;
    }
  }

  return r;
}

void ProcessRunner::withLock(std::function<void (UILocker::Session&)> f)
{
  auto ls = UILocker::instance().lock(m_lockReason);
  f(*ls);
}
