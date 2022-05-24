#include "modinfodialogfiletree.h"
#include "filerenamer.h"
#include "modinfodialog.h"
#include "organizercore.h"
#include "ui_modinfodialog.h"
#include <log.h>
#include <report.h>
#include <utility.h>

using namespace MOBase;
namespace shell = MOBase::shell;

// if there are more than 50 selected items in the filetree, don't bother
// checking whether menu items apply to them, just show all of them
const int max_scan_for_context_menu = 50;

FileTreeTab::FileTreeTab(ModInfoDialogTabContext cx)
    : ModInfoDialogTab(std::move(cx)), m_fs(nullptr)
{
  m_fs = new QFileSystemModel(this);
  m_fs->setReadOnly(false);
  ui->filetree->setModel(m_fs);
  ui->filetree->setColumnWidth(0, 300);

  m_actions.newFolder = new QAction(tr("&New Folder"), ui->filetree);
  m_actions.open      = new QAction(tr("&Open/Execute"), ui->filetree);
  m_actions.runHooked = new QAction(tr("Open with &VFS"), ui->filetree);
  m_actions.preview   = new QAction(tr("&Preview"), ui->filetree);
  m_actions.explore   = new QAction(tr("Open in &Explorer"), ui->filetree);
  m_actions.rename    = new QAction(tr("&Rename"), ui->filetree);
  m_actions.del       = new QAction(tr("&Delete"), ui->filetree);
  m_actions.hide      = new QAction(tr("&Hide"), ui->filetree);
  m_actions.unhide    = new QAction(tr("&Unhide"), ui->filetree);

  connect(m_actions.newFolder, &QAction::triggered, [&] {
    onCreateDirectory();
  });
  connect(m_actions.open, &QAction::triggered, [&] {
    onOpen();
  });
  connect(m_actions.runHooked, &QAction::triggered, [&] {
    onRunHooked();
  });
  connect(m_actions.preview, &QAction::triggered, [&] {
    onPreview();
  });
  connect(m_actions.explore, &QAction::triggered, [&] {
    onExplore();
  });
  connect(m_actions.rename, &QAction::triggered, [&] {
    onRename();
  });
  connect(m_actions.del, &QAction::triggered, [&] {
    onDelete();
  });
  connect(m_actions.hide, &QAction::triggered, [&] {
    onHide();
  });
  connect(m_actions.unhide, &QAction::triggered, [&] {
    onUnhide();
  });

  connect(ui->openInExplorer, &QToolButton::clicked, [&] {
    onOpenInExplorer();
  });

  connect(ui->filetree, &QTreeView::customContextMenuRequested, [&](const QPoint& pos) {
    onContextMenu(pos);
  });

  // disable renaming on double click, open the file instead
  ui->filetree->setEditTriggers(ui->filetree->editTriggers() &
                                (~QAbstractItemView::DoubleClicked));

  connect(ui->filetree, &QTreeView::activated, [&](auto&&) {
    onActivated();
  });
}

void FileTreeTab::clear()
{
  m_fs->setRootPath({});

  // always has data; even if the mod is empty, it still has a meta.ini
  setHasData(true);
}

void FileTreeTab::saveState(Settings& s)
{
  s.geometry().saveState(ui->filetree->header());
}

void FileTreeTab::restoreState(const Settings& s)
{
  if (!s.geometry().restoreState(ui->filetree->header())) {
    ui->filetree->sortByColumn(0, Qt::AscendingOrder);
  }
}

