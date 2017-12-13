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

LockedDialog::LockedDialog(QWidget *parent, bool unlockByButton)
  : QDialog(parent)
  , ui(new Ui::LockedDialog)
  , m_Unlocked(false)
  , m_allowClose(!unlockByButton)
{
  ui->setupUi(this);

  // Supposedly the Qt::CustomizeWindowHint should use a customized window
  // allowing us to select if there is a close button. In practice this doesn't
  // seem to work. We will ignore pressing the close button if unlockByButton == true
  Qt::WindowFlags flags =
    this->windowFlags() | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint;
  if (!unlockByButton)
    flags |= Qt::WindowCloseButtonHint;
  this->setWindowFlags(flags);

  if (!unlockByButton)
  {
    ui->unlockButton->hide();
    ui->verticalLayout->addItem(
      new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
  }

  if (parent != nullptr) {
    QPoint position = parent->mapToGlobal(QPoint(parent->width() / 2, parent->height() / 2));
    position.rx() -= this->width() / 2;
    position.ry() -= this->height() / 2;
    move(position);
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
  unlock();
}

void LockedDialog::reject()
{
  if (m_allowClose)
    unlock();
}

bool LockedDialog::unlockForced() {
  return m_Unlocked;
}

void LockedDialog::unlock() {
  m_Unlocked = true;
  ui->label->setText("unlocking may take a few seconds");
  ui->unlockButton->setEnabled(false);
}
