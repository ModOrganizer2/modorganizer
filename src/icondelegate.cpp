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

#include "icondelegate.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>


IconDelegate::IconDelegate(QObject *parent)
  : QStyledItemDelegate(parent)
{
}


void IconDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QStyledItemDelegate::paint(painter, option, index);

  QList<QIcon> icons = getIcons(index);

  int x = 4;
  painter->save();
  painter->translate(option.rect.topLeft());
  for (auto iter = icons.begin(); iter != icons.end(); ++iter) {
    painter->drawPixmap(x, 2, 16, 16, iter->pixmap(QSize(16, 16)));
    x += 20;
  }

  painter->restore();
}


QSize IconDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex &modelIndex) const
{
  int count = getNumIcons(modelIndex);
  unsigned int index = modelIndex.data(Qt::UserRole + 1).toInt();
  if (index < ModInfo::getNumMods()) {
    ModInfo::Ptr info = ModInfo::getByIndex(index);
    return QSize(info->getFlags().size() * 20, 20);
  } else {
    return QSize(1, 20);
  }
}

