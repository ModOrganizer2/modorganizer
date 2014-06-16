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

#include <directoryentry.h>
#include <QObject>
#include <QMutex>
#include <QStringList>
#include <vector>
#include <set>
#include <tuple>


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
  void setMods(const std::vector<std::tuple<QString, QString, int> > &mods, const std::set<QString> &managedArchives);

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
   * @param directoryStructure
   * @param modName
   * @param priority
   * @param directory
   * @param stealFiles
   * @param archives
   */
  void addModToStructure(MOShared::DirectoryEntry *directoryStructure, const QString &modName, int priority, const QString &directory, const QStringList &stealFiles, const QStringList &archives);

  /**
   * @brief add only the bsas of a mod to the directory structure
   * @param directoryStructure
   * @param modName
   * @param priority
   * @param directory
   * @param archives
   */
  void addModBSAToStructure(MOShared::DirectoryEntry *directoryStructure, const QString &modName, int priority, const QString &directory, const QStringList &archives);

  /**
   * @brief add only regular files ofr a mod to the directory structure
   * @param directoryStructure
   * @param modName
   * @param priority
   * @param directory
   * @param stealFiles
   */
  void addModFilesToStructure(MOShared::DirectoryEntry *directoryStructure, const QString &modName, int priority, const QString &directory, const QStringList &stealFiles);

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

  struct EntryInfo {
    EntryInfo(const QString &modName, const QString &absolutePath,
              const QStringList &stealFiles, const QStringList &archives, int priority)
      : modName(modName), absolutePath(absolutePath), stealFiles(stealFiles)
      , archives(archives), priority(priority) {}
    QString modName;
    QString absolutePath;
    QStringList stealFiles;
    QStringList archives;
    int priority;
  };

private:

  std::vector<EntryInfo> m_Mods;
  std::set<QString> m_EnabledArchives;
  MOShared::DirectoryEntry *m_DirectoryStructure;
  QMutex m_RefreshLock;

};

#endif // DIRECTORYREFRESHER_H
