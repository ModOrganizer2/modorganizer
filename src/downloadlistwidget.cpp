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
#include <log.h>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QApplication>
#include <QHeaderView>
#include <QCheckBox>
#include <QWidgetAction>

using namespace MOBase;

void DownloadProgressDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QModelIndex sourceIndex = m_SortProxy->mapToSource(index);
  bool pendingDownload = (sourceIndex.row() >= m_Manager->numTotalDownloads());
  if (sourceIndex.column() == DownloadList::COL_STATUS && !pendingDownload
      && m_Manager->getState(sourceIndex.row()) == DownloadManager::STATE_DOWNLOADING) {
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
          resizeSection(idx, std::max(sectionSize(idx) + width() - length(), minimumSectionSize()));
        else
          break;
      }
    }
  } else {
    for (int idx = 0; idx <= rightVisible; idx++) {
      if (!isSectionHidden(idx)) {
        if (length() != width())
          resizeSection(idx, std::max(sectionSize(idx) + width() - length(), minimumSectionSize()));
        else
          break;
      }
    }
  }
}

void DownloadListHeader::mouseReleaseEvent(QMouseEvent *event)
{
  QHeaderView::mouseReleaseEvent(event);
  customResizeSections();
}

DownloadListWidget::DownloadListWidget(QWidget *parent)
  : QTreeView(parent)
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

  connect(header(), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onHeaderCustomContextMenu(QPoint)));
  connect(this, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(onDoubleClick(QModelIndex)));
  connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onCustomContextMenu(QPoint)));
}

DownloadListWidget::~DownloadListWidget()
{
}

void DownloadListWidget::setManager(DownloadManager *manager)
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

void DownloadListWidget::setSourceModel(DownloadList *sourceModel)
{
  m_SourceModel = sourceModel;
}

void DownloadListWidget::setMetaDisplay(bool metaDisplay)
{
  if (m_SourceModel != nullptr)
    m_SourceModel->setMetaDisplay(metaDisplay);
}

void DownloadListWidget::onDoubleClick(const QModelIndex &index)
{
  QModelIndex sourceIndex = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(index);
  if (m_Manager->getState(sourceIndex.row()) >= DownloadManager::STATE_READY)
    emit installDownload(sourceIndex.row());
  else if ((m_Manager->getState(sourceIndex.row()) == DownloadManager::STATE_PAUSED)
          || (m_Manager->getState(sourceIndex.row()) == DownloadManager::STATE_PAUSING))
    emit resumeDownload(sourceIndex.row());
}

void DownloadListWidget::onHeaderCustomContextMenu(const QPoint &point)
{
  QMenu menu;

  // display a list of all headers as checkboxes
  QAbstractItemModel *model = header()->model();
  for (int i = 1; i < model->columnCount(); ++i) {
    QString columnName = model->headerData(i, Qt::Horizontal).toString();
    QCheckBox *checkBox = new QCheckBox(&menu);
    checkBox->setText(columnName);
    checkBox->setChecked(!header()->isSectionHidden(i));
    QWidgetAction *checkableAction = new QWidgetAction(&menu);
    checkableAction->setDefaultWidget(checkBox);
    menu.addAction(checkableAction);
  }

  menu.exec(header()->viewport()->mapToGlobal(point));

  // view/hide columns depending on check-state
  int i = 1;
  for (const QAction *action : menu.actions()) {
    const QWidgetAction *widgetAction = qobject_cast<const QWidgetAction*>(action);
    if (widgetAction != nullptr) {
      const QCheckBox *checkBox = qobject_cast<const QCheckBox*>(widgetAction->defaultWidget());
      if (checkBox != nullptr) {
        header()->setSectionHidden(i, !checkBox->isChecked());
      }
    }
    ++i;
  }

  qobject_cast<DownloadListHeader*>(header())->customResizeSections();
}

void DownloadListWidget::resizeEvent(QResizeEvent *event)
{
  QTreeView::resizeEvent(event);
  qobject_cast<DownloadListHeader*>(header())->customResizeSections();
}

