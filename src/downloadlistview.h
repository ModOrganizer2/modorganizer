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
  DownloadProgressDelegate(DownloadManager* manager, DownloadListView* list, DownloadList* sourceModel);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

private:
  DownloadManager* m_Manager{nullptr};
  DownloadListView* m_List{nullptr};
  DownloadList* m_sourceModel{nullptr};
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
  void installDownload(const QString& fileName);
  void queryInfo(const QString& fileName);
  void queryInfoMd5(const QString& fileName);
  void removeDownload(const QString& fileName, bool deleteFile, int flag);
  void restoreDownload(const QString& fileName);
  void cancelDownload(const QString& fileName);
  void pauseDownload(const QString& fileName);
  void resumeDownload(const QString& fileName);
  void visitOnNexus(const QString& fileName);
  void openFile(const QString& fileName);
  void openMetaFile(const QString& fileName);
  void openInDownloadsFolder(const QString& fileName);

protected:
  void keyPressEvent(QKeyEvent* event) override;

private slots:
  void onDoubleClick(const QModelIndex& index);
  void onCustomContextMenu(const QPoint& point);
  void onHeaderCustomContextMenu(const QPoint& point);

  void issueInstall(const QString& fileName);
  void issueDelete(const QString& fileName);
  void issueRemoveFromView(const QString& fileName);
  void issueRestoreToView(const QString& fileName);
  void issueRestoreToViewAll();
  void issueVisitOnNexus(const QString& fileName);
  void issueOpenFile(const QString& fileName);
  void issueOpenMetaFile(const QString& fileName);
  void issueOpenInDownloadsFolder(const QString& fileName);
  void issueCancel(const QString& fileName);
  void issuePause(const QString& fileName);
  void issueResume(const QString& fileName);
  void issueDeleteAll();
  void issueDeleteCompleted();
  void issueDeleteUninstalled();
  void issueRemoveFromViewAll();
  void issueRemoveFromViewCompleted();
  void issueRemoveFromViewUninstalled();
  void issueQueryInfo(const QString& fileName);
  void issueQueryInfoMd5(const QString& fileName);

private:
  DownloadManager* m_Manager{nullptr};
  DownloadList* m_SourceModel{nullptr};

  void resizeEvent(QResizeEvent* event);
};

#endif  // DOWNLOADLISTWIDGET_H
