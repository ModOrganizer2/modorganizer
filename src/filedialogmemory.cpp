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

#include "filedialogmemory.h"
#include "settings.h"
#include <QFileDialog>

static std::map<QString, QString> g_Cache;

void FileDialogMemory::save(Settings& s)
{
  s.paths().setRecent(g_Cache);
}

void FileDialogMemory::restore(const Settings& s)
{
  g_Cache = s.paths().recent();
}

QString FileDialogMemory::getOpenFileName(const QString& dirID, QWidget* parent,
                                          const QString& caption, const QString& dir,
                                          const QString& filter,
                                          QString* selectedFilter,
                                          QFileDialog::Options options)
{
  QString currentDir = dir;

  if (currentDir.isEmpty()) {
    auto itor = g_Cache.find(dirID);
    if (itor != g_Cache.end()) {
      currentDir = itor->second;
    }
  }

  QString result = QFileDialog::getOpenFileName(parent, caption, currentDir, filter,
                                                selectedFilter, options);

  if (!result.isNull()) {
    g_Cache[dirID] = QFileInfo(result).path();
  }

  return result;
}

QString FileDialogMemory::getExistingDirectory(const QString& dirID, QWidget* parent,
                                               const QString& caption,
                                               const QString& dir,
                                               QFileDialog::Options options)
{
  QString currentDir = dir;

  if (currentDir.isEmpty()) {
    auto itor = g_Cache.find(dirID);
    if (itor != g_Cache.end()) {
      currentDir = itor->second;
    }
  }

  QString result =
      QFileDialog::getExistingDirectory(parent, caption, currentDir, options);

  if (!result.isNull()) {
    g_Cache[dirID] = result;
  }

  return result;
}
