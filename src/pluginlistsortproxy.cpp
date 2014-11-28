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

#include "pluginlistsortproxy.h"
#include "pluginlist.h"
#include <QMenu>
#include <QCheckBox>
#include <QWidgetAction>


PluginListSortProxy::PluginListSortProxy(QObject *parent)
  : QSortFilterProxyModel(parent),
    m_SortIndex(0), m_SortOrder(Qt::AscendingOrder)
{
  m_EnabledColumns.set(PluginList::COL_NAME);
  m_EnabledColumns.set(PluginList::COL_PRIORITY);
  m_EnabledColumns.set(PluginList::COL_MODINDEX);
  this->setDynamicSortFilter(true);
}


unsigned int PluginListSortProxy::getEnabledColumns() const
{
  unsigned int result = 0;
  for (int i = 0; i <= PluginList::COL_LASTCOLUMN; ++i) {
    if (m_EnabledColumns.test(i)) {
      result |= 1 << i;
    }
  }
  return result;
}


void PluginListSortProxy::setEnabledColumns(unsigned int columns)
{
  emit layoutAboutToBeChanged();
  for (int i = 0; i <= PluginList::COL_LASTCOLUMN; ++i) {
    m_EnabledColumns.set(i, (columns & (1 << i)) != 0);
  }
  emit layoutChanged();
}


Qt::ItemFlags PluginListSortProxy::flags(const QModelIndex &modelIndex) const
{
  Qt::ItemFlags flags = sourceModel()->flags(mapToSource(modelIndex));
  if (sortColumn() == PluginList::COL_NAME) {
    flags &= ~Qt::ItemIsDragEnabled;
  }
  return flags;
}


void PluginListSortProxy::displayColumnSelection(const QPoint &pos)
{
  QMenu menu;

  for (int i = 0; i <= PluginList::COL_LASTCOLUMN; ++i) {
    QCheckBox *checkBox = new QCheckBox(&menu);
    checkBox->setText(PluginList::getColumnName(i));
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
    if (widgetAction != nullptr) {
      const QCheckBox *checkBox = qobject_cast<const QCheckBox*>(widgetAction->defaultWidget());
      if (checkBox != nullptr) {
        m_EnabledColumns.set(i, checkBox->checkState() == Qt::Checked);
      }
    }
    ++i;
  }
  emit layoutChanged();
}


void PluginListSortProxy::updateFilter(const QString &filter)
{
  m_CurrentFilter = filter;
  invalidateFilter();
}


bool PluginListSortProxy::filterAcceptsRow(int row, const QModelIndex&) const
{
  return m_CurrentFilter.isEmpty() ||
      sourceModel()->data(sourceModel()->index(row, 0)).toString().contains(m_CurrentFilter, Qt::CaseInsensitive);
}


bool PluginListSortProxy::lessThan(const QModelIndex &left,
                                   const QModelIndex &right) const
{
  PluginList *plugins = qobject_cast<PluginList*>(sourceModel());
  switch (left.column()) {
    case PluginList::COL_NAME: {
      return QString::compare(plugins->getName(left.row()), plugins->getName(right.row()), Qt::CaseInsensitive) < 0;
    } break;
    case PluginList::COL_PRIORITY:
    case PluginList::COL_MODINDEX: {
      return plugins->getPriority(left.row()) < plugins->getPriority(right.row());
    } break;
    default: {
      return plugins->getPriority(left.row()) < plugins->getPriority(right.row());
    } break;
  }
}


bool PluginListSortProxy::dropMimeData(const QMimeData *data, Qt::DropAction action,
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

