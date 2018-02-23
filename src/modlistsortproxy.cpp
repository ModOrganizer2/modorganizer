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

#include "modlistsortproxy.h"
#include "modinfo.h"
#include "profile.h"
#include "messagedialog.h"
#include "qtgroupingproxy.h"
#include <QMenu>
#include <QCheckBox>
#include <QWidgetAction>
#include <QApplication>
#include <QMimeData>
#include <QDebug>
#include <QTreeView>


ModListSortProxy::ModListSortProxy(Profile* profile, QObject *parent)
  : QSortFilterProxyModel(parent)
  , m_Profile(profile)
  , m_CategoryFilter()
  , m_CurrentFilter()
  , m_FilterActive(false)
  , m_FilterMode(FILTER_AND)
{
  m_EnabledColumns.set(ModList::COL_FLAGS);
  m_EnabledColumns.set(ModList::COL_NAME);
  m_EnabledColumns.set(ModList::COL_VERSION);
  m_EnabledColumns.set(ModList::COL_PRIORITY);
  setDynamicSortFilter(true); // this seems to work without dynamicsortfilter
                              // but I don't know why. This should be necessary
}

void ModListSortProxy::setProfile(Profile *profile)
{
  m_Profile = profile;
}

void ModListSortProxy::updateFilterActive()
{
  m_FilterActive = ((m_CategoryFilter.size() > 0)
                    || (m_ContentFilter.size() > 0)
                    || !m_CurrentFilter.isEmpty());
  emit filterActive(m_FilterActive);
}

void ModListSortProxy::setCategoryFilter(const std::vector<int> &categories)
{
  m_CategoryFilter = categories;
  updateFilterActive();
  invalidate();
}

void ModListSortProxy::setContentFilter(const std::vector<int> &content)
{
  m_ContentFilter = content;
  updateFilterActive();
  invalidate();
}

Qt::ItemFlags ModListSortProxy::flags(const QModelIndex &modelIndex) const
{
  Qt::ItemFlags flags = sourceModel()->flags(mapToSource(modelIndex));

  return flags;
}

void ModListSortProxy::enableAllVisible()
{
  if (m_Profile == nullptr) return;

  for (int i = 0; i < this->rowCount(); ++i) {
    int modID = mapToSource(index(i, 0)).data(Qt::UserRole + 1).toInt();
    m_Profile->setModEnabled(modID, true);
  }
  invalidate();
}

void ModListSortProxy::disableAllVisible()
{
  if (m_Profile == nullptr) return;

  for (int i = 0; i < this->rowCount(); ++i) {
    int modID = mapToSource(index(i, 0)).data(Qt::UserRole + 1).toInt();
    m_Profile->setModEnabled(modID, false);
  }
  invalidate();
}

unsigned long ModListSortProxy::flagsId(const std::vector<ModInfo::EFlag> &flags) const
{
  unsigned long result = 0;
  for (ModInfo::EFlag flag : flags) {
    if ((flag != ModInfo::FLAG_FOREIGN)
        && (flag != ModInfo::FLAG_OVERWRITE)) {
      result += 1 << (int)flag;
    }
  }
  return result;
}

