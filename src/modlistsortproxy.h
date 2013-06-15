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

#ifndef MODLISTSORTPROXY_H
#define MODLISTSORTPROXY_H

#include <QSortFilterProxyModel>
#include <bitset>
#include "modlist.h"

class Profile;

class ModListSortProxy : public QSortFilterProxyModel
{
  Q_OBJECT

public:

  explicit ModListSortProxy(Profile *profile, QObject *parent = 0);

  void setProfile(Profile *profile);

  void setCategoryFilter(const std::vector<int> &categories);

  virtual Qt::ItemFlags flags(const QModelIndex &modelIndex) const;
  virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                            int row, int column, const QModelIndex &parent);

  /**
   * @brief enable all mods visible under the current filter
   **/
  void enableAllVisible();

  /**
   * @brief disable all mods visible under the current filter
   **/
  void disableAllVisible();

  bool filterMatches(ModInfo::Ptr info, bool enabled) const;

public slots:

  void displayColumnSelection(const QPoint &pos);
  void updateFilter(const QString &filter);

signals:

  void filterActive(bool active);

protected:

  virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
  virtual bool filterAcceptsRow(int row, const QModelIndex &parent) const;

private:

  bool hasConflictFlag(const std::vector<ModInfo::EFlag> &flags) const;
  void updateFilterActive();

private:

  Profile *m_Profile;

  std::vector<int> m_CategoryFilter;
  std::bitset<ModList::COL_LASTCOLUMN + 1> m_EnabledColumns;
  QString m_CurrentFilter;

};

#endif // MODLISTSORTPROXY_H
