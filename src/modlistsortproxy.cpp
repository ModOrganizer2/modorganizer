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
#include "messagedialog.h"
#include "modinfo.h"
#include "modlistbypriorityproxy.h"
#include "modlistdropinfo.h"
#include "organizercore.h"
#include "profile.h"
#include "qtgroupingproxy.h"

#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QMenu>
#include <QMimeData>
#include <QTreeView>
#include <QWidgetAction>
#include <log.h>

using namespace MOBase;

ModListSortProxy::ModListSortProxy(Profile* profile, OrganizerCore* organizer)
    : QSortFilterProxyModel(organizer), m_Organizer(organizer), m_Profile(profile),
      m_FilterActive(false), m_FilterMode(FilterAnd),
      m_FilterSeparators(SeparatorFilter)
{
  setDynamicSortFilter(true);  // this seems to work without dynamicsortfilter
                               // but I don't know why. This should be necessary
}

void ModListSortProxy::setProfile(Profile* profile)
{
  m_Profile = profile;
}

void ModListSortProxy::updateFilterActive()
{
  m_FilterActive = (!m_Criteria.empty() || !m_Filter.isEmpty());
  emit filterActive(m_FilterActive);
}

void ModListSortProxy::setCriteria(const std::vector<Criteria>& criteria)
{
  // avoid refreshing the filter unless we are checking all mods for update.
  const bool changed = (criteria != m_Criteria);
  const bool isForUpdates =
      (!criteria.empty() && criteria[0].id == CategoryFactory::UpdateAvailable);

  if (changed || isForUpdates) {
    m_Criteria = criteria;
    updateFilterActive();
    invalidateFilter();
    emit filterInvalidated();
  }
}

unsigned long ModListSortProxy::flagsId(const std::vector<ModInfo::EFlag>& flags) const
{
  unsigned long result = 0;
  for (ModInfo::EFlag flag : flags) {
    if ((flag != ModInfo::FLAG_FOREIGN) && (flag != ModInfo::FLAG_OVERWRITE)) {
      result += 1 << (int)flag;
    }
  }
  return result;
}

unsigned long ModListSortProxy::conflictFlagsId(
    const std::vector<ModInfo::EConflictFlag>& flags) const
{
  unsigned long result = 0;
  for (ModInfo::EConflictFlag flag : flags) {
    if ((flag != ModInfo::FLAG_OVERWRITE_CONFLICT)) {
      result += 1 << (int)flag;
    }
  }
  return result;
}

