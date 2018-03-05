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
    class LockedDialog;
}

/**
 * a small borderless dialog displayed while the Mod Organizer UI is locked
 * The dialog contains only a label and a button to force the UI to be unlocked
 * 
 * The UI gets locked while running external applications since they may modify the
 * data on which Mod Organizer works. After the UI is unlocked (manually or after the
 * external application closed) MO will refresh all of its data sources
 **/
class LockedDialog : public LockedDialogBase
{
    Q_OBJECT

public:
  explicit LockedDialog(QWidget *parent = 0, bool unlockByButton = false);
  ~LockedDialog();

  void setProcessName(const QString &name) override;

protected:

  void unlock() override;

private slots:

  void on_unlockButton_clicked();

private:

  Ui::LockedDialog *ui;
};