void FileTreeTab::update()
{
  const auto rootPath = mod().absolutePath();

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
  index             = index.sibling(index.row(), 0);

  QString name = tr("New Folder");
  QString path = m_fs->filePath(index).append("/");

  QModelIndex existingIndex = m_fs->index(path + name);
  int suffix                = 1;
  while (existingIndex.isValid()) {
    name          = tr("New Folder") + QString::number(suffix++);
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

void FileTreeTab::onActivated()
{
  auto selection = singleSelection();
  if (!selection.isValid()) {
    return;
  }

  // Don't open explorer on directories as we just want them to be expanded instead.
  if (m_fs->isDir(selection)) {
    return;
  }

  const auto path       = m_fs->filePath(selection);
  const auto tryPreview = core().settings().interface().doubleClicksOpenPreviews();

  if (tryPreview && canPreviewFile(plugins(), false, path)) {
    onPreview();
  } else {
    onOpen();
  }
}

void FileTreeTab::onOpen()
{
  auto selection = singleSelection();
  if (!selection.isValid()) {
    return;
  }

  const auto path = m_fs->filePath(selection);
  core()
      .processRunner()
      .setFromFile(parentWidget(), QFileInfo(path))
      .setHooked(false)
      .setWaitForCompletion()
      .run();
}

void FileTreeTab::onRunHooked()
{
  auto selection = singleSelection();
  if (!selection.isValid()) {
    return;
  }

  const auto path = m_fs->filePath(selection);
  core()
      .processRunner()
      .setFromFile(parentWidget(), QFileInfo(path))
      .setHooked(true)
      .setWaitForCompletion()
      .run();
}

void FileTreeTab::onPreview()
{
  auto selection = singleSelection();
  if (!selection.isValid()) {
    return;
  }

  core().previewFile(parentWidget(), mod().name(), m_fs->filePath(selection));
}

void FileTreeTab::onExplore()
{
  auto selection = singleSelection();

  if (selection.isValid()) {
    shell::Explore(m_fs->filePath(selection));
  } else {
    shell::Explore(mod().absolutePath());
  }
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
    message          = tr("Are you sure you want to delete \"%1\"?").arg(fileName);
  } else {
    message = tr("Are you sure you want to delete the selected files?");
  }

  if (QMessageBox::question(parentWidget(), tr("Confirm"), message) !=
      QMessageBox::Yes) {
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
  shell::Explore(mod().absolutePath());
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
  for (int row = 0; row < m_fs->rowCount(parent); ++row) {
    QModelIndex index = m_fs->index(row, 0, parent);

    if (m_fs->isDir(index)) {
      if (!deleteFileRecursive(index)) {
        log::error("failed to delete {}", m_fs->fileName(index));
        return false;
      }
    } else {
      if (!m_fs->remove(index)) {
        log::error("failed to delete {}", m_fs->fileName(index));
        return false;
      }
    }
  }

  if (!m_fs->remove(parent)) {
    log::error("failed to delete {}", m_fs->fileName(parent));
    return false;
  }

  return true;
}

void FileTreeTab::changeVisibility(bool visible)
{
  const auto selection = ui->filetree->selectionModel()->selectedRows();

  bool changed = false;
  bool stop    = false;

  log::debug("{} {} filetree files", (visible ? "unhiding" : "hiding"),
             selection.size());

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
    auto result        = FileRenamer::RESULT_CANCEL;

    if (visible) {
      if (!canUnhideFile(false, path)) {
        log::debug("cannot unhide {}, skipping", path);
        continue;
      }
      result = unhideFile(renamer, path);
    } else {
      if (!canHideFile(false, path)) {
        log::debug("cannot hide {}, skipping", path);
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

  log::debug("{} filetree files done", (visible ? "unhiding" : "hiding"));

  if (changed) {
    if (origin()) {
      emitOriginModified();
    }
  }
}

void FileTreeTab::onContextMenu(const QPoint& pos)
{
  const auto selection = ui->filetree->selectionModel()->selectedRows();

  QMenu menu(ui->filetree);

  bool enableNewFolder = false;
  bool enableRun       = false;
  bool enableOpen      = false;
  bool enablePreview   = false;
  bool enableExplore   = false;
  bool enableRename    = false;
  bool enableDelete    = false;
  bool enableHide      = false;
  bool enableUnhide    = false;

  if (selection.size() == 0) {
    // no selection, only new folder and explore
    enableNewFolder = true;
    enableExplore   = true;
  } else if (selection.size() == 1) {
    // single selection
    enableNewFolder = true;
    enableRename    = true;
    enableDelete    = true;

    // only enable open action if a file is selected
    bool hasFiles = false;

    const QString fileName = m_fs->fileName(selection[0]);

    if (m_fs->fileInfo(selection[0]).isFile()) {
      if (canRunFile(false, fileName)) {
        enableRun = true;
      } else if (canOpenFile(false, fileName)) {
        enableOpen = true;
      }
    }

    enablePreview = canPreviewFile(plugins(), false, fileName);
    enableExplore = canExploreFile(false, fileName);
    enableHide    = canHideFile(false, fileName);
    enableUnhide  = canUnhideFile(false, fileName);
  } else {
    // this is a multiple selection, don't show open or explore actions so users
    // don't open a thousand files
    enableNewFolder = true;
    enableDelete    = true;

    if (selection.size() < max_scan_for_context_menu) {
      // if the number of selected items is low, checking them to accurately
      // show the menu items is worth it

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

  bool enableRunHooked = false;

  if (enableRun || enableOpen) {
    if (auto* p = core().currentProfile()) {
      if (mod().canBeEnabled()) {
        const auto index = ModInfo::getIndex(mod().name());
        if (index == UINT_MAX) {
          log::error("mod '{}' not found (filetree)", mod().name());
        } else {
          enableRunHooked = p->modEnabled(index);
        }
      }
    }
  }

  if (enableRun) {
    m_actions.open->setText(tr("&Execute"));
    m_actions.runHooked->setText(tr("Execute with &VFS"));
  } else if (enableOpen) {
    m_actions.open->setText(tr("&Open"));
    m_actions.runHooked->setText(tr("Open with &VFS"));
  }

  m_actions.preview->setEnabled(enablePreview);

  if ((enableRun || enableOpen) && enablePreview) {
    if (Settings::instance().interface().doubleClicksOpenPreviews()) {
      menu.addAction(m_actions.preview);
      menu.addAction(m_actions.open);
    } else {
      menu.addAction(m_actions.open);
      menu.addAction(m_actions.preview);
    }
  } else {
    if (enableOpen || enableRun) {
      menu.addAction(m_actions.open);
    }

    if (enablePreview) {
      menu.addAction(m_actions.preview);
    }
  }

  if (enableRunHooked) {
    menu.addAction(m_actions.runHooked);
  }

  menu.addAction(m_actions.explore);
  m_actions.explore->setEnabled(enableExplore);

  menu.addSeparator();

  menu.addAction(m_actions.newFolder);
  m_actions.newFolder->setEnabled(enableNewFolder);

  menu.addAction(m_actions.rename);
  m_actions.rename->setEnabled(enableRename);

  menu.addAction(m_actions.del);
  m_actions.del->setEnabled(enableDelete);

  menu.addSeparator();

  menu.addAction(m_actions.hide);
  m_actions.hide->setEnabled(enableHide);

  menu.addAction(m_actions.unhide);
  m_actions.unhide->setEnabled(enableUnhide);

  if (enableOpen || enableRun || enablePreview) {
    // bold the first option, unbold all the others
    for (int i = 0; i < menu.actions().size(); ++i) {
      if (auto* a = menu.actions()[i]) {
        auto f = a->font();
        f.setBold(i == 0);
        a->setFont(f);
      }
    }
  }

  menu.exec(ui->filetree->viewport()->mapToGlobal(pos));
}
