#include "modinfodialogfiletree.h"
#include "ui_modinfodialog.h"
#include "organizercore.h"
#include <utility.h>
#include <report.h>

using MOBase::reportError;
namespace shell = MOBase::shell;

FileTreeTab::FileTreeTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui)
    : ModInfoDialogTab(oc, plugin, parent, ui), m_fs(nullptr)
{
  m_fs = new QFileSystemModel(this);
  m_fs->setReadOnly(false);
  ui->fileTree1->setModel(m_fs);
  ui->fileTree1->setColumnWidth(0, 300);

  m_actions.newFolder = new QAction(tr("&New Folder"), ui->fileTree1);
  m_actions.open = new QAction(tr("&Open"), ui->fileTree1);
  m_actions.preview = new QAction(tr("&Preview"), ui->fileTree1);
  m_actions.rename = new QAction(tr("&Rename"), ui->fileTree1);
  m_actions.del = new QAction(tr("&Delete"), ui->fileTree1);
  m_actions.hide = new QAction(tr("&Hide"), ui->fileTree1);
  m_actions.unhide = new QAction(tr("&Unhide"), ui->fileTree1);

  connect(m_actions.newFolder, &QAction::triggered, [&]{ onCreateDirectory(); });
  connect(m_actions.open, &QAction::triggered, [&]{ onOpen(); });
  connect(m_actions.preview, &QAction::triggered, [&]{ onPreview(); });
  connect(m_actions.rename, &QAction::triggered, [&]{ onRename(); });
  connect(m_actions.del, &QAction::triggered, [&]{ onDelete(); });
  connect(m_actions.hide, &QAction::triggered, [&]{ onHide(); });
  connect(m_actions.unhide, &QAction::triggered, [&]{ onUnhide(); });
}

void FileTreeTab::clear()
{
  m_fs->setRootPath({});
  //ui->fileTree1->
}

void FileTreeTab::update()
{
  const auto rootPath = mod()->absolutePath();

  m_fs->setRootPath(rootPath);
  ui->fileTree1->setRootIndex(m_fs->index(rootPath));
}

QModelIndex FileTreeTab::singleSelection() const
{
  const auto rows = ui->fileTree1->selectionModel()->selectedRows();
  if (rows.size() != 1) {
    return {};
  }

  return rows[0];
}

void FileTreeTab::onCreateDirectory()
{
  auto selection = singleSelection();
  if (!selection.isValid()) {
    return;
  }

  QModelIndex index = m_fs->isDir(selection) ? selection : selection.parent();
  index = index.sibling(index.row(), 0);

  QString name = tr("New Folder");
  QString path = m_fs->filePath(index).append("/");

  QModelIndex existingIndex = m_fs->index(path + name);
  int suffix = 1;
  while (existingIndex.isValid()) {
    name = tr("New Folder") + QString::number(suffix++);
    existingIndex = m_fs->index(path + name);
  }

  QModelIndex newIndex = m_fs->mkdir(index, name);
  if (!newIndex.isValid()) {
    reportError(tr("Failed to create \"%1\"").arg(name));
    return;
  }

  ui->fileTree1->setCurrentIndex(newIndex);
  ui->fileTree1->edit(newIndex);
}

void FileTreeTab::onOpen()
{
  auto selection = singleSelection();
  if (!selection.isValid()) {
    return;
  }

  shell::OpenFile(m_fs->filePath(selection));
}

void FileTreeTab::onPreview()
{
  auto selection = singleSelection();
  if (!selection.isValid()) {
    return;
  }

  core().previewFile(parentWidget(), mod()->name(), m_fs->filePath(selection));
}

void FileTreeTab::onRename()
{
  auto selection = singleSelection();
  if (!selection.isValid()) {
    return;
  }

  QModelIndex index = selection.sibling(selection.row(), 0);
  if (!index.isValid() || m_fs->isReadOnly()) {
    return;
  }

  ui->fileTree1->edit(index);
}

void FileTreeTab::onDelete()
{
  const auto rows = ui->fileTree1->selectionModel()->selectedRows();
  if (rows.count() == 0) {
    return;
  }

  QString message;

  if (rows.count() == 1) {
    QString fileName = m_fs->fileName(rows[0]);
    message = tr("Are sure you want to delete \"%1\"?").arg(fileName);
  } else {
    message = tr("Are sure you want to delete the selected files?");
  }

  if (QMessageBox::question(parentWidget(), tr("Confirm"), message) != QMessageBox::Yes) {
    return;
  }

  foreach(QModelIndex index, m_FileSelection) {
    deleteFile(index);
  }
}


bool FileTreeTab::recursiveDelete(const QModelIndex &index)
{
  for (int childRow = 0; childRow < m_FileSystemModel->rowCount(index); ++childRow) {
    QModelIndex childIndex = m_FileSystemModel->index(childRow, 0, index);
    if (m_FileSystemModel->isDir(childIndex)) {
      if (!recursiveDelete(childIndex)) {
        qCritical("failed to delete %s", m_FileSystemModel->fileName(childIndex).toUtf8().constData());
        return false;
      }
    } else {
      if (!m_FileSystemModel->remove(childIndex)) {
        qCritical("failed to delete %s", m_FileSystemModel->fileName(childIndex).toUtf8().constData());
        return false;
      }
    }
  }
  if (!m_FileSystemModel->remove(index)) {
    qCritical("failed to delete %s", m_FileSystemModel->fileName(index).toUtf8().constData());
    return false;
  }
  return true;
}


void ModInfoDialog::on_openInExplorerButton_clicked()
{
  shell::ExploreFile(m_ModInfo->absolutePath());
}

