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

#include "downloadlistview.h"
#include "downloadlist.h"
#include <QApplication>
#include <QCheckBox>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QWidgetAction>
#include <log.h>
#include <report.h>

using namespace MOBase;

DownloadProgressDelegate::DownloadProgressDelegate(DownloadManager* manager,
                                                   DownloadListView* list)
    : QStyledItemDelegate(list), m_Manager(manager), m_List(list)
{}

void DownloadProgressDelegate::paint(QPainter* painter,
                                     const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const
{
  QModelIndex sourceIndex;

  if (auto* proxy = dynamic_cast<QSortFilterProxyModel*>(m_List->model())) {
    sourceIndex = proxy->mapToSource(index);
  } else {
    sourceIndex = index;
  }

  bool pendingDownload = (sourceIndex.row() >= m_Manager->numTotalDownloads());
  if (sourceIndex.column() == DownloadList::COL_STATUS && !pendingDownload &&
      m_Manager->getState(sourceIndex.row()) == DownloadManager::STATE_DOWNLOADING) {
    QProgressBar progressBar;
    progressBar.setProperty("downloadView", option.widget->property("downloadView"));
    progressBar.setProperty("downloadProgress", true);
    progressBar.resize(option.rect.width(), option.rect.height());
    progressBar.setTextVisible(true);
    progressBar.setAlignment(Qt::AlignCenter);
    progressBar.setMinimum(0);
    progressBar.setMaximum(100);
    progressBar.setValue(m_Manager->getProgress(sourceIndex.row()).first);
    progressBar.setFormat(m_Manager->getProgress(sourceIndex.row()).second);
    progressBar.setStyle(QApplication::style());

    // paint the background with default delegate first to preserve table cell styling
    QStyledItemDelegate::paint(painter, option, index);

    painter->save();
    painter->translate(option.rect.topLeft());
    progressBar.render(painter);
    painter->restore();
  } else {
    QStyledItemDelegate::paint(painter, option, index);
  }
}

void DownloadListHeader::customResizeSections()
{
  // find the rightmost column that is not hidden
  int rightVisible = count() - 1;
  while (isSectionHidden(rightVisible) && rightVisible > 0)
    rightVisible--;

  // if that column is already squashed, squash others to the right side --
  // otherwise to the left
  if (sectionSize(rightVisible) == minimumSectionSize()) {
    for (int idx = rightVisible; idx >= 0; idx--) {
      if (!isSectionHidden(idx)) {
        if (length() != width())
          resizeSection(idx, std::max(sectionSize(idx) + width() - length(),
                                      minimumSectionSize()));
        else
          break;
      }
    }
  } else {
    for (int idx = 0; idx <= rightVisible; idx++) {
      if (!isSectionHidden(idx)) {
        if (length() != width())
          resizeSection(idx, std::max(sectionSize(idx) + width() - length(),
                                      minimumSectionSize()));
        else
          break;
      }
    }
  }
}

void DownloadListHeader::mouseReleaseEvent(QMouseEvent* event)
{
  QHeaderView::mouseReleaseEvent(event);
  customResizeSections();
}

DownloadListView::DownloadListView(QWidget* parent) : QTreeView(parent)
{
  setHeader(new DownloadListHeader(Qt::Horizontal, this));

  header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  header()->setSectionsMovable(true);
  header()->setContextMenuPolicy(Qt::CustomContextMenu);
  header()->setCascadingSectionResizes(true);
  header()->setStretchLastSection(false);
  header()->setSectionResizeMode(QHeaderView::Interactive);
  header()->setDefaultSectionSize(100);

  setUniformRowHeights(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  sortByColumn(1, Qt::DescendingOrder);

  connect(header(), SIGNAL(customContextMenuRequested(QPoint)), this,
          SLOT(onHeaderCustomContextMenu(QPoint)));
  connect(this, SIGNAL(doubleClicked(QModelIndex)), this,
          SLOT(onDoubleClick(QModelIndex)));
  connect(this, SIGNAL(customContextMenuRequested(QPoint)), this,
          SLOT(onCustomContextMenu(QPoint)));
}

DownloadListView::~DownloadListView() {}

void DownloadListView::setManager(DownloadManager* manager)
{
  m_Manager = manager;

  // hide these columns by default
  //
  // note that this is overridden by the ini if MO has been started at least
  // once before, which is handled in MainWindow::processUpdates() for older
  // versions
  header()->hideSection(DownloadList::COL_MODNAME);
  header()->hideSection(DownloadList::COL_VERSION);
  header()->hideSection(DownloadList::COL_ID);
  header()->hideSection(DownloadList::COL_SOURCEGAME);
}

void DownloadListView::setSourceModel(DownloadList* sourceModel)
{
  m_SourceModel = sourceModel;
}

void DownloadListView::onDoubleClick(const QModelIndex& index)
{
  QModelIndex sourceIndex =
      qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(index);
  if (m_Manager->getState(sourceIndex.row()) >= DownloadManager::STATE_READY)
    emit installDownload(sourceIndex.row());
  else if ((m_Manager->getState(sourceIndex.row()) == DownloadManager::STATE_PAUSED) ||
           (m_Manager->getState(sourceIndex.row()) == DownloadManager::STATE_PAUSING))
    emit resumeDownload(sourceIndex.row());
}

void DownloadListView::onHeaderCustomContextMenu(const QPoint& point)
{
  QMenu menu;

  // display a list of all headers as checkboxes
  QAbstractItemModel* model = header()->model();
  for (int i = 1; i < model->columnCount(); ++i) {
    QString columnName  = model->headerData(i, Qt::Horizontal).toString();
    QCheckBox* checkBox = new QCheckBox(&menu);
    checkBox->setText(columnName);
    checkBox->setChecked(!header()->isSectionHidden(i));
    QWidgetAction* checkableAction = new QWidgetAction(&menu);
    checkableAction->setDefaultWidget(checkBox);
    menu.addAction(checkableAction);
  }

  menu.exec(header()->viewport()->mapToGlobal(point));

  // view/hide columns depending on check-state
  int i = 1;
  for (const QAction* action : menu.actions()) {
    const QWidgetAction* widgetAction = qobject_cast<const QWidgetAction*>(action);
    if (widgetAction != nullptr) {
      const QCheckBox* checkBox =
          qobject_cast<const QCheckBox*>(widgetAction->defaultWidget());
      if (checkBox != nullptr) {
        header()->setSectionHidden(i, !checkBox->isChecked());
      }
    }
    ++i;
  }

  qobject_cast<DownloadListHeader*>(header())->customResizeSections();
}

void DownloadListView::resizeEvent(QResizeEvent* event)
{
  QTreeView::resizeEvent(event);
  qobject_cast<DownloadListHeader*>(header())->customResizeSections();
}

void DownloadListView::onCustomContextMenu(const QPoint& point)
{
  QMenu menu(this);
  QModelIndex index = indexAt(point);
  bool hidden       = false;

  try {
    if (index.row() >= 0) {
      const int row =
          qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(index).row();
      DownloadManager::DownloadState state = m_Manager->getState(row);

      hidden = m_Manager->isHidden(row);

      if (state >= DownloadManager::STATE_READY) {
        menu.addAction(tr("Install"), [=] {
          issueInstall(row);
        });
        if (m_Manager->isInfoIncomplete(row))
          menu.addAction(tr("Query Info"), [=] {
            issueQueryInfoMd5(row);
          });
        else
          menu.addAction(tr("Visit on Nexus"), [=] {
            issueVisitOnNexus(row);
          });
        menu.addAction(tr("Open File"), [=] {
          issueOpenFile(row);
        });
        menu.addAction(tr("Open Meta File"), [=] {
          issueOpenMetaFile(row);
        });
        menu.addAction(tr("Reveal in Explorer"), [=] {
          issueOpenInDownloadsFolder(row);
        });

        menu.addSeparator();

        menu.addAction(tr("Delete..."), [=] {
          issueDelete(row);
        });
        if (hidden)
          menu.addAction(tr("Un-Hide"), [=] {
            issueRestoreToView(row);
          });
        else
          menu.addAction(tr("Hide"), [=] {
            issueRemoveFromView(row);
          });
      } else if (state == DownloadManager::STATE_DOWNLOADING) {
        menu.addAction(tr("Cancel"), [=] {
          issueCancel(row);
        });
        menu.addAction(tr("Pause"), [=] {
          issuePause(row);
        });
        menu.addAction(tr("Reveal in Explorer"), [=] {
          issueOpenInDownloadsFolder(row);
        });
      } else if ((state == DownloadManager::STATE_PAUSED) ||
                 (state == DownloadManager::STATE_ERROR) ||
                 (state == DownloadManager::STATE_PAUSING)) {
        menu.addAction(tr("Delete..."), [=] {
          issueDelete(row);
        });
        menu.addAction(tr("Resume"), [=] {
          issueResume(row);
        });
        menu.addAction(tr("Reveal in Explorer"), [=] {
          issueOpenInDownloadsFolder(row);
        });
      }

      menu.addSeparator();
    }
  } catch (std::exception&) {
    // this happens when the download index is not found, ignore it and don't
    // display download-specific actions
  }

  menu.addAction(tr("Delete Installed Downloads..."), [=] {
    issueDeleteCompleted();
  });
  menu.addAction(tr("Delete Uninstalled Downloads..."), [=] {
    issueDeleteUninstalled();
  });
  menu.addAction(tr("Delete All Downloads..."), [=] {
    issueDeleteAll();
  });

  menu.addSeparator();
  if (!hidden) {
    menu.addAction(tr("Hide Installed..."), [=] {
      issueRemoveFromViewCompleted();
    });
    menu.addAction(tr("Hide Uninstalled..."), [=] {
      issueRemoveFromViewUninstalled();
    });
    menu.addAction(tr("Hide All..."), [=] {
      issueRemoveFromViewAll();
    });
  } else {
    menu.addAction(tr("Un-Hide All..."), [=] {
      issueRestoreToViewAll();
    });
  }

  menu.exec(viewport()->mapToGlobal(point));
}

void DownloadListView::keyPressEvent(QKeyEvent* event)
{
  if (selectionModel()->hasSelection()) {
    const int row = qobject_cast<QSortFilterProxyModel*>(model())
                        ->mapToSource(currentIndex())
                        .row();
    auto state = m_Manager->getState(row);
    if (state >= DownloadManager::STATE_READY) {
      if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        issueInstall(row);
      } else if (event->key() == Qt::Key_Delete) {
        issueDelete(row);
      }
    } else if (state == DownloadManager::STATE_DOWNLOADING) {
      if (event->key() == Qt::Key_Delete) {
        issueCancel(row);
      } else if (event->key() == Qt::Key_Space) {
        issuePause(event->key());
      }
    } else if (state == DownloadManager::STATE_PAUSED ||
               state == DownloadManager::STATE_ERROR ||
               state == DownloadManager::STATE_PAUSING) {
      if (event->key() == Qt::Key_Delete) {
        issueDelete(row);
      } else if (event->key() == Qt::Key_Space) {
        issueResume(row);
      }
    }
  }
  QTreeView::keyPressEvent(event);
}

void DownloadListView::issueInstall(int index)
{
  emit installDownload(index);
}

void DownloadListView::issueQueryInfo(int index)
{
  emit queryInfo(index);
}

void DownloadListView::issueQueryInfoMd5(int index)
{
  emit queryInfoMd5(index);
}

void DownloadListView::issueDelete(int index)
{
  const auto r = MOBase::TaskDialog(this, tr("Delete download"))
                     .main("Are you sure you want to delete this download?")
                     .content(m_Manager->getFilePath(index))
                     .icon(QMessageBox::Question)
                     .button({tr("Move to the Recycle Bin"), QMessageBox::Yes})
                     .button({tr("Cancel"), QMessageBox::Cancel})
                     .exec();

  if (r != QMessageBox::Yes) {
    return;
  }

  emit removeDownload(index, true);
}

void DownloadListView::issueRemoveFromView(int index)
{
  log::debug("removing from view: {}", index);
  emit removeDownload(index, false);
}

void DownloadListView::issueRestoreToView(int index)
{
  emit restoreDownload(index);
}

void DownloadListView::issueRestoreToViewAll()
{
  emit restoreDownload(-1);
}

void DownloadListView::issueVisitOnNexus(int index)
{
  emit visitOnNexus(index);
}

void DownloadListView::issueOpenFile(int index)
{
  emit openFile(index);
}

void DownloadListView::issueOpenMetaFile(int index)
{
  emit openMetaFile(index);
}

void DownloadListView::issueOpenInDownloadsFolder(int index)
{
  emit openInDownloadsFolder(index);
}

void DownloadListView::issueCancel(int index)
{
  emit cancelDownload(index);
}

void DownloadListView::issuePause(int index)
{
  emit pauseDownload(index);
}

void DownloadListView::issueResume(int index)
{
  emit resumeDownload(index);
}

void DownloadListView::issueDeleteAll()
{
  if (QMessageBox::warning(
          nullptr, tr("Delete Files?"),
          tr("This will remove all finished downloads from this list and from "
             "disk.\n\nAre you absolutely sure you want to proceed?"),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, true);
  }
}

void DownloadListView::issueDeleteCompleted()
{
  if (QMessageBox::warning(
          nullptr, tr("Delete Files?"),
          tr("This will remove all installed downloads from this list and from "
             "disk.\n\nAre you absolutely sure you want to proceed?"),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, true);
  }
}

void DownloadListView::issueDeleteUninstalled()
{
  if (QMessageBox::warning(
          nullptr, tr("Delete Files?"),
          tr("This will remove all uninstalled downloads from this list and from "
             "disk.\n\nAre you absolutely sure you want to proceed?"),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-3, true);
  }
}

void DownloadListView::issueRemoveFromViewAll()
{
  if (QMessageBox::question(nullptr, tr("Hide Files?"),
                            tr("This will remove all finished downloads from this list "
                               "(but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, false);
  }
}

void DownloadListView::issueRemoveFromViewCompleted()
{
  if (QMessageBox::question(nullptr, tr("Hide Files?"),
                            tr("This will remove all installed downloads from this "
                               "list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, false);
  }
}

void DownloadListView::issueRemoveFromViewUninstalled()
{
  if (QMessageBox::question(nullptr, tr("Hide Files?"),
                            tr("This will remove all uninstalled downloads from this "
                               "list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-3, false);
  }
}
