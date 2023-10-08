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

#ifndef MODLIST_H
#define MODLIST_H

#include "categories.h"
#include "moddatacontent.h"
#include "modinfo.h"
#include "nexusinterface.h"
#include "profile.h"

#include <imodlist.h>

#include <QFile>
#include <QListWidget>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#ifndef Q_MOC_RUN
#include <boost/signals2.hpp>
#endif
#include <QVector>
#include <set>
#include <vector>

class QSortFilterProxyModel;
class PluginContainer;
class OrganizerCore;
class ModListDropInfo;

/**
 * Model presenting an overview of the installed mod
 * This is used in a view in the main window of MO. It combines general information
 *about the mods from ModInfo with status information from the Profile
 **/
class ModList : public QAbstractItemModel
{
  Q_OBJECT

public:
  enum ModListRole
  {

    // data(GroupingRole) contains the "group" role - This is used by the
    // category and Nexus ID grouping proxy (but not the ByPriority proxy)
    GroupingRole = Qt::UserRole,

    IndexRole = Qt::UserRole + 1,

    // data(AggrRole) contains aggregation information (for
    // grouping I assume?)
    AggrRole = Qt::UserRole + 2,

    GameNameRole = Qt::UserRole + 3,
    PriorityRole = Qt::UserRole + 4,

    // marking role for the scrollbar
    ScrollMarkRole = Qt::UserRole + 5,

    // this is the first available role
    ModUserRole = Qt::UserRole + 6
  };

  enum EColumn
  {
    COL_NAME,
    COL_CONFLICTFLAGS,
    COL_FLAGS,
    COL_CONTENT,
    COL_CATEGORY,
    COL_MODID,
    COL_GAME,
    COL_VERSION,
    COL_INSTALLTIME,
    COL_PRIORITY,
    COL_NOTES,
    COL_LASTCOLUMN = COL_NOTES,
  };

  using SignalModInstalled    = boost::signals2::signal<void(MOBase::IModInterface*)>;
  using SignalModRemoved      = boost::signals2::signal<void(QString const&)>;
  using SignalModStateChanged = boost::signals2::signal<void(
      const std::map<QString, MOBase::IModList::ModStates>&)>;
  using SignalModMoved        = boost::signals2::signal<void(const QString&, int, int)>;

public:
  /**
   * @brief constructor
   * @todo ensure this view works without a profile set, otherwise there are
   *intransparent dependencies on the initialisation order
   **/
  ModList(PluginContainer* pluginContainer, OrganizerCore* parent);

  ~ModList();

  /**
   * @brief set the profile used for status information
   *
   * @param profile the profile to use
   **/
  void setProfile(Profile* profile);

  /**
   * @brief retrieve the current sorting mode
   * @note this is used to store the sorting mode between sessions
   * @return current sorting mode, encoded to be compatible to previous versions
   **/
  int getCurrentSortingMode() const;

  /**
   * @brief remove the specified mod without asking for confirmation
   * @param row the row to remove
   */
  void removeRowForce(int row, const QModelIndex& parent);

  void notifyChange(int rowStart, int rowEnd = -1);
  static QString getColumnName(int column);

  void changeModPriority(int sourceIndex, int newPriority);
  void changeModPriority(std::vector<int> sourceIndices, int newPriority);

  void setPluginContainer(PluginContainer* pluginContainer);

  bool modInfoAboutToChange(ModInfo::Ptr info);
  void modInfoChanged(ModInfo::Ptr info);

  void disconnectSlots();

  int timeElapsedSinceLastChecked() const;

public:
  /**
   * @brief Notify the mod list that the given mod has been installed. This is used
   * to notify the plugin that registered through onModInstalled().
   *
   * @param mod The installed mod.
   */
  void notifyModInstalled(MOBase::IModInterface* mod) const;

  /**
   * @brief Notify the mod list that a mod has been removed. This is used
   * to notify the plugin that registered through onModRemoved().
   *
   * @param modName Name of the removed mod.
   */
  void notifyModRemoved(QString const& modName) const;

