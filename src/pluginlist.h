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

#include <ifiletree.h>
#include <ipluginlist.h>
#include "profile.h"
#include "loot.h"

namespace MOBase { class IPluginGame; }

#include <QString>
#include <QListWidget>
#include <QTimer>
#include <QTime>
#include <QElapsedTimer>
#include <QTemporaryFile>

#pragma warning(push)
#pragma warning(disable: 4100)
#ifndef Q_MOC_RUN
#include <boost/signals2.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#endif

#include <vector>
#include <map>

class OrganizerCore;


template <class C>
class ChangeBracket {
public:
  ChangeBracket(C *model)
    : m_Model(nullptr)
  {
    QVariant var = model->property("__aboutToChange");
    bool aboutToChange = var.isValid() && var.toBool();
    if (!aboutToChange) {
      model->layoutAboutToBeChanged();
      model->setProperty("__aboutToChange", true);
      m_Model = model;
    }
  }
  ~ChangeBracket() {
    finish();
  }

  void finish() {
    if (m_Model != nullptr) {
      m_Model->layoutChanged();
      m_Model->setProperty("__aboutToChange", false);
      m_Model = nullptr;
    }
  }

private:
  C *m_Model;
};



/**
 * @brief model representing the plugins (.esp/.esm) in the current virtual data folder
 **/
class PluginList : public QAbstractItemModel
{
  Q_OBJECT
  friend class ChangeBracket<PluginList>;
public:

  enum EColumn {
    COL_NAME,
    COL_FLAGS,
    COL_PRIORITY,
    COL_MODINDEX,

    COL_LASTCOLUMN = COL_MODINDEX
  };

  using PluginStates = MOBase::IPluginList::PluginStates;

  friend class PluginListProxy;

  using SignalRefreshed = boost::signals2::signal<void ()>;
  using SignalPluginMoved = boost::signals2::signal<void (const QString &, int, int)>;
  using SignalPluginStateChanged = boost::signals2::signal<void (const std::map<QString, PluginStates>&)>;

public:

  /**
   * @brief constructor
   *
   * @param parent parent object
   **/
  PluginList(OrganizerCore &organizer);

  ~PluginList();

  /**
   * @brief does a complete refresh of the list
   *
   * @param profileName name of the current profile
   * @param baseDirectory the root directory structure representing the virtual data directory
   * @param lockedOrderFile list of plugins that shouldn't change load order
   * @todo the profile is not used? If it was, we should pass the Profile-object instead
   **/
  void refresh(const QString &profileName
               , const MOShared::DirectoryEntry &baseDirectory
               , const QString &lockedOrderFile
               , bool refresh);

  /**
   * @brief enable a plugin based on its name
   *
   * @param name name of the plugin to enable
   * @param enable set to true to enable the esp, false to disable it
   **/
  void enableESP(const QString &name, bool enable = true);

  /**
   * @brief test if a plugin is enabled
   *
   * @param name name of the plugin to look up
   * @return true if the plugin is enabled, false otherwise
   **/
  bool isEnabled(const QString &name);

  /**
   * @brief clear all additional information we stored on plugins
   */
  void clearAdditionalInformation();

  /**
   * @brief reset additional information on a mod
   * @param name name of the plugin to clear the information of
   */
  void clearInformation(const QString &name);

  /**
   * @brief add additional information on a mod (i.e. from loot)
   * @param name name of the plugin to add information about
   * @param message the message to add to the plugin
   */
  void addInformation(const QString &name, const QString &message);

  /**
   * adds information from a loot report
   */
  void addLootReport(const QString& name, Loot::Plugin plugin);

  /**
   * @brief test if a plugin is enabled
   *
   * @param index index of the plugin to look up
   * @return true if the plugin is enabled, false otherwise
   * @throws std::out_of_range exception is thrown if index is invalid
   **/
  bool isEnabled(int index);

  /**
   * @brief save the plugin status to the specified file
   *
   * @param lockedOrderFileName path of the lockedorder.txt to write to
   **/
  void saveTo(const QString &lockedOrderFileName) const;

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

  /**
   * @return number of enabled plugins in the list
   */
  int enabledCount() const;

  int timeElapsedSinceLastChecked() const;

  QString getName(int index) const { return m_ESPs.at(index).name; }
  int getPriority(int index) const { return m_ESPs.at(index).priority; }
  QString getIndexPriority(int index) const;
  bool isESPLocked(int index) const;
  void lockESPIndex(int index, bool lock);

  static QString getColumnName(int column);
  static QString getColumnToolTip(int column);

  // highlight plugins contained in the mods at the given indices
  //
  void highlightPlugins(
    const std::vector<unsigned int>& modIndices,
    const MOShared::DirectoryEntry &directoryEntry);

  void refreshLoadOrder();

  void disconnectSlots();

public:

  QStringList pluginNames() const;
  PluginStates state(const QString &name) const;
  void setState(const QString &name, PluginStates state);
  int priority(const QString &name) const;
  int loadOrder(const QString &name) const;
  bool setPriority(const QString& name, int newPriority);
  bool isMaster(const QString &name) const;
  bool isLight(const QString &name) const;
  bool isLightFlagged(const QString &name) const;
  QStringList masters(const QString &name) const;
  QString origin(const QString &name) const;
  void setLoadOrder(const QStringList& pluginList);

