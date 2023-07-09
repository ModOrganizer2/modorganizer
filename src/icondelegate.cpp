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
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmapCache>
#include <log.h>

using namespace MOBase;

IconDelegate::IconDelegate(QTreeView* view, int column, int compactSize)
    : QStyledItemDelegate(view), m_column(column), m_compactSize(compactSize),
      m_compact(false)
{
  if (view) {
    connect(view->header(), &QHeaderView::sectionResized,
            [=](int column, int, int size) {
              if (column == m_column) {
                m_compact = size < m_compactSize;
              }
            });
  }
}

void IconDelegate::paintIcons(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index, const QList<QString>& icons)
{
  int x = 4;
  painter->save();

  int iconWidth = icons.size() > 0 ? ((option.rect.width() / icons.size()) - 4) : 16;
  iconWidth     = std::min(16, iconWidth);

  const int margin = (option.rect.height() - iconWidth) / 2;

  painter->translate(option.rect.topLeft());
  for (const QString& iconId : icons) {
    if (iconId.isEmpty()) {
      x += iconWidth + 4;
      continue;
    }
    QPixmap icon;
    QString fullIconId = QString("%1_%2").arg(iconId).arg(iconWidth);
    if (!QPixmapCache::find(fullIconId, &icon)) {
      icon = QIcon(iconId).pixmap(iconWidth, iconWidth);
      if (icon.isNull()) {
        log::warn("failed to load icon {}", iconId);
      }
      QPixmapCache::insert(fullIconId, icon);
    }
    painter->drawPixmap(x, margin, iconWidth, iconWidth, icon);
    x += iconWidth + 4;
  }

  painter->restore();
}

void IconDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const
{
  if (auto* w = qobject_cast<QAbstractItemView*>(parent())) {
    w->itemDelegate()->paint(painter, option, index);
  } else {
    QStyledItemDelegate::paint(painter, option, index);
  }

  paintIcons(painter, option, index, getIcons(index));
}
