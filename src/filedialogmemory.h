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

#ifndef FILEDIALOGMEMORY_H
#define FILEDIALOGMEMORY_H

#include <QFileDialog>
#include <QString>
#include <map>

class Settings;

class FileDialogMemory
{
public:
  FileDialogMemory() = delete;

  static void save(Settings& settings);
  static void restore(const Settings& settings);

  static QString getOpenFileName(const QString& dirID, QWidget* parent = 0,
                                 const QString& caption       = QString(),
                                 const QString& dir           = QString(),
                                 const QString& filter        = QString(),
                                 QString* selectedFilter      = 0,
                                 QFileDialog::Options options = QFileDialog::Option(0));

  static QString
  getExistingDirectory(const QString& dirID, QWidget* parent = 0,
                       const QString& caption       = QString(),
                       const QString& dir           = QString(),
                       QFileDialog::Options options = QFileDialog::ShowDirsOnly);
};

#endif  // FILEDIALOGMEMORY_H
