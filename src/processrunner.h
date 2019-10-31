#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

#include "spawn.h"
#include "lockwidget.h"
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
    Running = 1,
    Completed,
    Error,
    Cancelled,
    ForceUnlocked
  };

  enum WaitFlag
  {
    NoFlags   = 0x00,
    Refresh   = 0x01,
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
    WaitFlags flags=NoFlags, LockWidget::Reasons reason=LockWidget::LockUI);

  ProcessRunner& setFromFile(QWidget* parent, const QFileInfo& targetInfo);
  ProcessRunner& setFromExecutable(const Executable& exe);
  ProcessRunner& setFromShortcut(const MOShortcut& shortcut);

  ProcessRunner& setFromFileOrExecutable(
    const QString &executable,
    const QStringList &args,
    const QString &cwd={},
    const QString &profile={},
    const QString &forcedCustomOverwrite={},
    bool ignoreCustomOverwrite=false);

  Results run();
  Results attachToProcess(HANDLE h);

  DWORD exitCode() const;
  HANDLE getProcessHandle() const;
  env::HandlePtr stealProcessHandle();

  Results waitForAllUSVFSProcessesWithLock(LockWidget::Reasons reason);

private:
  OrganizerCore& m_core;
  IUserInterface* m_ui;
  spawn::SpawnParameters m_sp;
  QString m_customOverwrite;
  ForcedLibraries m_forcedLibraries;
  QString m_profileName;
  LockWidget::Reasons m_lockReason;
  WaitFlags m_waitFlags;
  QString m_shellOpen;
  env::HandlePtr m_handle;
  DWORD m_exitCode;

  Results postRun();
  void withLock(std::function<void (LockWidget&)> f);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ProcessRunner::WaitFlags);

#endif // PROCESSRUNNER_H
