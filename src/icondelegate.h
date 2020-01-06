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

#ifndef ICONDELEGATE_H
#define ICONDELEGATE_H

#include "modinfo.h"
#include <QStyledItemDelegate>
#include <QAbstractProxyModel>


class IconDelegate : public QStyledItemDelegate
{
  Q_OBJECT;

public:
  explicit IconDelegate(QObject *parent = 0);
  virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

  static void paintIcons(
    QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index, const QList<QString>& icons);

protected:
  virtual QList<QString> getIcons(const QModelIndex &index) const = 0;
  virtual size_t getNumIcons(const QModelIndex &index) const = 0;
};

#endif // ICONDELEGATE_H