bool ModListSortProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
  if (sourceModel()->hasChildren(left) || sourceModel()->hasChildren(right)) {
    // when sorting by priority, we do not want to use the parent lessThan because
    // it uses the display role which can be inconsistent (e.g. for backups)
    if (sortColumn() != ModList::COL_PRIORITY) {
      return QSortFilterProxyModel::lessThan(left, right);
    } else if (qobject_cast<QtGroupingProxy*>(sourceModel())) {
      // if the underlying proxy is a QtGroupingProxy we need to rely on
      // Qt::DisplayRole because the other roles are not correctly handled
      // by that kind of proxy
      return left.data(Qt::DisplayRole).toInt() < right.data(Qt::DisplayRole).toInt();
    }
  }

  bool lOk, rOk;
  int leftIndex  = left.data(ModList::IndexRole).toInt(&lOk);
  int rightIndex = right.data(ModList::IndexRole).toInt(&rOk);

  if (!lOk || !rOk) {
    return false;
  }

  ModInfo::Ptr leftMod  = ModInfo::getByIndex(leftIndex);
  ModInfo::Ptr rightMod = ModInfo::getByIndex(rightIndex);

  bool lt = left.data(ModList::PriorityRole).toInt() <
            right.data(ModList::PriorityRole).toInt();

  switch (left.column()) {
  case ModList::COL_FLAGS: {
    std::vector<ModInfo::EFlag> leftFlags  = leftMod->getFlags();
    std::vector<ModInfo::EFlag> rightFlags = rightMod->getFlags();
    if (leftFlags.size() != rightFlags.size()) {
      lt = leftFlags.size() < rightFlags.size();
    } else {
      lt = flagsId(leftFlags) < flagsId(rightFlags);
    }
  } break;
  case ModList::COL_CONFLICTFLAGS: {
    std::vector<ModInfo::EConflictFlag> leftFlags  = leftMod->getConflictFlags();
    std::vector<ModInfo::EConflictFlag> rightFlags = rightMod->getConflictFlags();
    if (leftFlags.size() != rightFlags.size()) {
      lt = leftFlags.size() < rightFlags.size();
    } else {
      lt = conflictFlagsId(leftFlags) < conflictFlagsId(rightFlags);
    }
  } break;
  case ModList::COL_CONTENT: {
    const auto& lContents = leftMod->getContents();
    const auto& rContents = rightMod->getContents();
    unsigned int lValue   = 0;
    unsigned int rValue   = 0;
    m_Organizer->modDataContents().forEachContentIn(
        lContents, [&lValue](auto const& content) {
          lValue += 2U << static_cast<unsigned int>(content.id());
        });
    m_Organizer->modDataContents().forEachContentIn(
        rContents, [&rValue](auto const& content) {
          rValue += 2U << static_cast<unsigned int>(content.id());
        });
    lt = lValue < rValue;
  } break;
  case ModList::COL_NAME: {
    int comp = QString::compare(leftMod->name(), rightMod->name(), Qt::CaseInsensitive);
    if (comp != 0)
      lt = comp < 0;
  } break;
  case ModList::COL_CATEGORY: {
    if (leftMod->primaryCategory() != rightMod->primaryCategory()) {
      if (leftMod->primaryCategory() < 0)
        lt = false;
      else if (rightMod->primaryCategory() < 0)
        lt = true;
      else {
        try {
          CategoryFactory& categories = CategoryFactory::instance();
          QString leftCatName         = categories.getCategoryName(
              categories.getCategoryIndex(leftMod->primaryCategory()));
          QString rightCatName = categories.getCategoryName(
              categories.getCategoryIndex(rightMod->primaryCategory()));
          lt = leftCatName < rightCatName;
        } catch (const std::exception& e) {
          log::error("failed to compare categories: {}", e.what());
        }
      }
    }
  } break;
  case ModList::COL_MODID: {
    if (leftMod->nexusId() != rightMod->nexusId())
      lt = leftMod->nexusId() < rightMod->nexusId();
  } break;
  case ModList::COL_VERSION: {
    if (leftMod->version() != rightMod->version())
      lt = leftMod->version() < rightMod->version();
  } break;
  case ModList::COL_INSTALLTIME: {
    QDateTime leftTime  = left.data().toDateTime();
    QDateTime rightTime = right.data().toDateTime();
    if (leftTime != rightTime)
      return leftTime < rightTime;
  } break;
  case ModList::COL_GAME: {
    if (leftMod->gameName() != rightMod->gameName()) {
      lt = leftMod->gameName() < rightMod->gameName();
    } else {
      int comp =
          QString::compare(leftMod->name(), rightMod->name(), Qt::CaseInsensitive);
      if (comp != 0)
        lt = comp < 0;
    }
  } break;
  case ModList::COL_NOTES: {
    QString leftComments  = leftMod->comments();
    QString rightComments = rightMod->comments();
    if (leftComments != rightComments) {
      if (leftComments.isEmpty()) {
        lt = sortOrder() == Qt::DescendingOrder;
      } else if (rightComments.isEmpty()) {
        lt = sortOrder() == Qt::AscendingOrder;
      } else {
        lt = leftComments < rightComments;
      }
    }
  } break;
  case ModList::COL_PRIORITY: {
    if (leftMod->isBackup() != rightMod->isBackup()) {
      lt = leftMod->isBackup();
    } else if (leftMod->isOverwrite() != rightMod->isOverwrite()) {
      lt = rightMod->isOverwrite();
    }
  } break;
  default: {
    log::warn("Sorting is not defined for column {}", left.column());
  } break;
  }
  return lt;
}

void ModListSortProxy::updateFilter(const QString& filter)
{
  m_Filter = filter;
  updateFilterActive();
  invalidateFilter();
  emit filterInvalidated();
}

