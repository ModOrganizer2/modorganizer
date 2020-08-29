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

#include "moddatacontent.h"
#include "categories.h"
#include "nexusinterface.h"
#include "modinfo.h"
#include "profile.h"

#include <imodlist.h>

#include <QFile>
#include <QListWidget>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#ifndef Q_MOC_RUN
#include <boost/signals2.hpp>
#endif
#include <set>
#include <vector>
#include <QVector>


class QSortFilterProxyModel;
class PluginContainer;
class OrganizerCore;

/**
 * Model presenting an overview of the installed mod
 * This is used in a view in the main window of MO. It combines general information about
 * the mods from ModInfo with status information from the Profile
 **/
class ModList : public QAbstractItemModel, public MOBase::IModList
{
  Q_OBJECT

public:

  enum EColumn {
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

  typedef boost::signals2::signal<void (const std::map<QString, ModStates>&)> SignalModStateChanged;
  typedef boost::signals2::signal<void (const QString &, int, int)> SignalModMoved;

public:

  /**
   * @brief constructor
   * @todo ensure this view works without a profile set, otherwise there are intransparent dependencies on the initialisation order
   **/
  ModList(PluginContainer *pluginContainer, OrganizerCore *parent);

  ~ModList();

  /**
   * @brief set the profile used for status information
   *
   * @param profile the profile to use
   **/
  void setProfile(Profile *profile);

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
  void removeRowForce(int row, const QModelIndex &parent);

  void notifyChange(int rowStart, int rowEnd = -1);
  static QString getColumnName(int column);

  void changeModPriority(int sourceIndex, int newPriority);
  void changeModPriority(std::vector<int> sourceIndices, int newPriority);

  void setOverwriteMarkers(const std::set<unsigned int> &overwrite, const std::set<unsigned int> &overwritten);
  void setPluginContainer(PluginContainer *pluginContainer);

  void setArchiveOverwriteMarkers(const std::set<unsigned int> &overwrite, const std::set<unsigned int> &overwritten);
  void setArchiveLooseOverwriteMarkers(const std::set<unsigned int> &overwrite, const std::set<unsigned int> &overwritten);

  bool modInfoAboutToChange(ModInfo::Ptr info);
  void modInfoChanged(ModInfo::Ptr info);

  void disconnectSlots();

  int timeElapsedSinceLastChecked() const;

  void highlightMods(const QItemSelectionModel *selection, const MOShared::DirectoryEntry &directoryEntry);

  /**
   * @brief Notify the mod list that the state of the specified mods has changed. This is used
   * to notify the plugin that registered through onModStateChanged().
   *
   * @param modIndices Indices of the mods that changed.
   */
  void notifyModStateChanged(QList<unsigned int> modIndices) const;

public:

  /// \copydoc MOBase::IModList::displayName
  virtual QString displayName(const QString &internalName) const override;

  /// \copydoc MOBase::IModList::allMods
  virtual QStringList allMods() const override;

  /// \copydoc MOBase::IModList::state
  virtual ModStates state(const QString &name) const override;

  /// \copydoc MOBase::IModList::setActive
  virtual bool setActive(const QString &name, bool active) override;

  /// \copydoc MOBase::IModList::setActive
  int setActive(const QStringList& names, bool active) override;

  /// \copydoc MOBase::IModList::priority
  virtual int priority(const QString &name) const override;

  /// \copydoc MOBase::IModList::setPriority
  virtual bool setPriority(const QString &name, int newPriority) override;

  /// \copydoc MOBase::IModList::onModStateChanged
  virtual bool onModStateChanged(const std::function<void(const std::map<QString, ModStates>&)>& func) override;

  /// \copydoc MOBase::IModList::onModMoved
  virtual bool onModMoved(const std::function<void (const QString &, int, int)> &func) override;

public: // implementation of virtual functions of QAbstractItemModel

  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
  virtual bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
  virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
  virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
  virtual Qt::ItemFlags flags(const QModelIndex &modelIndex) const;
  virtual Qt::DropActions supportedDropActions() const { return Qt::MoveAction | Qt::CopyAction; }
  virtual QStringList mimeTypes() const;
  virtual QMimeData *mimeData(const QModelIndexList &indexes) const;
  virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
  virtual bool removeRows(int row, int count, const QModelIndex &parent);

  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent = QModelIndex()) const;
  virtual QModelIndex parent(const QModelIndex &child) const;

  virtual QMap<int, QVariant> itemData(const QModelIndex &index) const;

public slots:

  void dropModeUpdate(bool dropOnItems);

  void enableSelected(const QItemSelectionModel *selectionModel);

  void disableSelected(const QItemSelectionModel *selectionModel);

signals:

  /**
   * @brief emitted whenever the sorting in the list was changed by the user
   *
   * the sorting of the list can only be manually changed if the list is sorted by priority
   * in which case the move is intended to change the priority of a mod
   **/
  void modorder_changed();

