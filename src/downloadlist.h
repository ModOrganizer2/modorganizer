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

#include <QAbstractTableModel>
#include <QList>

class OrganizerCore;
class DownloadManager;
class Settings;

/**
 * @brief model of the list of active, pending, and completed downloads
 **/
struct Download
{
  int pendingIndex{-1};
  bool isPending{false};
  bool showInfoIncompleteWarning{false};
  QString fileName;
  QString name;
  QString status;
  QString size;
  QDateTime fileTime;
  QString modName;
  QString version;
  QString modId;
  QString game;
  QString tooltip;
};

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
  Qt::ItemFlags flags(const QModelIndex& idx) const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

  const Download& getDownloadByRow(int row);

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

private:
  void getDownloadInfo(Download& download);
  Download* getDownload(const QString& fileName);
  Download* getDownloadByPendingIndex(int index);
  int getDownloadRow(const QString& fileName);
  int getPendingRow(int index);
  void downloadUpdated(const QString& fileName);
  void updateData(int row);

public slots:
  /**
   * @brief used to inform the model that data has changed
   *
   * @param row the row that changed. This can be negative to update the whole view
   **/
  void update(const QString& fileName);

  void aboutToUpdate();
  void downloadAdded(const QString& fileName);  
  void downloadRemoved(const QString& fileName);
  void pendingDownloadAdded(int index);
  void pendingDownloadRemoved(int index);
  void pendingDownloadFilenameUpdated(int index, QString& fileName);

private:
  DownloadManager& m_manager;
  Settings& m_settings;

  QList<Download> m_downloads;
};

#endif  // DOWNLOADLIST_H
