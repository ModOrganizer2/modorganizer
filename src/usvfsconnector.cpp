/*
Copyright (C) 2015 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "usvfsconnector.h"
#include "envmodule.h"
#include "organizercore.h"
#include "settings.h"
#include "shared/util.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QProgressDialog>
#include <QTemporaryFile>
#include <iomanip>
#include <memory>
#include <qstandardpaths.h>
#include <sstream>
#include <usvfs/usvfs.h>

static const char SHMID[] = "mod_organizer_instance";
using namespace MOBase;

std::string to_hex(void* bufferIn, size_t bufferSize)
{
  unsigned char* buffer = static_cast<unsigned char*>(bufferIn);
  std::ostringstream temp;
  temp << std::hex;
  for (size_t i = 0; i < bufferSize; ++i) {
    temp << std::setfill('0') << std::setw(2) << (unsigned int)buffer[i];
    if ((i % 16) == 15) {
      temp << "\n";
    } else {
      temp << " ";
    }
  }
  return temp.str();
}

LogWorker::LogWorker()
    : m_Buffer(1024, '\0'), m_QuitRequested(false),
      m_LogFile(
          qApp->property("dataPath").toString() +
          QString("/logs/usvfs-%1.log")
              .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd_hh-mm-ss")))
{
  m_LogFile.open(QIODevice::WriteOnly);
  log::debug("usvfs log messages are written to {}", m_LogFile.fileName());
}

LogWorker::~LogWorker() {}

void LogWorker::process()
{
  MOShared::SetThisThreadName("LogWorker");

  int noLogCycles = 0;
  while (!m_QuitRequested) {
    if (usvfsGetLogMessages(&m_Buffer[0], m_Buffer.size(), false)) {
      m_LogFile.write(m_Buffer.c_str());
      m_LogFile.write("\n");
      m_LogFile.flush();
      noLogCycles = 0;
    } else {
      QThread::msleep(std::min(40, noLogCycles) * 5);
      ++noLogCycles;
    }
  }
  emit finished();
}

void LogWorker::exit()
{
  m_QuitRequested = true;
}

LogLevel toUsvfsLogLevel(log::Levels level)
{
  switch (level) {
  case log::Info:
    return LogLevel::Info;
  case log::Warning:
    return LogLevel::Warning;
  case log::Error:
    return LogLevel::Error;
  case log::Debug:  // fall-through
  default:
    return LogLevel::Debug;
  }
}

CrashDumpsType toUsvfsCrashDumpsType(env::CoreDumpTypes type)
{
  switch (type) {
  case env::CoreDumpTypes::None:
    return CrashDumpsType::None;

  case env::CoreDumpTypes::Data:
    return CrashDumpsType::Data;

  case env::CoreDumpTypes::Full:
    return CrashDumpsType::Full;

  case env::CoreDumpTypes::Mini:
  default:
    return CrashDumpsType::Mini;
  }
}

UsvfsConnector::UsvfsConnector()
{
  using namespace std::chrono;

  const auto& s = Settings::instance();

  const LogLevel logLevel = toUsvfsLogLevel(s.diagnostics().logLevel());
  const auto dumpType     = toUsvfsCrashDumpsType(s.diagnostics().coreDumpType());
  const auto delay        = duration_cast<milliseconds>(s.diagnostics().spawnDelay());
  std::string dumpPath =
      MOShared::ToString(OrganizerCore::getGlobalCoreDumpPath(), true);

  usvfsParameters* params = usvfsCreateParameters();

  usvfsSetInstanceName(params, SHMID);
  usvfsSetDebugMode(params, false);
  usvfsSetLogLevel(params, logLevel);
  usvfsSetCrashDumpType(params, dumpType);
  usvfsSetCrashDumpPath(params, dumpPath.c_str());
  usvfsSetProcessDelay(params, delay.count());

  usvfsInitLogging(false);

  log::debug("initializing usvfs:\n"
             " . instance: {}\n"
             " . log: {}\n"
             " . dump: {} ({})",
             SHMID, usvfsLogLevelToString(logLevel), dumpPath.c_str(),
             usvfsCrashDumpTypeToString(dumpType));

  usvfsCreateVFS(params);
  usvfsFreeParameters(params);

  usvfsClearExecutableBlacklist();
  for (auto exec : s.executablesBlacklist().split(";")) {
    std::wstring buf = exec.toStdWString();
    usvfsBlacklistExecutable(buf.data());
  }

  usvfsClearSkipFileSuffixes();
  for (auto& suffix : s.skipFileSuffixes()) {
    if (suffix.isEmpty()) {
      continue;
    }
    std::wstring buf = suffix.toStdWString();
    usvfsAddSkipFileSuffix(buf.data());
  }

  usvfsClearSkipDirectories();
  for (auto& dir : s.skipDirectories()) {
    std::wstring buf = dir.toStdWString();
    usvfsAddSkipDirectory(buf.data());
  }

  usvfsClearLibraryForceLoads();

  m_LogWorker.moveToThread(&m_WorkerThread);

  connect(&m_WorkerThread, SIGNAL(started()), &m_LogWorker, SLOT(process()));
  connect(&m_LogWorker, SIGNAL(finished()), &m_WorkerThread, SLOT(quit()));

  m_WorkerThread.start(QThread::LowestPriority);
}

UsvfsConnector::~UsvfsConnector()
{
  usvfsDisconnectVFS();
  m_LogWorker.exit();
  m_WorkerThread.quit();
  m_WorkerThread.wait();
}

void UsvfsConnector::updateMapping(const MappingType& mapping)
{
  const auto start = std::chrono::high_resolution_clock::now();

  QProgressDialog progress(qApp->activeWindow());
  progress.setLabelText(tr("Preparing vfs"));
  progress.setMaximum(static_cast<int>(mapping.size()));
  progress.show();

  int value = 0;
  int files = 0;
  int dirs  = 0;

  log::debug("Updating VFS mappings...");

  usvfsClearVirtualMappings();

  for (auto map : mapping) {
    if (progress.wasCanceled()) {
      usvfsClearVirtualMappings();
      throw UsvfsConnectorException("VFS mapping canceled by user");
    }
    progress.setValue(value++);
    if (value % 10 == 0) {
      QCoreApplication::processEvents();
    }

    if (map.isDirectory) {
      usvfsVirtualLinkDirectoryStatic(
          map.source.toStdWString().c_str(), map.destination.toStdWString().c_str(),
          (map.createTarget ? LINKFLAG_CREATETARGET : 0) | LINKFLAG_RECURSIVE);
      ++dirs;
    } else {
      usvfsVirtualLinkFile(map.source.toStdWString().c_str(),
                           map.destination.toStdWString().c_str(), 0);
      ++files;
    }
  }

  const auto end  = std::chrono::high_resolution_clock::now();
  const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  log::debug("VFS mappings updated, linked {} dirs and {} files in {}ms", dirs, files,
             time.count());
}

void UsvfsConnector::updateParams(MOBase::log::Levels logLevel,
                                  env::CoreDumpTypes coreDumpType,
                                  const QString& crashDumpsPath,
                                  std::chrono::seconds spawnDelay,
                                  QString executableBlacklist,
                                  const QStringList& skipFileSuffixes,
                                  const QStringList& skipDirectories)
{
  using namespace std::chrono;

  usvfsParameters* p = usvfsCreateParameters();

  usvfsSetDebugMode(p, FALSE);
  usvfsSetLogLevel(p, toUsvfsLogLevel(logLevel));
  usvfsSetCrashDumpType(p, toUsvfsCrashDumpsType(coreDumpType));
  usvfsSetCrashDumpPath(p, crashDumpsPath.toStdString().c_str());
  usvfsSetProcessDelay(p, duration_cast<milliseconds>(spawnDelay).count());

  usvfsUpdateParameters(p);
  usvfsFreeParameters(p);

  usvfsClearExecutableBlacklist();
  for (auto exec : executableBlacklist.split(";")) {
    std::wstring buf = exec.toStdWString();
    usvfsBlacklistExecutable(buf.data());
  }

  usvfsClearSkipFileSuffixes();
  for (auto& suffix : skipFileSuffixes) {
    if (suffix.isEmpty()) {
      continue;
    }
    std::wstring buf = suffix.toStdWString();
    usvfsAddSkipFileSuffix(buf.data());
  }

  usvfsClearSkipDirectories();
  for (auto& dir : skipDirectories) {
    std::wstring buf = dir.toStdWString();
    usvfsAddSkipDirectory(buf.data());
  }
}

void UsvfsConnector::updateForcedLibraries(
    const QList<MOBase::ExecutableForcedLoadSetting>& forcedLibraries)
{
  usvfsClearLibraryForceLoads();
  for (auto setting : forcedLibraries) {
    if (setting.enabled()) {
      usvfsForceLoadLibrary(setting.process().toStdWString().data(),
                            setting.library().toStdWString().data());
    }
  }
}

std::vector<HANDLE> getRunningUSVFSProcesses()
{
  std::vector<DWORD> pids;

  {
    size_t count  = 0;
    DWORD* buffer = nullptr;
    if (!::usvfsGetVFSProcessList2(&count, &buffer)) {
      log::error("failed to get usvfs process list");
      return {};
    }

    if (buffer) {
      pids.assign(buffer, buffer + count);
      std::free(buffer);
    }
  }

  const auto thisPid = GetCurrentProcessId();
  std::vector<HANDLE> v;

  const auto rights =
      PROCESS_QUERY_LIMITED_INFORMATION |     // exit code, image name, etc.
      SYNCHRONIZE |                           // wait functions
      PROCESS_SET_QUOTA | PROCESS_TERMINATE;  // add to job

  for (auto&& pid : pids) {
    if (pid == thisPid) {
      continue;  // obviously don't wait for MO process
    }

    HANDLE handle = ::OpenProcess(rights, FALSE, pid);

    if (handle == INVALID_HANDLE_VALUE) {
      const auto e = GetLastError();

      log::warn("failed to open usvfs process {}: {}", pid, formatSystemMessage(e));

      continue;
    }

    v.push_back(handle);
  }

  return v;
}
