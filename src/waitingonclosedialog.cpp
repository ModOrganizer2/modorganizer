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

#include "waitingonclosedialog.h"
#include "ui_waitingonclosedialog.h"

#include <QPoint>
#include <QResizeEvent>
#include <QWidget>
#include <Qt>                 // for Qt::FramelessWindowHint, etc

WaitingOnCloseDialog::WaitingOnCloseDialog(QWidget *parent)
  : LockedDialogBase(parent,true)
  , ui(new Ui::WaitingOnCloseDialog)
{
  ui->setupUi(this);

  // Supposedly the Qt::CustomizeWindowHint should use a customized window
  // allowing us to select if there is a close button. In practice this doesn't
  // seem to work. We will ignore pressing the close button if unlockByButton == true
  Qt::WindowFlags flags =
    this->windowFlags() | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint;
  if (m_allowClose)
    flags |= Qt::WindowCloseButtonHint;
  this->setWindowFlags(flags);
}

WaitingOnCloseDialog::~WaitingOnCloseDialog()
{
    delete ui;
}


void WaitingOnCloseDialog::setProcessName(const QString &name)
{
  ui->processLabel->setText(name);
}

void WaitingOnCloseDialog::on_closeButton_clicked()
{
  unlock();
}

void WaitingOnCloseDialog::on_cancelButton_clicked()
{
  cancel();
  unlock();
}

void WaitingOnCloseDialog::unlock() {
  LockedDialogBase::unlock();
  ui->label->setText("unlocking may take a few seconds");
  ui->closeButton->setEnabled(false);
  ui->cancelButton->setEnabled(false);
}