void ModInfoDialog::deleteFile(const QModelIndex &index)
{
  bool res = m_FileSystemModel->isDir(index) ? recursiveDelete(index)
    : m_FileSystemModel->remove(index);
  if (!res) {
    QString fileName = m_FileSystemModel->fileName(index);
    reportError(tr("Failed to delete %1").arg(fileName));
  }
}

void ModInfoDialog::delete_activated()
{
  if (ui->fileTree->hasFocus()) {
    QItemSelectionModel *selection = ui->fileTree->selectionModel();

    if (selection->hasSelection() && selection->selectedRows().count() >= 1) {

      if (selection->selectedRows().count() == 0) {
        return;
      }
      else if (selection->selectedRows().count() == 1) {
        QString fileName = m_FileSystemModel->fileName(selection->selectedRows().at(0));
        if (QMessageBox::question(this, tr("Confirm"), tr("Are sure you want to delete \"%1\"?").arg(fileName),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
          return;
        }
      }
      else {
        if (QMessageBox::question(this, tr("Confirm"), tr("Are sure you want to delete the selected files?"),
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





void ModInfoDialog::hideTriggered()
{
  changeFiletreeVisibility(false);
}


void ModInfoDialog::unhideTriggered()
{
  changeFiletreeVisibility(true);
}

void ModInfoDialog::changeFiletreeVisibility(bool visible)
{
  bool changed = false;
  bool stop = false;

  qDebug().nospace()
    << (visible ? "unhiding" : "hiding") << " "
    << m_FileSelection.size() << " filetree files";

  QFlags<FileRenamer::RenameFlags> flags =
    (visible ? FileRenamer::UNHIDE : FileRenamer::HIDE);

  if (m_FileSelection.size() > 1) {
    flags |= FileRenamer::MULTIPLE;
  }

  FileRenamer renamer(this, flags);

  for (const auto& index : m_FileSelection) {
    if (stop) {
      break;
    }

    const QString path = m_FileSystemModel->filePath(index);
    auto result = FileRenamer::RESULT_CANCEL;

    if (visible) {
      if (!canUnhideFile(false, path)) {
        qDebug().nospace() << "cannot unhide " << path << ", skipping";
        continue;
      }
      result = unhideFile(renamer, path);
    } else {
      if (!canHideFile(false, path)) {
        qDebug().nospace() << "cannot hide " << path << ", skipping";
        continue;
      }
      result = hideFile(renamer, path);
    }

    switch (result) {
      case FileRenamer::RESULT_OK: {
        // will trigger a refresh at the end
        changed = true;
        break;
      }

      case FileRenamer::RESULT_SKIP: {
        // nop
        break;
      }

      case FileRenamer::RESULT_CANCEL: {
        // stop right now, but make sure to refresh if needed
        stop = true;
        break;
      }
    }
  }

  qDebug().nospace() << (visible ? "unhiding" : "hiding") << " filetree files done";

  if (changed) {
    qDebug().nospace() << "triggering refresh";
    if (m_Origin) {
      emit originModified(m_Origin->getID());
    }
    refreshLists();
  }
}




void ModInfoDialog::on_fileTree_customContextMenuRequested(const QPoint &pos)
{
  QItemSelectionModel *selectionModel = ui->fileTree->selectionModel();
  m_FileSelection = selectionModel->selectedRows(0);

  QMenu menu(ui->fileTree);

  menu.addAction(m_NewFolderAction);

  if (selectionModel->hasSelection()) {
    bool enableOpen = true;
    bool enablePreview = true;
    bool enableRename = true;
    bool enableDelete = true;
    bool enableHide = true;
    bool enableUnhide = true;

    if (m_FileSelection.size() == 1) {
      // single selection

      // only enable open action if a file is selected
      bool hasFiles = false;

      foreach(QModelIndex idx, m_FileSelection) {
        if (m_FileSystemModel->fileInfo(idx).isFile()) {
          hasFiles = true;
          break;
        }
      }

      if (!hasFiles) {
        enableOpen = false;
        enablePreview = false;
      }

      const QString fileName = m_FileSystemModel->fileName(m_FileSelection.at(0));

      if (!canPreviewFile(*m_PluginContainer, false, fileName)) {
        enablePreview = false;
      }

      if (!canHideFile(false, fileName)) {
        enableHide = false;
      }

      if (!canUnhideFile(false, fileName)) {
        enableUnhide = false;
      }
    } else {
      // this is a multiple selection, don't show open action so users don't open
      // a thousand files
      enableOpen = false;
      enablePreview = false;
      enableRename = false;

      if (m_FileSelection.size() < max_scan_for_context_menu) {
        // if the number of selected items is low, checking them to accurately
        // show the menu items is worth it
        enableHide = false;
        enableUnhide = false;

        for (const auto& index : m_FileSelection) {
          const QString fileName = m_FileSystemModel->fileName(index);

          if (canHideFile(false, fileName)) {
            enableHide = true;
          }

          if (canUnhideFile(false, fileName)) {
            enableUnhide = true;
          }

          if (enableHide && enableUnhide) {
            // found both, no need to check more
            break;
          }
        }
      }
    }

    if (enableOpen) {
      menu.addAction(m_OpenAction);
    }

    if (enablePreview) {
      menu.addAction(m_PreviewAction);
    }

    if (enableRename) {
      menu.addAction(m_RenameAction);
    }

    if (enableDelete) {
      menu.addAction(m_DeleteAction);
    }

    if (enableHide) {
      menu.addAction(m_HideAction);
    }

    if (enableUnhide) {
      menu.addAction(m_UnhideAction);
    }
  } else {
    m_FileSelection.clear();
    m_FileSelection.append(m_FileSystemModel->index(m_FileSystemModel->rootPath(), 0));
  }

  menu.exec(ui->fileTree->viewport()->mapToGlobal(pos));
}