  boost::signals2::connection onRefreshed(const std::function<void()>& callback);
  boost::signals2::connection onPluginMoved(const std::function<void(const QString&, int, int)>& func);
  boost::signals2::connection onPluginStateChanged(const std::function<void (const std::map<QString, PluginStates>&)> &func);

public: // implementation of the QAbstractTableModel interface

  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
  virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
  virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
  virtual Qt::ItemFlags flags(const QModelIndex &index) const;
  virtual Qt::DropActions supportedDropActions() const { return Qt::MoveAction; }
  virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
  virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
  virtual QModelIndex parent(const QModelIndex &child) const;

public slots:

  // enable/disable all plugins
  //
  void setEnabledAll(bool enabled);

  // enable/disable plugins at the given indices.
  //
  void setEnabled(const QModelIndexList& indices, bool enabled);

  // send plugins to the given priority
  //
  void sendToPriority(const QModelIndexList& indices, int priority);

  // shift the priority of mods at the given indices by the given offset
  //
  void shiftPluginsPriority(const QModelIndexList& indices, int offset);

  // toggle the active state of mods at the given indices
  //
  void toggleState(const QModelIndexList& indices);

  /**
   * @brief The currently managed game has changed
   * @param gamePlugin
   */
  void managedGameChanged(MOBase::IPluginGame const *gamePlugin);

  /**
   * @brief Generate the plugin indexes because something was changed
   **/
  void generatePluginIndexes();

signals:

 /**
  * @brief emitted when the plugin list changed, i.e. the load order was modified or a plugin was checked/unchecked
  * @note this is currently only used to signal that there are changes that can be saved, it does
  *       not immediately cause anything to be written to disc
  **/
 void esplist_changed();

 void writePluginsList();


private:

  struct ESPInfo
  {
    ESPInfo(
      const QString &name, bool enabled, const QString &originName,
      const QString &fullPath, bool hasIni, std::set<QString> archives,
      bool lightSupported);

    QString name;
    QString fullPath;
    bool enabled;
    bool forceEnabled;
    int priority;
    QString index;
    int loadOrder;
    FILETIME time;
    QString originName;
    bool isMaster;
    bool isLight;
    bool isLightFlagged;
    bool modSelected;
    QString author;
    QString description;
    bool hasIni;
    std::set<QString, MOBase::FileNameComparator> archives;
    std::set<QString, MOBase::FileNameComparator> masters;
    mutable std::set<QString, MOBase::FileNameComparator> masterUnset;

    bool operator < (const ESPInfo& str) const
    {
      return (loadOrder < str.loadOrder);
    }
  };

  struct AdditionalInfo {
    QStringList messages;
    Loot::Plugin loot;
  };

  friend bool ByName(const ESPInfo& LHS, const ESPInfo& RHS);
  friend bool ByDate(const ESPInfo& LHS, const ESPInfo& RHS);
  friend bool ByPriority(const ESPInfo& LHS, const ESPInfo& RHS);

private:

  void syncLoadOrder();
  void updateIndices();

  void writeLockedOrder(const QString &fileName) const;

  void readLockedOrderFrom(const QString &fileName);
  void setPluginPriority(int row, int &newPriority, bool isForced=false);
  void changePluginPriority(std::vector<int> rows, int newPriority);

  void testMasters();

  void fixPrimaryPlugins();
  void fixPriorities();
  void fixPluginRelationships();

  int findPluginByPriority(int priority);

  /**
   * @brief Notify MO2 plugins that the states of the given plugins have changed to the given state.
   *
   * @param pluginNames Names of the plugin.
   * @param state New state of the plugin.
   *
   */
  void pluginStatesChanged(QStringList const& pluginNames, PluginStates state) const;

private:

  OrganizerCore& m_Organizer;

  std::vector<ESPInfo> m_ESPs;
  mutable std::map<QString, QByteArray> m_LastSaveHash;

  std::map<QString, int, MOBase::FileNameComparator> m_ESPsByName;
  std::vector<int> m_ESPsByPriority;

  std::map<QString, int, MOBase::FileNameComparator> m_LockedOrder;

  std::map<QString, AdditionalInfo, MOBase::FileNameComparator> m_AdditionalInfo; // maps esp names to boss information

  QString m_CurrentProfile;
  QFontMetrics m_FontMetrics;

  SignalRefreshed m_Refreshed;
  SignalPluginMoved m_PluginMoved;
  SignalPluginStateChanged m_PluginStateChanged;

  QTemporaryFile m_TempFile;

  QElapsedTimer m_LastCheck;

  const MOBase::IPluginGame *m_GamePlugin;


  QVariant displayData(const QModelIndex &modelIndex) const;
  QVariant checkstateData(const QModelIndex &modelIndex) const;
  QVariant foregroundData(const QModelIndex &modelIndex) const;
  QVariant backgroundData(const QModelIndex &modelIndex) const;
  QVariant fontData(const QModelIndex &modelIndex) const;
  QVariant alignmentData(const QModelIndex &modelIndex) const;
  QVariant tooltipData(const QModelIndex &modelIndex) const;
  QVariant iconData(const QModelIndex &modelIndex) const;

  QString makeLootTooltip(const Loot::Plugin& loot) const;
  bool isProblematic(const ESPInfo& esp, const AdditionalInfo* info) const;
  bool hasInfo(const ESPInfo& esp, const AdditionalInfo* info) const;
};

#pragma warning(pop)

#endif // PLUGINLIST_H