bool ModListSortProxy::lessThan(const QModelIndex &left,
                                const QModelIndex &right) const
{
  if (sourceModel()->hasChildren(left) || sourceModel()->hasChildren(right)) {
    return QSortFilterProxyModel::lessThan(left, right);
  }

  bool lOk, rOk;
  int leftIndex  = left.data(Qt::UserRole + 1).toInt(&lOk);
  int rightIndex = right.data(Qt::UserRole + 1).toInt(&rOk);
  if (!lOk || !rOk) {
    return false;
  }

  ModInfo::Ptr leftMod = ModInfo::getByIndex(leftIndex);
  ModInfo::Ptr rightMod = ModInfo::getByIndex(rightIndex);

  bool lt = false;

  {
    QModelIndex leftPrioIdx = left.sibling(left.row(), ModList::COL_PRIORITY);
    QVariant leftPrio = leftPrioIdx.data();
    if (!leftPrio.isValid()) leftPrio = left.data(Qt::UserRole);
    QModelIndex rightPrioIdx = right.sibling(right.row(), ModList::COL_PRIORITY);
    QVariant rightPrio = rightPrioIdx.data();
    if (!rightPrio.isValid()) rightPrio = right.data(Qt::UserRole);

    lt = leftPrio.toInt() < rightPrio.toInt();
  }

  switch (left.column()) {
    case ModList::COL_FLAGS: {
      std::vector<ModInfo::EFlag> leftFlags = leftMod->getFlags();
      std::vector<ModInfo::EFlag> rightFlags = rightMod->getFlags();
      if (leftFlags.size() != rightFlags.size()) {
        lt = leftFlags.size() < rightFlags.size();
      } else {
        lt = flagsId(leftFlags) < flagsId(rightFlags);
      }
    } break;
    case ModList::COL_CONTENT: {
      std::vector<ModInfo::EContent> lContent = leftMod->getContents();
      std::vector<ModInfo::EContent> rContent = rightMod->getContents();
      if (lContent.size() != rContent.size()) {
        lt = lContent.size() < rContent.size();
      }

      int lValue = 0;
      int rValue = 0;
      for (ModInfo::EContent content : lContent) {
        lValue += 2 << (unsigned int)content;
      }
      for (ModInfo::EContent content : rContent) {
        rValue += 2 << (unsigned int)content;
      }

      lt = lValue < rValue;
    } break;
    case ModList::COL_NAME: {
      int comp = QString::compare(leftMod->name(), rightMod->name(), Qt::CaseInsensitive);
      if (comp != 0)
        lt = comp < 0;
    } break;
    case ModList::COL_CATEGORY: {
      if (leftMod->getPrimaryCategory() != rightMod->getPrimaryCategory()) {
        if (leftMod->getPrimaryCategory() < 0) lt = false;
        else if (rightMod->getPrimaryCategory() < 0) lt = true;
        else {
          try {
            CategoryFactory &categories = CategoryFactory::instance();
            QString leftCatName = categories.getCategoryName(categories.getCategoryIndex(leftMod->getPrimaryCategory()));
            QString rightCatName = categories.getCategoryName(categories.getCategoryIndex(rightMod->getPrimaryCategory()));
            lt = leftCatName < rightCatName;
          } catch (const std::exception &e) {
            qCritical("failed to compare categories: %s", e.what());
          }
        }
      }
    } break;
    case ModList::COL_MODID: {
      if (leftMod->getNexusID() != rightMod->getNexusID())
        lt = leftMod->getNexusID() < rightMod->getNexusID();
    } break;
    case ModList::COL_VERSION: {
      if (leftMod->getVersion() != rightMod->getVersion())
        lt = leftMod->getVersion() < rightMod->getVersion();
    } break;
    case ModList::COL_INSTALLTIME: {
      QDateTime leftTime = left.data().toDateTime();
      QDateTime rightTime = right.data().toDateTime();
      if (leftTime != rightTime)
        return leftTime < rightTime;
    } break;
    case ModList::COL_PRIORITY: {
      // nop, already compared by priority
    } break;
  }
  return lt;
}

void ModListSortProxy::updateFilter(const QString &filter)
{
  m_CurrentFilter = filter;
  updateFilterActive();
  // using invalidateFilter here should be enough but that crashes the application? WTF?
  // invalidateFilter();
  invalidate();
}

bool ModListSortProxy::hasConflictFlag(const std::vector<ModInfo::EFlag> &flags) const
{
  for (ModInfo::EFlag flag : flags) {
    if ((flag == ModInfo::FLAG_CONFLICT_MIXED) ||
        (flag == ModInfo::FLAG_CONFLICT_OVERWRITE) ||
        (flag == ModInfo::FLAG_CONFLICT_OVERWRITTEN) ||
        (flag == ModInfo::FLAG_CONFLICT_REDUNDANT)) {
      return true;
    }
  }

  return false;
}

