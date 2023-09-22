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
#include "modlistdropinfo.h"
#include "organizercore.h"
#include "settings.h"
#include <QColor>
#include <QEvent>
#include <QIcon>
#include <QSortFilterProxyModel>
#include <log.h>
#include <utility.h>

using namespace MOBase;

DownloadList::DownloadList(OrganizerCore& core, QObject* parent)
    : QAbstractTableModel(parent), m_manager(*core.downloadManager()),
      m_settings(core.settings())
{
  connect(&m_manager, SIGNAL(update(int)), this, SLOT(update(int)));
  connect(&m_manager, SIGNAL(aboutToUpdate()), this, SLOT(aboutToUpdate()));
  connect(&m_manager, SIGNAL(downloadAdded(int)), this, SLOT(downloadAdded(int)));
  connect(&m_manager, SIGNAL(downloadUpdated(int)), this, SLOT(downloadUpdated(int)));
  connect(&m_manager, SIGNAL(downloadRemoved(int)), this, SLOT(downloadRemoved(int)));
  connect(&m_manager, SIGNAL(pendingDownloadAdded(int)), this, SLOT(pendingDownloadAdded(int)));
  connect(&m_manager, SIGNAL(pendingDownloadRemoved(int)), this, SLOT(pendingDownloadRemoved(int)));
}

int DownloadList::rowCount(const QModelIndex& parent) const
{
  return parent.isValid() ? 0 : m_downloads.size();
}

int DownloadList::columnCount(const QModelIndex&) const
{
  return COL_COUNT;
}

QModelIndex DownloadList::index(int row, int column, const QModelIndex&) const
{
  return createIndex(row, column, row);
}

QModelIndex DownloadList::parent(const QModelIndex&) const
{
  return QModelIndex();
}

QVariant DownloadList::headerData(int section, Qt::Orientation orientation,
                                  int role) const
{
  if ((role == Qt::DisplayRole) && (orientation == Qt::Horizontal)) {
    switch (section) {
    case COL_NAME:
      return tr("Name");
    case COL_MODNAME:
      return tr("Mod name");
    case COL_VERSION:
      return tr("Version");
    case COL_ID:
      return tr("Nexus ID");
    case COL_SIZE:
      return tr("Size");
    case COL_STATUS:
      return tr("Status");
    case COL_FILETIME:
      return tr("Filetime");
    case COL_SOURCEGAME:
      return tr("Source Game");
    default:
      return QVariant();
    }
  } else {
    return QAbstractItemModel::headerData(section, orientation, role);
  }
}

const Download& DownloadList::getDownloadByRow(int row)
{
  auto& download = m_downloads[row]; 
  return download;
}

Qt::ItemFlags DownloadList::flags(const QModelIndex& idx) const
{
  return QAbstractTableModel::flags(idx) | Qt::ItemIsDragEnabled;
}

QMimeData* DownloadList::mimeData(const QModelIndexList& indexes) const
{
  QMimeData* result = QAbstractItemModel::mimeData(indexes);
  result->setData("text/plain", ModListDropInfo::DownloadText);
  return result;
}

QVariant DownloadList::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return QVariant();
  }

  if (index.row() >= m_downloads.size() || index.row() < 0) {
    return QVariant();
  }
  
  const auto& download = m_downloads.at(index.row());

  if (role == Qt::DisplayRole) {

    // TODO: Figure out how to handle translations
    switch (index.column()) {
    case COL_NAME:
      return download.name;
    case COL_STATUS:
      return download.status;
    case COL_FILETIME:
      return download.fileTime;
    case COL_SIZE:
      return download.size;
    case COL_MODNAME:
      return download.modName;
    case COL_VERSION:
      return download.version;
    case COL_ID:
      return download.modId;
    case COL_SOURCEGAME:
      return download.game;
    }
  } else if (role == Qt::ForegroundRole && index.column() == COL_STATUS) {
    if (download.isPending) {
      return QColor(Qt::darkBlue);
    } else {
      if (download.status == "Downloaded") {
        return QColor(Qt::darkGreen);
      } else if (download.status == "Paused") {
        return QColor(Qt::darkYellow);
      } else if (download.status == "Uninstalled") {
        return QColor(Qt::darkRed);
      }
    }
  } else if (role == Qt::ToolTipRole) {
    return download.tooltip;
  } else if (role == Qt::DecorationRole && index.column() == COL_NAME) {
    if (download.showInfoIncompleteWarning) {
      return QIcon(":/MO/gui/warning_16");
    }
  } else if (role == Qt::TextAlignmentRole) {
    if (index.column() == COL_SIZE)
      return QVariant(Qt::AlignVCenter | Qt::AlignRight);
    else
      return QVariant(Qt::AlignVCenter | Qt::AlignLeft);
  }

  return QVariant();
}

