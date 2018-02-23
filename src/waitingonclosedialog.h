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

#pragma once

#include "lockeddialogbase.h"

namespace Ui {
    class WaitingOnCloseDialog;
}

/**
 * Similar to the LockedDialog but used for waiting on running process during
 * a process close request which requries a slightly different dialog.
 **/
class WaitingOnCloseDialog : public LockedDialogBase
{
    Q_OBJECT

public:
  explicit WaitingOnCloseDialog(QWidget *parent = 0);
  ~WaitingOnCloseDialog();

  bool canceled() const { return m_Canceled; }

  void setProcessName(const QString &name) override;

protected:

  void unlock() override;

private slots:

  void on_closeButton_clicked();
  void on_cancelButton_clicked();

private:

  Ui::WaitingOnCloseDialog *ui;
};