  /**
   * @brief Notify the mod list that the state of the specified mods has changed. This
   * is used to notify the plugin that registered through onModStateChanged().
   *
   * @param modIndices Indices of the mods that changed.
   */
  void notifyModStateChanged(QList<unsigned int> modIndices) const;

public:
  /// \copydoc MOBase::IModList::displayName
  QString displayName(const QString& internalName) const;

  /// \copydoc MOBase::IModList::allMods
  QStringList allMods() const;
  QStringList allModsByProfilePriority(MOBase::IProfile* profile = nullptr) const;

  // \copydoc MOBase::IModList::getMod
  MOBase::IModInterface* getMod(const QString& name) const;

  // \copydoc MOBase::IModList::remove
  bool removeMod(MOBase::IModInterface* mod);

  // \copydoc MOBase::IModList::renameMod
  MOBase::IModInterface* renameMod(MOBase::IModInterface* mod, const QString& name);

  /// \copydoc MOBase::IModList::state
  MOBase::IModList::ModStates state(const QString& name) const;

  /// \copydoc MOBase::IModList::setActive
  bool setActive(const QString& name, bool active);

  /// \copydoc MOBase::IModList::setActive
  int setActive(const QStringList& names, bool active);

  /// \copydoc MOBase::IModList::priority
  int priority(const QString& name) const;

  /// \copydoc MOBase::IModList::setPriority
  bool setPriority(const QString& name, int newPriority);

  boost::signals2::connection
  onModInstalled(const std::function<void(MOBase::IModInterface*)>& func);
  boost::signals2::connection
  onModRemoved(const std::function<void(QString const&)>& func);
  boost::signals2::connection onModStateChanged(
      const std::function<void(const std::map<QString, MOBase::IModList::ModStates>&)>&
          func);
  boost::signals2::connection
  onModMoved(const std::function<void(const QString&, int, int)>& func);

public:  // implementation of virtual functions of QAbstractItemModel
  virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
  virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const;
  virtual int columnCount(const QModelIndex& parent = QModelIndex()) const;
  virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);
  virtual QVariant headerData(int section, Qt::Orientation orientation,
                              int role = Qt::DisplayRole) const;
  virtual Qt::ItemFlags flags(const QModelIndex& modelIndex) const;
  virtual bool removeRows(int row, int count, const QModelIndex& parent);

  Qt::DropActions supportedDropActions() const override
  {
    return Qt::MoveAction | Qt::CopyAction;
  }
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                       int column, const QModelIndex& parent) const override;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                    const QModelIndex& parent) override;

  virtual QModelIndex index(int row, int column,
                            const QModelIndex& parent = QModelIndex()) const;
  virtual QModelIndex parent(const QModelIndex& child) const;

  virtual QMap<int, QVariant> itemData(const QModelIndex& index) const;

public slots:

  void onDragEnter(const QMimeData* data);

  // enable/disable mods at the given indices.
  //
  void setActive(const QModelIndexList& indices, bool active);

  // shift the priority of mods at the given indices by the given offset
  //
  void shiftModsPriority(const QModelIndexList& indices, int offset);

  // change the priority of the mods specified by the given indices
  //
  void changeModsPriority(const QModelIndexList& indices, int priority);

  // toggle the active state of mods at the given indices
  //
  bool toggleState(const QModelIndexList& indices);

