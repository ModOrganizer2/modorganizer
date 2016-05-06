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
#include "settings.h"
#include <memory>
#include <QTemporaryFile>
#include <QProgressDialog>
#include <QDateTime>
#include <QCoreApplication>

static const char SHMID[] = "mod_organizer_instance";

/*
extern "C" DLLEXPORT BOOL WINAPI VirtualLinkFile(LPCWSTR source, LPCWSTR
destination, BOOL failIfExists);
extern "C" DLLEXPORT BOOL WINAPI VirtualLinkDirectoryStatic(LPCWSTR source,
LPCWSTR destination, unsigned int flags);
extern "C" DLLEXPORT BOOL WINAPI ConnectVFS(const usvfs::Parameters
*parameters);
extern "C" DLLEXPORT void WINAPI DisconnectVFS();
extern "C" DLLEXPORT void WINAPI GetCurrentVFSName(char *buffer, size_t size);
extern "C" DLLEXPORT BOOL WINAPI CreateProcessHooked(LPCWSTR lpApplicationName
                                                     , LPWSTR lpCommandLine
                                                     , LPSECURITY_ATTRIBUTES
lpProcessAttributes
                                                     , LPSECURITY_ATTRIBUTES
lpThreadAttributes
                                                     , BOOL bInheritHandles
                                                     , DWORD dwCreationFlags
                                                     , LPVOID lpEnvironment
                                                     , LPCWSTR
lpCurrentDirectory
                                                     , LPSTARTUPINFOW
lpStartupInfo
                                                     , LPPROCESS_INFORMATION
lpProcessInformation
                                                     );
extern "C" DLLEXPORT void __cdecl InitLogging(bool toLocal = false);
extern "C" DLLEXPORT void __cdecl InitHooks(LPVOID userData, size_t
userDataSize);
*/

LogWorker::LogWorker()
  : m_Buffer(1024, '\0')
  , m_QuitRequested(false)
  , m_LogFile(qApp->property("dataPath").toString()
              + QString("/logs/usvfs-%1.log")
                    .arg(QDateTime::currentDateTimeUtc().toString(
                        "yyyy-MM-dd_hh-mm-ss")))
{
  m_LogFile.open(QIODevice::WriteOnly);
  qDebug("usvfs log messages are written to %s",
         qPrintable(m_LogFile.fileName()));
}

LogWorker::~LogWorker()
{
}

void LogWorker::process()
{
  int noLogCycles = 0;
  while (!m_QuitRequested) {
    if (GetLogMessages(&m_Buffer[0], m_Buffer.size(), false)) {
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

LogLevel logLevel(int level)
{
  switch (level) {
    case LogLevel::Info:
      return LogLevel::Info;
    case LogLevel::Warning:
      return LogLevel::Warning;
    case LogLevel::Error:
      return LogLevel::Error;
    default:
      return LogLevel::Debug;
  }
}

UsvfsConnector::UsvfsConnector()
{
  USVFSParameters params;
  USVFSInitParameters(&params, SHMID, false,
                      logLevel(Settings::instance().logLevel()));
  InitLogging(false);
  ConnectVFS(&params);

  BlacklistExecutable(L"TSVNCache.exe");

  m_LogWorker.moveToThread(&m_WorkerThread);

  connect(&m_WorkerThread, SIGNAL(started()), &m_LogWorker, SLOT(process()));
  connect(&m_LogWorker, SIGNAL(finished()), &m_WorkerThread, SLOT(quit()));

  m_WorkerThread.start(QThread::LowestPriority);
}

UsvfsConnector::~UsvfsConnector()
{
  DisconnectVFS();
  m_LogWorker.exit();
  m_WorkerThread.quit();
  m_WorkerThread.wait();
}

void UsvfsConnector::updateMapping(const MappingType &mapping)
{
  QProgressDialog progress;
  progress.setLabelText(tr("Preparing vfs"));
  progress.setMaximum(static_cast<int>(mapping.size()));
  progress.show();
  int value = 0;

  ClearVirtualMappings();

  for (auto map : mapping) {
    progress.setValue(value++);
    if (value % 10 == 0) {
      QCoreApplication::processEvents();
    }

    if (map.isDirectory) {
      VirtualLinkDirectoryStatic(map.source.toStdWString().c_str(),
                                 map.destination.toStdWString().c_str(),
                                 (map.createTarget ? LINKFLAG_CREATETARGET : 0)
                                     | LINKFLAG_RECURSIVE
                                 );
    } else {
      VirtualLinkFile(map.source.toStdWString().c_str(),
                      map.destination.toStdWString().c_str(), 0);
    }
  }
  /*
    size_t dumpSize = 0;
    CreateVFSDump(nullptr, &dumpSize);
    std::unique_ptr<char[]> buffer(new char[dumpSize]);
    CreateVFSDump(buffer.get(), &dumpSize);
    qDebug(buffer.get());
  */
}
