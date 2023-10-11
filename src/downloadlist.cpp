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
  connect(&m_manager, SIGNAL(update(DownloadManager::DownloadInfo*)), this,
          SLOT(update(DownloadManager::DownloadInfo*)));
  connect(&m_manager, SIGNAL(downloadAdded(DownloadManager::DownloadInfo*)), this,
          SLOT(downloadAdded(DownloadManager::DownloadInfo*)));
  connect(&m_manager, SIGNAL(downloadRemoved(QUuid)), this,
          SLOT(downloadRemoved(QUuid)));
  connect(&m_manager, SIGNAL(pendingDownloadAdded(QString)), this,
          SLOT(pendingDownloadAdded(QString)));
  connect(&m_manager, SIGNAL(pendingDownloadRemoved(QString)), this,
          SLOT(pendingDownloadRemoved(QString)));
}

QModelIndex DownloadList::index(int row, int column, const QModelIndex&) const
{
  return createIndex(row, column, row);
}

QModelIndex DownloadList::parent(const QModelIndex&) const
{
  return QModelIndex();
}

int DownloadList::rowCount(const QModelIndex& parent) const
{
  return parent.isValid() ? 0 : m_downloadListItems.size();
}

int DownloadList::columnCount(const QModelIndex&) const
{
  return COL_COUNT;
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
    case COL_MOID:
      return tr("MO ID");
    default:
      return QVariant();
    }
  } else {
    return QAbstractItemModel::headerData(section, orientation, role);
  }
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

  if (index.row() >= m_downloadListItems.size() || index.row() < 0) {
    return QVariant();
  }

  const auto& downloadListItem = m_downloadListItems.at(index.row());

  if (role == Qt::DisplayRole) {

    switch (index.column()) {
    case COL_NAME:
      return downloadListItem.name;
    case COL_STATUS:
      return downloadListItem.status;
    case COL_FILETIME:
      return downloadListItem.fileTime;
    case COL_SIZE:
      return downloadListItem.size;
    case COL_MODNAME:
      return downloadListItem.modName;
    case COL_VERSION:
      return downloadListItem.version;
    case COL_ID:
      return downloadListItem.modId;
    case COL_SOURCEGAME:
      return downloadListItem.game;
    case COL_MOID:
      return downloadListItem.moId;
    }

  } else if (role == Qt::ForegroundRole && index.column() == COL_STATUS) {
    if (downloadListItem.isPending) {
      return QColor(Qt::darkBlue);
    } else {
      if (downloadListItem.state == DownloadManager::STATE_READY) {
        return QColor(Qt::darkGreen);
      } else if (downloadListItem.state == DownloadManager::STATE_UNINSTALLED) {
        return QColor(Qt::darkYellow);
      } else if (downloadListItem.state == DownloadManager::STATE_PAUSED) {
        return QColor(Qt::darkRed);
      }
    }
  } else if (role == Qt::ToolTipRole) {
    if (downloadListItem.isPending) {
      return tr("Pending download");
    } else {
      QString text = downloadListItem.fileName + "\n";
      if (downloadListItem.showInfoIncompleteWarning) {
        text += tr("Information missing, please select \"Query Info\" from the context "
                   "menu to re-retrieve.");
      } else {
        const MOBase::ModRepositoryFileInfo* info =
            m_manager.getFileInfo(downloadListItem.moId);
        return QString("%1 (ID %2) %3<br><span>%4</span>")
            .arg(downloadListItem.modName)
            .arg(downloadListItem.modId)
            .arg(downloadListItem.version)
            .arg(info->description.chopped(4096));
      }
      return text;
    }
  } else if (role == Qt::DecorationRole && index.column() == COL_NAME) {
    if (downloadListItem.showInfoIncompleteWarning) {
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

void DownloadList::downloadAdded(DownloadManager::DownloadInfo* downloadInfo)
{
  auto* dliExists = getDownloadListItem(downloadInfo->m_moId);
  if (dliExists) {
    return;
  }

  DownloadListItem downloadListItem;
  downloadListItem.isPending = false;
  downloadListItem.fileName  = downloadInfo->GetFileName();
  downloadListItem.moId      = downloadInfo->m_moId;
  setDownloadListItem(downloadInfo, downloadListItem);

  int row = m_downloadListItems.size() == 0 ? 0 : m_downloadListItems.size();

  log::debug("Download List Size (before add): {}", m_downloadListItems.size());
  log::debug("Row: {}", row);

  emit beginInsertRows(QModelIndex(), row, row);
  m_downloadIndexCache.insert({downloadListItem.moId.toString(), row});
  m_downloadListItems.append(downloadListItem);
  emit endInsertRows();

  log::debug("Download List Size (after add): {}", m_downloadListItems.size());
}

void DownloadList::downloadUpdated(DownloadManager::DownloadInfo* downloadInfo)
{
  auto* downloadListItem = getDownloadListItem(downloadInfo->m_moId);
  if (!downloadListItem) {
    return;
  }

  setDownloadListItem(downloadInfo, *downloadListItem);
  updateData();
}

void DownloadList::downloadRemoved(QUuid moId)
{
  auto downloadListItem =
      std::find_if(m_downloadListItems.begin(), m_downloadListItems.end(),
                   [moId](const DownloadListItem download) {
                     return download.moId == moId;
                   });

  if (downloadListItem != m_downloadListItems.end()) {
    log::debug("Download List Size (before remove): {}", m_downloadListItems.size());
    auto downloadIndex = m_downloadIndexCache.at(moId.toString());
    log::debug("Download List - Removing Item: {}", downloadIndex);
    emit beginRemoveRows(QModelIndex(), downloadIndex, downloadIndex);
    m_downloadIndexCache.erase(downloadListItem->moId.toString());
    m_downloadListItems.removeAt(downloadListItem - m_downloadListItems.begin());
    emit endRemoveRows();

    log::debug("Download List Size (after remove): {}", m_downloadListItems.size());
  }
}

void DownloadList::pendingDownloadAdded(QString moId)
{
  DownloadListItem downloadListItem;
  downloadListItem.isPending    = true;
  downloadListItem.moId = QUuid::fromString(moId);
  setDownloadListItem(nullptr, downloadListItem);

  int row = m_downloadListItems.size() == 0 ? 0 : m_downloadListItems.size();

  log::debug("(pending) Download List Size (before add): {}", m_downloadListItems.size());
  log::debug("Row: {}", row);

  emit beginInsertRows(QModelIndex(), row, row);
  m_downloadIndexCache.insert({moId, row});
  m_downloadListItems.append(downloadListItem);
  emit endInsertRows();

  log::debug("(pending) Download List Size (after add): {}", m_downloadListItems.size());
}

void DownloadList::pendingDownloadRemoved(QString moId)
{
  downloadRemoved(QUuid::fromString(moId));
}

bool DownloadList::lessThanPredicate(const QModelIndex& left, const QModelIndex& right)
{
  int leftIndex   = left.row();
  int rightIndex  = right.row();
  auto& lMoId = m_downloadListItems[leftIndex].moId;
  auto& rMoId = m_downloadListItems[rightIndex].moId;

  if ((leftIndex < m_manager.numTotalDownloads()) &&
      (rightIndex < m_manager.numTotalDownloads())) {
    if (left.column() == DownloadList::COL_NAME) {
      return left.data(Qt::DisplayRole)
                 .toString()
                 .compare(right.data(Qt::DisplayRole).toString(), Qt::CaseInsensitive) <
             0;
    } else if (left.column() == DownloadList::COL_MODNAME) {
      QString leftName, rightName;

      if (!m_manager.isInfoIncomplete(lMoId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(lMoId);
        leftName                                  = info->modName;
      }

      if (!m_manager.isInfoIncomplete(rMoId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(rMoId);
        rightName                                 = info->modName;
      }

      return leftName.compare(rightName, Qt::CaseInsensitive) < 0;
    } else if (left.column() == DownloadList::COL_VERSION) {
      MOBase::VersionInfo versionLeft, versionRight;

      if (!m_manager.isInfoIncomplete(lMoId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(lMoId);
        versionLeft                               = info->version;
      }

      if (!m_manager.isInfoIncomplete(rMoId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(rMoId);
        versionRight                              = info->version;
      }

      return versionLeft < versionRight;
    } else if (left.column() == DownloadList::COL_ID) {
      int leftID = 0, rightID = 0;

      if (!m_manager.isInfoIncomplete(lMoId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(lMoId);
        leftID                                    = info->modID;
      }

      if (!m_manager.isInfoIncomplete(rMoId)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(rMoId);
        rightID                                   = info->modID;
      }

      return leftID < rightID;
    } else if (left.column() == DownloadList::COL_STATUS) {
      DownloadManager::DownloadState leftState  = m_manager.getState(lMoId);
      DownloadManager::DownloadState rightState = m_manager.getState(rMoId);
      if (leftState == rightState)
        return m_manager.getFileTime(lMoId) > m_manager.getFileTime(rMoId);
      else
        return leftState < rightState;
    } else if (left.column() == DownloadList::COL_SIZE) {
      return m_manager.getFileSize(lMoId) < m_manager.getFileSize(rMoId);
    } else if (left.column() == DownloadList::COL_FILETIME) {
      return m_manager.getFileTime(lMoId) < m_manager.getFileTime(rMoId);
    } else if (left.column() == DownloadList::COL_SOURCEGAME) {
      return m_manager.getDisplayGameName(lMoId) <
             m_manager.getDisplayGameName(rMoId);
    } else {
      return leftIndex < rightIndex;
    }
  } else {
    return leftIndex < rightIndex;
  }
}

DownloadListItem* DownloadList::getDownloadListItem(QUuid moId)
{
  auto downloadListItem = std::find_if(m_downloadListItems.begin(), m_downloadListItems.end(),
                   [moId](const DownloadListItem download) {
                     return download.moId == moId;
                               });
  if (downloadListItem != m_downloadListItems.end()) {
    return downloadListItem;
  }

  return nullptr;
}

void DownloadList::setDownloadListItem(DownloadManager::DownloadInfo* downloadInfo,
                                       DownloadListItem& downloadListItem)
{
  if (downloadListItem.isPending) {
    // In the case of a pending download, the downloadId will be the index
    std::tuple<QString, int, int, QString> nexusids =
        m_manager.getPendingDownload(downloadListItem.moId.toString());
    downloadListItem.name = tr("< game %1 mod %2 file %3 >")
                                .arg(std::get<0>(nexusids))
                                .arg(std::get<1>(nexusids))
                                .arg(std::get<2>(nexusids));
    downloadListItem.game     = std::get<0>(nexusids);
    downloadListItem.modId    = QString::number(std::get<1>(nexusids));
    downloadListItem.status   = tr("Pending");
    downloadListItem.size     = tr("Unknown");
    downloadListItem.moId     = QUuid::fromString(std::get<3>(nexusids));
  } else {
    downloadListItem.name = m_settings.interface().metaDownloads()
                                ? m_manager.getDisplayName(downloadListItem.moId)
                                : downloadInfo->m_FileName;

    downloadListItem.size     = MOBase::localizedByteSize(downloadInfo->m_TotalSize);
    downloadListItem.fileTime = m_manager.getFileTime(downloadListItem.moId);
    downloadListItem.showInfoIncompleteWarning =
        m_manager.isInfoIncomplete(downloadListItem.moId);

    if (!m_manager.isInfoIncomplete(downloadListItem.moId)) {
      const MOBase::ModRepositoryFileInfo* info = downloadInfo->m_FileInfo;
      downloadListItem.modName                  = info->modName;
      downloadListItem.version                  = info->version.canonicalString();
      downloadListItem.modId                    = QString("%1").arg(info->modID);
      downloadListItem.game =
          QString("%1").arg(m_manager.getDisplayGameName(downloadListItem.moId));
    }

    downloadListItem.state = downloadInfo->m_State;

    switch (downloadListItem.state) {
    // STATE_DOWNLOADING handled by DownloadProgressDelegate
    case DownloadManager::STATE_STARTED:
      downloadListItem.status = tr("Started");
      break;
    case DownloadManager::STATE_CANCELING:
      downloadListItem.status = tr("Canceling");
      break;
    case DownloadManager::STATE_PAUSING:
      downloadListItem.status = tr("Pausing");
      break;
    case DownloadManager::STATE_CANCELED:
      downloadListItem.status = tr("Canceled");
      break;
    case DownloadManager::STATE_PAUSED:
      downloadListItem.status = tr("Paused");
      break;
    case DownloadManager::STATE_ERROR:
      downloadListItem.status = tr("Error");
      break;
    case DownloadManager::STATE_FETCHINGMODINFO:
      downloadListItem.status = tr("Fetching Info");
      break;
    case DownloadManager::STATE_FETCHINGFILEINFO:
      downloadListItem.status = tr("Fetching Info");
      break;
    case DownloadManager::STATE_FETCHINGMODINFO_MD5:
      downloadListItem.status = tr("Fetching Info");
      break;
    case DownloadManager::STATE_READY:
      downloadListItem.status = tr("Downloaded");
      break;
    case DownloadManager::STATE_INSTALLED:
      downloadListItem.status = tr("Installed");
      break;
    case DownloadManager::STATE_UNINSTALLED:
      downloadListItem.status = tr("Uninstalled");
      break;
    default:
      downloadListItem.status = tr("Unknown");
    }
  }
}

void DownloadList::update(DownloadManager::DownloadInfo* downloadInfo)
{
  if (downloadInfo) {
    if (downloadInfo->m_Hidden) {
      downloadAdded(downloadInfo);
      return;
    }

    // This is to show files that were dragged into the DownloadListView
    if (downloadInfo->m_State == DownloadManager::DownloadState::STATE_READY &&
        downloadInfo->m_FileInfo->modID == 0) {
      downloadAdded(downloadInfo);
      return;
    }

    downloadUpdated(downloadInfo);
    updateData();
  }
}

void DownloadList::updateData()
{
  emit dataChanged(this->index(0, 0, QModelIndex()),
                   this->index(this->rowCount(QModelIndex()) - 1,
                               this->columnCount(QModelIndex()) - 1, QModelIndex()));
}
