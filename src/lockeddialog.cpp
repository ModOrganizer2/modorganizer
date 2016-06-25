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

#include "lockeddialog.h"
#include "ui_lockeddialog.h"

#include <QPoint>
#include <QResizeEvent>
#include <QWidget>
#include <Qt>                 // for Qt::FramelessWindowHint, etc

LockedDialog::LockedDialog(QWidget *parent, const QString &text, bool unlockButton)
  : QDialog(parent)
  , ui(new Ui::LockedDialog)
  , m_UnlockClicked(false)
{
  ui->setupUi(this);

  this->setWindowFlags(this->windowFlags() | Qt::ToolTip | Qt::FramelessWindowHint);

  if (parent != nullptr) {
    QPoint position = parent->mapToGlobal(QPoint(parent->width() / 2, parent->height() / 2));
    position.rx() -= this->width() / 2;
    position.ry() -= this->height() / 2;
    move(position);
  }

  if (text.length() > 0) {
    ui->label->setText(text);
  }
  if (!unlockButton) {
    ui->unlockButton->hide();
  }
}

LockedDialog::~LockedDialog()
{
    delete ui;
}


void LockedDialog::setProcessName(const QString &name)
{
  ui->processLabel->setText(name);
}


void LockedDialog::resizeEvent(QResizeEvent *event)
{
  QWidget *par = parentWidget();
  if (par != nullptr) {
    QPoint position = par->mapToGlobal(QPoint(par->width() / 2, par->height() / 2));
    position.rx() -= event->size().width() / 2;
    position.ry() -= event->size().height() / 2;
    move(position);
  }
}

void LockedDialog::on_unlockButton_clicked()
{
  m_UnlockClicked = true;
}
