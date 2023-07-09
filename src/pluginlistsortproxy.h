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

#ifndef PLUGINLISTSORTPROXY_H
#define PLUGINLISTSORTPROXY_H

#include "pluginlist.h"
#include <QSortFilterProxyModel>
#include <bitset>

class PluginListSortProxy : public QSortFilterProxyModel
{
  Q_OBJECT
public:
  enum ESorting
  {
    SORT_ASCENDING,
    SORT_DESCENDING
  };

public:
  explicit PluginListSortProxy(QObject* parent = 0);

  void setEnabledColumns(unsigned int columns);

  virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                            int column, const QModelIndex& parent);

  bool filterMatchesPlugin(const QString& plugin) const;

public slots:

  void updateFilter(const QString& filter);

protected:
  virtual bool filterAcceptsRow(int row, const QModelIndex& parent) const;
  virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const;

private:
  int m_SortIndex;
  Qt::SortOrder m_SortOrder;

  std::bitset<PluginList::COL_LASTCOLUMN + 1> m_EnabledColumns;
  QString m_CurrentFilter;
};

#endif  // PLUGINLISTSORTPROXY_H
