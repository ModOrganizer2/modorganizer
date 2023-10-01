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
  connect(&m_manager, SIGNAL(update(QString)), this, SLOT(update(QString)));
  connect(&m_manager, SIGNAL(aboutToUpdate()), this, SLOT(aboutToUpdate()));
  connect(&m_manager, SIGNAL(downloadAdded(QString&)), this,
          SLOT(downloadAdded(QString&)));
  connect(&m_manager, SIGNAL(downloadRemoved(QString)), this,
          SLOT(downloadRemoved(QString)));
  connect(&m_manager, SIGNAL(pendingDownloadAdded(int)), this,
          SLOT(pendingDownloadAdded(int)));
  connect(&m_manager, SIGNAL(pendingDownloadFilenameUpdated(int, QString)), this,
          SLOT(pendingDownloadFilenameUpdated(int, QString)));
  connect(&m_manager, SIGNAL(pendingDownloadRemoved(int)), this,
          SLOT(pendingDownloadRemoved(int)));
}

int DownloadList::rowCount(const QModelIndex& parent) const
{
  return parent.isValid() ? 0 : m_downloads.size();
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
    case COL_FILENAME:
      return tr("File name");
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

  if (index.row() >= m_downloads.size() || index.row() < 0) {
    return QVariant();
  }

  const auto& download = m_downloads.at(index.row());

  if (role == Qt::DisplayRole) {

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
    case COL_FILENAME:
      return download.fileName;
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

void DownloadList::downloadAdded(QString& fileName)
{
  DownloadListItem download;
  download.isPending = false;
  download.fileName  = fileName;
  getDownloadInfo(download);

  emit beginResetModel();
  m_downloads.append(download);
  emit endResetModel();
}

void DownloadList::downloadUpdated(QString fileName)
{
  auto download = getDownloadListItem(fileName);
  if (!download)
    return;

  getDownloadInfo(*download);
  updateData();
}

void DownloadList::downloadRemoved(QString fileName)
{
  auto downloadListItem = std::find_if(m_downloads.begin(), m_downloads.end(),
                               [fileName](const DownloadListItem download) {
                                 return download.fileName == fileName;
                               });

  if (downloadListItem != m_downloads.end()) {
    emit beginResetModel();
    m_downloads.removeAt(downloadListItem - m_downloads.begin());
    emit endResetModel();
  }
}

void DownloadList::pendingDownloadAdded(int index)
{
  DownloadListItem download;
  download.isPending    = true;
  download.pendingIndex = index;
  getDownloadInfo(download);

  emit beginResetModel();
  m_downloads.append(download);
  emit endResetModel();
}

void DownloadList::pendingDownloadRemoved(int index)
{
  auto pendingRow = getPendingRow(index);
  if (pendingRow < 0) {
    return;
  }

  emit beginResetModel();
  m_downloads.removeAt(pendingRow);
  emit endResetModel();
}

void DownloadList::pendingDownloadFilenameUpdated(int index, QString fileName)
{
  if (index < 0 || fileName.isEmpty()) {
    return;
  }

  auto* download = getDownloadByPendingIndex(index);
  if (download->isPending) {
    download->fileName = fileName;
  }
}

void DownloadList::pendingDownloadFilenameUpdated(int index, QString& fileName)
{
  if (index < 0 || fileName.isEmpty()) {
    return;
  }

  auto* download = getDownloadByPendingIndex(index);
  if (download->isPending) {
    download->fileName = fileName;
  }
}

bool DownloadList::lessThanPredicate(const QModelIndex& left, const QModelIndex& right)
{
  int leftIndex   = left.row();
  int rightIndex  = right.row();
  auto& lFilename = m_downloads[leftIndex].fileName;
  auto& rFilename = m_downloads[rightIndex].fileName;

  if ((leftIndex < m_manager.numTotalDownloads()) &&
      (rightIndex < m_manager.numTotalDownloads())) {
    if (left.column() == DownloadList::COL_NAME) {
      return left.data(Qt::DisplayRole)
                 .toString()
                 .compare(right.data(Qt::DisplayRole).toString(), Qt::CaseInsensitive) <
             0;
    } else if (left.column() == DownloadList::COL_MODNAME) {
      QString leftName, rightName;

      if (!m_manager.isInfoIncomplete(lFilename)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(lFilename);
        leftName                                  = info->modName;
      }

      if (!m_manager.isInfoIncomplete(rFilename)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(rFilename);
        rightName                                 = info->modName;
      }

      return leftName.compare(rightName, Qt::CaseInsensitive) < 0;
    } else if (left.column() == DownloadList::COL_VERSION) {
      MOBase::VersionInfo versionLeft, versionRight;

      if (!m_manager.isInfoIncomplete(lFilename)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(lFilename);
        versionLeft                               = info->version;
      }

      if (!m_manager.isInfoIncomplete(rFilename)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(rFilename);
        versionRight                              = info->version;
      }

      return versionLeft < versionRight;
    } else if (left.column() == DownloadList::COL_ID) {
      int leftID = 0, rightID = 0;

      if (!m_manager.isInfoIncomplete(lFilename)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(lFilename);
        leftID                                    = info->modID;
      }

      if (!m_manager.isInfoIncomplete(rFilename)) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(rFilename);
        rightID                                   = info->modID;
      }

      return leftID < rightID;
    } else if (left.column() == DownloadList::COL_STATUS) {
      DownloadManager::DownloadState leftState  = m_manager.getState(lFilename);
      DownloadManager::DownloadState rightState = m_manager.getState(rFilename);
      if (leftState == rightState)
        return m_manager.getFileTime(lFilename) > m_manager.getFileTime(rFilename);
      else
        return leftState < rightState;
    } else if (left.column() == DownloadList::COL_SIZE) {
      return m_manager.getFileSize(lFilename) < m_manager.getFileSize(rFilename);
    } else if (left.column() == DownloadList::COL_FILETIME) {
      return m_manager.getFileTime(lFilename) < m_manager.getFileTime(rFilename);
    } else if (left.column() == DownloadList::COL_SOURCEGAME) {
      return m_manager.getDisplayGameName(lFilename) <
             m_manager.getDisplayGameName(rFilename);
    } else {
      return leftIndex < rightIndex;
    }
  } else {
    return leftIndex < rightIndex;
  }
}

Download* DownloadList::getDownload(const QString& fileName)
{
  auto download = std::find_if(m_downloads.begin(), m_downloads.end(),
                               [fileName](const Download download) {
                                 return download.fileName == fileName;
                               });
  if (download != m_downloads.end()) {
    return download;
  }

  return nullptr;
}

DownloadListItem* DownloadList::getDownloadListItem(QString fileName)
{
  auto download = std::find_if(m_downloads.begin(), m_downloads.end(),
                               [fileName](const DownloadListItem download) {
                                 return download.fileName == fileName;
                               });
  if (download != m_downloads.end()) {
    return download;
  }

  return nullptr;
}

DownloadListItem* DownloadList::getDownloadByPendingIndex(int index)
{
  auto download = std::find_if(m_downloads.begin(), m_downloads.end(),
                               [index](const DownloadListItem download) {
                                 return download.pendingIndex == index;
                               });
  if (download != m_downloads.end()) {
    return download;
  }

  return nullptr;
}

int DownloadList::getPendingRow(int index)
{
  auto download = std::find_if(m_downloads.begin(), m_downloads.end(),
                               [index](const DownloadListItem download) {
                                 return download.pendingIndex == index;
                               });
  if (download != m_downloads.end()) {
    return download - m_downloads.begin();
  }

  log::error("pending download row at index {} not found", index);
  return -1;
}

void DownloadList::getDownloadInfo(DownloadListItem& downloadListItem)
{
  if (downloadListItem.isPending) {
    // In the case of a pending download, the downloadId will be the index
    std::tuple<QString, int, int> nexusids =
        m_manager.getPendingDownload(downloadListItem.pendingIndex);
    downloadListItem.name = tr("< game %1 mod %2 file %3 >")
                                .arg(std::get<0>(nexusids))
                                .arg(std::get<1>(nexusids))
                                .arg(std::get<2>(nexusids));
    downloadListItem.game   = std::get<0>(nexusids);
    downloadListItem.modId  = QString::number(std::get<1>(nexusids));
    downloadListItem.status = tr("Pending");
    downloadListItem.size   = tr("Unknown");
    downloadListItem.fileName = tr("< game %1 mod %2 file %3 >")
                                    .arg(std::get<0>(nexusids))
                                    .arg(std::get<1>(nexusids))
                                    .arg(std::get<2>(nexusids));
  } else {
    downloadListItem.name = m_settings.interface().metaDownloads()
                                ? m_manager.getDisplayName(downloadListItem.fileName)
                                : downloadListItem.fileName;

    downloadListItem.size =
        MOBase::localizedByteSize(m_manager.getFileSize(downloadListItem.fileName));
    downloadListItem.fileTime = m_manager.getFileTime(downloadListItem.fileName);
    downloadListItem.showInfoIncompleteWarning =
        m_manager.isInfoIncomplete(downloadListItem.fileName);

    if (!m_manager.isInfoIncomplete(downloadListItem.fileName)) {
      const MOBase::ModRepositoryFileInfo* info =
          m_manager.getFileInfo(downloadListItem.fileName);
      downloadListItem.modName = info->modName;
      downloadListItem.version = info->version.canonicalString();
      downloadListItem.modId =
          QString::number(m_manager.getModID(downloadListItem.fileName));
      downloadListItem.game =
          QString("%1").arg(m_manager.getDisplayGameName(downloadListItem.fileName));
    }

    switch (m_manager.getState(downloadListItem.fileName)) {
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

void DownloadList::update(QString fileName)
{
  if (fileName == "__endResetModel__") {
    emit endResetModel();
    return;
  }

  downloadUpdated(fileName);
  updateData();
}

void DownloadList::aboutToUpdate()
{
  emit beginResetModel();
}

void DownloadList::updateData()
{
  emit dataChanged(this->index(0, 0, QModelIndex()),
                   this->index(this->rowCount(QModelIndex()) - 1,
                               this->columnCount(QModelIndex()) - 1, QModelIndex()));
}