void DownloadListWidget::onCustomContextMenu(const QPoint &point)
{
  QMenu menu(this);
  QModelIndex index = indexAt(point);
  bool hidden = false;

  try
  {
    if (index.row() >= 0) {
      m_ContextRow = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(index).row();
      DownloadManager::DownloadState state = m_Manager->getState(m_ContextRow);

      hidden = m_Manager->isHidden(m_ContextRow);

      if (state >= DownloadManager::STATE_READY) {
        menu.addAction(tr("Install"), this, SLOT(issueInstall()));
        if (m_Manager->isInfoIncomplete(m_ContextRow))
          menu.addAction(tr("Query Info"), this, SLOT(issueQueryInfoMd5()));
        else
          menu.addAction(tr("Visit on Nexus"), this, SLOT(issueVisitOnNexus()));
        menu.addAction(tr("Open File"), this, SLOT(issueOpenFile()));
        menu.addAction(tr("Open Meta File"), this, SLOT(issueOpenMetaFile()));
        menu.addAction(tr("Reveal in Explorer"), this, SLOT(issueOpenInDownloadsFolder()));

        menu.addSeparator();

        menu.addAction(tr("Delete"), this, SLOT(issueDelete()));
        if (hidden)
          menu.addAction(tr("Un-Hide"), this, SLOT(issueRestoreToView()));
        else
          menu.addAction(tr("Hide"), this, SLOT(issueRemoveFromView()));
      } else if (state == DownloadManager::STATE_DOWNLOADING) {
        menu.addAction(tr("Cancel"), this, SLOT(issueCancel()));
        menu.addAction(tr("Pause"), this, SLOT(issuePause()));
        menu.addAction(tr("Reveal in Explorer"), this, SLOT(issueOpenInDownloadsFolder()));
      } else if ((state == DownloadManager::STATE_PAUSED) || (state == DownloadManager::STATE_ERROR)
                || (state == DownloadManager::STATE_PAUSING)) {
        menu.addAction(tr("Delete"), this, SLOT(issueDelete()));
        menu.addAction(tr("Resume"), this, SLOT(issueResume()));
        menu.addAction(tr("Reveal in Explorer"), this, SLOT(issueOpenInDownloadsFolder()));
      }

      menu.addSeparator();
    }
  } catch(std::exception&)
  {
    // this happens when the download index is not found, ignore it and don't
    // display download-specific actions
  }

  menu.addAction(tr("Delete Installed Downloads..."), this, SLOT(issueDeleteCompleted()));
  menu.addAction(tr("Delete Uninstalled Downloads..."), this, SLOT(issueDeleteUninstalled()));
  menu.addAction(tr("Delete All Downloads..."), this, SLOT(issueDeleteAll()));

  menu.addSeparator();
  if (!hidden) {
    menu.addAction(tr("Hide Installed..."), this, SLOT(issueRemoveFromViewCompleted()));
    menu.addAction(tr("Hide Uninstalled..."), this, SLOT(issueRemoveFromViewUninstalled()));
    menu.addAction(tr("Hide All..."), this, SLOT(issueRemoveFromViewAll()));
  } else {
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

void DownloadListWidget::issueQueryInfoMd5()
{
  emit queryInfoMd5(m_ContextRow);
}

void DownloadListWidget::issueDelete()
{
  if (QMessageBox::warning(nullptr, tr("Delete Files?"),
                            tr("This will permanently delete the selected download.\n\nAre you absolutely sure you want to proceed?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(m_ContextRow, true);
  }
}

void DownloadListWidget::issueRemoveFromView()
{
  log::debug("removing from view: {}", m_ContextRow);
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

void DownloadListWidget::issueOpenMetaFile() {
  emit openMetaFile(m_ContextRow);
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
  if (QMessageBox::warning(nullptr, tr("Delete Files?"),
                            tr("This will remove all finished downloads from this list and from disk.\n\nAre you absolutely sure you want to proceed?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, true);
  }
}

void DownloadListWidget::issueDeleteCompleted()
{
  if (QMessageBox::warning(nullptr, tr("Delete Files?"),
                            tr("This will remove all installed downloads from this list and from disk.\n\nAre you absolutely sure you want to proceed?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, true);
  }
}

void DownloadListWidget::issueDeleteUninstalled()
{
  if (QMessageBox::warning(nullptr, tr("Delete Files?"),
                            tr("This will remove all uninstalled downloads from this list and from disk.\n\nAre you absolutely sure you want to proceed?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-3, true);
  }
}

void DownloadListWidget::issueRemoveFromViewAll()
{
  if (QMessageBox::question(nullptr, tr("Hide Files?"),
                            tr("This will remove all finished downloads from this list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-1, false);
  }
}

void DownloadListWidget::issueRemoveFromViewCompleted()
{
  if (QMessageBox::question(nullptr, tr("Hide Files?"),
                            tr("This will remove all installed downloads from this list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-2, false);
  }
}

void DownloadListWidget::issueRemoveFromViewUninstalled()
{
  if (QMessageBox::question(nullptr, tr("Hide Files?"),
                            tr("This will remove all uninstalled downloads from this list (but NOT from disk)."),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit removeDownload(-3, false);
  }
}
