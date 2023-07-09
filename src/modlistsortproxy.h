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

#include "modlist.h"
#include <QSortFilterProxyModel>
#include <bitset>

class Profile;
class OrganizerCore;

class ModListSortProxy : public QSortFilterProxyModel
{
  Q_OBJECT

public:
  enum FilterMode
  {
    FilterAnd,
    FilterOr
  };

  enum CriteriaType
  {
    TypeSpecial,
    TypeCategory,
    TypeContent
  };

  enum SeparatorsMode
  {
    SeparatorFilter,
    SeparatorShow,
    SeparatorHide
  };

  struct Criteria
  {
    CriteriaType type;
    int id;
    bool inverse;

    bool operator==(const Criteria& other) const
    {
      return (type == other.type) && (id == other.id) && (inverse == other.inverse);
    }

    bool operator!=(const Criteria& other) const { return !(*this == other); }
  };

public:
  explicit ModListSortProxy(Profile* profile, OrganizerCore* organizer);

  void setProfile(Profile* profile);

  bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                       int column, const QModelIndex& parent) const override;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                    const QModelIndex& parent) override;

  virtual void setSourceModel(QAbstractItemModel* sourceModel) override;

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

  void setCriteria(const std::vector<Criteria>& criteria);
  void setOptions(FilterMode mode, SeparatorsMode separators);

  auto filterMode() const { return m_FilterMode; }
  auto separatorsMode() const { return m_FilterSeparators; }

  /**
   * @brief tests if the specified index has child nodes
   * @param parent the node to test
   * @return true if there are child nodes
   */
  virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const
  {
    return rowCount(parent) > 0;
  }

  /**
   * @brief sets whether a column is visible
   * @param column the index of the column
   * @param visible the visibility of the column
   */
  void setColumnVisible(int column, bool visible);

public slots:

  void updateFilter(const QString& filter);

signals:

  void filterActive(bool active);
  void filterInvalidated();

protected:
  virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const;
  virtual bool filterAcceptsRow(int row, const QModelIndex& parent) const;

private:
  unsigned long flagsId(const std::vector<ModInfo::EFlag>& flags) const;
  unsigned long conflictFlagsId(const std::vector<ModInfo::EConflictFlag>& flags) const;
  bool hasConflictFlag(const std::vector<ModInfo::EConflictFlag>& flags) const;
  void updateFilterActive();
  bool filterMatchesModAnd(ModInfo::Ptr info, bool enabled) const;
  bool filterMatchesModOr(ModInfo::Ptr info, bool enabled) const;

  // check if the source model is the by-priority proxy
  //
  bool sourceIsByPriorityProxy() const;

private slots:

  void aboutToChangeData();
  void postDataChanged();

private:
  OrganizerCore* m_Organizer;

  Profile* m_Profile;
  std::vector<Criteria> m_Criteria;
  QString m_Filter;
  std::bitset<ModList::COL_LASTCOLUMN + 1> m_EnabledColumns;

  bool m_FilterActive;
  FilterMode m_FilterMode;
  SeparatorsMode m_FilterSeparators;

  std::vector<Criteria> m_PreChangeCriteria;

  bool optionsMatchMod(ModInfo::Ptr info, bool enabled) const;
  bool criteriaMatchMod(ModInfo::Ptr info, bool enabled, const Criteria& c) const;
  bool categoryMatchesMod(ModInfo::Ptr info, bool enabled, int category) const;
  bool contentMatchesMod(ModInfo::Ptr info, bool enabled, int content) const;
};

#endif  // MODLISTSORTPROXY_H
