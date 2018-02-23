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

  enum FilterMode {
    FILTER_AND,
    FILTER_OR
  };

  enum FilterType {
    TYPE_SPECIAL,
    TYPE_CATEGORY,
    TYPE_CONTENT
  };

public:

  explicit ModListSortProxy(Profile *profile, QObject *parent = 0);

  void setProfile(Profile *profile);

  void setCategoryFilter(const std::vector<int> &categories);
  std::vector<int> categoryFilter() const { return m_CategoryFilter; }

  void setContentFilter(const std::vector<int> &content);

  virtual Qt::ItemFlags flags(const QModelIndex &modelIndex) const;
  virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                            int row, int column, const QModelIndex &parent);

  virtual void setSourceModel(QAbstractItemModel *sourceModel) override;

  /**
   * @brief enable all mods visible under the current filter
   **/
  void enableAllVisible();

  /**
   * @brief disable all mods visible under the current filter
   **/
  void disableAllVisible();

  /**
   * @brief tests if a filtere matches for a mod
   * @param info mod information
   * @param enabled true if the mod is currently active
   * @return true if current active filters match for the specified mod
   */
  bool filterMatchesMod(ModInfo::Ptr info, bool enabled) const;

  /**
   * @return true if a filter is currently active
   */
  bool isFilterActive() const { return m_FilterActive; }

  void setFilterMode(FilterMode mode);

  /**
   * @brief tests if the specified index has child nodes
   * @param parent the node to test
   * @return true if there are child nodes
   */
  virtual bool hasChildren ( const QModelIndex & parent = QModelIndex() ) const {
    return rowCount(parent) > 0;
  }

public slots:

  void updateFilter(const QString &filter);

signals:

  void filterActive(bool active);

protected:

  virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
  virtual bool filterAcceptsRow(int row, const QModelIndex &parent) const;

private:

  unsigned long flagsId(const std::vector<ModInfo::EFlag> &flags) const;
  bool hasConflictFlag(const std::vector<ModInfo::EFlag> &flags) const;
  void updateFilterActive();
  bool filterMatchesModAnd(ModInfo::Ptr info, bool enabled) const;
  bool filterMatchesModOr(ModInfo::Ptr info, bool enabled) const;

private slots:

  void aboutToChangeData();
  void postDataChanged();

private:

  Profile *m_Profile;

  std::vector<int> m_CategoryFilter;
  std::vector<int> m_ContentFilter;
  std::bitset<ModList::COL_LASTCOLUMN + 1> m_EnabledColumns;
  QString m_CurrentFilter;

  bool m_FilterActive;
  FilterMode m_FilterMode;

  std::vector<int> m_PreChangeFilters;

};

#endif // MODLISTSORTPROXY_H
