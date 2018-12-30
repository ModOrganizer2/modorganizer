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

#include "downloadmanager.h"
#include <QWidget>
#include <QItemDelegate>
#include <QLabel>
#include <QProgressBar>
#include <QTreeView>

namespace Ui {
  class DownloadListWidget;
}

class DownloadManager;

class DownloadListWidget : public QTreeView
{
  Q_OBJECT

public:
  explicit DownloadListWidget(QWidget *parent = 0);
  ~DownloadListWidget();

  void setManager(DownloadManager *manager);

signals:
  void installDownload(int index);
  void queryInfo(int index);
  void removeDownload(int index, bool deleteFile);
  void restoreDownload(int index);
  void cancelDownload(int index);
  void pauseDownload(int index);
  void resumeDownload(int index);
  void visitOnNexus(int index);
  void openFile(int index);
  void openInDownloadsFolder(int index);

private slots:
  void onDoubleClick(const QModelIndex &index);
  void onCustomContextMenu(const QPoint &point);
  void issueInstall();
  void issueDelete();
  void issueRemoveFromView();
  void issueRestoreToView();
  void issueRestoreToViewAll();
  void issueVisitOnNexus();
  void issueOpenFile();
  void issueOpenInDownloadsFolder();
  void issueCancel();
  void issuePause();
  void issueResume();
  void issueDeleteAll();
  void issueDeleteCompleted();
  void issueDeleteUninstalled();
  void issueRemoveFromViewAll();
  void issueRemoveFromViewCompleted();
  void issueRemoveFromViewUninstalled();
  void issueQueryInfo();

private:
  DownloadManager *m_Manager;
  int m_ContextRow;
};

#endif // DOWNLOADLISTWIDGET_H