bool ModListSortProxy::hasConflictFlag(
    const std::vector<ModInfo::EConflictFlag>& flags) const
{
  for (ModInfo::EConflictFlag flag : flags) {
    if ((flag == ModInfo::FLAG_CONFLICT_MIXED) ||
        (flag == ModInfo::FLAG_CONFLICT_OVERWRITE) ||
        (flag == ModInfo::FLAG_CONFLICT_OVERWRITTEN) ||
        (flag == ModInfo::FLAG_CONFLICT_REDUNDANT) ||
        (flag == ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITE) ||
        (flag == ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITTEN) ||
        (flag == ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED) ||
        (flag == ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE) ||
        (flag == ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN)) {
      return true;
    }
  }

  return false;
}

bool ModListSortProxy::filterMatchesModAnd(ModInfo::Ptr info, bool enabled) const
{
  for (auto&& c : m_Criteria) {
    if (!criteriaMatchMod(info, enabled, c)) {
      return false;
    }
  }

  return true;
}

bool ModListSortProxy::filterMatchesModOr(ModInfo::Ptr info, bool enabled) const
{
  for (auto&& c : m_Criteria) {
    if (criteriaMatchMod(info, enabled, c)) {
      return true;
    }
  }

  if (!m_Criteria.empty()) {
    // nothing matched
    return false;
  }

  return true;
}

bool ModListSortProxy::optionsMatchMod(ModInfo::Ptr info, bool) const
{
  return true;
}

bool ModListSortProxy::criteriaMatchMod(ModInfo::Ptr info, bool enabled,
                                        const Criteria& c) const
{
  bool b = false;

  switch (c.type) {
  case TypeSpecial:  // fall-through
  case TypeCategory: {
    b = categoryMatchesMod(info, enabled, c.id);
    break;
  }

  case TypeContent: {
    b = contentMatchesMod(info, enabled, c.id);
    break;
  }

  default: {
    log::error("bad criteria type {}", c.type);
    break;
  }
  }

  if (c.inverse) {
    b = !b;
  }

  return b;
}

bool ModListSortProxy::categoryMatchesMod(ModInfo::Ptr info, bool enabled,
                                          int category) const
{
  bool b = false;

  switch (category) {
  case CategoryFactory::Checked: {
    b = (enabled || info->alwaysEnabled());
    break;
  }

  case CategoryFactory::UpdateAvailable: {
    b = (info->updateAvailable() || info->downgradeAvailable());
    break;
  }

  case CategoryFactory::HasCategory: {
    b = !info->getCategories().empty();
    break;
  }

  case CategoryFactory::Conflict: {
    b = (hasConflictFlag(info->getConflictFlags()));
    break;
  }

  case CategoryFactory::HasHiddenFiles: {
    b = (info->hasFlag(ModInfo::FLAG_HIDDEN_FILES));
    break;
  }

  case CategoryFactory::Endorsed: {
    b = (info->endorsedState() == EndorsedState::ENDORSED_TRUE);
    break;
  }

  case CategoryFactory::Backup: {
    b = (info->hasFlag(ModInfo::FLAG_BACKUP));
    break;
  }

  case CategoryFactory::Managed: {
    b = (!info->hasFlag(ModInfo::FLAG_FOREIGN));
    break;
  }

  case CategoryFactory::HasGameData: {
    b = !info->hasFlag(ModInfo::FLAG_INVALID);
    break;
  }

  case CategoryFactory::HasNexusID: {
    // never show these
    if (info->hasFlag(ModInfo::FLAG_FOREIGN) || info->hasFlag(ModInfo::FLAG_BACKUP) ||
        info->hasFlag(ModInfo::FLAG_OVERWRITE)) {
      return false;
    }

    b = (info->nexusId() > 0);
    break;
  }

  case CategoryFactory::Tracked: {
    b = (info->trackedState() == TrackedState::TRACKED_TRUE);
    break;
  }

  default: {
    b = (info->categorySet(category));
    break;
  }
  }

  return b;
}

bool ModListSortProxy::contentMatchesMod(ModInfo::Ptr info, bool enabled,
                                         int content) const
{
  return info->hasContent(content);
}

