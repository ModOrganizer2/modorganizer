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

#ifndef SIMPLEINSTALLDIALOG_H
#define SIMPLEINSTALLDIALOG_H

#include <QDialog>

namespace Ui {
    class SimpleInstallDialog;
}

/**
 * @brief Dialog for the installation of a simple archive
 * a simple archive is one that doesn't require any manual changes to work correctly
 **/
class SimpleInstallDialog : public QDialog
{
    Q_OBJECT

public:
 /**
  * @brief constructor
  *
  * @param preset suggested name for the mod
  * @param parent parent widget
  **/
 explicit SimpleInstallDialog(const QString &preset, QWidget *parent = 0);
  ~SimpleInstallDialog();

  /**
   * @return true if the user requested the manual installation dialog
   **/
  bool manualRequested() const { return m_Manual; }
  /**
   * @return the (user-modified) mod name
   **/
  QString getName() const;

private slots:

  void on_okBtn_clicked();

  void on_cancelBtn_clicked();

  void on_manualBtn_clicked();

private:
  Ui::SimpleInstallDialog *ui;
  bool m_Manual;
};

#endif // SIMPLEINSTALLDIALOG_H