void DownloadList::downloadAdded(int downloadId) {
  auto download = new Download;
  download->isPending = false;
  download->downloadId = downloadId;
  buildDownload(download);

  m_downloads.push_back(*download);
}

void DownloadList::downloadUpdated(int downloadId) {
  auto download = getDownload(downloadId);
  if (!download)
    return;

  buildDownload(download);
  auto index = getDownloadIndex(downloadId);
  if (index < 0) {
    return;
  }

  m_downloads[index].downloadId = download->downloadId;
  m_downloads[index].fileTime   = download->fileTime;
  m_downloads[index].game       = download->game;
  m_downloads[index].isPending  = download->isPending;
  m_downloads[index].modId      = download->modId;
  m_downloads[index].modName    = download->modName;
  m_downloads[index].name       = download->name;
  m_downloads[index].size       = download->size;
  m_downloads[index].status     = download->status;
  m_downloads[index].tooltip    = download->tooltip;
  m_downloads[index].version    = download->version;
  m_downloads[index].showInfoIncompleteWarning = download->showInfoIncompleteWarning;
}

void DownloadList::downloadRemoved(int downloadId) {
  auto index = getDownloadIndex(downloadId);
  if (index < 0) {
    return;
  }

  m_downloads.removeAt(index);
}

void DownloadList::pendingDownloadAdded(int index) {
  auto download       = new Download;
  download->isPending = true;
  download->downloadId = index;
  buildDownload(download);

  m_downloads.push_back(*download);
  delete download;
}

void DownloadList::pendingDownloadRemoved(int index) {
  auto downloadIndex = getDownloadIndex(index);
  if (downloadIndex < 0) {
    return;
  }

  m_downloads.removeAt(downloadIndex);
}

