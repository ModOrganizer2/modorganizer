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

#ifndef EDITEXECUTABLESDIALOG_H
#define EDITEXECUTABLESDIALOG_H

#include "tutorabledialog.h"
#include <QListWidgetItem>
#include <QTimer>
#include "executableslist.h"

namespace Ui {
    class EditExecutablesDialog;
}

/**
 * @brief Dialog to manage the list of executables
 **/
class EditExecutablesDialog : public MOBase::TutorableDialog
{
    Q_OBJECT

public:

  /**
   * @brief constructor
   *
   * @param executablesList current list of executables
   * @param parent parent widget
   **/
  explicit EditExecutablesDialog(const ExecutablesList &executablesList, QWidget *parent = 0);

  ~EditExecutablesDialog();

  /**
   * @brief retrieve the updated list of executables
   *
   * @return updated list of executables
   **/
  ExecutablesList getExecutablesList() const;

  void saveExecutable();
private slots:

  void on_binaryEdit_textChanged(const QString &arg1);

  void on_addButton_clicked();

  void on_browseButton_clicked();

  void on_removeButton_clicked();

  void on_titleEdit_textChanged(const QString &arg1);

  void on_overwriteAppIDBox_toggled(bool checked);

  void on_browseDirButton_clicked();

  void on_closeButton_clicked();

  void on_executablesListBox_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

  void delayedRefresh();

private:

  void resetInput();

  void refreshExecutablesWidget();

  bool executableChanged();

private:
    Ui::EditExecutablesDialog *ui;

    QListWidgetItem *m_CurrentItem;

    ExecutablesList m_ExecutablesList;

};

#endif // EDITEXECUTABLESDIALOG_H
