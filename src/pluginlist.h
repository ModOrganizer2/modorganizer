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

#ifndef PLUGINLIST_H
#define PLUGINLIST_H

#include <QString>
#include <QListWidget>
#include <QTimer>
#include <vector>
#include <directoryentry.h>


/**
 * @brief model representing the plugins (.esp/.esm) in the current virtual data folder
 **/
class PluginList : public QAbstractTableModel
{
  Q_OBJECT

public:

  enum EColumn {
    COL_NAME,
    COL_PRIORITY,
    COL_MODINDEX,

    COL_LASTCOLUMN = COL_MODINDEX
  };

public:

  /**
   * @brief constructor
   *
   * @param parent parent object
   **/
  PluginList(QObject *parent = NULL);

  /**
   * @brief does a complete refresh of the list
   *
   * @param profileName name of the current profile
   * @param baseDirectory the root directory structure representing the virtual data directory
   * @todo the profile is not used? If it was, we should pass the Profile-object instead
   **/
  void refresh(const QString &profileName, const MOShared::DirectoryEntry &baseDirectory, const QString &pluginsFile, const QString &loadOrderFile, const QString &lockedOrderFile);

  /**
   * @brief enable a plugin based on its name
   *
   * @param name name of the plugin to enable
   **/
  void enableESP(const QString &name);

  /**
   * @brief test if a plugin is enabled
   *
   * @param name name of the plugin to look up
   * @return true if the plugin is enabled, false otherwise
   **/
  bool isEnabled(const QString &name);

  /**
   * @brief test if a plugin is enabled
   *
   * @param index index of the plugin to look up
   * @return true if the plugin is enabled, false otherwise
   * @throws std::out_of_range exception is thrown if index is invalid
   **/
  bool isEnabled(int index);

  /**
   * @brief update the plugin status (enabled/disabled) from the specified file
   *
   * @param fileName path of the file to load. the filename should be "plugin.txt"
   * @todo it would make sense to move this into the Profile-class
   **/
  void readEnabledFrom(const QString &fileName);

  /**
   * @brief save the plugin status to the specified file
   *
   * @param pluginFileName path of the plugin.txt to write to
   * @param loadOrderFileName path of the loadorder.txt to write to
   * @param lockedOrderFileName path of the lockedorder.txt to write to
   * @param deleterFileName file to receive a list of files to hide from the virtual data tree. This is used to hide unchecked plugins if "hideUnchecked" is true
   * @param hideUnchecked if true, plugins that aren't enabled will be hidden from the virtual data directory
   **/
  void saveTo(const QString &pluginFileName, const QString &loadOrderFileName, const QString &lockedOrderFileName,
              const QString &deleterFileName, bool hideUnchecked) const;

  /**
   * @brief save the current load order
   *
   * the load order used by the game is defined by the last modification time which this
   * function sets. An exception is newer version of skyrim where the load order is defined
   * by the order of files in plugins.txt
   * @param directoryStructure the root directory structure representing the virtual data directory
   * @return true on success or if there was nothing to save, false if the load order can't be saved, i.e. because files are locked
   * @todo since this works on actual files the load order can't be configured per-profile. Files of the same name
   *       in different mods can also have different load orders which makes this very intransparent
   * @note also stores to disk the list of locked esps
   **/
  bool saveLoadOrder(MOShared::DirectoryEntry &directoryStructure);

  QString getName(int index) const { return m_ESPs.at(index).m_Name; }
  int getPriority(int index) const { return m_ESPs.at(index).m_Priority; }
  bool isESPLocked(int index) const;
  void lockESPIndex(int index, bool lock);

  bool eventFilter(QObject *obj, QEvent *event);

  static QString getColumnName(int column);
  static QString getColumnToolTip(int column);

  void refreshLoadOrder();

public: // implementation of the QAbstractTableModel interface

  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
  virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
  virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
  virtual Qt::ItemFlags flags(const QModelIndex &index) const;
  virtual Qt::DropActions supportedDropActions() const { return Qt::MoveAction; }
  virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);

public slots:

  /**
   * @brief enables ALL plugins
   **/
  void enableAll();

  /**
   * @brief disables ALL plugins
   **/
  void disableAll();

signals:

 /**
  * @brief emitted when the plugin list changed, i.e. the load order was modified or a plugin was checked/unchecked
  * @note this is currently only used to signal that there are changes that can be saved, it does
  *       not immediately cause anything to be written to disc
  **/
 void esplist_changed();

 /**
  * @brief emitted when the plugin list should be saved
  */
 void saveTimer();

private:

  struct ESPInfo {

    ESPInfo(const QString &name, bool enabled, FILETIME time, const QString &originName, const QString &fullPath);
    QString m_Name;
    QString m_FullPath;
    bool m_Enabled;
    bool m_ForceEnabled;
    bool m_Removed;
    int m_Priority;
    int m_LoadOrder;
    FILETIME m_Time;
    QString m_OriginName;
    bool m_IsMaster;
    std::set<QString> m_Masters;
    mutable bool m_MasterUnset;
  };

  friend bool ByName(const ESPInfo& LHS, const ESPInfo& RHS);
  friend bool ByDate(const ESPInfo& LHS, const ESPInfo& RHS);
  friend bool ByPriority(const ESPInfo& LHS, const ESPInfo& RHS);

private:

  void syncLoadOrder();
  void updateIndices();

  void writePlugins(const QString &fileName, bool writeUnchecked) const;
  void writeLockedOrder(const QString &fileName) const;

  bool readLoadOrder(const QString &fileName);
  void readLockedOrderFrom(const QString &fileName);
  void setPluginPriority(int row, int &newPriority);
  void changePluginPriority(std::vector<int> rows, int newPriority);

  void startSaveTime();

  void testMasters();

private:

  std::vector<ESPInfo> m_ESPs;

  std::map<QString, int> m_ESPsByName;
  std::vector<int> m_ESPsByPriority;

  // maps esp names to the priority specified in loadorder.txt. The esp names are
  // all lowercase!! This is to work around the fact that BOSS for some reason writes some file with
  // capitalization that doesn't match the actual name
  std::map<QString, int> m_ESPLoadOrder;
  std::map<QString, int> m_LockedOrder;

  QString m_CurrentProfile;
  QFontMetrics m_FontMetrics;

  QTextCodec *m_Utf8Codec;
  QTextCodec *m_LocalCodec;

  mutable QTimer m_SaveTimer;

};

#endif // PLUGINLIST_H
