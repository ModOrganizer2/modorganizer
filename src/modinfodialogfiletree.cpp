#include "modinfodialogfiletree.h"
#include "ui_modinfodialog.h"
#include "modinfodialog.h"
#include "organizercore.h"
#include "filerenamer.h"
#include <utility.h>
#include <report.h>

using MOBase::reportError;
namespace shell = MOBase::shell;

// if there are more than 50 selected items in the filetree, don't bother
// checking whether menu items apply to them, just show all of them
const int max_scan_for_context_menu = 50;

FileTreeTab::FileTreeTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id)
    : ModInfoDialogTab(oc, plugin, parent, ui, id), m_fs(nullptr)
{
  m_fs = new QFileSystemModel(this);
  m_fs->setReadOnly(false);
  ui->filetree->setModel(m_fs);
  ui->filetree->setColumnWidth(0, 300);

  m_actions.newFolder = new QAction(tr("&New Folder"), ui->filetree);
  m_actions.open = new QAction(tr("&Open"), ui->filetree);
  m_actions.preview = new QAction(tr("&Preview"), ui->filetree);
  m_actions.rename = new QAction(tr("&Rename"), ui->filetree);
  m_actions.del = new QAction(tr("&Delete"), ui->filetree);
  m_actions.hide = new QAction(tr("&Hide"), ui->filetree);
  m_actions.unhide = new QAction(tr("&Unhide"), ui->filetree);

  connect(m_actions.newFolder, &QAction::triggered, [&]{ onCreateDirectory(); });
  connect(m_actions.open, &QAction::triggered, [&]{ onOpen(); });
  connect(m_actions.preview, &QAction::triggered, [&]{ onPreview(); });
  connect(m_actions.rename, &QAction::triggered, [&]{ onRename(); });
  connect(m_actions.del, &QAction::triggered, [&]{ onDelete(); });
  connect(m_actions.hide, &QAction::triggered, [&]{ onHide(); });
  connect(m_actions.unhide, &QAction::triggered, [&]{ onUnhide(); });

  connect(ui->openInExplorer, &QToolButton::clicked, [&]{ onOpenInExplorer(); });

  connect(
    ui->filetree, &QTreeView::customContextMenuRequested,
    [&](const QPoint& pos){ onContextMenu(pos); });
}

void FileTreeTab::clear()
{
  m_fs->setRootPath({});

  // always has data; even if the mod is empty, it still has a meta.ini
  setHasData(true);
}

void FileTreeTab::update()
{
  const auto rootPath = mod()->absolutePath();

  m_fs->setRootPath(rootPath);
  ui->filetree->setRootIndex(m_fs->index(rootPath));
}

bool FileTreeTab::deleteRequested()
{
  if (!ui->filetree->hasFocus()) {
    return false;
  }

  onDelete();
  return true;
}

QModelIndex FileTreeTab::singleSelection() const
{
  const auto rows = ui->filetree->selectionModel()->selectedRows();
  if (rows.size() != 1) {
    return {};
  }

  return rows[0];
}

