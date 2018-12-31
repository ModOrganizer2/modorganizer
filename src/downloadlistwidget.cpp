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
#include "downloadlistwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QApplication>

void DownloadProgressDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QModelIndex sourceIndex = m_SortProxy->mapToSource(index);
  if (sourceIndex.column() == DownloadList::COL_STATUS && sourceIndex.row() < m_Manager->numTotalDownloads()
      && m_Manager->getState(sourceIndex.row()) == DownloadManager::STATE_DOWNLOADING) {
    bool pendingDownload = sourceIndex.row() >= m_Manager->numTotalDownloads();
    QProgressBar progressBarOption;
    progressBarOption.setProperty("compact", option.widget->property("compact"));
    progressBarOption.setMinimum(0);
    progressBarOption.setMaximum(100);
    progressBarOption.setAlignment(Qt::AlignCenter);
    progressBarOption.resize(option.rect.width(), option.rect.height());
    progressBarOption.setValue(m_Manager->getProgress(sourceIndex.row()).first);
    progressBarOption.setFormat(m_Manager->getProgress(sourceIndex.row()).second);

    // paint the background with default delegate first to preserve table cell styling
    QStyledItemDelegate::paint(painter, option, index);

    painter->save();
    painter->translate(option.rect.topLeft());
    progressBarOption.render(painter);
    painter->restore();
  } else {
    QStyledItemDelegate::paint(painter, option, index);
  }
}

DownloadListWidget::DownloadListWidget(QWidget *parent)
  : QTreeView(parent)
{
  connect(this, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(onDoubleClick(QModelIndex)));
  connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onCustomContextMenu(QPoint)));
}

DownloadListWidget::~DownloadListWidget()
{
}

void DownloadListWidget::setManager(DownloadManager *manager)
{
  m_Manager = manager;
}

void DownloadListWidget::onDoubleClick(const QModelIndex &index)
{
  QModelIndex sourceIndex = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(index);
  if (m_Manager->getState(sourceIndex.row()) >= DownloadManager::STATE_READY) {
    emit installDownload(sourceIndex.row());
  } else if ((m_Manager->getState(sourceIndex.row()) >= DownloadManager::STATE_PAUSED) || (m_Manager->getState(sourceIndex.row()) == DownloadManager::STATE_PAUSING)) {
    emit resumeDownload(sourceIndex.row());
  }
}

void DownloadListWidget::onCustomContextMenu(const QPoint &point)
{
  QMenu menu(this);
  QModelIndex index = indexAt(point);
  bool hidden = false;
  if (index.row() >= 0) {
    m_ContextRow = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(index).row();
    DownloadManager::DownloadState state = m_Manager->getState(m_ContextRow);
    hidden = m_Manager->isHidden(m_ContextRow);
    if (state >= DownloadManager::STATE_READY) {
      menu.addAction(tr("Install"), this, SLOT(issueInstall()));
      if (m_Manager->isInfoIncomplete(m_ContextRow)) {
        menu.addAction(tr("Query Info"), this, SLOT(issueQueryInfo()));
      }
      else {
        menu.addAction(tr("Visit on Nexus"), this, SLOT(issueVisitOnNexus()));
      }

      menu.addAction(tr("Open File"), this, SLOT(issueOpenFile()));
      menu.addAction(tr("Show in Folder"), this, SLOT(issueOpenInDownloadsFolder()));

      menu.addSeparator();

      menu.addAction(tr("Delete"), this, SLOT(issueDelete()));
      if (hidden) {
        menu.addAction(tr("Un-Hide"), this, SLOT(issueRestoreToView()));
      }
      else {
        menu.addAction(tr("Hide"), this, SLOT(issueRemoveFromView()));
      }
    }
    else if (state == DownloadManager::STATE_DOWNLOADING) {
      menu.addAction(tr("Cancel"), this, SLOT(issueCancel()));
      menu.addAction(tr("Pause"), this, SLOT(issuePause()));
      menu.addAction(tr("Show in Folder"), this, SLOT(issueOpenInDownloadsFolder()));
    }
    else if ((state == DownloadManager::STATE_PAUSED) || (state == DownloadManager::STATE_ERROR) || (state == DownloadManager::STATE_PAUSING)) {
      menu.addAction(tr("Delete"), this, SLOT(issueDelete()));
      menu.addAction(tr("Resume"), this, SLOT(issueResume()));
      menu.addAction(tr("Show in Folder"), this, SLOT(issueOpenInDownloadsFolder()));
    }

    menu.addSeparator();
  }
  menu.addAction(tr("Delete Installed..."), this, SLOT(issueDeleteCompleted()));
  menu.addAction(tr("Delete Uninstalled..."), this, SLOT(issueDeleteUninstalled()));
  menu.addAction(tr("Delete All..."), this, SLOT(issueDeleteAll()));

  if (!hidden) {
    menu.addSeparator();
    menu.addAction(tr("Hide Installed..."), this, SLOT(issueRemoveFromViewCompleted()));
    menu.addAction(tr("Hide Uninstalled..."), this, SLOT(issueRemoveFromViewUninstalled()));
    menu.addAction(tr("Hide All..."), this, SLOT(issueRemoveFromViewAll()));
  }
  if (hidden) {
    menu.addSeparator();
    menu.addAction(tr("Un-Hide All..."), this, SLOT(issueRestoreToViewAll()));
  }

  menu.exec(viewport()->mapToGlobal(point));
}

