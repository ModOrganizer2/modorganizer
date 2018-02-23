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

#ifndef DOWNLOADLISTSORTPROXY_H
#define DOWNLOADLISTSORTPROXY_H


#include <QSortFilterProxyModel>


class DownloadManager;


class DownloadListSortProxy : public QSortFilterProxyModel
{
  Q_OBJECT
public:

  explicit DownloadListSortProxy(const DownloadManager *manager, QObject *parent = 0);

public slots:

  void updateFilter(const QString &filter);

protected:

  bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
  bool filterAcceptsRow(int sourceRow, const QModelIndex &source_parent) const;

signals:
  
public slots:

private:

  const DownloadManager *m_Manager;
  QString m_CurrentFilter;


};

#endif // DOWNLOADLISTSORTPROXY_H
