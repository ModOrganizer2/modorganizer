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
#include "modlistdropinfo.h"
#include "organizercore.h"
#include <QDialog>
#include <QFileSystemModel>

namespace Ui
{
class OverwriteInfoDialog;
}

class OverwriteFileSystemModel : public QFileSystemModel
{
  Q_OBJECT;

public:
  OverwriteFileSystemModel(QObject* parent, OrganizerCore& organizer)
      : QFileSystemModel(parent), m_Organizer(organizer), m_RegularColumnCount(0)
  {}

  virtual int columnCount(const QModelIndex& parent) const
  {
    m_RegularColumnCount = QFileSystemModel::columnCount(parent);
    return m_RegularColumnCount;
  }

  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const
  {
    if ((orientation == Qt::Horizontal) && (section >= m_RegularColumnCount)) {
      if (role == Qt::DisplayRole) {
        return tr("Overwrites");
      } else {
        return QVariant();
      }
    } else {
      return QFileSystemModel::headerData(section, orientation, role);
    }
  }

  virtual QVariant data(const QModelIndex& index, int role) const
  {
    if (index.column() == m_RegularColumnCount + 0) {
      if (role == Qt::DisplayRole) {
        return tr("not implemented");
      } else {
        return QVariant();
      }
    } else {
      return QFileSystemModel::data(index, role);
    }
  }

  virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                            int column, const QModelIndex& parent)
  {
    ModListDropInfo dropInfo(data, m_Organizer);
    if (dropInfo.isLocalFileDrop()) {
      for (auto entry : dropInfo.localUrls()) {
        QFileInfo sourceInfo(entry.url.toLocalFile());
        if (sourceInfo.isDir() &&
            m_Organizer.managedGame()->getModMappings().keys().contains(
                entry.relativePath, Qt::CaseInsensitive)) {
          return false;
        }
      }
    }
    return QFileSystemModel::dropMimeData(data, action, row, column, parent);
  }

private:
  mutable int m_RegularColumnCount;

  OrganizerCore& m_Organizer;
};

class OverwriteInfoDialog : public QDialog
{
  Q_OBJECT

public:
  explicit OverwriteInfoDialog(ModInfo::Ptr modInfo, OrganizerCore& organizer,
                               QWidget* parent = 0);
  ~OverwriteInfoDialog();

  ModInfo::Ptr modInfo() const { return m_ModInfo; }

  // saves geometry
  //
  void done(int r) override;

  void setModInfo(ModInfo::Ptr modInfo);

protected:
  // restores geometry
  //
  void showEvent(QShowEvent* e) override;

private:
  void openFile(const QModelIndex& index);
  bool recursiveDelete(const QModelIndex& index);
  void deleteFile(const QModelIndex& index);

private slots:

  void delete_activated();

  void deleteTriggered();
  void renameTriggered();
  void openTriggered();
  void createDirectoryTriggered();

  void on_explorerButton_clicked();
  void on_filesView_customContextMenuRequested(const QPoint& pos);

private:
  Ui::OverwriteInfoDialog* ui;
  QFileSystemModel* m_FileSystemModel;
  QModelIndexList m_FileSelection;
  QAction* m_DeleteAction;
  QAction* m_RenameAction;
  QAction* m_OpenAction;
  QAction* m_NewFolderAction;

  ModInfo::Ptr m_ModInfo;
  OrganizerCore& m_Organizer;
};

#endif  // OVERWRITEINFODIALOG_H
