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

#include <QApplication>
#include <QDir>
#include <QString>


using namespace MOBase;
using namespace MOShared;


DirectoryRefresher::DirectoryRefresher()
  : m_DirectoryStructure(nullptr)
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

  for (const QString &archive : archives) {
    QFileInfo fileInfo(archive);
    if (m_EnabledArchives.find(fileInfo.fileName()) != m_EnabledArchives.end()) {
      try {
        directoryStructure->addFromBSA(ToWString(modName), directoryW,
                                       ToWString(QDir::toNativeSeparators(fileInfo.absoluteFilePath())), priority);
      } catch (const std::exception &e) {
        throw MyException(tr("failed to parse bsa %1: %2").arg(archive, e.what()));
      }
    }
  }
}

void DirectoryRefresher::addModFilesToStructure(DirectoryEntry *directoryStructure, const QString &modName,
                                                int priority, const QString &directory, const QStringList &stealFiles)
{
  std::wstring directoryW = ToWString(QDir::toNativeSeparators(directory));

  if (stealFiles.length() > 0) {
    // instead of adding all the files of the target directory, we just change the root of the specified
    // files to this mod
    FilesOrigin &origin = directoryStructure->createOrigin(ToWString(modName), directoryW, priority);
    for (const QString &filename : stealFiles) {
      QFileInfo fileInfo(filename);
      FileEntry::Ptr file = directoryStructure->findFile(ToWString(fileInfo.fileName()));
      if (file.get() != nullptr) {
        if (file->getOrigin() == 0) {
          // replace data as the origin on this bsa
          file->removeOrigin(0);
        }
        origin.addFile(file->getIndex());
        file->addOrigin(origin.getID(), file->getFileTime(), L"");
      } else {
        qWarning("%s not found", qPrintable(fileInfo.fileName()));
      }
    }
  } else {
    directoryStructure->addFromOrigin(ToWString(modName), directoryW, priority);
  }
}

void DirectoryRefresher::addModToStructure(DirectoryEntry *directoryStructure
                                           , const QString &modName
                                           , int priority
                                           , const QString &directory
                                           , const QStringList &stealFiles
                                           , const QStringList &archives)
{
  addModFilesToStructure(directoryStructure, modName, priority, directory, stealFiles);
  addModBSAToStructure(directoryStructure, modName, priority, directory, archives);
}

void DirectoryRefresher::refresh()
{
  QMutexLocker locker(&m_RefreshLock);

  delete m_DirectoryStructure;

  m_DirectoryStructure = new DirectoryEntry(L"data", nullptr, 0);

  IPluginGame const *game = qApp->property("managed_game").value<IPluginGame const *>();

  std::wstring dataDirectory = QDir::toNativeSeparators(game->dataDirectory().absolutePath()).toStdWString();
  m_DirectoryStructure->addFromOrigin(L"data", dataDirectory, 0);

  // TODO what was the point of having the priority in this tuple? the list is already sorted by priority
  auto iter = m_Mods.begin();

  //TODO i is the priority here, where higher = more important. the input vector is also sorted by priority but inverted!
  for (int i = 1; iter != m_Mods.end(); ++iter, ++i) {
    try {
      addModToStructure(m_DirectoryStructure, iter->modName, i, iter->absolutePath, iter->stealFiles, iter->archives);
    } catch (const std::exception &e) {
      emit error(tr("failed to read mod (%1): %2").arg(iter->modName, e.what()));
    }
    emit progress((i * 100) / m_Mods.size() + 1);
  }

  emit progress(100);

  cleanStructure(m_DirectoryStructure);

  emit refreshed();
}
