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

#include <QAbstractItemView>
#include <QAbstractProxyModel>
#include <QStyledItemDelegate>

class IconDelegate : public QStyledItemDelegate
{
  Q_OBJECT;

public:
  explicit IconDelegate(QTreeView* view, int column = -1, int compactSize = 100);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

protected:
  // check if icons should be compacted or not
  //
  bool compact() const { return m_compact; }

  static void paintIcons(QPainter* painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index, const QList<QString>& icons);

  virtual QList<QString> getIcons(const QModelIndex& index) const = 0;
  virtual size_t getNumIcons(const QModelIndex& index) const      = 0;

private:
  int m_column;
  int m_compactSize;
  bool m_compact;
};

#endif  // ICONDELEGATE_H
