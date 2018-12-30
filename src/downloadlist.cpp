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

#include "downloadlist.h"
#include "downloadmanager.h"
#include <QEvent>

#include <QSortFilterProxyModel>


DownloadList::DownloadList(DownloadManager *manager, QObject *parent)
  : QAbstractTableModel(parent), m_Manager(manager)
{
  connect(m_Manager, SIGNAL(update(int)), this, SLOT(update(int)));
  connect(m_Manager, SIGNAL(aboutToUpdate()), this, SLOT(aboutToUpdate()));
}


int DownloadList::rowCount(const QModelIndex&) const
{
  return m_Manager->numTotalDownloads() + m_Manager->numPendingDownloads();
}


int DownloadList::columnCount(const QModelIndex&) const
{
  return 3;
}


QModelIndex DownloadList::index(int row, int column, const QModelIndex&) const
{
  return createIndex(row, column, row);
}


QModelIndex DownloadList::parent(const QModelIndex&) const
{
  return QModelIndex();
}


QVariant DownloadList::headerData(int section, Qt::Orientation orientation, int role) const
{
  if ((role == Qt::DisplayRole) &&
      (orientation == Qt::Horizontal)) {
    switch (section) {
      case COL_NAME   : return tr("Name");
      case COL_SIZE   : return tr("Size");
      case COL_STATUS : return tr("Status");
      default         : return QVariant();
    }
  } else {
    return QAbstractItemModel::headerData(section, orientation, role);
  }
}

QVariant DownloadList::data(const QModelIndex &index, int role) const
{
  bool pendingDownload = index.row() >= m_Manager->numTotalDownloads();
  if (role == Qt::DisplayRole) {
    if (pendingDownload) {
      std::tuple<QString, int, int> nexusids = m_Manager->getPendingDownload(index.row() - m_Manager->numTotalDownloads());
      switch (index.column()) {
        case COL_NAME:
          return tr("< game %1 mod %2 file %3 >").arg(std::get<0>(nexusids)).arg(std::get<1>(nexusids)).arg(std::get<2>(nexusids));
        case COL_SIZE:
          return tr("Unknown");
        case COL_STATUS:
          return tr("Pending");
        default:
          return QVariant();
      }
    } else {
      switch (index.column()) {
        case COL_NAME:
          return m_Manager->getFileName(index.row());
        case COL_SIZE:
          return sizeFormat(m_Manager->getFileSize(index.row()));
        case COL_STATUS:
          switch (m_Manager->getState(index.row())) {
            case DownloadManager::STATE_STARTED:
              return tr("Started");
            case DownloadManager::STATE_DOWNLOADING:
              return m_Manager->getProgress(index.row()).second;
            case DownloadManager::STATE_CANCELING:
              return tr("Canceling");
            case DownloadManager::STATE_PAUSING:
              return tr("Pausing");
            case DownloadManager::STATE_CANCELED:
              return tr("Canceled");
            case DownloadManager::STATE_PAUSED:
              return tr("Paused");
            case DownloadManager::STATE_ERROR:
              return tr("Error");
            case DownloadManager::STATE_FETCHINGMODINFO:
              return tr("Fetching Info 1");
            case DownloadManager::STATE_FETCHINGFILEINFO:
              return tr("Fetching Info 2");
            case DownloadManager::STATE_READY:
              return tr("Downloaded");
            case DownloadManager::STATE_INSTALLED:
              return tr("Installed");
            case DownloadManager::STATE_UNINSTALLED:
              return tr("Uninstalled");
            default:
              return QVariant();
          }
        default:
          return QVariant();
      }
    }
  } else if (role == Qt::ToolTipRole) {
    if (pendingDownload) {
      return tr("pending download");
    } else {
      QString text = m_Manager->getFileName(index.row()) + "\n";
      if (m_Manager->isInfoIncomplete(index.row())) {
        text += tr("Information missing, please select \"Query Info\" from the context menu to re-retrieve.");
      } else {
        const MOBase::ModRepositoryFileInfo *info = m_Manager->getFileInfo(index.row());
        return QString("%1 (ID %2) %3<br><span>%4</span>").arg(info->modName).arg(m_Manager->getModID(index.row())).arg(info->version.canonicalString()).arg(info->description);
      }
      return text;
    }
  } else {
    return QVariant();
  }
}


void DownloadList::aboutToUpdate()
{
  emit beginResetModel();
}


void DownloadList::update(int row)
{
  if (row < 0) {
    emit endResetModel();
  } else if (row < this->rowCount()) {
    emit dataChanged(this->index(row, 0, QModelIndex()), this->index(row, this->columnCount(QModelIndex())-1, QModelIndex()));
  } else {
    qCritical("invalid row %d in download list, update failed", row);
  }
}

QString DownloadList::sizeFormat(quint64 size) const
{
  qreal calc = size;
  QStringList list;
  list << "MB" << "GB" << "TB";

  QStringListIterator i(list);
  QString unit("KB");

  calc /= 1024.0;
  while (calc >= 1024.0 && i.hasNext())
  {
    unit = i.next();
    calc /= 1024.0;
  }

  return QString().setNum(calc, 'f', 2) + " " + unit;
}
