/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

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

#include "directoryrefresher.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"

#include "iplugingame.h"
#include "utility.h"
#include "report.h"
#include "modinfo.h"
#include "settings.h"
#include "envfs.h"
#include "modinfodialogfwd.h"
#include "shared/util.h"

#include <gameplugins.h>

#include <QApplication>
#include <QDir>
#include <QString>

#include <fstream>


using namespace MOBase;
using namespace MOShared;


DirectoryStats::DirectoryStats()
{
  std::memset(this, 0, sizeof(DirectoryStats));
}

DirectoryStats& DirectoryStats::operator+=(const DirectoryStats& o)
{
  dirTimes += o.dirTimes;
  fileTimes += o.fileTimes;
  sortTimes += o.sortTimes;

  subdirLookupTimes += o.subdirLookupTimes;
  addDirectoryTimes += o.addDirectoryTimes;

  filesLookupTimes += o.filesLookupTimes;
  addFileTimes += o.addFileTimes;
  addOriginToFileTimes += o.addOriginToFileTimes;
  addFileToOriginTimes += o.addFileToOriginTimes;
  addFileToRegisterTimes += o.addFileToRegisterTimes;

  originExists += o.originExists;
  originCreate += o.originCreate;
  originsNeededEnabled += o.originsNeededEnabled;

  subdirExists += o.subdirExists;
  subdirCreate += o.subdirCreate;

  fileExists += o.fileExists;
  fileCreate += o.fileCreate;
  filesInsertedInRegister += o.filesInsertedInRegister;
  filesAssignedInRegister += o.filesAssignedInRegister;

  return *this;
}

std::string DirectoryStats::csvHeader()
{
  QStringList sl = {
    "dirTimes",
    "fileTimes",
    "sortTimes",
    "subdirLookupTimes",
    "addDirectoryTimes",
    "filesLookupTimes",
    "addFileTimes",
    "addOriginToFileTimes",
    "addFileToOriginTimes",
    "addFileToRegisterTimes",
    "originExists",
    "originCreate",
    "originsNeededEnabled",
    "subdirExists",
    "subdirCreate",
    "fileExists",
    "fileCreate",
    "filesInsertedInRegister",
    "filesAssignedInRegister"};

  return sl.join(",").toStdString();
}

std::string DirectoryStats::toCsv() const
{
  QStringList oss;

  auto s = [](auto ns) {
    return ns.count() / 1000.0 / 1000.0 / 1000.0;
  };

  oss
    << QString::number(s(dirTimes))
    << QString::number(s(fileTimes))
    << QString::number(s(sortTimes))

    << QString::number(s(subdirLookupTimes))
    << QString::number(s(addDirectoryTimes))

    << QString::number(s(filesLookupTimes))
    << QString::number(s(addFileTimes))
    << QString::number(s(addOriginToFileTimes))
    << QString::number(s(addFileToOriginTimes))
    << QString::number(s(addFileToRegisterTimes))

    << QString::number(originExists)
    << QString::number(originCreate)
    << QString::number(originsNeededEnabled)

    << QString::number(subdirExists)
    << QString::number(subdirCreate)

    << QString::number(fileExists)
    << QString::number(fileCreate)
    << QString::number(filesInsertedInRegister)
    << QString::number(filesAssignedInRegister);

  return oss.join(",").toStdString();
}

void dumpStats(std::vector<DirectoryStats>& stats)
{
  static int run = 0;
  static const std::string file("c:\\tmp\\data.csv");

  if (run == 0) {
    std::ofstream out(file, std::ios::out|std::ios::trunc);
    out << fmt::format("what,run,{}", DirectoryStats::csvHeader()) << "\n";
  }

  std::sort(stats.begin(), stats.end(), [](auto&& a, auto&& b){
    return (naturalCompare(QString::fromStdString(a.mod), QString::fromStdString(b.mod)) < 0);
    });

  std::ofstream out(file, std::ios::app);

  DirectoryStats total;
  for (const auto& s : stats) {
    out << fmt::format("{},{},{}", s.mod, run, s.toCsv()) << "\n";
    total += s;
  }

  out << fmt::format("total,{},{}", run, total.toCsv()) << "\n";

  ++run;
}


DirectoryRefresher::DirectoryRefresher(std::size_t threadCount)
  : m_threadCount(threadCount), m_lastFileCount(0)
{
}

DirectoryEntry *DirectoryRefresher::stealDirectoryStructure()
{
  QMutexLocker locker(&m_RefreshLock);
  return m_Root.release();
}

void DirectoryRefresher::setMods(const std::vector<std::tuple<QString, QString, int> > &mods
                                 , const std::set<QString> &managedArchives)
{
  QMutexLocker locker(&m_RefreshLock);

  m_Mods.clear();
  for (auto mod = mods.begin(); mod != mods.end(); ++mod) {
    QString name = std::get<0>(*mod);
    ModInfo::Ptr info = ModInfo::getByIndex(ModInfo::getIndex(name));
    m_Mods.push_back(EntryInfo(name, std::get<1>(*mod), info->stealFiles(), info->archives(), std::get<2>(*mod)));
  }

  m_EnabledArchives = managedArchives;
}

