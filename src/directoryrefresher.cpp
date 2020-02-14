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

#include "iplugingame.h"
#include "utility.h"
#include "report.h"
#include "modinfo.h"
#include "settings.h"
#include "envfs.h"
#include "modinfodialogfwd.h"

#include <QApplication>
#include <QDir>
#include <QString>
#include <QTextCodec>
#include <gameplugins.h>


using namespace MOBase;
using namespace MOShared;


DirectoryRefresher::DirectoryRefresher(std::size_t threadCount)
  : m_DirectoryStructure(nullptr), m_threadCount(threadCount)
{
}

DirectoryRefresher::~DirectoryRefresher()
{
  delete m_DirectoryStructure;
}

DirectoryEntry *DirectoryRefresher::getDirectoryStructure()
{
  QMutexLocker locker(&m_RefreshLock);
  DirectoryEntry *result = m_DirectoryStructure;
  m_DirectoryStructure = nullptr;
  return result;
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

void DirectoryRefresher::addModBSAToStructure(DirectoryEntry *directoryStructure, const QString &modName,
                                              int priority, const QString &directory, const QStringList &archives)
{
  std::wstring directoryW = ToWString(QDir::toNativeSeparators(directory));
  IPluginGame *game = qApp->property("managed_game").value<IPluginGame*>();

  GamePlugins *gamePlugins = game->feature<GamePlugins>();
  QStringList loadOrder = QStringList();
  gamePlugins->getLoadOrder(loadOrder);

  for (const QString &archive : archives) {
    QFileInfo fileInfo(archive);
    if (m_EnabledArchives.find(fileInfo.fileName()) != m_EnabledArchives.end()) {

      int order = -1;

      for (auto plugin : loadOrder)
      {
        QString name = plugin.left(plugin.size() - 4);
        if (fileInfo.fileName().startsWith(name + " - ", Qt::CaseInsensitive) || fileInfo.fileName().startsWith(name + ".", Qt::CaseInsensitive)) {
          order = loadOrder.indexOf(plugin);
        }
      }

      try {
        IPluginGame *game = qApp->property("managed_game").value<IPluginGame*>();
        directoryStructure->addFromBSA(ToWString(modName), directoryW, ToWString(QDir::toNativeSeparators(fileInfo.absoluteFilePath())), priority, order);
      } catch (const std::exception &e) {
        throw MyException(tr("failed to parse bsa %1: %2").arg(archive, e.what()));
      }
    }
  }
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
      continue;
    }
    QFileInfo fileInfo(filename);
    FileEntry::Ptr file = directoryStructure->findFile(ToWString(fileInfo.fileName()));
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
  TimeThis tt("addModFilesToStructure()");

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
  TimeThis tt("addModToStructure()");

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
  DirectoryEntry* ds = nullptr;
  std::wstring modName;
  std::wstring path;
  int prio = -1;
  env::Directory* dir = nullptr;
  DirectoryStats* stats =  nullptr;

  std::condition_variable cv;
  std::mutex mutex;
  bool ready = false;

  void wakeup()
  {
    ready = true;
    cv.notify_one();
  }

  void run()
  {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&]{ return ready; });

    SetThisThreadName(QString::fromStdWString(modName + L" refresher"));
    ds->addFromOrigin(modName, path, prio, *stats);

    /*if (Settings::instance().archiveParsing()) {
      addModBSAToStructure(
        directoryStructure,
        entries[i].modName,
        prio,
        entries[i].absolutePath,
        entries[i].archives);
    }*/

    ready = false;
  }
};

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

void DirectoryRefresher::addMultipleModsFilesToStructure(
  MOShared::DirectoryEntry *directoryStructure,
  const std::vector<EntryInfo>& entries, bool emitProgress)
{
  std::vector<env::Directory> dirs(entries.size());
  std::vector<DirectoryStats> stats(entries.size());

  {
    TimeThis tt("walk dirs");

    env::ThreadPool<ModThread> threads(m_threadCount);

    for (std::size_t i=0; i<entries.size(); ++i) {
      const auto& e = entries[i];
      const int prio = static_cast<int>(i + 1);

      try
      {
        if (e.stealFiles.length() > 0) {
          stealModFilesIntoStructure(
            directoryStructure, e.modName, prio, e.absolutePath, e.stealFiles);
        } else {
          auto& mt = threads.request();

          mt.ds = directoryStructure;
          mt.modName = entries[i].modName.toStdWString();
          mt.path = QDir::toNativeSeparators(e.absolutePath).toStdWString();
          mt.prio = prio;
          mt.dir = &dirs[i];
          mt.stats = &stats[i];

          stats[i].mod = entries[i].modName.toStdString();

          mt.wakeup();
        }
      } catch (const std::exception& ex) {
        emit error(tr("failed to read mod (%1): %2").arg(e.modName, ex.what()));
      }

      if (emitProgress) {
        emit progress((static_cast<int>(i) * 100) / static_cast<int>(entries.size()) + 1);
      }
    }

    threads.join();
  }

  dumpStats(stats);
}

namespace MOShared{ void logcounts(std::string w); }

void DirectoryRefresher::refresh()
{
  SetThisThreadName("DirectoryRefresher");

  for (int i=0; i<1; ++i) {
    QMutexLocker locker(&m_RefreshLock);

    //logcounts("before delete");
    delete m_DirectoryStructure;
    //logcounts("after delete");

    m_DirectoryStructure = new DirectoryEntry(L"data", nullptr, 0);

    IPluginGame *game = qApp->property("managed_game").value<IPluginGame*>();

    std::wstring dataDirectory =
      QDir::toNativeSeparators(game->dataDirectory().absolutePath()).toStdWString();

    {
      DirectoryStats dummy;
      m_DirectoryStructure->addFromOrigin(L"data", dataDirectory, 0, dummy);
    }

    std::sort(m_Mods.begin(), m_Mods.end(), [](auto lhs, auto rhs) {
      return lhs.priority < rhs.priority;
    });

    addMultipleModsFilesToStructure(m_DirectoryStructure, m_Mods, true);

    m_DirectoryStructure->getFileRegister()->sortOrigins();

    emit progress(100);

    cleanStructure(m_DirectoryStructure);

    emit refreshed();

    //logcounts("after refresh");
  }
}
