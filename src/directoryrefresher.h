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

#ifndef DIRECTORYREFRESHER_H
#define DIRECTORYREFRESHER_H

#include <QObject>
#include <vector>
#include <QMutex>
#include <tuple>
#include <directoryentry.h>


/**
 * @brief used to asynchronously generate the virtual view of the combined data directory
 **/
class DirectoryRefresher : public QObject
{

  Q_OBJECT

public:

  /**
   * @brief constructor
   *
   **/
  DirectoryRefresher();

  ~DirectoryRefresher();

  /**
   * @brief retrieve the updated directory structure
   *
   * returns a pointer to the updated directory structure. DirectoryRefresher
   * deletes its own pointer and the caller takes custody of the pointer
   * 
   * @return updated directory structure
   **/
  MOShared::DirectoryEntry *getDirectoryStructure();

  /**
   * @brief sets up the mods to be included in the directory structure
   *
   * @param mods list of the mods to include
   **/
  void setMods(const std::vector<std::tuple<QString, QString, int> > &mods);

  /**
   * @brief sets up the directory where mods are stored
   * @param modDirectory the mod directory
   * @note this function could be obsoleted easily by storing absolute paths in the parameter to setMods. This is legacy
   */
  void setModDirectory(const QString &modDirectory);

  /**
   * @brief remove files from the directory structure that are known to be irrelevant to the game
   * @param the structure to clean
   */
  static void cleanStructure(MOShared::DirectoryEntry *structure);

  /**
   * @brief add files for a mod to the directory structure, including bsas
   * @param modName
   * @param priorityBSA
   * @param directory
   * @param priorityDir
   */
  static void addModToStructure(MOShared::DirectoryEntry *directoryStructure, const QString &modName, int priority, const QString &directory);

public slots:

  /**
   * @brief generate a directory structure from the mods set earlier
   **/
  void refresh();

signals:

  void progress(int progress);
  void error(const QString &error);
  void refreshed();

private:

  std::vector<std::tuple<QString, QString, int> > m_Mods;
  MOShared::DirectoryEntry *m_DirectoryStructure;
  QMutex m_RefreshLock;

};

#endif // DIRECTORYREFRESHER_H
