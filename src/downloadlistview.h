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
  DownloadProgressDelegate(DownloadManager* manager, DownloadListView* list);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

private:
  DownloadManager* m_Manager;
  DownloadListView* m_List;
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
  void installDownload(int index);
  void queryInfo(int index);
  void queryInfoMd5(int index);
  void removeDownload(int index, bool deleteFile);
  void restoreDownload(int index);
  void cancelDownload(int index);
  void pauseDownload(int index);
  void resumeDownload(int index);
  void visitOnNexus(int index);
  void openFile(int index);
  void openMetaFile(int index);
  void openInDownloadsFolder(int index);

protected:
  void keyPressEvent(QKeyEvent* event) override;

private slots:
  void onDoubleClick(const QModelIndex& index);
  void onCustomContextMenu(const QPoint& point);
  void onHeaderCustomContextMenu(const QPoint& point);

  void issueInstall(int index);
  void issueDelete(int index);
  void issueRemoveFromView(int index);
  void issueRestoreToView(int index);
  void issueRestoreToViewAll();
  void issueVisitOnNexus(int index);
  void issueOpenFile(int index);
  void issueOpenMetaFile(int index);
  void issueOpenInDownloadsFolder(int index);
  void issueCancel(int index);
  void issuePause(int index);
  void issueResume(int index);
  void issueDeleteAll();
  void issueDeleteCompleted();
  void issueDeleteUninstalled();
  void issueRemoveFromViewAll();
  void issueRemoveFromViewCompleted();
  void issueRemoveFromViewUninstalled();
  void issueQueryInfo(int index);
  void issueQueryInfoMd5(int index);

private:
  DownloadManager* m_Manager;
  DownloadList* m_SourceModel = 0;

  void resizeEvent(QResizeEvent* event);
};

#endif  // DOWNLOADLISTWIDGET_H
