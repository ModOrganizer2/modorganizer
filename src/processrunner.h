#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

#include "spawn.h"
#include "uilocker.h"
#include "envmodule.h"
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
    NoFlags   = 0x00,

    // the ui will be refreshed once the process has completed
    Refresh   = 0x01,

    // the process will be waited for even if locking is disabled
    ForceWait = 0x02
  };

  using WaitFlags = QFlags<WaitFlag>;
  using ForcedLibraries = QList<MOBase::ExecutableForcedLoadSetting>;

  ProcessRunner(OrganizerCore& core, IUserInterface* ui);

  // move only
  ProcessRunner(ProcessRunner&&) = default;
  ProcessRunner& operator=(const ProcessRunner&) = delete;
  ProcessRunner(const ProcessRunner&) = delete;
  ProcessRunner& operator=(ProcessRunner&&) = delete;

  ProcessRunner& setBinary(const QFileInfo &binary);
  ProcessRunner& setArguments(const QString& arguments);
  ProcessRunner& setCurrentDirectory(const QDir& directory);
  ProcessRunner& setSteamID(const QString& steamID);
  ProcessRunner& setCustomOverwrite(const QString& customOverwrite);
  ProcessRunner& setForcedLibraries(const ForcedLibraries& forcedLibraries);
  ProcessRunner& setProfileName(const QString& profileName);
  ProcessRunner& setWaitForCompletion(
    WaitFlags flags=NoFlags, UILocker::Reasons reason=UILocker::LockUI);

  // if the target is an executable file, runs that; for anything else, calls
  // ShellExecute() on it
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
  ProcessRunner& setFromFileOrExecutable(
    const QString &executable,
    const QStringList &args,
    const QString &cwd={},
    const QString &profile={},
    const QString &forcedCustomOverwrite={},
    bool ignoreCustomOverwrite=false);

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
  QString m_shellOpen;
  env::HandlePtr m_handle;
  DWORD m_exitCode;


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
  void withLock(std::function<void (UILocker::Session&)> f);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ProcessRunner::WaitFlags);

#endif // PROCESSRUNNER_H
