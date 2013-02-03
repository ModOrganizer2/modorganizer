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

#ifndef ARCHIVETREE_H
#define ARCHIVETREE_H

#include <QTreeWidget>

/**
 * @brief QT tree widget used to display the content of an archive in the manual installation dialog
 **/
class ArchiveTree : public QTreeWidget
{

  Q_OBJECT

public:

  explicit ArchiveTree(QWidget *parent = 0);

signals:

  void changed();

public slots:

protected:

  virtual void dragEnterEvent(QDragEnterEvent *event);
  virtual void dragMoveEvent(QDragMoveEvent *event);
  virtual void dropEvent(QDropEvent *event);

private:

  bool testMovePossible(QTreeWidgetItem *source, QTreeWidgetItem *target);

};

#endif // ARCHIVETREE_H
