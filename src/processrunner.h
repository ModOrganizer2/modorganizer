#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

#include "spawn.h"
#include "lockwidget.h"
#include <executableinfo.h>

class OrganizerCore;
class IUserInterface;
class Executable;
class MOShortcut;

class SpawnedProcess
{
public:
  SpawnedProcess(HANDLE handle, spawn::SpawnParameters sp);

  SpawnedProcess(const SpawnedProcess&) = delete;
  SpawnedProcess& operator=(const SpawnedProcess&) = delete;
  SpawnedProcess(SpawnedProcess&& other);
  SpawnedProcess& operator=(SpawnedProcess&& other);
  ~SpawnedProcess();

  HANDLE releaseHandle();
  void wait();

private:
  HANDLE m_handle;
  spawn::SpawnParameters m_parameters;

  void destroy();
};


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

  enum RefreshModes
  {
    NoRefresh = 1,
    Refresh
  };

  using ForcedLibraries = QList<MOBase::ExecutableForcedLoadSetting>;

  ProcessRunner(OrganizerCore& core, IUserInterface* ui);

  ProcessRunner& setBinary(const QFileInfo &binary);
  ProcessRunner& setArguments(const QString& arguments);
  ProcessRunner& setCurrentDirectory(const QDir& directory);
  ProcessRunner& setSteamID(const QString& steamID);
  ProcessRunner& setCustomOverwrite(const QString& customOverwrite);
  ProcessRunner& setForcedLibraries(const ForcedLibraries& forcedLibraries);
  ProcessRunner& setProfileName(const QString& profileName);
  ProcessRunner& setWaitForCompletion(
    RefreshModes refresh, LockWidget::Reasons reason=LockWidget::LockUI);

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

  DWORD exitCode();
  HANDLE processHandle();

  Results waitForAllUSVFSProcessesWithLock(LockWidget::Reasons reason);

private:
  OrganizerCore& m_core;
  IUserInterface* m_ui;
  spawn::SpawnParameters m_sp;
  QString m_customOverwrite;
  ForcedLibraries m_forcedLibraries;
  QString m_profileName;
  LockWidget::Reasons m_lock;
  RefreshModes m_refresh;
  QString m_shellOpen;
  HANDLE m_handle;
  DWORD m_exitCode;

  Results postRun();

  void withLock(
    LockWidget::Reasons reason, std::function<void (LockWidget&)> f);

  Results waitForProcessCompletionWithLock(
    HANDLE handle, LPDWORD exitCode, LockWidget::Reasons reason);

  Results waitForApplication(
    HANDLE processHandle, LPDWORD exitCode, LockWidget::Reasons reason);

  Results waitForAllUSVFSProcesses(LockWidget& lock);
};

#endif // PROCESSRUNNER_H
