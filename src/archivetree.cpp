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

#include "archivetree.h"
#include <QDragMoveEvent>

ArchiveTree::ArchiveTree(QWidget *parent) :
    QTreeWidget(parent)
{
}


bool ArchiveTree::testMovePossible(QTreeWidgetItem *source, QTreeWidgetItem *target)
{
  if ((target == NULL) ||
      (source == NULL)) {
    return false;
  }

  if ((source == target) ||
      (source->parent() == target)) {
    return false;
  }

  return true;
}

void ArchiveTree::dragEnterEvent(QDragEnterEvent *event)
{
  QTreeWidgetItem *source = this->currentItem();
  if ((source == NULL) || (source->parent() == NULL)) {
    // can't change top level
    event->ignore();
    return;
  } else {
    QTreeWidget::dragEnterEvent(event);
  }

}

void ArchiveTree::dragMoveEvent(QDragMoveEvent *event)
{
  if (!testMovePossible(this->currentItem(), itemAt(event->pos()))) {
    event->ignore();
  } else {
    QTreeWidget::dragMoveEvent(event);
  }
}


void ArchiveTree::dropEvent(QDropEvent *event)
{
  event->ignore();

  QTreeWidgetItem *target = itemAt(event->pos());

  QList<QTreeWidgetItem*> sourceItems = this->selectedItems();
  for (QList<QTreeWidgetItem*>::iterator iter = sourceItems.begin();
       iter != sourceItems.end(); ++iter) {
    QTreeWidgetItem *source = *iter;
    if ((source->parent() != NULL) &&
        testMovePossible(source, target)) {
      source->parent()->removeChild(source);
      if (target->data(0, Qt::UserRole).toInt() != 0) {
        // target is a file
        if (target->parent() == NULL) {
          // this should really not happen, how should a
          // file get to the top level?
          return;
        }
        int index = target->parent()->indexOfChild(target);
        target->parent()->insertChild(index, source);
        emit changed();
      } else {
        // target is a directory
        target->insertChild(target->childCount(), source);
        emit changed();
      }
    }
  }

}
