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

#ifndef DOWNLOADLISTWIDGET_H
#define DOWNLOADLISTWIDGET_H

#include "downloadlist.h"
#include "downloadmanager.h"
#include <QHeaderView>
#include <QItemDelegate>
#include <QLabel>
#include <QProgressBar>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QWidget>

namespace Ui
{
class DownloadListView;
}

class DownloadListView;

class DownloadProgressDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  DownloadProgressDelegate(DownloadManager* manager, DownloadListView* list,
                           DownloadList* sourceModel);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

private:
  DownloadManager* m_Manager;
  DownloadListView* m_List;
  DownloadList* m_sourceModel;
};

class DownloadListHeader : public QHeaderView
{
  Q_OBJECT

public:
  explicit DownloadListHeader(Qt::Orientation orientation, QWidget* parent = nullptr)
      : QHeaderView(orientation, parent)
  {}
  void customResizeSections();

private:
  void mouseReleaseEvent(QMouseEvent* event) override;
};

class DownloadListView : public QTreeView
{
  Q_OBJECT

public:
  explicit DownloadListView(QWidget* parent = 0);
  ~DownloadListView();

  void setManager(DownloadManager* manager);
  void setSourceModel(DownloadList* sourceModel);

signals:
  void installDownload(QUuid moId);
  void queryInfo(QUuid moId);
  void queryInfoMd5(QUuid moId);
  void removeDownload(QUuid moId, bool deleteFile, int flag);
  void restoreDownload(QUuid moId);
  void cancelDownload(QUuid moId);
  void pauseDownload(QUuid moId);
  void resumeDownload(QUuid moId);
  void visitOnNexus(QUuid moId);
  void openFile(QUuid moId);
  void openMetaFile(QUuid moId);
  void openInDownloadsFolder(QUuid moId);

protected:
  void keyPressEvent(QKeyEvent* event) override;

private slots:
  void onDoubleClick(const QModelIndex& index);
  void onCustomContextMenu(const QPoint& point);
  void onHeaderCustomContextMenu(const QPoint& point);

  void issueInstall(QUuid moId);
  void issueDelete(QUuid moId);
  void issueRemoveFromView(QUuid moId);
  void issueRestoreToView(QUuid moId);
  void issueRestoreToViewAll();
  void issueVisitOnNexus(QUuid moId);
  void issueOpenFile(QUuid moId);
  void issueOpenMetaFile(QUuid moId);
  void issueOpenInDownloadsFolder(QUuid moId);
  void issueCancel(QUuid moId);
  void issuePause(QUuid moId);
  void issueResume(QUuid moId);
  void issueDeleteAll();
  void issueDeleteCompleted();
  void issueDeleteUninstalled();
  void issueRemoveFromViewAll();
  void issueRemoveFromViewCompleted();
  void issueRemoveFromViewUninstalled();
  void issueQueryInfo(QUuid moId);
  void issueQueryInfoMd5(QUuid moId);

private:
  DownloadManager* m_Manager;
  DownloadList* m_SourceModel{nullptr};

  void resizeEvent(QResizeEvent* event);
};

#endif  // DOWNLOADLISTWIDGET_H
