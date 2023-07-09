#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

#include "envmodule.h"
#include "spawn.h"
#include "uilocker.h"
#include <executableinfo.h>

class OrganizerCore;
class IUserInterface;
class Executable;
class MOShortcut;

// handles spawning a process and waiting for it, including setting up the lock
// widget if required
//
class ProcessRunner
{
public:
  enum Results
  {
    // the process is still running
    Running = 1,

    // the process has run to completion
    Completed,

    // the process couldn't be started or waited for
    Error,

    // the user has clicked the cancel button in the lock widget
    Cancelled,

    // the user has clicked the unlock button in the lock widget
    ForceUnlocked
  };

  enum WaitFlag
  {
    NoFlags = 0x00,

    // the directory structure will be refreshed once the process has completed
    TriggerRefresh = 0x01,

    // the process will be waited for even if locking is disabled or the
    // process is not hooked
    ForceWait = 0x02,

    // only valid with TriggerRefresh; run() will block until the refresh has
    // completed
    WaitForRefresh = 0x04,

    // combination of flags used to run programs from the command line
    //
    //  1) TriggerRefresh: MO must refresh after running the program because
    //     programs can modify files behind its back; for example, external
    //     LOOT will modify loadorder.txt, so MO must read it back or it will
    //     write back the old order when exiting
    //
    //  2) WaitForRefresh: refreshing is asynchronous, so the refresh must
    //     complete before MO exits or stale data might be written to disk
    //
    //  3) ForceWait: MO must wait for the program to finish even if locking the
    //     ui is disabled
    ForCommandLine = TriggerRefresh | WaitForRefresh | ForceWait
  };

  using WaitFlags       = QFlags<WaitFlag>;
  using ForcedLibraries = QList<MOBase::ExecutableForcedLoadSetting>;

  ProcessRunner(OrganizerCore& core, IUserInterface* ui);

  // move only
  ProcessRunner(ProcessRunner&&)                 = default;
  ProcessRunner& operator=(const ProcessRunner&) = delete;
  ProcessRunner(const ProcessRunner&)            = delete;
  ProcessRunner& operator=(ProcessRunner&&)      = delete;

  ProcessRunner& setBinary(const QFileInfo& binary);
  ProcessRunner& setArguments(const QString& arguments);
  ProcessRunner& setCurrentDirectory(const QDir& directory);
  ProcessRunner& setSteamID(const QString& steamID);
  ProcessRunner& setCustomOverwrite(const QString& customOverwrite);
  ProcessRunner& setForcedLibraries(const ForcedLibraries& forcedLibraries);
  ProcessRunner& setProfileName(const QString& profileName);
  ProcessRunner& setWaitForCompletion(WaitFlags flags          = NoFlags,
                                      UILocker::Reasons reason = UILocker::LockUI);
  ProcessRunner& setHooked(bool b);

  // - if the target is an executable file, runs it hooked
  // - if the target is a file:
  //     - if forceHook is false, calls ShellExecute() on it
  //     - if forceHook is true, gets the executable associated with the file
  //       and runs that hooked by passing the file as an argument
  //
  ProcessRunner& setFromFile(QWidget* parent, const QFileInfo& targetInfo);

  ProcessRunner& setFromExecutable(const Executable& exe);
  ProcessRunner& setFromShortcut(const MOShortcut& shortcut);

  // this is a messy one that's used for running an arbitrary file from the
  // command line, or by plugins (see OrganizerProxy::startApplication())
  //
  // 1) if `executable` contains a path separator, it's treated as a binary on
  //    disk and will be launched with given settings; it's also looked up in
  //    the list of configured executables, which sets the steam ID and forced
  //    libraries
  //
  // 2) if `executable` has no path separators, it's treated purely as an
  //    executable, but its arguments, current directory and custom overwrite
  //    can also be overridden
  //
  //    if the executable is not found in the list, the binary is run solely
  //    based on the parameters given
  //
  ProcessRunner& setFromFileOrExecutable(const QString& executable,
                                         const QStringList& args,
                                         const QString& cwd                   = {},
                                         const QString& profile               = {},
                                         const QString& forcedCustomOverwrite = {},
                                         bool ignoreCustomOverwrite           = false);

  // spawns the process and waits for it if required
  //
  Results run();

  // takes ownership of the given handle and waits for it if required
  //
  Results attachToProcess(HANDLE h);

  // exit code of the process, will return -1 if the process wasn't waited for
  //
  DWORD exitCode() const;

  // this may be INVALID_HANDLE_VALUE if:
  //
  //  1) no process was started, or
  //  2) the process was started successfully, but the system didn't return a
  //     handle for it; this can happen for inproc handlers, for example, such
  //     the photo viewer
  //
  // note that the handle is still owned by this ProcessRunner and will be
  // closed when destroyed; see stealProcessHandle()
  //
  HANDLE getProcessHandle() const;

  // releases ownership of the process handle; if this is called after the
  // process is completed, exitCode() will still return the correct value
  //
  env::HandlePtr stealProcessHandle();

  // waits for all usvfs processes spawned by this instance of MO; returns
  // immediately with ForceUnlocked if locking is disabled
  //
  // strictly speaking, this shouldn't be here, as it has nothing to do with
  // running a process, but it uses the same internal stuff as when running a
  // process
  //
  Results waitForAllUSVFSProcessesWithLock(UILocker::Reasons reason);

private:
  OrganizerCore& m_core;
  IUserInterface* m_ui;
  spawn::SpawnParameters m_sp;
  QString m_customOverwrite;
  ForcedLibraries m_forcedLibraries;
  QString m_profileName;
  UILocker::Reasons m_lockReason;
  WaitFlags m_waitFlags;
  QFileInfo m_shellOpen;
  env::HandlePtr m_handle;
  DWORD m_exitCode;

  bool shouldRunShell() const;
  bool shouldRefresh(Results r) const;

  // runs the command in m_shellOpen; returns empty if it can be waited for
  //
  std::optional<Results> runShell();

  // runs the binary; returns empty if it can be waited for
  //
  std::optional<Results> runBinary();

  // waits for process completion if required
  //
  Results postRun();

  // creates the lock widget and calls f()
  //
  void withLock(std::function<void(UILocker::Session&)> f);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ProcessRunner::WaitFlags);

#endif  // PROCESSRUNNER_H