bool ModListSortProxy::filterMatchesMod(ModInfo::Ptr info, bool enabled) const
{
  // don't check if there are no filters selected
  if (!m_FilterActive) {
    return true;
  }

  // special case for separators
  if (info->hasFlag(ModInfo::FLAG_SEPARATOR)) {
    switch (m_FilterSeparators) {
    case SeparatorFilter: {
      // filter normally
      break;
    }

    case SeparatorShow: {
      // force visible
      return true;
    }

    case SeparatorHide: {
      // force hide
      return false;
    }
    }
  }

  if (!m_Filter.isEmpty()) {
    bool display       = false;
    QString filterCopy = QString(m_Filter);
    filterCopy.replace("||", ";").replace("OR", ";").replace("|", ";");
    QStringList ORList = filterCopy.split(";", Qt::SkipEmptyParts);

    bool segmentGood = true;

    // split in ORSegments that internally use AND logic
    for (auto& ORSegment : ORList) {
      QStringList ANDKeywords = ORSegment.split(" ", Qt::SkipEmptyParts);
      segmentGood             = true;
      bool foundKeyword       = false;

      // check each word in the segment for match, each word needs to be matched but it
      // doesn't matter where.
      for (auto& currentKeyword : ANDKeywords) {
        foundKeyword = false;

        // search keyword in name
        if (m_EnabledColumns[ModList::COL_NAME] &&
            info->name().contains(currentKeyword, Qt::CaseInsensitive)) {
          foundKeyword = true;
        }

        // Search by notes
        if (!foundKeyword && m_EnabledColumns[ModList::COL_NOTES] &&
            (info->notes().contains(currentKeyword, Qt::CaseInsensitive) ||
             info->comments().contains(currentKeyword, Qt::CaseInsensitive))) {
          foundKeyword = true;
        }

        // Search by categories
        if (!foundKeyword && m_EnabledColumns[ModList::COL_CATEGORY]) {
          for (auto category : info->categories()) {
            if (category.contains(currentKeyword, Qt::CaseInsensitive)) {
              foundKeyword = true;
              break;
            }
          }
        }

        // Search by Nexus ID
        if (!foundKeyword && m_EnabledColumns[ModList::COL_MODID]) {
          bool ok;
          int filterID = currentKeyword.toInt(&ok);
          if (ok) {
            int modID = info->nexusId();
            while (modID > 0) {
              if (modID == filterID) {
                foundKeyword = true;
                break;
              }
              modID = (int)(modID / 10);
            }
          }
        }

        if (!foundKeyword) {
          // currentKeword is missing from everything, AND fails and we need to check
          // next ORsegment
          segmentGood = false;
          break;
        }

      }  // for ANDKeywords loop

      if (segmentGood) {
        // the last AND loop didn't break so the ORSegments is true so mod matches
        // filter
        display = true;
        break;
      }

    }  // for ORList loop

    if (!display) {
      return false;
    }
  }  // if (!m_CurrentFilter.isEmpty())

  if (m_FilterMode == FilterAnd) {
    return filterMatchesModAnd(info, enabled);
  } else {
    return filterMatchesModOr(info, enabled);
  }
}

void ModListSortProxy::setColumnVisible(int column, bool visible)
{
  m_EnabledColumns[column] = visible;
}

void ModListSortProxy::setOptions(ModListSortProxy::FilterMode mode,
                                  SeparatorsMode separators)
{
  if (m_FilterMode != mode || separators != m_FilterSeparators) {
    m_FilterMode       = mode;
    m_FilterSeparators = separators;
    invalidateFilter();
    emit filterInvalidated();
  }
}

bool ModListSortProxy::filterAcceptsRow(int source_row, const QModelIndex& parent) const
{
  if (m_Profile == nullptr) {
    return false;
  }

  if (source_row >= static_cast<int>(m_Profile->numMods())) {
    log::warn("invalid row index: {}", source_row);
    return false;
  }

  QModelIndex idx = sourceModel()->index(source_row, 0, parent);
  if (!idx.isValid()) {
    log::debug("invalid mod index");
    return false;
  }

  unsigned int index = ULONG_MAX;
  {
    bool ok = false;
    index   = idx.data(ModList::IndexRole).toInt(&ok);
    if (!ok) {
      index = ULONG_MAX;
    }
  }

  if (sourceModel()->hasChildren(idx)) {
    // we need to check the separator itself first
    if (index < ModInfo::getNumMods() && ModInfo::getByIndex(index)->isSeparator()) {
      if (filterMatchesMod(ModInfo::getByIndex(index), false)) {
        return true;
      }
    }
    for (int i = 0; i < sourceModel()->rowCount(idx); ++i) {
      if (filterAcceptsRow(i, idx)) {
        return true;
      }
    }

    return false;
  } else {
    bool modEnabled =
        idx.sibling(source_row, 0).data(Qt::CheckStateRole).toInt() == Qt::Checked;
    return filterMatchesMod(ModInfo::getByIndex(index), modEnabled);
  }
}

