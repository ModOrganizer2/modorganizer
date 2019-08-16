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

#include "overwriteinfodialog.h"
#include "ui_overwriteinfodialog.h"
#include "report.h"
#include "utility.h"
#include "organizercore.h"
#include <QMessageBox>
#include <QMenu>
#include <QShortcut>
#include <Shlwapi.h>


using namespace MOBase;


class MyFileSystemModel : public QFileSystemModel {
public:
  MyFileSystemModel(QObject *parent)
    : QFileSystemModel(parent), m_RegularColumnCount(0) {}

  virtual int columnCount(const QModelIndex &parent) const {
    m_RegularColumnCount = QFileSystemModel::columnCount(parent);
    return m_RegularColumnCount;
  }

  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const {
    if ((orientation == Qt::Horizontal) &&
        (section >= m_RegularColumnCount)) {
      if (role == Qt::DisplayRole) {
        return tr("Overwrites");
      } else {
        return QVariant();
      }
    } else {
      return QFileSystemModel::headerData(section, orientation, role);
    }
  }

  virtual QVariant data(const QModelIndex &index, int role) const {
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

private:
  mutable int m_RegularColumnCount;
};


OverwriteInfoDialog::OverwriteInfoDialog(ModInfo::Ptr modInfo, QWidget *parent)
  : QDialog(parent), ui(new Ui::OverwriteInfoDialog), m_FileSystemModel(nullptr),
    m_DeleteAction(nullptr), m_RenameAction(nullptr), m_OpenAction(nullptr)
{
  ui->setupUi(this);

  this->setWindowModality(Qt::NonModal);

  m_FileSystemModel = new MyFileSystemModel(this);
  m_FileSystemModel->setReadOnly(false);
  setModInfo(modInfo);
  ui->filesView->setModel(m_FileSystemModel);
  ui->filesView->setRootIndex(m_FileSystemModel->index(modInfo->absolutePath()));
  ui->filesView->setColumnWidth(0, 250);

  m_DeleteAction = new QAction(tr("&Delete"), ui->filesView);
  m_RenameAction = new QAction(tr("&Rename"), ui->filesView);
  m_OpenAction = new QAction(tr("&Open"), ui->filesView);
  m_NewFolderAction = new QAction(tr("&New Folder"), ui->filesView);

  new QShortcut(QKeySequence::Delete, this, SLOT(delete_activated()));

  QObject::connect(m_DeleteAction, SIGNAL(triggered()), this, SLOT(deleteTriggered()));
  QObject::connect(m_RenameAction, SIGNAL(triggered()), this, SLOT(renameTriggered()));
  QObject::connect(m_OpenAction, SIGNAL(triggered()), this, SLOT(openTriggered()));
  QObject::connect(m_NewFolderAction, SIGNAL(triggered()), this, SLOT(createDirectoryTriggered()));
}

OverwriteInfoDialog::~OverwriteInfoDialog()
{
  delete ui;
}

void OverwriteInfoDialog::showEvent(QShowEvent* e)
{
  Settings::instance().restoreGeometry(this);
  QDialog::showEvent(e);
}

void OverwriteInfoDialog::done(int r)
{
  Settings::instance().saveGeometry(this);
  QDialog::done(r);
}

void OverwriteInfoDialog::setModInfo(ModInfo::Ptr modInfo)
{
  m_ModInfo = modInfo;
  if (QDir(modInfo->absolutePath()).exists()) {
    m_FileSystemModel->setRootPath(modInfo->absolutePath());
  } else {
    throw MyException(tr("mod not found: %1").arg(qUtf8Printable(modInfo->absolutePath())));
  }
}

bool OverwriteInfoDialog::recursiveDelete(const QModelIndex &index)
{
  for (int childRow = 0; childRow < m_FileSystemModel->rowCount(index); ++childRow) {
    QModelIndex childIndex = m_FileSystemModel->index(childRow, 0, index);
    if (m_FileSystemModel->isDir(childIndex)) {
      if (!recursiveDelete(childIndex)) {
        log::error("failed to delete {}", m_FileSystemModel->fileName(childIndex));
        return false;
      }
    } else {
      if (!m_FileSystemModel->remove(childIndex)) {
        log::error("failed to delete {}", m_FileSystemModel->fileName(childIndex));
        return false;
      }
    }
  }
  if (!m_FileSystemModel->remove(index)) {
    log::error("failed to delete {}", m_FileSystemModel->fileName(index));
    return false;
  }
  return true;
}


void OverwriteInfoDialog::deleteFile(const QModelIndex &index)
{

  bool res = m_FileSystemModel->isDir(index) ? recursiveDelete(index)
                                             : m_FileSystemModel->remove(index);
  if (!res) {
    QString fileName = m_FileSystemModel->fileName(index);
    reportError(tr("Failed to delete \"%1\"").arg(fileName));
  }
}

void OverwriteInfoDialog::delete_activated()
{
	if (ui->filesView->hasFocus()) {
		QItemSelectionModel *selection = ui->filesView->selectionModel();

		if (selection->hasSelection() && selection->selectedRows().count() >= 1) {

			if (selection->selectedRows().count() == 0) {
				return;
			}
			else if (selection->selectedRows().count() == 1) {
				QString fileName = m_FileSystemModel->fileName(selection->selectedRows().at(0));
				if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to delete \"%1\"?").arg(fileName),
					QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
					return;
				}
			}
			else {
				if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to delete the selected files?"),
					QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
					return;
				}
			}

