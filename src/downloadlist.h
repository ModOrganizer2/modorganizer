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

#ifndef DOWNLOADLIST_H
#define DOWNLOADLIST_H

#include "downloadlistitem.h"
#include <QAbstractTableModel>
#include <QList>

class OrganizerCore;
class DownloadManager;
class DownloadInfo;
class Settings;

/**
 * @brief model of the list of active and completed downloads
 **/
class DownloadList : public QAbstractTableModel
{

  Q_OBJECT

public:
  enum EColumn
  {
    COL_NAME = 0,
    COL_STATUS,
    COL_SIZE,
    COL_FILETIME,
    COL_MODNAME,
    COL_VERSION,
    COL_ID,
    COL_SOURCEGAME,
    COL_MOID,

    // number of columns
    COL_COUNT
  };

public:
  explicit DownloadList(OrganizerCore& core, QObject* parent = 0);

  /**
   * @brief retrieve the number of rows to display. Invoked by Qt
   *
   * @param parent not relevant for this implementation
   * @return number of rows to display
   **/
  virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
  virtual int columnCount(const QModelIndex& parent) const;

  QModelIndex index(int row, int column, const QModelIndex& parent) const;
  QModelIndex parent(const QModelIndex& child) const;
  Qt::ItemFlags flags(const QModelIndex& idx) const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

  /**
   * @brief retrieve the data to display in a specific row. Invoked by Qt
   *
   * @param index location to look up
   * @param role ... Defaults to Qt::DisplayRole.
   * @return this implementation only returns the row, the QItemDelegate implementation
   *is expected to fetch its information from the DownloadManager
   **/
  virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

  // used in DownloadsTab as the sorting predicate for the filter widget
  //
  bool lessThanPredicate(const QModelIndex& left, const QModelIndex& right);

  DownloadListItem* getDownloadListItem(QUuid moId);

private:
  void setDownloadListItem(DownloadManager::DownloadInfo* downloadInfo,
                           DownloadListItem& downloadListItem);

  void downloadUpdated(DownloadManager::DownloadInfo* downloadInfo);
  void updateData();

public slots:
  /**
   * @brief used to inform the model that data has changed
   *
   * @param downloadInfo the downloadInfo that changed.
   **/
  void update(DownloadManager::DownloadInfo* downloadInfo);

  /**
   * @brief used to inform the model that data has been added
   *
   * @param downloadInfo the downloadInfo that changed.
   **/
  void downloadAdded(DownloadManager::DownloadInfo* downloadInfo);

  /**
   * @brief used to inform the model that data has been removed
   *
   * @param MO ID the MO ID that changed.
   **/
  void downloadRemoved(QUuid moId);

  /**
   * @brief used to inform the model that pending data has been added
   *
   * @param MO ID the MO ID that changed.
   **/
  void pendingDownloadAdded(QString moId);

  /**
   * @brief used to inform the model that pending data has been removed
   *
   * @param MO ID the MO ID that changed.
   **/
  void pendingDownloadRemoved(QString moId);

private:
  DownloadManager& m_manager;
  Settings& m_settings;

  QList<DownloadListItem> m_downloadListItems;
  mutable std::unordered_map<QString, int> m_downloadIndexCache;
};

#endif  // DOWNLOADLIST_H