bool ModListSortProxy::sourceIsByPriorityProxy() const
{
  return dynamic_cast<ModListByPriorityProxy*>(sourceModel()) != nullptr;
}

bool ModListSortProxy::canDropMimeData(const QMimeData* data, Qt::DropAction action,
                                       int row, int column,
                                       const QModelIndex& parent) const
{
  ModListDropInfo dropInfo(data, *m_Organizer);

  if (!dropInfo.isLocalFileDrop() && sortColumn() != ModList::COL_PRIORITY) {
    return false;
  }

  // disable drop install with group proxy, except the one for collapsible separator
  // - it would be nice to be able to "install to category" or something like that but
  //   it's a bit more complicated since the drop position is based on the category, so
  //   just disabling for now
  if (dropInfo.isDownloadDrop()) {
    // maybe there is a cleaner way?
    if (qobject_cast<QtGroupingProxy*>(sourceModel())) {
      return false;
    }
  }

  // see dropMimeData for details
  if (sortOrder() == Qt::DescendingOrder && row != -1 && !sourceIsByPriorityProxy()) {
    --row;
  }

  return QSortFilterProxyModel::canDropMimeData(data, action, row, column, parent);
}

bool ModListSortProxy::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                    int row, int column, const QModelIndex& parent)
{
  ModListDropInfo dropInfo(data, *m_Organizer);

  if (!dropInfo.isLocalFileDrop() && sortColumn() != ModList::COL_PRIORITY) {
    QWidget* wid = qApp->activeWindow()->findChild<QTreeView*>("modList");
    MessageDialog::showMessage(
        tr("Drag&Drop is only supported when sorting by priority"), wid);
    return false;
  }

  if (row == -1 && column == -1) {
    return sourceModel()->dropMimeData(data, action, -1, -1, mapToSource(parent));
  }

  // in the regular model, when dropping between rows, the row-value passed to
  // the sourceModel is inconsistent between ascending and descending ordering
  //
  // we want to fix that, but we cannot do it for the by-priority proxy because
  // it messes up with non top-level items, so we simply forward the row and the
  // by-priority proxy will fix the row for us
  if (sortOrder() == Qt::DescendingOrder && row != -1 && !sourceIsByPriorityProxy()) {
    --row;
  }

  return QSortFilterProxyModel::dropMimeData(data, action, row, column, parent);
}

void ModListSortProxy::setSourceModel(QAbstractItemModel* sourceModel)
{
  QSortFilterProxyModel::setSourceModel(sourceModel);
  QAbstractProxyModel* proxy = qobject_cast<QAbstractProxyModel*>(sourceModel);
  if (proxy != nullptr) {
    sourceModel = proxy->sourceModel();
  }
  if (sourceModel) {
    connect(sourceModel, SIGNAL(aboutToChangeData()), this, SLOT(aboutToChangeData()),
            Qt::UniqueConnection);
    connect(sourceModel, SIGNAL(postDataChanged()), this, SLOT(postDataChanged()),
            Qt::UniqueConnection);
  }
}

void ModListSortProxy::aboutToChangeData()
{
  // having a filter active when dataChanged is called caused a crash
  // (at least with some Qt versions)
  // this may be related to the fact that the item being edited may disappear from the
  // view as a result of the edit
  m_PreChangeCriteria = m_Criteria;
  setCriteria({});
}

void ModListSortProxy::postDataChanged()
{
  // if the filter is re-activated right away the editor can't be deleted but becomes
  // invisible or at least the view continues to think it's being edited. As a result no
  // new editor can be opened
  QTimer::singleShot(10, [this]() {
    setCriteria(m_PreChangeCriteria);
    m_PreChangeCriteria.clear();
  });
}