void DirectoryRefresher::cleanStructure(DirectoryEntry *structure)
{
  static const wchar_t *files[] = { L"meta.ini", L"readme.txt" };
  for (int i = 0; i < sizeof(files) / sizeof(wchar_t*); ++i) {
    structure->removeFile(files[i]);
  }

  static const wchar_t *dirs[] = { L"fomod" };
  for (int i = 0; i < sizeof(dirs) / sizeof(wchar_t*); ++i) {
    structure->removeDir(std::wstring(dirs[i]));
  }
}

void DirectoryRefresher::addModBSAToStructure(
  DirectoryEntry* root, const QString& modName,
  int priority, const QString& directory, const QStringList& archives)
{
  const IPluginGame *game = qApp->property("managed_game").value<IPluginGame*>();

  QStringList loadOrder;

  GamePlugins* gamePlugins = game->feature<GamePlugins>();
  if (gamePlugins) {
    loadOrder = gamePlugins->getLoadOrder();
  }

  std::vector<std::wstring> lo;
  for (auto&& s : loadOrder) {
    lo.push_back(s.toStdWString());
  }

  std::vector<std::wstring> archivesW;
  for (auto&& a : archives) {
    archivesW.push_back(a.toStdWString());
  }

  std::set<std::wstring> enabledArchives;
  for (auto&& a : m_EnabledArchives) {
    enabledArchives.insert(a.toStdWString());
  }

  DirectoryStats dummy;

  root->addFromAllBSAs(
    modName.toStdWString(),
    QDir::toNativeSeparators(directory).toStdWString(),
    priority,
    archivesW,
    enabledArchives,
    lo,
    dummy);
}

void DirectoryRefresher::stealModFilesIntoStructure(
  DirectoryEntry *directoryStructure, const QString &modName,
  int priority, const QString &directory, const QStringList &stealFiles)
{
  std::wstring directoryW = ToWString(QDir::toNativeSeparators(directory));

  // instead of adding all the files of the target directory, we just change the root of the specified
  // files to this mod
  DirectoryStats dummy;
  FilesOrigin &origin = directoryStructure->createOrigin(
    ToWString(modName), directoryW, priority, dummy);

  for (const QString &filename : stealFiles) {
    if (filename.isEmpty()) {
      log::warn("Trying to find file with no name");
      log::warn(" . modName: {}", modName);
      log::warn(" . directory: {}", directory);
      log::warn(" . priority: {}", priority);
      for (int i = 0; i < stealFiles.length(); ++i)
        log::warn(" . stealFiles[{}]: {}", i, stealFiles[i]);
      continue;
    }
    QFileInfo fileInfo(filename);
    FileEntryPtr file = directoryStructure->findFile(ToWString(fileInfo.fileName()));
    if (file.get() != nullptr) {
      if (file->getOrigin() == 0) {
        // replace data as the origin on this bsa
        file->removeOrigin(0);
      }
      origin.addFile(file->getIndex());
      file->addOrigin(origin.getID(), file->getFileTime(), L"", -1);
    } else {
      QString warnStr = fileInfo.absolutePath();
      if (warnStr.isEmpty())
        warnStr = filename;
      log::warn("file not found: {}", warnStr);
    }
  }
}

void DirectoryRefresher::addModFilesToStructure(
  DirectoryEntry *directoryStructure, const QString &modName,
  int priority, const QString &directory, const QStringList &stealFiles)
{
  TimeThis tt("DirectoryRefresher::addModFilesToStructure()");

  std::wstring directoryW = ToWString(QDir::toNativeSeparators(directory));
  DirectoryStats dummy;

  if (stealFiles.length() > 0) {
    stealModFilesIntoStructure(
      directoryStructure, modName, priority, directory, stealFiles);
  } else {
    directoryStructure->addFromOrigin(
      ToWString(modName), directoryW, priority, dummy);
  }
}

void DirectoryRefresher::addModToStructure(DirectoryEntry *directoryStructure
  , const QString &modName
  , int priority
  , const QString &directory
  , const QStringList &stealFiles
  , const QStringList &archives)
{
  TimeThis tt("DirectoryRefresher::addModToStructure()");

  DirectoryStats dummy;

  if (stealFiles.length() > 0) {
    stealModFilesIntoStructure(
      directoryStructure, modName, priority, directory, stealFiles);
  } else {
    std::wstring directoryW = ToWString(QDir::toNativeSeparators(directory));
    directoryStructure->addFromOrigin(
      ToWString(modName), directoryW, priority, dummy);
  }

  if (Settings::instance().archiveParsing()) {
    addModBSAToStructure(
      directoryStructure, modName, priority, directory, archives);
  }
}


