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

#include "downloadlistwidgetcompact.h"
#include "ui_downloadlistwidgetcompact.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>


DownloadListWidgetCompact::DownloadListWidgetCompact(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::DownloadListWidgetCompact)
{
  ui->setupUi(this);
}

DownloadListWidgetCompact::~DownloadListWidgetCompact()
{
  delete ui;
}


DownloadListWidgetCompactDelegate::DownloadListWidgetCompactDelegate(DownloadManager *manager, bool metaDisplay, QTreeView *view, QObject *parent)
  : QItemDelegate(parent)
  , m_Manager(manager)
  , m_MetaDisplay(metaDisplay)
  , m_ItemWidget(new DownloadListWidgetCompact)
  , m_View(view)
{
  m_NameLabel = m_ItemWidget->findChild<QLabel*>("nameLabel");
  m_SizeLabel = m_ItemWidget->findChild<QLabel*>("sizeLabel");
  m_Progress = m_ItemWidget->findChild<QProgressBar*>("downloadProgress");
  m_DoneLabel = m_ItemWidget->findChild<QLabel*>("doneLabel");

  m_DoneLabel->setVisible(false);

  connect(manager, SIGNAL(stateChanged(int,DownloadManager::DownloadState)),
          this, SLOT(stateChanged(int,DownloadManager::DownloadState)));
  connect(manager, SIGNAL(downloadRemoved(int)), this, SLOT(resetCache(int)));
}


DownloadListWidgetCompactDelegate::~DownloadListWidgetCompactDelegate()
{
  delete m_ItemWidget;
}


void DownloadListWidgetCompactDelegate::stateChanged(int row,DownloadManager::DownloadState)
{
  m_Cache.remove(row);
}


void DownloadListWidgetCompactDelegate::resetCache(int)
{
  m_Cache.clear();
}


void DownloadListWidgetCompactDelegate::drawCache(QPainter *painter, const QStyleOptionViewItem &option, const QPixmap &cache) const
{
  QRect rect = option.rect;
  rect.setLeft(0);
  rect.setWidth(m_View->columnWidth(0) + m_View->columnWidth(1) + m_View->columnWidth(2));
  painter->drawPixmap(rect, cache);
}


void DownloadListWidgetCompactDelegate::paintPendingDownload(int downloadIndex) const
{
  std::pair<int, int> nexusids = m_Manager->getPendingDownload(downloadIndex);
  m_NameLabel->setText(tr("< mod %1 file %2 >").arg(nexusids.first).arg(nexusids.second));
  if (m_SizeLabel != nullptr) {
    m_SizeLabel->setText("???");
  }
  m_DoneLabel->setVisible(true);
  m_DoneLabel->setText(tr("Pending"));
  m_Progress->setVisible(false);
}


void DownloadListWidgetCompactDelegate::paintRegularDownload(int downloadIndex) const
{
  QString name = m_MetaDisplay ? m_Manager->getDisplayName(downloadIndex) : m_Manager->getFileName(downloadIndex);
  if (name.length() > 53) {
    name.truncate(50);
    name.append("...");
  }
  m_NameLabel->setText(name);

  DownloadManager::DownloadState state = m_Manager->getState(downloadIndex);

  if ((m_SizeLabel != nullptr) && (state >= DownloadManager::STATE_READY)) {
    m_SizeLabel->setText(QString::number(m_Manager->getFileSize(downloadIndex) / 1048576));
  }

  if ((state == DownloadManager::STATE_PAUSED) || (state == DownloadManager::STATE_ERROR)) {
    m_DoneLabel->setVisible(true);
    m_Progress->setVisible(false);
    m_DoneLabel->setText(QString("%1<img src=\":/MO/gui/inactive\">").arg(tr("Paused")));
  } else if (state == DownloadManager::STATE_FETCHINGMODINFO) {
    m_DoneLabel->setText(QString("%1").arg(tr("Fetching Info 1")));
  } else if (state == DownloadManager::STATE_FETCHINGFILEINFO) {
    m_DoneLabel->setText(QString("%1").arg(tr("Fetching Info 2")));
  } else if (state >= DownloadManager::STATE_READY) {
    m_DoneLabel->setVisible(true);
    m_Progress->setVisible(false);
    if (state == DownloadManager::STATE_INSTALLED) {
      m_DoneLabel->setText(QString("%1<img src=\":/MO/gui/check\">").arg(tr("Installed")));
    } else if (state == DownloadManager::STATE_UNINSTALLED) {
      m_DoneLabel->setText(QString("%1<img src=\":/MO/gui/awaiting\">").arg(tr("Uninstalled")));
    } else {
      m_DoneLabel->setText(QString("%1<img src=\":/MO/gui/active\">").arg(tr("Done")));
    }
    if (m_Manager->isInfoIncomplete(downloadIndex)) {
      m_NameLabel->setText("<img src=\":/MO/gui/warning_16\"/> " + m_NameLabel->text());
    }
  } else {
    m_DoneLabel->setVisible(false);
    m_Progress->setVisible(true);
    m_Progress->setValue(m_Manager->getProgress(downloadIndex));
  }
}

void DownloadListWidgetCompactDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
#pragma message("This is quite costy - room for optimization?")
  try {
    auto iter = m_Cache.find(index.row());
    if (iter != m_Cache.end()) {
      drawCache(painter, option, *iter);
      return;
    }

    m_ItemWidget->resize(QSize(m_View->columnWidth(0) + m_View->columnWidth(1) + m_View->columnWidth(2), option.rect.height()));
    if (index.row() % 2 == 1) {
      m_ItemWidget->setBackgroundRole(QPalette::AlternateBase);
    } else {
      m_ItemWidget->setBackgroundRole(QPalette::Base);
    }

    int downloadIndex = index.data().toInt();
    if (downloadIndex >= m_Manager->numTotalDownloads()) {
      paintPendingDownload(downloadIndex - m_Manager->numTotalDownloads());
    } else {
      paintRegularDownload(downloadIndex);
    }

#pragma message("caching disabled because changes in the list (including resorting) doesn't work correctly")
    if (false) {
//    if (state >= DownloadManager::STATE_READY) {
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
      QPixmap cache = m_ItemWidget->grab();
#else
      QPixmap cache = QPixmap::grabWidget(m_ItemWidget);
#endif
      m_Cache[index.row()] = cache;
      drawCache(painter, option, cache);
    } else {
      painter->save();
      painter->translate(QPoint(0, option.rect.topLeft().y()));

      m_ItemWidget->render(painter);
      painter->restore();
    }
  } catch (const std::exception &e) {
    qCritical("failed to paint download list item %d: %s", index.row(), e.what());
  }
}

QSize DownloadListWidgetCompactDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
  const int width = m_ItemWidget->minimumWidth();
  const int height = m_ItemWidget->height();
  return QSize(width, height);
}


void DownloadListWidgetCompactDelegate::issueInstall()
{
  emit installDownload(m_ContextIndex.row());
}

void DownloadListWidgetCompactDelegate::issueQueryInfo()
{
  emit queryInfo(m_ContextIndex.row());
}

void DownloadListWidgetCompactDelegate::issueDelete()
{
  emit removeDownload(m_ContextIndex.row(), true);
}

void DownloadListWidgetCompactDelegate::issueRemoveFromView()
{
  emit removeDownload(m_ContextIndex.row(), false);
}

void DownloadListWidgetCompactDelegate::issueRestoreToView()
{
  emit restoreDownload(m_ContextIndex.row());
}

void DownloadListWidgetCompactDelegate::issueCancel()
{
  emit cancelDownload(m_ContextIndex.row());
}

void DownloadListWidgetCompactDelegate::issuePause()
{
  emit pauseDownload(m_ContextIndex.row());
}

void DownloadListWidgetCompactDelegate::issueResume()
{
  emit resumeDownload(m_ContextIndex.row());
}

void DownloadListWidgetCompactDelegate::issueDeleteAll()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will remove all finished downloads from this list and from disk."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, true);
  }
}

void DownloadListWidgetCompactDelegate::issueDeleteCompleted()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will remove all installed downloads from this list and from disk."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, true);
  }
}

void DownloadListWidgetCompactDelegate::issueRemoveFromViewAll()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will permanently remove all finished downloads from this list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, false);
  }
}

void DownloadListWidgetCompactDelegate::issueRemoveFromViewCompleted()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will permanently remove all installed downloads from this list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, false);
  }
}


bool DownloadListWidgetCompactDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                   const QStyleOptionViewItem &option, const QModelIndex &index)
{
  try {
    if (event->type() == QEvent::MouseButtonDblClick) {
      QModelIndex sourceIndex = qobject_cast<QSortFilterProxyModel*>(model)->mapToSource(index);
      if (m_Manager->getState(sourceIndex.row()) >= DownloadManager::STATE_READY) {
        emit installDownload(sourceIndex.row());
      } else if (m_Manager->getState(sourceIndex.row()) >= DownloadManager::STATE_PAUSED) {
        emit resumeDownload(sourceIndex.row());
      }
      return true;
    } else if (event->type() == QEvent::MouseButtonRelease) {
      QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
      if (mouseEvent->button() == Qt::RightButton) {
        QMenu menu;
        bool hidden = false;
        m_ContextIndex = qobject_cast<QSortFilterProxyModel*>(model)->mapToSource(index);
        if (m_ContextIndex.row() < m_Manager->numTotalDownloads()) {
          DownloadManager::DownloadState state = m_Manager->getState(m_ContextIndex.row());
          hidden = m_Manager->isHidden(m_ContextIndex.row());
          if (state >= DownloadManager::STATE_READY) {
            menu.addAction(tr("Install"), this, SLOT(issueInstall()));
            if (m_Manager->isInfoIncomplete(m_ContextIndex.row())) {
              menu.addAction(tr("Query Info"), this, SLOT(issueQueryInfo()));
            }
            menu.addAction(tr("Delete"), this, SLOT(issueDelete()));
            if (hidden) {
              menu.addAction(tr("Un-Hide"), this, SLOT(issueRestoreToView()));
            } else {
              menu.addAction(tr("Hide"), this, SLOT(issueRemoveFromView()));
            }
          } else if (state == DownloadManager::STATE_DOWNLOADING){
            menu.addAction(tr("Cancel"), this, SLOT(issueCancel()));
            menu.addAction(tr("Pause"), this, SLOT(issuePause()));
          } else if ((state == DownloadManager::STATE_PAUSED) || (state == DownloadManager::STATE_ERROR)) {
            menu.addAction(tr("Remove"), this, SLOT(issueDelete()));
            menu.addAction(tr("Resume"), this, SLOT(issueResume()));
          }

          menu.addSeparator();
        }
        menu.addAction(tr("Delete Installed..."), this, SLOT(issueDeleteCompleted()));
        menu.addAction(tr("Delete All..."), this, SLOT(issueDeleteAll()));
        if (!hidden) {
          menu.addSeparator();
          menu.addAction(tr("Hide Installed..."), this, SLOT(issueRemoveFromViewCompleted()));
          menu.addAction(tr("Hide All..."), this, SLOT(issueRemoveFromViewAll()));
        }
        menu.exec(mouseEvent->globalPos());

        event->accept();
        return false;
      }
    }
  } catch (const std::exception &e) {
    qCritical("failed to handle editor event: %s", e.what());
  }

  return QItemDelegate::editorEvent(event, model, option, index);
}