bool DownloadList::lessThanPredicate(const QModelIndex& left, const QModelIndex& right)
{
  int leftIndex  = left.row();
  int rightIndex = right.row();
  int lDownloadId = m_downloads[leftIndex].downloadId;
  int rDownloadId = m_downloads[rightIndex].downloadId;

  if ((leftIndex < m_manager.numTotalDownloads()) &&
      (rightIndex < m_manager.numTotalDownloads())) {
    if (left.column() == DownloadList::COL_NAME) {
      return left.data(Qt::DisplayRole)
                 .toString()
                 .compare(right.data(Qt::DisplayRole).toString(), Qt::CaseInsensitive) <
             0;
    } else if (left.column() == DownloadList::COL_MODNAME) {
      QString leftName, rightName;

      if (!m_manager.isInfoIncomplete(lDownloadId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(lDownloadId);
        leftName                                  = info->modName;
      }

      if (!m_manager.isInfoIncomplete(rDownloadId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(rDownloadId);
        rightName                                 = info->modName;
      }

      return leftName.compare(rightName, Qt::CaseInsensitive) < 0;
    } else if (left.column() == DownloadList::COL_VERSION) {
      MOBase::VersionInfo versionLeft, versionRight;

      if (!m_manager.isInfoIncomplete(lDownloadId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(lDownloadId);
        versionLeft                               = info->version;
      }

      if (!m_manager.isInfoIncomplete(rDownloadId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(rDownloadId);
        versionRight                              = info->version;
      }

      return versionLeft < versionRight;
    } else if (left.column() == DownloadList::COL_ID) {
      int leftID = 0, rightID = 0;

      if (!m_manager.isInfoIncomplete(lDownloadId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(lDownloadId);
        leftID                                    = info->modID;
      }

      if (!m_manager.isInfoIncomplete(rDownloadId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(rDownloadId);
        rightID                                   = info->modID;
      }

      return leftID < rightID;
    } else if (left.column() == DownloadList::COL_STATUS) {
      DownloadManager::DownloadState leftState  = m_manager.getState(lDownloadId);
      DownloadManager::DownloadState rightState = m_manager.getState(rDownloadId);
      if (leftState == rightState)
        return m_manager.getFileTime(lDownloadId) >
               m_manager.getFileTime(rDownloadId);
      else
        return leftState < rightState;
    } else if (left.column() == DownloadList::COL_SIZE) {
      return m_manager.getFileSize(lDownloadId) < m_manager.getFileSize(rDownloadId);
    } else if (left.column() == DownloadList::COL_FILETIME) {
      return m_manager.getFileTime(lDownloadId) <
             m_manager.getFileTime(rDownloadId);
    } else if (left.column() == DownloadList::COL_SOURCEGAME) {
      return m_manager.getDisplayGameName(lDownloadId) <
             m_manager.getDisplayGameName(rDownloadId);
    } else {
      return leftIndex < rightIndex;
    }
  } else {
    return leftIndex < rightIndex;
  }
}

Download* DownloadList::getDownload(int downloadId) {
  auto download = std::find_if(m_downloads.begin(), m_downloads.end(),
                      [downloadId](const Download download) {
                        return download.downloadId == downloadId;
                      });
  if (download != m_downloads.end()) {
    return download;
  }

  return nullptr;
}

int DownloadList::getDownloadIndex(int downloadId)
{
  auto download = std::find_if(m_downloads.begin(), m_downloads.end(),
                               [downloadId](const Download download) {
                                 return download.downloadId == downloadId;
                               });
  
  if (download != m_downloads.end()) {
    return download - m_downloads.begin();
  }

  return -1;
}

void DownloadList::buildDownload(Download* download) {
  if (download->isPending) {
    // In the case of a pending download, the downloadId will be the index
    std::tuple<QString, int, int> nexusids = m_manager.getPendingDownload(download->downloadId);
    download->name = tr("< game %1 mod %2 file %3 >")
        .arg(std::get<0>(nexusids))
        .arg(std::get<1>(nexusids))
        .arg(std::get<2>(nexusids));
    download->status = tr("Pending");
    download->size   = tr("Unknown");
  } else {
    download->name = m_settings.interface().metaDownloads()
                         ? m_manager.getDisplayName(download->downloadId)
                         : m_manager.getFileName(download->downloadId);

    download->size =
        MOBase::localizedByteSize(m_manager.getFileSize(download->downloadId));
    download->fileTime = m_manager.getFileTime(download->downloadId);

    if (!m_manager.isInfoIncomplete(download->downloadId)) {
      const MOBase::ModRepositoryFileInfo* info =
          m_manager.getFileInfo(download->downloadId);
      download->modName = info->modName;
      download->version = info->version.canonicalString();
      download->modId   = QString("%1").arg(m_manager.getModID(download->downloadId));
      download->game =
          QString("%1").arg(m_manager.getDisplayGameName(download->downloadId));
    }

    switch (m_manager.getState(download->downloadId)) {
    // STATE_DOWNLOADING handled by DownloadProgressDelegate
    case DownloadManager::STATE_STARTED:
      download->status = tr("Started");
      break;
    case DownloadManager::STATE_CANCELING:
      download->status = tr("Canceling");
      break;
    case DownloadManager::STATE_PAUSING:
      download->status = tr("Pausing");
      break;
    case DownloadManager::STATE_CANCELED:
      download->status = tr("Canceled");
      break;
    case DownloadManager::STATE_PAUSED:
      download->status = tr("Paused");
      break;
    case DownloadManager::STATE_ERROR:
      download->status = tr("Error");
      break;
    case DownloadManager::STATE_FETCHINGMODINFO:
      download->status = tr("Fetching Info");
      break;
    case DownloadManager::STATE_FETCHINGFILEINFO:
      download->status = tr("Fetching Info");
      break;
    case DownloadManager::STATE_FETCHINGMODINFO_MD5:
      download->status = tr("Fetching Info");
      break;
    case DownloadManager::STATE_READY:
      download->status = tr("Downloaded");
      break;
    case DownloadManager::STATE_INSTALLED:
      download->status = tr("Installed");
      break;
    case DownloadManager::STATE_UNINSTALLED:
      download->status = tr("Uninstalled");
      break;
    default:
      download->status = tr("Unknown");
    }
  }
}

void DownloadList::update(int downloadId)
{
  if (downloadId < 0) {
    emit endResetModel();
    return;
  }

  auto row = getDownloadIndex(downloadId);

  if (row < 0) {
    log::error("invalid row {} in download list, update failed", row);
    return;
  }

  auto download = getDownload(downloadId);
  buildDownload(download);

  emit dataChanged(
    this->index(row, 0, QModelIndex()),
    this->index(row, this->columnCount(QModelIndex()) - 1, QModelIndex()));  
}

void DownloadList::aboutToUpdate()
{
  emit beginResetModel();
}