struct ModThread
{
  DirectoryRefreshProgress* progress = nullptr;
  DirectoryEntry* ds = nullptr;
  std::wstring modName;
  std::wstring path;
  int prio = -1;
  std::vector<std::wstring> archives;
  std::set<std::wstring> enabledArchives;
  DirectoryStats* stats =  nullptr;
  env::DirectoryWalker walker;

  std::condition_variable cv;
  std::mutex mutex;
  bool ready = false;

  void wakeup()
  {
    {
      std::scoped_lock lock(mutex);
      ready = true;
    }

    cv.notify_one();
  }

  void run()
  {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&]{ return ready; });

    SetThisThreadName(QString::fromStdWString(modName + L" refresher"));
    ds->addFromOrigin(walker, modName, path, prio, *stats);

    if (Settings::instance().archiveParsing()) {
      const IPluginGame *game = qApp->property("managed_game").value<IPluginGame*>();

      QStringList loadOrder;
      GamePlugins* gamePlugins = game->feature<GamePlugins>();
      if (gamePlugins) {
        loadOrder = gamePlugins->getLoadOrder();
      }

      std::vector<std::wstring> lo;
      for (auto&& s : loadOrder) {
        lo.push_back(s.toStdWString());
      }

      ds->addFromAllBSAs(
        modName, path, prio, archives, enabledArchives, lo, *stats);
    }

    if (progress) {
      progress->addDone();
    }

    SetThisThreadName(QString::fromStdWString(L"idle refresher"));
    ready = false;
  }
};

env::ThreadPool<ModThread> g_threads;


void DirectoryRefresher::updateProgress(const DirectoryRefreshProgress* p)
{
  // careful: called from multiple threads
  emit progress(p);
}

void DirectoryRefresher::addMultipleModsFilesToStructure(
  MOShared::DirectoryEntry *directoryStructure,
  const std::vector<EntryInfo>& entries, DirectoryRefreshProgress* progress)
{
  std::vector<DirectoryStats> stats(entries.size());

  if (progress) {
    progress->start(entries.size());
  }

  log::debug("refresher: using {} threads", m_threadCount);
  g_threads.setMax(m_threadCount);

  for (std::size_t i=0; i<entries.size(); ++i) {
    const auto& e = entries[i];
    const int prio = e.priority + 1;

    if constexpr (DirectoryStats::EnableInstrumentation) {
      stats[i].mod = entries[i].modName.toStdString();
    }

    try
    {
      if (e.stealFiles.length() > 0) {
        stealModFilesIntoStructure(
          directoryStructure, e.modName, prio, e.absolutePath, e.stealFiles);

        if (progress) {
          progress->addDone();
        }
      } else {
        auto& mt = g_threads.request();

        mt.progress = progress;
        mt.ds = directoryStructure;
        mt.modName = e.modName.toStdWString();
        mt.path = QDir::toNativeSeparators(e.absolutePath).toStdWString();
        mt.prio = prio;

        mt.archives.clear();
        for (auto&& a : e.archives) {
          mt.archives.push_back(a.toStdWString());
        }

        mt.enabledArchives.clear();
        for (auto&& a : m_EnabledArchives) {
          mt.enabledArchives.insert(a.toStdWString());
        }

        mt.stats = &stats[i];

        mt.wakeup();
      }
    } catch (const std::exception& ex) {
      emit error(tr("failed to read mod (%1): %2").arg(e.modName, ex.what()));
    }
  }

  g_threads.waitForAll();

  if constexpr (DirectoryStats::EnableInstrumentation) {
    dumpStats(stats);
  }
}

void DirectoryRefresher::refresh()
{
  SetThisThreadName("DirectoryRefresher");
  TimeThis tt("DirectoryRefresher::refresh()");
  auto* p = new DirectoryRefreshProgress(this);

  {
    QMutexLocker locker(&m_RefreshLock);

    m_Root.reset(new DirectoryEntry(L"data", nullptr, 0));

    IPluginGame *game = qApp->property("managed_game").value<IPluginGame*>();

    std::wstring dataDirectory =
      QDir::toNativeSeparators(game->dataDirectory().absolutePath()).toStdWString();

    {
      DirectoryStats dummy;
      m_Root->addFromOrigin(L"data", dataDirectory, 0, dummy);
    }

    std::sort(m_Mods.begin(), m_Mods.end(), [](auto lhs, auto rhs) {
      return lhs.priority < rhs.priority;
    });

    addMultipleModsFilesToStructure(m_Root.get(), m_Mods, p);

    m_Root->getFileRegister()->sortOrigins();

    cleanStructure(m_Root.get());

    m_lastFileCount = m_Root->getFileRegister()->highestCount();
    log::debug("refresher saw {} files", m_lastFileCount);
  }

  p->finish();

  emit progress(p);
  emit refreshed();
}
