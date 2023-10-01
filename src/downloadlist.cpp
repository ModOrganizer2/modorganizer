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
  connect(&m_manager, SIGNAL(update(const QString&)), this,
          SLOT(update(const QString&)));
  connect(&m_manager, SIGNAL(aboutToUpdate()), this, SLOT(aboutToUpdate()));
  connect(&m_manager, SIGNAL(downloadAdded(const QString&)), this,
          SLOT(downloadAdded(const QString&)));
  connect(&m_manager, SIGNAL(downloadRemoved(const QString&)), this,
          SLOT(downloadRemoved(const QString&)));
  connect(&m_manager, SIGNAL(pendingDownloadAdded(int)), this,
          SLOT(pendingDownloadAdded(int)));
  connect(&m_manager, SIGNAL(pendingDownloadFilenameUpdated(int, QString&)), this,
          SLOT(pendingDownloadFilenameUpdated(int, QString&)), Qt::DirectConnection);
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

void DownloadList::downloadAdded(const QString& fileName)
{
  Download download;
  download.isPending = false;
  getDownloadInfo(download);

  m_downloads.append(download);
}

void DownloadList::downloadUpdated(const QString& fileName)
{
  auto download = getDownload(fileName);
  if (!download)
    return;

  getDownloadInfo(*download);
}

void DownloadList::downloadRemoved(const QString& fileName)
{
  auto index = getDownloadRow(fileName);
  if (index < 0) {
    return;
  }

  m_downloads.removeAt(index);
}

void DownloadList::pendingDownloadAdded(int index)
{
  Download download;
  download.isPending    = true;
  download.pendingIndex = index;
  getDownloadInfo(download);

  m_downloads.append(download);
}

void DownloadList::pendingDownloadRemoved(int index)
{
  auto pendingRow = getPendingRow(index);
  if (pendingRow < 0) {
    return;
  }

  m_downloads.removeAt(pendingRow);
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

Download* DownloadList::getDownloadByPendingIndex(int index)
{
  auto download = std::find_if(m_downloads.begin(), m_downloads.end(),
                               [index](const Download download) {
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
                               [index](const Download download) {
                                 return download.pendingIndex == index;
                               });
  if (download != m_downloads.end()) {
    return download - m_downloads.begin();
  }

  log::error("pending download row at index {} not found", index);
  return -1;
}

int DownloadList::getDownloadRow(const QString& fileName)
{
  auto download = std::find_if(m_downloads.begin(), m_downloads.end(),
                               [fileName](const Download download) {
                                 return download.fileName == fileName;
                               });

  if (download != m_downloads.end()) {
    return download - m_downloads.begin();
  }

  return -1;
}

void DownloadList::getDownloadInfo(Download& download)
{
  if (download.isPending) {
    // In the case of a pending download, the downloadId will be the index
    std::tuple<QString, int, int> nexusids =
        m_manager.getPendingDownload(download.pendingIndex);
    download.name = tr("< game %1 mod %2 file %3 >")
                        .arg(std::get<0>(nexusids))
                        .arg(std::get<1>(nexusids))
                        .arg(std::get<2>(nexusids));
    download.game   = std::get<0>(nexusids);
    download.modId  = QString::number(std::get<1>(nexusids));
    download.status = tr("Pending");
    download.size   = tr("Unknown");
  } else {
    download.name = m_settings.interface().metaDownloads()
                        ? m_manager.getDisplayName(download.fileName)
                        : download.fileName;

    download.size = MOBase::localizedByteSize(m_manager.getFileSize(download.fileName));
    download.fileTime                  = m_manager.getFileTime(download.fileName);
    download.showInfoIncompleteWarning = m_manager.isInfoIncomplete(download.fileName);

    if (!m_manager.isInfoIncomplete(download.fileName)) {
      const MOBase::ModRepositoryFileInfo* info =
          m_manager.getFileInfo(download.fileName);
      download.modName = info->modName;
      download.version = info->version.canonicalString();
      download.modId   = QString::number(m_manager.getModID(download.fileName));
      download.game =
          QString("%1").arg(m_manager.getDisplayGameName(download.fileName));
    }

    switch (m_manager.getState(download.fileName)) {
    // STATE_DOWNLOADING handled by DownloadProgressDelegate
    case DownloadManager::STATE_STARTED:
      download.status = tr("Started");
      break;
    case DownloadManager::STATE_CANCELING:
      download.status = tr("Canceling");
      break;
    case DownloadManager::STATE_PAUSING:
      download.status = tr("Pausing");
      break;
    case DownloadManager::STATE_CANCELED:
      download.status = tr("Canceled");
      break;
    case DownloadManager::STATE_PAUSED:
      download.status = tr("Paused");
      break;
    case DownloadManager::STATE_ERROR:
      download.status = tr("Error");
      break;
    case DownloadManager::STATE_FETCHINGMODINFO:
      download.status = tr("Fetching Info");
      break;
    case DownloadManager::STATE_FETCHINGFILEINFO:
      download.status = tr("Fetching Info");
      break;
    case DownloadManager::STATE_FETCHINGMODINFO_MD5:
      download.status = tr("Fetching Info");
      break;
    case DownloadManager::STATE_READY:
      download.status = tr("Downloaded");
      break;
    case DownloadManager::STATE_INSTALLED:
      download.status = tr("Installed");
      break;
    case DownloadManager::STATE_UNINSTALLED:
      download.status = tr("Uninstalled");
      break;
    default:
      download.status = tr("Unknown");
    }
  }
}

void DownloadList::update(const QString& fileName)
{
  if (fileName == "__endResetModel__") {
    emit endResetModel();
    return;
  }

  auto row = getDownloadRow(fileName);

  if (row < 0) {
    log::error("invalid row {} in download list, update failed", row);
    return;
  }

  downloadUpdated(fileName);
  updateData(row);
}

void DownloadList::aboutToUpdate()
{
  emit beginResetModel();
}

void DownloadList::updateData(int row)
{
  emit dataChanged(
      this->index(row, 0, QModelIndex()),
      this->index(row, this->columnCount(QModelIndex()) - 1, QModelIndex()));
}