  /**
   * @brief emitted when the model wants a text to be displayed by the UI
   *
   * @param message the message to display
   **/
  void showMessage(const QString &message);

  /**
   * @brief signals change to the count of headers
   */
  void resizeHeaders();

  /**
   * @brief emitted to remove a file origin
   * @param name name of the orign to remove
   */
  void removeOrigin(const QString &name);

  /**
   * @brief emitted after a mod has been renamed
   * This signal MUST be used to fix the mod names in profiles (except the active one) and to invalidate/refresh other
   * structures that may have become invalid with the rename
   *
   * @param oldName the old name of the mod
   * @param newName new name of the mod
   */
  void modRenamed(const QString &oldName, const QString &newName);

  /**
   * @brief emitted after a mod has been uninstalled
   * @param fileName filename of the mod being uninstalled
   */
  void modUninstalled(const QString &fileName);

  /**
   * @brief emitted whenever a row in the list has changed
   *
   * @param index the index of the changed field
   * @param role role of the field that changed
   * @note this signal must only be emitted if the row really did change.
   *       Slots handling this signal therefore do not have to verify that a change has happened
   **/
  void modlistChanged(const QModelIndex &index, int role);

  /**
  * @brief emitted whenever multiple row sin the list has changed
  *
  * @param indicies the list of indicies of the changed field
  * @param role role of the field that changed
  * @note this signal must only be emitted if the row really did change.
  *       Slots handling this signal therefore do not have to verify that a change has happened
  **/
  void modlistChanged(const QModelIndexList &indicies, int role);

  /**
   * @brief QML seems to handle overloaded signals poorly - create unique signal for tutorials
   */
  void tutorialModlistUpdate();

  /**
   * @brief emitted to have all selected mods deleted
   */
  void removeSelectedMods();

  /**
   * @brief fileMoved emitted when a file is moved from one mod to another
   * @param relativePath relative path of the file moved
   * @param oldOriginName name of the origin that previously contained the file
   * @param newOriginName name of the origin that now contains the file
   */
  void fileMoved(const QString &relativePath, const QString &oldOriginName, const QString &newOriginName);

  /**
  * @brief emitted to have the overwrite folder cleared
  */
  void clearOverwrite();

  void aboutToChangeData();

  void postDataChanged();

protected:

  // event filter, handles event from the header and the tree view itself
  bool eventFilter(QObject *obj, QEvent *event);

private:

  QVariant getOverwriteData(int column, int role) const;

  QString getFlagText(ModInfo::EFlag flag, ModInfo::Ptr modInfo) const;

  QString getConflictFlagText(ModInfo::EConflictFlag flag, ModInfo::Ptr modInfo) const;

  QString getColumnToolTip(int column) const;

  QVariantList contentsToIcons(const std::set<int> &contentIds) const;

  QString contentsToToolTip(const std::set<int> &contentsIds) const;

  ModList::EColumn getEnabledColumn(int index) const;

  QVariant categoryData(int categoryID, int column, int role) const;
  QVariant modData(int modID, int modelColumn, int role) const;

  bool renameMod(int index, const QString &newName);

  bool dropURLs(const QMimeData *mimeData, int row, const QModelIndex &parent);

  bool dropMod(const QMimeData *mimeData, int row, const QModelIndex &parent);

  ModStates state(unsigned int modIndex) const;

  bool moveSelection(QAbstractItemView *itemView, int direction);

  bool deleteSelection(QAbstractItemView *itemView);

  bool toggleSelection(QAbstractItemView *itemView);

private slots:

private:

  struct TModInfo {
    TModInfo(unsigned int index, ModInfo::Ptr modInfo)
        : modInfo(modInfo), nameOrder(index), priorityOrder(0), modIDOrder(0), categoryOrder(0) {}
    ModInfo::Ptr modInfo;
    unsigned int nameOrder;
    unsigned int priorityOrder;
    unsigned int modIDOrder;
    unsigned int categoryOrder;
  };

  struct TModInfoChange {
    QString name;
    QFlags<IModList::ModState> state;
  };

private:

  OrganizerCore *m_Organizer;
  Profile *m_Profile;

  NexusInterface *m_NexusInterface;
  std::set<int> m_RequestIDs;

  mutable bool m_Modified;
  bool m_InNotifyChange;

  QFontMetrics m_FontMetrics;

  bool m_DropOnItems;

  std::set<unsigned int> m_Overwrite;
  std::set<unsigned int> m_Overwritten;
  std::set<unsigned int> m_ArchiveOverwrite;
  std::set<unsigned int> m_ArchiveOverwritten;
  std::set<unsigned int> m_ArchiveLooseOverwrite;
  std::set<unsigned int> m_ArchiveLooseOverwritten;

  TModInfoChange m_ChangeInfo;

  SignalModStateChanged m_ModStateChanged;
  SignalModMoved m_ModMoved;

  QElapsedTimer m_LastCheck;

  PluginContainer *m_PluginContainer;

};

#endif // MODLIST_H