signals:

  // emitted when the priority of one or multiple mods have changed
  //
  // the sorting of the list can only be manually changed if the list is sorted by
  // priority in which case the move is intended to change the priority of a mod.
  //
  void modPrioritiesChanged(const QModelIndexList& indices) const;

  // emitted when the state (active/inactive) of one or multiple mods have changed
  //
  void modStatesChanged(const QModelIndexList& indices) const;

  /**
   * @brief emitted when the model wants a text to be displayed by the UI
   *
   * @param message the message to display
   **/
  void showMessage(const QString& message);

  /**
   * @brief signals change to the count of headers
   */
  void resizeHeaders();

  /**
   * @brief emitted to remove a file origin
   * @param name name of the orign to remove
   */
  void removeOrigin(const QString& name);

  /**
   * @brief emitted after a mod has been renamed
   * This signal MUST be used to fix the mod names in profiles (except the active one)
   * and to invalidate/refresh other structures that may have become invalid with the
   * rename
   *
   * @param oldName the old name of the mod
   * @param newName new name of the mod
   */
  void modRenamed(const QString& oldName, const QString& newName);

  /**
   * @brief emitted after a mod has been uninstalled
   * @param fileName filename of the mod being uninstalled
   */
  void modUninstalled(const QString& fileName);

  /**
   * @brief QML seems to handle overloaded signals poorly - create unique signal for
   * tutorials
   */
  void tutorialModlistUpdate();

  /**
   * @brief fileMoved emitted when a file is moved from one mod to another
   * @param relativePath relative path of the file moved
   * @param oldOriginName name of the origin that previously contained the file
   * @param newOriginName name of the origin that now contains the file
   */
  void fileMoved(const QString& relativePath, const QString& oldOriginName,
                 const QString& newOriginName);

  /**
   * @brief emitted to have the overwrite folder cleared
   */
  void clearOverwrite();

  void aboutToChangeData();

  void postDataChanged();

  // emitted when an item is dropped from the download list, the row is from the
  // download list
  //
  void downloadArchiveDropped(QUuid moId, int priority);

  // emitted when an external archive is dropped on the mod list
  //
  void externalArchiveDropped(const QUrl& url, int priority);

  // emitted when an external folder is dropped on the mod list
  //
  void externalFolderDropped(const QUrl& url, int priority);

private:
  // retrieve the display name of a mod or convert from a user-provided
  // name to internal name
  //
  QString getDisplayName(ModInfo::Ptr info) const;
  QString makeInternalName(ModInfo::Ptr info, QString name) const;

  QString getFlagText(ModInfo::EFlag flag, ModInfo::Ptr modInfo) const;

  QString getConflictFlagText(ModInfo::EConflictFlag flag, ModInfo::Ptr modInfo) const;

  QString getColumnToolTip(int column) const;

  bool renameMod(int index, const QString& newName);

  MOBase::IModList::ModStates state(unsigned int modIndex) const;

  // handle dropping of local URLs files
  //
  bool dropLocalFiles(const ModListDropInfo& dropInfo, int row,
                      const QModelIndex& parent);

  // return the priority of the mod for a drop event
  //
  int dropPriority(int row, const QModelIndex& parent) const;

private:
  struct TModInfo
  {
    TModInfo(unsigned int index, ModInfo::Ptr modInfo)
        : modInfo(modInfo), nameOrder(index), priorityOrder(0), modIDOrder(0),
          categoryOrder(0)
    {}
    ModInfo::Ptr modInfo;
    unsigned int nameOrder;
    unsigned int priorityOrder;
    unsigned int modIDOrder;
    unsigned int categoryOrder;
  };

  struct TModInfoChange
  {
    QString name;
    QFlags<MOBase::IModList::ModState> state;
  };

private:
  OrganizerCore* m_Organizer;
  Profile* m_Profile;

  NexusInterface* m_NexusInterface;
  std::set<int> m_RequestIDs;

  mutable bool m_Modified;
  bool m_InNotifyChange;
  bool m_DropOnMod = false;

  QFontMetrics m_FontMetrics;

  TModInfoChange m_ChangeInfo;

  SignalModInstalled m_ModInstalled;
  SignalModMoved m_ModMoved;
  SignalModRemoved m_ModRemoved;
  SignalModStateChanged m_ModStateChanged;

  QElapsedTimer m_LastCheck;

  PluginContainer* m_PluginContainer;
};

#endif  // MODLIST_H
