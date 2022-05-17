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
}

int DownloadList::rowCount(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    // root item
    return m_manager.numTotalDownloads() + m_manager.numPendingDownloads();
  } else {
    return 0;
  }
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
  bool pendingDownload = index.row() >= m_manager.numTotalDownloads();
  if (role == Qt::DisplayRole) {
    if (pendingDownload) {
      std::tuple<QString, int, int> nexusids =
          m_manager.getPendingDownload(index.row() - m_manager.numTotalDownloads());
      switch (index.column()) {
      case COL_NAME:
        return tr("< game %1 mod %2 file %3 >")
            .arg(std::get<0>(nexusids))
            .arg(std::get<1>(nexusids))
            .arg(std::get<2>(nexusids));
      case COL_SIZE:
        return tr("Unknown");
      case COL_STATUS:
        return tr("Pending");
      }
    } else {
      switch (index.column()) {
      case COL_NAME:
        return m_settings.interface().metaDownloads()
                   ? m_manager.getDisplayName(index.row())
                   : m_manager.getFileName(index.row());
      case COL_MODNAME: {
        if (m_manager.isInfoIncomplete(index.row())) {
          return {};
        } else {
          const MOBase::ModRepositoryFileInfo* info =
              m_manager.getFileInfo(index.row());
          return info->modName;
        }
      }
      case COL_VERSION: {
        if (m_manager.isInfoIncomplete(index.row())) {
          return {};
        } else {
          const MOBase::ModRepositoryFileInfo* info =
              m_manager.getFileInfo(index.row());
          return info->version.canonicalString();
        }
      }
      case COL_ID: {
        if (m_manager.isInfoIncomplete(index.row())) {
          return {};
        } else {
          return QString("%1").arg(m_manager.getModID(index.row()));
        }
      }
      case COL_SOURCEGAME: {
        if (m_manager.isInfoIncomplete(index.row())) {
          return {};
        } else {
          return QString("%1").arg(m_manager.getDisplayGameName(index.row()));
        }
      }
      case COL_SIZE:
        return MOBase::localizedByteSize(m_manager.getFileSize(index.row()));
      case COL_FILETIME:
        return m_manager.getFileTime(index.row());
      case COL_STATUS:
        switch (m_manager.getState(index.row())) {
        // STATE_DOWNLOADING handled by DownloadProgressDelegate
        case DownloadManager::STATE_STARTED:
          return tr("Started");
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
          return tr("Fetching Info");
        case DownloadManager::STATE_FETCHINGFILEINFO:
          return tr("Fetching Info");
        case DownloadManager::STATE_FETCHINGMODINFO_MD5:
          return tr("Fetching Info");
        case DownloadManager::STATE_READY:
          return tr("Downloaded");
        case DownloadManager::STATE_INSTALLED:
          return tr("Installed");
        case DownloadManager::STATE_UNINSTALLED:
          return tr("Uninstalled");
        }
      }
    }
  } else if (role == Qt::ForegroundRole && index.column() == COL_STATUS) {
    if (pendingDownload) {
      return QColor(Qt::darkBlue);
    } else {
      DownloadManager::DownloadState state = m_manager.getState(index.row());
      if (state == DownloadManager::STATE_READY)
        return QColor(Qt::darkGreen);
      else if (state == DownloadManager::STATE_UNINSTALLED)
        return QColor(Qt::darkYellow);
      else if (state == DownloadManager::STATE_PAUSED)
        return QColor(Qt::darkRed);
    }
  } else if (role == Qt::ToolTipRole) {
    if (pendingDownload) {
      return tr("Pending download");
    } else {
      QString text = m_manager.getFileName(index.row()) + "\n";
      if (m_manager.isInfoIncomplete(index.row())) {
        text += tr("Information missing, please select \"Query Info\" from the context "
                   "menu to re-retrieve.");
      } else {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(index.row());
        return QString("%1 (ID %2) %3<br><span>%4</span>")
            .arg(info->modName)
            .arg(m_manager.getModID(index.row()))
            .arg(info->version.canonicalString())
            .arg(info->description.chopped(4096));
      }
      return text;
    }
  } else if (role == Qt::DecorationRole && index.column() == COL_NAME) {
    if (!pendingDownload &&
        m_manager.getState(index.row()) >= DownloadManager::STATE_READY &&
        m_manager.isInfoIncomplete(index.row()))
      return QIcon(":/MO/gui/warning_16");
  } else if (role == Qt::TextAlignmentRole) {
    if (index.column() == COL_SIZE)
      return QVariant(Qt::AlignVCenter | Qt::AlignRight);
    else
      return QVariant(Qt::AlignVCenter | Qt::AlignLeft);
  }
  return QVariant();
}

