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

#ifndef QUERYOVERWRITEDIALOG_H
#define QUERYOVERWRITEDIALOG_H

#include <QDialog>

namespace Ui {
class QueryOverwriteDialog;
}

class QueryOverwriteDialog : public QDialog
{
  Q_OBJECT
public:
  enum Action {
    ACT_NONE,
    ACT_MERGE,
    ACT_REPLACE,
    ACT_RENAME
  };

  enum Backup {
    BACKUP_NO,
    BACKUP_YES
  };

public:
  QueryOverwriteDialog(QWidget *parent, Backup b);
  ~QueryOverwriteDialog();
  bool backup() const;
  Action action() const { return m_Action; }
private slots:
  void on_mergeBtn_clicked();
  void on_replaceBtn_clicked();
  void on_renameBtn_clicked();
  void on_cancelBtn_clicked();

private:
  Ui::QueryOverwriteDialog *ui;
  Action m_Action;
};

#endif // QUERYOVERWRITEDIALOG_H
