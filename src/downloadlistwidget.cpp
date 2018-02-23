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

#include "downloadlistwidget.h"
#include "ui_downloadlistwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>


DownloadListWidget::DownloadListWidget(QWidget *parent)
  : QWidget(parent), ui(new Ui::DownloadListWidget)
{
  ui->setupUi(this);
}


DownloadListWidget::~DownloadListWidget()
{
    delete ui;
}


DownloadListWidgetDelegate::DownloadListWidgetDelegate(DownloadManager *manager, bool metaDisplay, QTreeView *view, QObject *parent)
  : QItemDelegate(parent)
  , m_Manager(manager)
  , m_MetaDisplay(metaDisplay)
  , m_ItemWidget(new DownloadListWidget)
  , m_ContextRow(0)
  , m_View(view)
{
  m_NameLabel = m_ItemWidget->findChild<QLabel*>("nameLabel");
  m_SizeLabel = m_ItemWidget->findChild<QLabel*>("sizeLabel");
  m_Progress = m_ItemWidget->findChild<QProgressBar*>("downloadProgress");
  m_InstallLabel = m_ItemWidget->findChild<QLabel*>("installLabel");

  m_InstallLabel->setVisible(false);

  connect(manager, SIGNAL(stateChanged(int,DownloadManager::DownloadState)), this, SLOT(stateChanged(int,DownloadManager::DownloadState)));
  connect(manager, SIGNAL(downloadRemoved(int)), this, SLOT(resetCache(int)));
}


DownloadListWidgetDelegate::~DownloadListWidgetDelegate()
{
  delete m_ItemWidget;
}


void DownloadListWidgetDelegate::stateChanged(int row,DownloadManager::DownloadState)
{
  m_Cache.remove(row);
}


void DownloadListWidgetDelegate::resetCache(int)
{
  m_Cache.clear();
}


void DownloadListWidgetDelegate::drawCache(QPainter *painter, const QStyleOptionViewItem &option, const QPixmap &cache) const
{
  QRect rect = option.rect;
  rect.setLeft(0);
  rect.setWidth(m_View->columnWidth(0) + m_View->columnWidth(1) + m_View->columnWidth(2));
  painter->drawPixmap(rect, cache);
}


void DownloadListWidgetDelegate::paintPendingDownload(int downloadIndex) const
{
  std::pair<int, int> nexusids = m_Manager->getPendingDownload(downloadIndex);
  m_NameLabel->setText(tr("< mod %1 file %2 >").arg(nexusids.first).arg(nexusids.second));
  m_SizeLabel->setText("???");
  m_InstallLabel->setVisible(true);
  m_InstallLabel->setText(tr("Pending"));
  m_Progress->setVisible(false);
}


void DownloadListWidgetDelegate::paintRegularDownload(int downloadIndex) const
{
  QString name = m_MetaDisplay ? m_Manager->getDisplayName(downloadIndex) : m_Manager->getFileName(downloadIndex);
  if (name.length() > 53) {
    name.truncate(50);
    name.append("...");
  }
  m_NameLabel->setText(name);
  m_SizeLabel->setText(QString::number(m_Manager->getFileSize(downloadIndex) / 1024));
  DownloadManager::DownloadState state = m_Manager->getState(downloadIndex);
  if ((state == DownloadManager::STATE_PAUSED) || (state == DownloadManager::STATE_ERROR)) {
    QPalette labelPalette;
    m_InstallLabel->setVisible(true);
    m_Progress->setVisible(false);
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    m_InstallLabel->setText(QApplication::translate("DownloadListWidget", "Paused - Double Click to resume", 0));
#else
    m_InstallLabel->setText(QApplication::translate("DownloadListWidget", "Paused - Double Click to resume", 0, QApplication::UnicodeUTF8));
#endif
    labelPalette.setColor(QPalette::WindowText, Qt::darkRed);
    m_InstallLabel->setPalette(labelPalette);
  } else if (state == DownloadManager::STATE_FETCHINGMODINFO) {
    m_InstallLabel->setText(tr("Fetching Info 1"));
    m_Progress->setVisible(false);
  } else if (state == DownloadManager::STATE_FETCHINGFILEINFO) {
    m_InstallLabel->setText(tr("Fetching Info 2"));
    m_Progress->setVisible(false);
  } else if (state >= DownloadManager::STATE_READY) {
    QPalette labelPalette;
    m_InstallLabel->setVisible(true);
    m_Progress->setVisible(false);
    if (state == DownloadManager::STATE_INSTALLED) {
      // the tr-macro doesn't work here, maybe because the translation is actually associated with DownloadListWidget instead
      // of DownloadListWidgetDelegate?
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
      m_InstallLabel->setText(QApplication::translate("DownloadListWidget", "Installed - Double Click to re-install", 0));
#else
      m_InstallLabel->setText(QApplication::translate("DownloadListWidget", "Installed - Double Click to re-install", 0, QApplication::UnicodeUTF8));
#endif
      labelPalette.setColor(QPalette::WindowText, Qt::darkGray);
    } else if (state == DownloadManager::STATE_UNINSTALLED) {
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
      m_InstallLabel->setText(QApplication::translate("DownloadListWidget", "Uninstalled - Double Click to re-install", 0));
#else
      m_InstallLabel->setText(QApplication::translate("DownloadListWidget", "Uninstalled - Double Click to re-install", 0, QApplication::UnicodeUTF8));
#endif
      labelPalette.setColor(QPalette::WindowText, Qt::lightGray);
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
      m_InstallLabel->setText(QApplication::translate("DownloadListWidget", "Done - Double Click to install", 0));
#else
      m_InstallLabel->setText(QApplication::translate("DownloadListWidget", "Done - Double Click to install", 0, QApplication::UnicodeUTF8));
#endif
      labelPalette.setColor(QPalette::WindowText, Qt::darkGreen);
    }
    m_InstallLabel->setPalette(labelPalette);
    if (m_Manager->isInfoIncomplete(downloadIndex)) {
      m_NameLabel->setText("<img src=\":/MO/gui/warning_16\" /> " + m_NameLabel->text());
    }
  } else {
    m_InstallLabel->setVisible(false);
    m_Progress->setVisible(true);
    m_Progress->setValue(m_Manager->getProgress(downloadIndex));
  }
}

void DownloadListWidgetDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  try {
    auto iter = m_Cache.find(index.row());
    if (iter != m_Cache.end()) {
      drawCache(painter, option, *iter);
      return;
    }

    m_ItemWidget->resize(QSize(m_View->columnWidth(0) + m_View->columnWidth(1) + m_View->columnWidth(2), option.rect.height()));

    int downloadIndex = index.data().toInt();

    if (downloadIndex >= m_Manager->numTotalDownloads()) {
      paintPendingDownload(downloadIndex - m_Manager->numTotalDownloads());
    } else {
      paintRegularDownload(downloadIndex);
    }

#pragma message("caching disabled because changes in the list (including resorting) doesn't work correctly")
//    if (state >= DownloadManager::STATE_READY) {
    if (false) {
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
    qCritical("failed to paint download list: %s", e.what());
  }
}

QSize DownloadListWidgetDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
  const int width = m_ItemWidget->minimumWidth();
  const int height = m_ItemWidget->height();
  return QSize(width, height);
}


void DownloadListWidgetDelegate::issueInstall()
{
  emit installDownload(m_ContextRow);
}

void DownloadListWidgetDelegate::issueQueryInfo()
{
  emit queryInfo(m_ContextRow);
}

void DownloadListWidgetDelegate::issueDelete()
{
  emit removeDownload(m_ContextRow, true);
}

void DownloadListWidgetDelegate::issueRemoveFromView()
{
  emit removeDownload(m_ContextRow, false);
}

void DownloadListWidgetDelegate::issueRestoreToView()
{
  emit restoreDownload(m_ContextRow);
}

void DownloadListWidgetDelegate::issueCancel()
{
  emit cancelDownload(m_ContextRow);
}

void DownloadListWidgetDelegate::issuePause()
{
  emit pauseDownload(m_ContextRow);
}

void DownloadListWidgetDelegate::issueResume()
{
  emit resumeDownload(m_ContextRow);
}

void DownloadListWidgetDelegate::issueDeleteAll()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will remove all finished downloads from this list and from disk."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, true);
  }
}

void DownloadListWidgetDelegate::issueDeleteCompleted()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will remove all installed downloads from this list and from disk."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, true);
  }
}

void DownloadListWidgetDelegate::issueRemoveFromViewAll()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will remove all finished downloads from this list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, false);
  }
}

void DownloadListWidgetDelegate::issueRemoveFromViewCompleted()
{
  if (QMessageBox::question(nullptr, tr("Are you sure?"),
                            tr("This will remove all installed downloads from this list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, false);
  }
}

bool DownloadListWidgetDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
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
        QMenu menu(m_View);
        bool hidden = false;
        m_ContextRow = qobject_cast<QSortFilterProxyModel*>(model)->mapToSource(index).row();
        if (m_ContextRow < m_Manager->numTotalDownloads()) {
          DownloadManager::DownloadState state = m_Manager->getState(m_ContextRow);
          hidden = m_Manager->isHidden(m_ContextRow);
          if (state >= DownloadManager::STATE_READY) {
            menu.addAction(tr("Install"), this, SLOT(issueInstall()));
            if (m_Manager->isInfoIncomplete(m_ContextRow)) {
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
        return true;
      }
    }
  } catch (const std::exception &e) {
    qCritical("failed to handle editor event: %s", e.what());
  }
  return QItemDelegate::editorEvent(event, model, option, index);
}