void DownloadListWidget::issueInstall()
{
  emit installDownload(m_ContextRow);
}

void DownloadListWidget::issueQueryInfo()
{
  emit queryInfo(m_ContextRow);
}

void DownloadListWidget::issueDelete()
{
	if (QMessageBox::question(nullptr, tr("Delete Files?"),
		tr("This will permanently delete the selected download."),
		QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
		emit removeDownload(m_ContextRow, true);
	}
}

void DownloadListWidget::issueRemoveFromView()
{
  qDebug() << "removing from view: " << m_ContextRow;
  emit removeDownload(m_ContextRow, false);
}

void DownloadListWidget::issueRestoreToView()
{
		emit restoreDownload(m_ContextRow);
}

void DownloadListWidget::issueRestoreToViewAll()
{
	emit restoreDownload(-1);
}

void DownloadListWidget::issueVisitOnNexus()
{
	emit visitOnNexus(m_ContextRow);
}

void DownloadListWidget::issueOpenFile()
{
  emit openFile(m_ContextRow);
}

void DownloadListWidget::issueOpenInDownloadsFolder()
{
  emit openInDownloadsFolder(m_ContextRow);
}

void DownloadListWidget::issueCancel()
{
  emit cancelDownload(m_ContextRow);
}

void DownloadListWidget::issuePause()
{
  emit pauseDownload(m_ContextRow);
}

void DownloadListWidget::issueResume()
{
  emit resumeDownload(m_ContextRow);
}

void DownloadListWidget::issueDeleteAll()
{
  if (QMessageBox::question(nullptr, tr("Delete Files?"),
                            tr("This will remove all finished downloads from this list and from disk."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, true);
  }
}

void DownloadListWidget::issueDeleteCompleted()
{
  if (QMessageBox::question(nullptr, tr("Delete Files?"),
                            tr("This will remove all installed downloads from this list and from disk."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, true);
  }
}

void DownloadListWidget::issueDeleteUninstalled()
{
  if (QMessageBox::question(nullptr, tr("Delete Files?"),
    tr("This will remove all uninstalled downloads from this list and from disk."),
    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-3, true);
  }
}

void DownloadListWidget::issueRemoveFromViewAll()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will remove all finished downloads from this list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, false);
  }
}

void DownloadListWidget::issueRemoveFromViewCompleted()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will remove all installed downloads from this list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, false);
  }
}

void DownloadListWidget::issueRemoveFromViewUninstalled()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
    tr("This will remove all uninstalled downloads from this list (but NOT from disk)."),
    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-3, false);
  }
}
