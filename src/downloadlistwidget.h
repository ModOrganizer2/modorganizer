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

#include <QWidget>
#include <QItemDelegate>
#include <QLabel>
#include <QProgressBar>
#include <QTreeView>

#include "downloadmanager.h"

namespace Ui {
    class DownloadListWidget;
}

class DownloadListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadListWidget(QWidget *parent = 0);
    ~DownloadListWidget();


private:
    Ui::DownloadListWidget *ui;
};

class DownloadManager;

class DownloadListWidgetDelegate : public QItemDelegate
{

  Q_OBJECT

public:

  DownloadListWidgetDelegate(DownloadManager *manager, bool metaDisplay, QTreeView *view, QObject *parent = 0);
  ~DownloadListWidgetDelegate();

  virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
  virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

  void paintPendingDownload(int downloadIndex) const;
  void paintRegularDownload(int downloadIndex) const;

signals:

  void installDownload(int index);
  void queryInfo(int index);
  void removeDownload(int index, bool deleteFile);
  void restoreDownload(int index);
  void cancelDownload(int index);
  void pauseDownload(int index);
  void resumeDownload(int index);

protected:

  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option, const QModelIndex &index);

private:

  void drawCache(QPainter *painter, const QStyleOptionViewItem &option, const QPixmap &cache) const;

private slots:

  void issueInstall();
  void issueDelete();
  void issueRemoveFromView();
  void issueRestoreToView();
  void issueCancel();
  void issuePause();
  void issueResume();
  void issueDeleteAll();
  void issueDeleteCompleted();
  void issueRemoveFromViewAll();
  void issueRemoveFromViewCompleted();
  void issueQueryInfo();

  void stateChanged(int row, DownloadManager::DownloadState);
  void resetCache(int);

private:

  DownloadListWidget *m_ItemWidget;
  DownloadManager *m_Manager;

  bool m_MetaDisplay;

  QLabel *m_NameLabel;
  QLabel *m_SizeLabel;
  QProgressBar *m_Progress;
  QLabel *m_InstallLabel;
  int m_ContextRow;

  QTreeView *m_View;

  mutable QMap<int, QPixmap> m_Cache;
};

#endif // DOWNLOADLISTWIDGET_H