			foreach(QModelIndex index, selection->selectedRows()) {
				deleteFile(index);
			}
		}
	}
}

void OverwriteInfoDialog::deleteTriggered()
{
  if (m_FileSelection.count() == 0) {
    return;
  } else if (m_FileSelection.count() == 1) {
    QString fileName = m_FileSystemModel->fileName(m_FileSelection.at(0));
    if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to delete \"%1\"?").arg(fileName),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
      return;
    }
  } else {
    if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to delete the selected files?"),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
      return;
    }
  }

  foreach(QModelIndex index, m_FileSelection) {
    deleteFile(index);
  }
}


void OverwriteInfoDialog::renameTriggered()
{
  QModelIndex selection = m_FileSelection.at(0);
  QModelIndex index = selection.sibling(selection.row(), 0);
  if (!index.isValid() || m_FileSystemModel->isReadOnly()) {
      return;
  }

  ui->filesView->edit(index);
}


void OverwriteInfoDialog::openFile(const QModelIndex &index)
{
  shell::OpenFile(m_FileSystemModel->filePath(index));
}


void OverwriteInfoDialog::openTriggered()
{
  foreach(QModelIndex idx, m_FileSelection) {
    openFile(idx);
  }
}

void OverwriteInfoDialog::createDirectoryTriggered()
{
  QModelIndex selection = m_FileSelection.at(0);

  QModelIndex index = m_FileSystemModel->isDir(selection) ? selection
                                                          : selection.parent();
  index = index.sibling(index.row(), 0);

  QString name = tr("New Folder");
  QString path = m_FileSystemModel->filePath(index).append("/");

  QModelIndex existingIndex = m_FileSystemModel->index(path + name);
  int suffix = 1;
  while (existingIndex.isValid()) {
    name = tr("New Folder") + QString::number(suffix++);
    existingIndex = m_FileSystemModel->index(path + name);
  }

  QModelIndex newIndex = m_FileSystemModel->mkdir(index, name);
  if (!newIndex.isValid()) {
    reportError(tr("Failed to create \"%1\"").arg(name));
    return;
  }

  ui->filesView->setCurrentIndex(newIndex);
  ui->filesView->edit(newIndex);
}

void OverwriteInfoDialog::on_explorerButton_clicked()
{
  shell::ExploreFile(m_ModInfo->absolutePath());
}

void OverwriteInfoDialog::on_filesView_customContextMenuRequested(const QPoint &pos)
{
  QItemSelectionModel *selectionModel = ui->filesView->selectionModel();
  m_FileSelection = selectionModel->selectedRows(0);

  QMenu menu(ui->filesView);

  menu.addAction(m_NewFolderAction);

  bool hasFiles = false;

  foreach(QModelIndex idx, m_FileSelection) {
    if (m_FileSystemModel->fileInfo(idx).isFile()) {
      hasFiles = true;
      break;
    }
  }

  if (selectionModel->hasSelection()) {
    if (hasFiles) {
      menu.addAction(m_OpenAction);
    }
    menu.addAction(m_RenameAction);
    menu.addAction(m_DeleteAction);
  } else {
    m_FileSelection.clear();
    m_FileSelection.append(m_FileSystemModel->index(m_FileSystemModel->rootPath(), 0));
  }

  menu.exec(ui->filesView->viewport()->mapToGlobal(pos));
}
