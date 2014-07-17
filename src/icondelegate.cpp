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
#include <QDebug>


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

  int iconWidth = icons.size() > 0 ? ((option.rect.width() / icons.size()) - 4) : 16;
  iconWidth = std::min(16, iconWidth);

  painter->translate(option.rect.topLeft());
  for (auto iter = icons.begin(); iter != icons.end(); ++iter) {
    painter->drawPixmap(x, 2, iconWidth, iconWidth, iter->pixmap(QSize(iconWidth, iconWidth)));
    x += iconWidth + 4;
  }

  painter->restore();
}


QSize IconDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &modelIndex) const
{
  int count = getNumIcons(modelIndex);
  unsigned int index = modelIndex.data(Qt::UserRole + 1).toInt();
  QSize result;
  if (index < ModInfo::getNumMods()) {
    result = QSize(count * 40, 20);
  } else {
    result = QSize(1, 20);
  }
  if (option.rect.width() > 0) {
    result.setWidth(std::min(option.rect.width(), result.width()));
  }
  return result;
}