void DownloadList::aboutToUpdate()
{
  emit beginResetModel();
}

void DownloadList::update(int row)
{
  if (row < 0)
    emit endResetModel();
  else if (row < this->rowCount())
    emit dataChanged(
        this->index(row, 0, QModelIndex()),
        this->index(row, this->columnCount(QModelIndex()) - 1, QModelIndex()));
  else
    log::error("invalid row {} in download list, update failed", row);
}

bool DownloadList::lessThanPredicate(const QModelIndex& left, const QModelIndex& right)
{
  int leftIndex  = left.row();
  int rightIndex = right.row();
  if ((leftIndex < m_manager.numTotalDownloads()) &&
      (rightIndex < m_manager.numTotalDownloads())) {
    if (left.column() == DownloadList::COL_NAME) {
      return left.data(Qt::DisplayRole)
                 .toString()
                 .compare(right.data(Qt::DisplayRole).toString(), Qt::CaseInsensitive) <
             0;
    } else if (left.column() == DownloadList::COL_MODNAME) {
      QString leftName, rightName;

      if (!m_manager.isInfoIncomplete(left.row())) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(left.row());
        leftName                                  = info->modName;
      }

      if (!m_manager.isInfoIncomplete(right.row())) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(right.row());
        rightName                                 = info->modName;
      }

      return leftName.compare(rightName, Qt::CaseInsensitive) < 0;
    } else if (left.column() == DownloadList::COL_VERSION) {
      MOBase::VersionInfo versionLeft, versionRight;

      if (!m_manager.isInfoIncomplete(left.row())) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(left.row());
        versionLeft                               = info->version;
      }

      if (!m_manager.isInfoIncomplete(right.row())) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(right.row());
        versionRight                              = info->version;
      }

      return versionLeft < versionRight;
    } else if (left.column() == DownloadList::COL_ID) {
      int leftID = 0, rightID = 0;

      if (!m_manager.isInfoIncomplete(left.row())) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(left.row());
        leftID                                    = info->modID;
      }

      if (!m_manager.isInfoIncomplete(right.row())) {
        const MOBase::ModRepositoryFileInfo* info = m_manager.getFileInfo(right.row());
        rightID                                   = info->modID;
      }

      return leftID < rightID;
    } else if (left.column() == DownloadList::COL_STATUS) {
      DownloadManager::DownloadState leftState  = m_manager.getState(left.row());
      DownloadManager::DownloadState rightState = m_manager.getState(right.row());
      if (leftState == rightState)
        return m_manager.getFileTime(left.row()) > m_manager.getFileTime(right.row());
      else
        return leftState < rightState;
    } else if (left.column() == DownloadList::COL_SIZE) {
      return m_manager.getFileSize(left.row()) < m_manager.getFileSize(right.row());
    } else if (left.column() == DownloadList::COL_FILETIME) {
      return m_manager.getFileTime(left.row()) < m_manager.getFileTime(right.row());
    } else if (left.column() == DownloadList::COL_SOURCEGAME) {
      return m_manager.getDisplayGameName(left.row()) <
             m_manager.getDisplayGameName(right.row());
    } else {
      return leftIndex < rightIndex;
    }
  } else {
    return leftIndex < rightIndex;
  }
}
