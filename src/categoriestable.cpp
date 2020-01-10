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

#include "categoriestable.h"

CategoriesTable::CategoriesTable(QWidget* parent) : QTableWidget(parent) {}

bool CategoriesTable::dropMimeData(int row, int column, const QMimeData* data, Qt::DropAction action)
{
  if (row == -1)
    return false;

  if (action == Qt::IgnoreAction)
    return true;

  if (!data->hasFormat("application/x-qabstractitemmodeldatalist"))
    return false;

  QByteArray encoded = data->data("application/x-qabstractitemmodeldatalist");
  QDataStream stream(&encoded, QIODevice::ReadOnly);

  while (!stream.atEnd())
  {
    int curRow, curCol;
    QMap<int, QVariant> roleDataMap;
    stream >> curRow >> curCol >> roleDataMap;

    for (auto item : findItems(roleDataMap.value(Qt::DisplayRole).toString(), Qt::MatchContains | Qt::MatchWrap))
    {
      if (item->column() != 3) continue;
      QVariantList newData;
      for (auto nexData : item->data(Qt::UserRole).toList()) {
        if (nexData.toList()[1].toInt() != roleDataMap.value(Qt::UserRole)) {
          newData.insert(newData.length(), nexData);
        }
      }
      QStringList names;
      for (auto nexData : newData) {
        names.append(nexData.toList()[0].toString());
      }
      item->setData(Qt::DisplayRole, names.join(", "));
      item->setData(Qt::UserRole, newData);
    }

    auto nexusItem = item(row, 3);
    auto itemData = nexusItem->data(Qt::UserRole).toList();
    QVariantList newData;
    newData.append(roleDataMap.value(Qt::DisplayRole).toString());
    newData.append(roleDataMap.value(Qt::UserRole).toInt());
    itemData.insert(itemData.length(), newData);
    QStringList names;
    for (auto cat : itemData) {
      names.append(cat.toList()[0].toString());
    }
    nexusItem->setData(Qt::UserRole, itemData);
    nexusItem->setData(Qt::DisplayRole, names.join(", "));
  }

  return true;
}