void FileTreeTab::onCreateDirectory()
{
  const auto selectedRows = ui->filetree->selectionModel()->selectedRows();
  if (selectedRows.size() > 1) {
    return;
  }

  QModelIndex selection;

  if (selectedRows.size() == 0) {
    selection = m_fs->index(m_fs->rootPath(), 0);
  } else {
    selection = selectedRows[0];
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

  ui->filetree->setCurrentIndex(newIndex);
  ui->filetree->edit(newIndex);
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

  ui->filetree->edit(index);
}

void FileTreeTab::onDelete()
{
  const auto rows = ui->filetree->selectionModel()->selectedRows();
  if (rows.count() == 0) {
    return;
  }

  QString message;

  if (rows.count() == 1) {
    QString fileName = m_fs->fileName(rows[0]);
    message = tr("Are you sure you want to delete \"%1\"?").arg(fileName);
  } else {
    message = tr("Are you sure you want to delete the selected files?");
  }

  if (QMessageBox::question(parentWidget(), tr("Confirm"), message) != QMessageBox::Yes) {
    return;
  }

  for (const auto& index : rows) {
    deleteFile(index);
  }
}

void FileTreeTab::onHide()
{
  changeVisibility(false);
}

void FileTreeTab::onUnhide()
{
  changeVisibility(true);
}

void FileTreeTab::onOpenInExplorer()
{
  shell::ExploreFile(mod()->absolutePath());
}

bool FileTreeTab::deleteFile(const QModelIndex& index)
{
  bool res = false;

  if (m_fs->isDir(index)) {
    res = deleteFileRecursive(index);
  } else {
    res = m_fs->remove(index);
  }

  if (!res) {
    reportError(tr("Failed to delete %1").arg(m_fs->fileName(index)));
  }

  return res;
}

bool FileTreeTab::deleteFileRecursive(const QModelIndex& parent)
{
  for (int row = 0; row<m_fs->rowCount(parent); ++row) {
    QModelIndex index = m_fs->index(row, 0, parent);

    if (m_fs->isDir(index)) {
      if (!deleteFileRecursive(index)) {
        qCritical() << "failed to delete" << m_fs->fileName(index);
        return false;
      }
    } else {
      if (!m_fs->remove(index)) {
        qCritical() << "failed to delete", m_fs->fileName(index);
        return false;
      }
    }
  }

  if (!m_fs->remove(parent)) {
    qCritical() << "failed to delete" << m_fs->fileName(parent);
    return false;
  }

  return true;
}

void FileTreeTab::changeVisibility(bool visible)
{
  const auto selection = ui->filetree->selectionModel()->selectedRows();

  bool changed = false;
  bool stop = false;

  qDebug().nospace()
    << (visible ? "unhiding" : "hiding") << " "
    << selection.size() << " filetree files";

  QFlags<FileRenamer::RenameFlags> flags =
    (visible ? FileRenamer::UNHIDE : FileRenamer::HIDE);

  if (selection.size() > 1) {
    flags |= FileRenamer::MULTIPLE;
  }

  FileRenamer renamer(parentWidget(), flags);

  for (const auto& index : selection) {
    if (stop) {
      break;
    }

    const QString path = m_fs->filePath(index);
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
    if (origin()) {
      emitOriginModified();
    }
  }
}

void FileTreeTab::onContextMenu(const QPoint &pos)
{
  const auto selection = ui->filetree->selectionModel()->selectedRows();

  QMenu menu(ui->filetree);

  menu.addAction(m_actions.newFolder);

  if (selection.size() > 0) {
    bool enableOpen = true;
    bool enablePreview = true;
    bool enableRename = true;
    bool enableDelete = true;
    bool enableHide = true;
    bool enableUnhide = true;

    if (selection.size() == 1) {
      // single selection

      // only enable open action if a file is selected
      bool hasFiles = false;

      for (auto index : selection) {
        if (m_fs->fileInfo(index).isFile()) {
          hasFiles = true;
          break;
        }
      }

      if (!hasFiles) {
        enableOpen = false;
        enablePreview = false;
      }

      const QString fileName = m_fs->fileName(selection[0]);

      if (!canPreviewFile(plugin(), false, fileName)) {
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

      if (selection.size() < max_scan_for_context_menu) {
        // if the number of selected items is low, checking them to accurately
        // show the menu items is worth it
        enableHide = false;
        enableUnhide = false;

        for (const auto& index : selection) {
          const QString fileName = m_fs->fileName(index);

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
      menu.addAction(m_actions.open);
    }

    if (enablePreview) {
      menu.addAction(m_actions.preview);
    }

    if (enableRename) {
      menu.addAction(m_actions.rename);
    }

    if (enableDelete) {
      menu.addAction(m_actions.del);
    }

    if (enableHide) {
      menu.addAction(m_actions.hide);
    }

    if (enableUnhide) {
      menu.addAction(m_actions.unhide);
    }
  }

  menu.exec(ui->filetree->viewport()->mapToGlobal(pos));
}
