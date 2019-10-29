#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

#include "spawn.h"
#include <executableinfo.h>

class OrganizerCore;
class ILockedWaitingForProcess;
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
  ProcessRunner(OrganizerCore& core);

  void setUserInterface(IUserInterface* ui);

  bool runFile(QWidget* parent, const QFileInfo& targetInfo);

  bool runExecutableFile(
    const QFileInfo &binary, const QString &arguments,
    const QDir &currentDirectory, const QString &steamAppID={},
    const QString &customOverwrite={},
    const QList<MOBase::ExecutableForcedLoadSetting> &forcedLibraries={},
    bool refresh=true);

  bool runExecutable(const Executable& exe, bool refresh=true);

  bool runShortcut(const MOShortcut& shortcut);

  HANDLE runExecutableOrExecutableFile(
    const QString &executable, const QStringList &args, const QString &cwd,
    const QString &profile, const QString &forcedCustomOverwrite = "",
    bool ignoreCustomOverwrite = false);

  bool waitForApplication(HANDLE processHandle, LPDWORD exitCode = nullptr);

  bool waitForAllUSVFSProcessesWithLock();

private:
  OrganizerCore& m_core;
  IUserInterface* m_ui;

  HANDLE spawnAndWait(
    const QFileInfo &binary, const QString &arguments,
    const QString &profileName,
    const QDir &currentDirectory,
    const QString &steamAppID,
    const QString &customOverwrite,
    const QList<MOBase::ExecutableForcedLoadSetting> &forcedLibraries={},
    LPDWORD exitCode = nullptr);

  SpawnedProcess spawn(spawn::SpawnParameters sp);

  void withLock(std::function<void (ILockedWaitingForProcess*)> f);

  bool waitForProcessCompletionWithLock(HANDLE handle, LPDWORD exitCode);

  bool waitForProcessCompletion(
    HANDLE handle, LPDWORD exitCode, ILockedWaitingForProcess* uilock);

  bool waitForAllUSVFSProcesses(ILockedWaitingForProcess* uilock);
};

#endif // PROCESSRUNNER_H
