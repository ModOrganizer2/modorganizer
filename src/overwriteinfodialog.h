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

#ifndef OVERWRITEINFODIALOG_H
#define OVERWRITEINFODIALOG_H

#include "modinfo.h"
#include <QDialog>
#include <QFileSystemModel>

namespace Ui {
class OverwriteInfoDialog;
}

class OverwriteInfoDialog : public QDialog
{
  Q_OBJECT
  
public:

  explicit OverwriteInfoDialog(ModInfo::Ptr modInfo, QWidget *parent = 0);
  ~OverwriteInfoDialog();

  ModInfo::Ptr modInfo() const { return m_ModInfo; }

  void setModInfo(ModInfo::Ptr modInfo);

private:

  void openFile(const QModelIndex &index);
  bool recursiveDelete(const QModelIndex &index);
  void deleteFile(const QModelIndex &index);

private slots:

  void deleteTriggered();
  void renameTriggered();
  void openTriggered();
  void createDirectoryTriggered();

  void on_filesView_customContextMenuRequested(const QPoint &pos);

private:

  Ui::OverwriteInfoDialog *ui;
  QFileSystemModel *m_FileSystemModel;
  QModelIndexList m_FileSelection;
  QAction *m_DeleteAction;
  QAction *m_RenameAction;
  QAction *m_OpenAction;
  QAction *m_NewFolderAction;

  ModInfo::Ptr m_ModInfo;

};

#endif // OVERWRITEINFODIALOG_H