bool ModListSortProxy::filterMatchesModAnd(ModInfo::Ptr info, bool enabled) const
{
  for (auto iter = m_CategoryFilter.begin(); iter != m_CategoryFilter.end(); ++iter) {
    switch (*iter) {
      case CategoryFactory::CATEGORY_SPECIAL_CHECKED: {
        if (!enabled && !info->alwaysEnabled()) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UNCHECKED: {
        if (enabled || info->alwaysEnabled()) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE: {
        if (!info->updateAvailable() && !info->downgradeAvailable()) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_NOCATEGORY: {
        if (info->getCategories().size() > 0) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_CONFLICT: {
        if (!hasConflictFlag(info->getFlags())) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_NOTENDORSED: {
        ModInfo::EEndorsedState state = info->endorsedState();
        if (state != ModInfo::ENDORSED_FALSE) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_MANAGED: {
        if (info->hasFlag(ModInfo::FLAG_FOREIGN)) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UNMANAGED: {
        if (!info->hasFlag(ModInfo::FLAG_FOREIGN)) return false;
      } break;
      default: {
        if (!info->categorySet(*iter)) return false;
      } break;
    }
  }

  foreach (int content, m_ContentFilter) {
    if (!info->hasContent(static_cast<ModInfo::EContent>(content))) return false;
  }

  return true;
}

bool ModListSortProxy::filterMatchesModOr(ModInfo::Ptr info, bool enabled) const
{
  for (auto iter = m_CategoryFilter.begin(); iter != m_CategoryFilter.end(); ++iter) {
    switch (*iter) {
      case CategoryFactory::CATEGORY_SPECIAL_CHECKED: {
        if (enabled || info->alwaysEnabled()) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UNCHECKED: {
        if (!enabled && !info->alwaysEnabled()) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE: {
        if (info->updateAvailable() || info->downgradeAvailable()) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_NOCATEGORY: {
        if (info->getCategories().size() == 0) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_CONFLICT: {
        if (hasConflictFlag(info->getFlags())) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_NOTENDORSED: {
        ModInfo::EEndorsedState state = info->endorsedState();
        if ((state == ModInfo::ENDORSED_FALSE) || (state == ModInfo::ENDORSED_NEVER)) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_MANAGED: {
        if (!info->hasFlag(ModInfo::FLAG_FOREIGN)) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UNMANAGED: {
        if (info->hasFlag(ModInfo::FLAG_FOREIGN)) return true;
      } break;
      default: {
        if (info->categorySet(*iter)) return true;
      } break;
    }
  }

  foreach (int content, m_ContentFilter) {
    if (info->hasContent(static_cast<ModInfo::EContent>(content))) return true;
  }

  return false;
}

bool ModListSortProxy::filterMatchesMod(ModInfo::Ptr info, bool enabled) const
{
  if (!m_CurrentFilter.isEmpty() &&
      !info->name().contains(m_CurrentFilter, Qt::CaseInsensitive)) {
    return false;
  }

  if (m_FilterMode == FILTER_AND) {
    return filterMatchesModAnd(info, enabled);
  } else {
    return filterMatchesModOr(info, enabled);
  }
}

void ModListSortProxy::setFilterMode(ModListSortProxy::FilterMode mode)
{
  if (m_FilterMode != mode) {
    m_FilterMode = mode;
    this->invalidate();
  }
}

bool ModListSortProxy::filterAcceptsRow(int row, const QModelIndex &parent) const
{
  if (m_Profile == nullptr) {
    return false;
  }

  if (row >= static_cast<int>(m_Profile->numMods())) {
    qWarning("invalid row idx %d", row);
    return false;
  }

  QModelIndex idx = sourceModel()->index(row, 0, parent);
  if (!idx.isValid()) {
    qDebug("invalid index");
    return false;
  }
  if (sourceModel()->hasChildren(idx)) {
    for (int i = 0; i < sourceModel()->rowCount(idx); ++i) {
      if (filterAcceptsRow(i, idx)) {
        return true;
      }
    }

    return false;
  } else {
    bool modEnabled = idx.sibling(row, 0).data(Qt::CheckStateRole).toInt() == Qt::Checked;
    unsigned int index = idx.data(Qt::UserRole + 1).toInt();
    return filterMatchesMod(ModInfo::getByIndex(index), modEnabled);
  }
}

bool ModListSortProxy::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                    int row, int column, const QModelIndex &parent)
{
  if (!data->hasUrls() && (sortColumn() != ModList::COL_PRIORITY)) {
    QWidget *wid = qApp->activeWindow()->findChild<QTreeView*>("modList");
    MessageDialog::showMessage(tr("Drag&Drop is only supported when sorting by priority"), wid);
    return false;
  }
  if ((row == -1) && (column == -1)) {
    return sourceModel()->dropMimeData(data, action, -1, -1, mapToSource(parent));
  }
  // in the regular model, when dropping between rows, the row-value passed to
  // the sourceModel is inconsistent between ascending and descending ordering.
  // This should fix that
  if (sortOrder() == Qt::DescendingOrder) {
    --row;
  }

  QModelIndex proxyIndex = index(row, column, parent);
  QModelIndex sourceIndex = mapToSource(proxyIndex);
  return this->sourceModel()->dropMimeData(data, action, sourceIndex.row(), sourceIndex.column(),
                                           sourceIndex.parent());
}

void ModListSortProxy::setSourceModel(QAbstractItemModel *sourceModel)
{
  QSortFilterProxyModel::setSourceModel(sourceModel);
  QtGroupingProxy *proxy = qobject_cast<QtGroupingProxy*>(sourceModel);
  if (proxy != nullptr) {
    sourceModel = proxy->sourceModel();
  }
  connect(sourceModel, SIGNAL(aboutToChangeData()), this, SLOT(aboutToChangeData()), Qt::UniqueConnection);
  connect(sourceModel, SIGNAL(postDataChanged()), this, SLOT(postDataChanged()), Qt::UniqueConnection);
}

void ModListSortProxy::aboutToChangeData()
{
  // having a filter active when dataChanged is called caused a crash
  // (at least with some Qt versions)
  // this may be related to the fact that the item being edited may disappear from the view as a
  // result of the edit
  m_PreChangeFilters = categoryFilter();
  setCategoryFilter(std::vector<int>());
}

void ModListSortProxy::postDataChanged()
{
  // if the filter is re-activated right away the editor can't be deleted but becomes invisible
  // or at least the view continues to think it's being edited. As a result no new editor can be
  // opened
  QTimer::singleShot(10, [this] () {
    setCategoryFilter(m_PreChangeFilters);
    m_PreChangeFilters.clear();
  });
}

