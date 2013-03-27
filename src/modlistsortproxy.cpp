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
#include "modlist.h"
#include <QMenu>
#include <QCheckBox>
#include <QWidgetAction>
#include <QMessageBox>


ModListSortProxy::ModListSortProxy(Profile* profile, QObject *parent)
  : QSortFilterProxyModel(parent), m_Profile(profile),
    m_CategoryFilter(CategoryFactory::CATEGORY_NONE), m_CurrentFilter()
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


void ModListSortProxy::setCategoryFilter(int category)
{
  m_CategoryFilter = category;
  this->invalidate();
}


Qt::ItemFlags ModListSortProxy::flags(const QModelIndex &modelIndex) const
{
  Qt::ItemFlags flags = sourceModel()->flags(mapToSource(modelIndex));
  if (sortColumn() != ModList::COL_PRIORITY) {
    flags &= ~Qt::ItemIsDragEnabled;
  }
  return flags;
}



void ModListSortProxy::displayColumnSelection(const QPoint &pos)
{
  QMenu menu;

  for (int i = 0; i <= ModList::COL_LASTCOLUMN; ++i) {
    QCheckBox *checkBox = new QCheckBox(&menu);
    checkBox->setText(ModList::getColumnName(i));
    checkBox->setChecked(m_EnabledColumns.test(i) ? Qt::Checked : Qt::Unchecked);
    QWidgetAction *checkableAction = new QWidgetAction(&menu);
    checkableAction->setDefaultWidget(checkBox);
    menu.addAction(checkableAction);
  }
  menu.exec(pos);
  int i = 0;

  emit layoutAboutToBeChanged();
  m_EnabledColumns.reset();
  foreach (const QAction *action, menu.actions()) {
    const QWidgetAction *widgetAction = qobject_cast<const QWidgetAction*>(action);
    if (widgetAction != NULL) {
      const QCheckBox *checkBox = qobject_cast<const QCheckBox*>(widgetAction->defaultWidget());
      if (checkBox != NULL) {
        m_EnabledColumns.set(i, checkBox->checkState() == Qt::Checked);
      }
    }
    ++i;
  }
  emit layoutChanged();
}


void ModListSortProxy::enableAllVisible()
{
  if (m_Profile == NULL) return;

  for (int i = 0; i < this->rowCount(); ++i) {
    int modID = mapToSource(index(i, 0)).row();
    m_Profile->setModEnabled(modID, true);
  }
  invalidate();
}


void ModListSortProxy::disableAllVisible()
{
  if (m_Profile == NULL) return;
  for (int i = 0; i < this->rowCount(); ++i) {
    int modID = mapToSource(index(i, 0)).row();
    m_Profile->setModEnabled(modID, false);
  }
}


bool ModListSortProxy::lessThan(const QModelIndex &left,
                                const QModelIndex &right) const
{
  int leftIndex  = left.row();
  int rightIndex = right.row();
  ModInfo::Ptr leftMod = ModInfo::getByIndex(leftIndex);
  ModInfo::Ptr rightMod = ModInfo::getByIndex(rightIndex);

  bool lt = false;
  switch (left.column()) {
    case ModList::COL_FLAGS: lt = leftMod->getFlags().size() < rightMod->getFlags().size(); break;
    case ModList::COL_NAME: lt = QString::compare(leftMod->name(), rightMod->name(), Qt::CaseInsensitive) < 0; break;
    case ModList::COL_CATEGORY: {
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
      } break;
    case ModList::COL_MODID: lt = leftMod->getNexusID() < rightMod->getNexusID(); break;
    case ModList::COL_VERSION: lt = leftMod->getVersion() < rightMod->getVersion(); break;
    case ModList::COL_PRIORITY: {
      if (m_Profile != NULL) {
        int leftPrio  = leftMod->getFixedPriority();
        int rightPrio = rightMod->getFixedPriority();
        if (leftPrio == INT_MIN)  leftPrio  = m_Profile->getModPriority(leftIndex);
        if (rightPrio == INT_MIN) rightPrio = m_Profile->getModPriority(rightIndex);

        lt = leftPrio < rightPrio;
      } else {
        lt = leftMod->name() < rightMod->name();
      }
    } break;
  }
  return lt;
}


void ModListSortProxy::updateFilter(const QString &filter)
{
  m_CurrentFilter = filter;
  invalidateFilter();
}


bool ModListSortProxy::hasConflictFlag(const std::vector<ModInfo::EFlag> &flags) const
{
  foreach (ModInfo::EFlag flag, flags) {
    if ((flag == ModInfo::FLAG_CONFLICT_MIXED) ||
        (flag == ModInfo::FLAG_CONFLICT_OVERWRITE) ||
        (flag == ModInfo::FLAG_CONFLICT_OVERWRITTEN) ||
        (flag == ModInfo::FLAG_CONFLICT_REDUNDANT)) {
      return true;
    }
  }

  return false;
}


bool ModListSortProxy::filterMatches(ModInfo::Ptr info, bool enabled) const
{
  if (!m_CurrentFilter.isEmpty() &&
      !info->name().contains(m_CurrentFilter, Qt::CaseInsensitive)) {
    return false;
  }

  return ((m_CategoryFilter == CategoryFactory::CATEGORY_NONE) ||
      ((m_CategoryFilter < CategoryFactory::CATEGORY_SPECIAL_FIRST) && (info->categorySet(m_CategoryFilter))) ||
      ((m_CategoryFilter == CategoryFactory::CATEGORY_SPECIAL_CHECKED) && enabled) ||
      ((m_CategoryFilter == CategoryFactory::CATEGORY_SPECIAL_UNCHECKED) && !enabled) ||
      ((m_CategoryFilter == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) && info->updateAvailable()) ||
      ((m_CategoryFilter == CategoryFactory::CATEGORY_SPECIAL_NOCATEGORY) && (info->getCategories().size() == 0)) ||
          ((m_CategoryFilter == CategoryFactory::CATEGORY_SPECIAL_CONFLICT) && (hasConflictFlag(info->getFlags()))));
}


bool ModListSortProxy::filterAcceptsRow(int row, const QModelIndex&) const
{
  bool modEnabled = m_Profile != NULL ? m_Profile->modEnabled(row) : false;

  return filterMatches(ModInfo::getByIndex(row), modEnabled);
}


/*bool ModListSortProxy::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const
{
  return m_EnabledColumns.test(source_column);
}*/


bool ModListSortProxy::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                    int row, int column, const QModelIndex &parent)
{
    if ((row == -1) && (column == -1)) {
      return this->sourceModel()->dropMimeData(data, action, -1, -1, mapToSource(parent));
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
