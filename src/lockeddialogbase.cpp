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

#include "lockeddialogbase.h"

#include <QPoint>
#include <QResizeEvent>
#include <QWidget>
#include <Qt>                 // for Qt::FramelessWindowHint, etc

LockedDialogBase::LockedDialogBase(QWidget *parent, bool allowClose)
  : QDialog(parent)
  , m_Unlocked(false)
  , m_Canceled(false)
  , m_allowClose(allowClose)
{
  if (parent != nullptr) {
    QPoint position = parent->mapToGlobal(QPoint(parent->width() / 2, parent->height() / 2));
    position.rx() -= this->width() / 2;
    position.ry() -= this->height() / 2;
    move(position);
  }
}

void LockedDialogBase::resizeEvent(QResizeEvent *event)
{
  QWidget *par = parentWidget();
  if (par != nullptr) {
    QPoint position = par->mapToGlobal(QPoint(par->width() / 2, par->height() / 2));
    position.rx() -= event->size().width() / 2;
    position.ry() -= event->size().height() / 2;
    move(position);
  }
}

void LockedDialogBase::reject()
{
  if (m_allowClose)
    unlock();
}

bool LockedDialogBase::unlockForced() const {
  return m_Unlocked;
}

bool LockedDialogBase::canceled() const {
  return m_Canceled;
}

void LockedDialogBase::unlock() {
  m_Unlocked = true;
}

void LockedDialogBase::cancel() {
  m_Canceled = true;
}

