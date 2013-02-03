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

#ifndef FINDDIALOG_H
#define FINDDIALOG_H

#include <QDialog>

namespace Ui {
    class FindDialog;
}

/**
 * @brief Find dialog used in the TextView dialog
 **/
class FindDialog : public QDialog
{

  Q_OBJECT

public:

  /**
   * @brief constructor
   *
   * @param parent parent widget
   **/
  explicit FindDialog(QWidget *parent = 0);

  ~FindDialog();

signals:

  /**
   * @brief emitted when the user wants to jump to the next location matching the pattern
   **/
  void findNext();

  /**
   * @brief emitted when the user changes the pattern to search for
   *
   * @param pattern the new search pattern
   **/
  void patternChanged(const QString &pattern);

private slots:
  void on_nextBtn_clicked();

  void on_patternEdit_textChanged(const QString &arg1);

  void on_closeBtn_clicked();

private:
    Ui::FindDialog *ui;
};

#endif // FINDDIALOG_H
