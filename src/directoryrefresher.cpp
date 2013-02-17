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
#include "utility.h"
#include "report.h"
#include <gameinfo.h>
#include <QDir>


using namespace MOBase;
using namespace MOShared;


DirectoryRefresher::DirectoryRefresher()
  : m_DirectoryStructure(NULL)
{
}

DirectoryEntry *DirectoryRefresher::getDirectoryStructure()
{
  QMutexLocker locker(&m_RefreshLock);
  DirectoryEntry *result = m_DirectoryStructure;
  m_DirectoryStructure = NULL;
  return result;
}

void DirectoryRefresher::setMods(const std::vector<std::tuple<QString, QString, int> > &mods)
{
  QMutexLocker locker(&m_RefreshLock);
  m_Mods = mods;
}


void DirectoryRefresher::cleanStructure(DirectoryEntry *structure)
{
  static wchar_t *files[] = { L"meta.ini", L"readme.txt" };
  for (int i = 0; i < sizeof(files) / sizeof(wchar_t*); ++i) {
    structure->removeFile(files[i]);
  }

  static wchar_t *dirs[] = { L"fomod" };
  for (int i = 0; i < sizeof(files) / sizeof(wchar_t*); ++i) {
    structure->removeDir(dirs[i]);
  }
}

void DirectoryRefresher::addModToStructure(DirectoryEntry *directoryStructure, const QString &modName, int priority, const QString &directory)
{
  std::wstring directoryW = ToWString(QDir::toNativeSeparators(directory));

  directoryStructure->addFromOrigin(ToWString(modName), directoryW, priority);
  QDir dir(directory);
  QFileInfoList bsaFiles = dir.entryInfoList(QStringList("*.bsa"), QDir::Files);
  foreach (QFileInfo file, bsaFiles) {
    directoryStructure->addFromBSA(ToWString(modName), directoryW,
                                   ToWString(QDir::toNativeSeparators(file.absoluteFilePath())), priority);
  }
}

void DirectoryRefresher::refresh()
{
  QMutexLocker locker(&m_RefreshLock);

  delete m_DirectoryStructure;

  m_DirectoryStructure = new DirectoryEntry(L"data", NULL, 0);

  // TODO what was the point of having the priority in this tuple? the list is already sorted by priority
  std::vector<std::tuple<QString, QString, int> >::const_iterator iter = m_Mods.begin();

  //TODO i is the priority here, where higher = more important. the input vector is also sorted by priority but inverted!
  for (int i = 1; iter != m_Mods.end(); ++iter, ++i) {
    QString modName = std::get<0>(*iter);
    try {
      addModToStructure(m_DirectoryStructure, modName, i, std::get<1>(*iter));
    } catch (const std::exception &e) {
      emit error(tr("failed to read bsa: %1").arg(e.what()));
    }
    emit progress((i * 100) / m_Mods.size() + 1);
  }

  std::wstring dataDirectory = GameInfo::instance().getGameDirectory() + L"\\data";
  m_DirectoryStructure->addFromOrigin(L"data", dataDirectory, 0);

  emit progress(100);

  cleanStructure(m_DirectoryStructure);

  emit refreshed();
}
