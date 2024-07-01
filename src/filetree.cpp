#include "filetree.h"
#include "envshell.h"
#include "filetreeitem.h"
#include "filetreemodel.h"
#include "organizercore.h"
#include "previewgenerator.h"
#include "shared/directoryentry.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"
#include <log.h>
#include <widgetutility.h>

using namespace MOShared;
using namespace MOBase;

bool canPreviewFile(const PluginManager& pc, const FileEntry& file)
{
  return canPreviewFile(pc, file.isFromArchive(),
                        QString::fromStdWString(file.getName()));
}

bool canRunFile(const FileEntry& file)
{
  return canRunFile(file.isFromArchive(), QString::fromStdWString(file.getName()));
}

bool canOpenFile(const FileEntry& file)
{
  return canOpenFile(file.isFromArchive(), QString::fromStdWString(file.getName()));
}

bool isHidden(const FileEntry& file)
{
  return (QString::fromStdWString(file.getName())
              .endsWith(ModInfo::s_HiddenExt, Qt::CaseInsensitive));
}

bool canExploreFile(const FileEntry& file);
bool canHideFile(const FileEntry& file);
bool canUnhideFile(const FileEntry& file);

class MenuItem
{
public:
  MenuItem(QString s = {}) : m_action(new QAction(std::move(s))) {}

  MenuItem& caption(const QString& s)
  {
    m_action->setText(s);
    return *this;
  }

  template <class F>
  MenuItem& callback(F&& f)
  {
    QObject::connect(m_action, &QAction::triggered, std::forward<F>(f));
    return *this;
  }

  MenuItem& hint(const QString& s)
  {
    m_tooltip = s;
    return *this;
  }

  MenuItem& disabledHint(const QString& s)
  {
    m_disabledHint = s;
    return *this;
  }

  MenuItem& enabled(bool b)
  {
    m_action->setEnabled(b);
    return *this;
  }

  void addTo(QMenu& menu)
  {
    QString s;

    setTips();

    m_action->setParent(&menu);
    menu.addAction(m_action);
  }

private:
  QAction* m_action;
  QString m_tooltip;
  QString m_disabledHint;

  void setTips()
  {
    if (m_action->isEnabled() || m_disabledHint.isEmpty()) {
      m_action->setStatusTip(m_tooltip);
      return;
    }

    QString s = m_tooltip.trimmed();

    if (!s.isEmpty()) {
      if (!s.endsWith(".")) {
        s += ".";
      }

      s += "\n";
    }

    s += QObject::tr("Disabled because") + ": " + m_disabledHint.trimmed();

    if (!s.endsWith(".")) {
      s += ".";
    }

    m_action->setStatusTip(s);
  }
};

FileTree::FileTree(OrganizerCore& core, PluginManager& pc, QTreeView* tree)
    : m_core(core), m_plugins(pc), m_tree(tree), m_model(new FileTreeModel(core))
{
  m_tree->sortByColumn(0, Qt::AscendingOrder);
  m_tree->setModel(m_model);
  m_tree->header()->resizeSection(0, 200);

  MOBase::setCustomizableColumns(m_tree);

  connect(m_tree, &QTreeView::customContextMenuRequested, [&](auto pos) {
    onContextMenu(pos);
  });

  connect(m_tree, &QTreeView::expanded, [&](auto&& index) {
    onExpandedChanged(index, true);
  });

  connect(m_tree, &QTreeView::collapsed, [&](auto&& index) {
    onExpandedChanged(index, false);
  });

  connect(m_tree, &QTreeView::activated, [&](auto&& index) {
    onItemActivated(index);
  });
}

FileTreeModel* FileTree::model()
{
  return m_model;
}

void FileTree::refresh()
{
  m_model->refresh();
}

void FileTree::clear()
{
  m_model->clear();
}

bool FileTree::fullyLoaded() const
{
  return m_model->fullyLoaded();
}

void FileTree::ensureFullyLoaded()
{
  m_model->ensureFullyLoaded();
}

FileTreeItem* FileTree::singleSelection()
{
  const auto sel = m_tree->selectionModel()->selectedRows();
  if (sel.size() == 1) {
    return m_model->itemFromIndex(proxiedIndex(sel[0]));
  }

  return nullptr;
}

void FileTree::open(FileTreeItem* item)
{
  if (!item) {
    item = singleSelection();

    if (!item) {
      return;
    }
  }

  if (item->isFromArchive() || item->isDirectory()) {
    return;
  }

  const QString path = item->realPath();
  const QFileInfo targetInfo(path);

  m_core.processRunner()
      .setFromFile(m_tree->window(), targetInfo)
      .setHooked(false)
      .setWaitForCompletion(ProcessRunner::TriggerRefresh)
      .run();
}

void FileTree::openHooked(FileTreeItem* item)
{
  if (!item) {
    item = singleSelection();

    if (!item) {
      return;
    }
  }

  if (item->isFromArchive() || item->isDirectory()) {
    return;
  }

  const QString path = item->realPath();
  const QFileInfo targetInfo(path);

  m_core.processRunner()
      .setFromFile(m_tree->window(), targetInfo)
      .setHooked(true)
      .setWaitForCompletion(ProcessRunner::TriggerRefresh)
      .run();
}

void FileTree::preview(FileTreeItem* item)
{
  if (!item) {
    item = singleSelection();

    if (!item) {
      return;
    }
  }

  const QString path = item->dataRelativeFilePath();
  m_core.previewFileWithAlternatives(m_tree->window(), path);
}

void FileTree::activate(FileTreeItem* item)
{
  if (item->isDirectory()) {
    // activating a directory should just toggle expansion
    return;
  }

  if (item->isFromArchive()) {
    log::warn("cannot activate file from archive '{}'", item->filename());
    return;
  }

  const auto tryPreview = m_core.settings().interface().doubleClicksOpenPreviews();

  if (tryPreview) {
    const QFileInfo fi(item->realPath());
    if (m_plugins.previewGenerator().previewSupported(fi.suffix().toLower())) {
      preview(item);
      return;
    }
  }

  open(item);
}

void FileTree::addAsExecutable(FileTreeItem* item)
{
  if (!item) {
    item = singleSelection();

    if (!item) {
      return;
    }
  }

  const QString path = item->realPath();
  const QFileInfo target(path);
  const auto fec = spawn::getFileExecutionContext(m_tree->window(), target);

  switch (fec.type) {
  case spawn::FileExecutionTypes::Executable: {
    const QString name = QInputDialog::getText(
        m_tree->window(), tr("Enter Name"), tr("Enter a name for the executable"),
        QLineEdit::Normal, target.completeBaseName());

    if (!name.isEmpty()) {
      // Note: If this already exists, you'll lose custom settings
      m_core.executablesList()->setExecutable(
          Executable()
              .title(name)
              .binaryInfo(fec.binary)
              .arguments(fec.arguments)
              .workingDirectory(target.absolutePath()));

      emit executablesChanged();
    }

    break;
  }

  case spawn::FileExecutionTypes::Other:  // fall-through
  default: {
    QMessageBox::information(m_tree->window(), tr("Not an executable"),
                             tr("This is not a recognized executable."));

    break;
  }
  }
}

void FileTree::exploreOrigin(FileTreeItem* item)
{
  if (!item) {
    item = singleSelection();

    if (!item) {
      return;
    }
  }

  if (item->isFromArchive() || item->isDirectory()) {
    return;
  }

  const auto path = item->realPath();

  log::debug("opening in explorer: {}", path);
  shell::Explore(path);
}

void FileTree::openModInfo(FileTreeItem* item)
{
  if (!item) {
    item = singleSelection();

    if (!item) {
      return;
    }
  }

  const auto originID = item->originID();

  if (originID == 0) {
    // unmanaged
    return;
  }

  const auto& origin = m_core.directoryStructure()->getOriginByID(originID);
  const auto& name   = QString::fromStdWString(origin.getName());

  unsigned int index = ModInfo::getIndex(name);
  if (index == UINT_MAX) {
    log::error("can't open mod info, mod '{}' not found", name);
    return;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  if (modInfo) {
    emit displayModInformation(modInfo, index, ModInfoTabIDs::None);
  }
}

void FileTree::toggleVisibility(bool visible, FileTreeItem* item)
{
  if (!item) {
    item = singleSelection();

    if (!item) {
      return;
    }
  }

  const QString currentName = item->realPath();
  QString newName;

  if (visible) {
    if (!currentName.endsWith(ModInfo::s_HiddenExt)) {
      log::error("cannot unhide '{}', doesn't end with '{}'", currentName,
                 ModInfo::s_HiddenExt);

      return;
    }

    newName = currentName.left(currentName.size() - ModInfo::s_HiddenExt.size());
  } else {
    if (currentName.endsWith(ModInfo::s_HiddenExt)) {
      log::error("cannot hide '{}', already ends with '{}'", currentName,
                 ModInfo::s_HiddenExt);

      return;
    }

    newName = currentName + ModInfo::s_HiddenExt;
  }

  log::debug("attempting to rename '{}' to '{}'", currentName, newName);

  FileRenamer renamer(m_tree->window(),
                      (visible ? FileRenamer::UNHIDE : FileRenamer::HIDE));

  if (renamer.rename(currentName, newName) == FileRenamer::RESULT_OK) {
    emit originModified(item->originID());
    refresh();
  }
}

void FileTree::hide(FileTreeItem* item)
{
  toggleVisibility(false, item);
}

void FileTree::unhide(FileTreeItem* item)
{
  toggleVisibility(true, item);
}

class DumpFailed
{};

void FileTree::dumpToFile() const
{
  log::debug("dumping filetree to file");

  QString file = QFileDialog::getSaveFileName(m_tree->window());
  if (file.isEmpty()) {
    log::debug("user canceled");
    return;
  }

  m_core.directoryStructure()->dump(file.toStdWString());
}

void FileTree::onExpandedChanged(const QModelIndex& index, bool expanded)
{
  if (auto* item = m_model->itemFromIndex(proxiedIndex(index))) {
    item->setExpanded(expanded);
  }
}

void FileTree::onItemActivated(const QModelIndex& index)
{
  auto* item = m_model->itemFromIndex(proxiedIndex(index));
  if (!item) {
    return;
  }

  activate(item);
}

void FileTree::onContextMenu(const QPoint& pos)
{
  const auto m = QApplication::keyboardModifiers();

  if (m & Qt::ShiftModifier) {
    // shift+right-click won't select individual items because it interferes
    // with regular shift selection; this makes it behave like explorer:
    //   - if the right-clicked item is currently part of the selection, show
    //     the menu for the selection
    //   - if not, select this item only and show the menu for it
    const auto index = m_tree->indexAt(pos);

    if (!m_tree->selectionModel()->isSelected(index)) {
      m_tree->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect |
                                                  QItemSelectionModel::Rows |
                                                  QItemSelectionModel::Current);
    }

    // if no shell menu was available, continue on and show the regular
    // context menu
    if (showShellMenu(pos)) {
      return;
    }
  }

  QMenu menu;

  if (auto* item = singleSelection()) {
    if (item->isDirectory()) {
      addDirectoryMenus(menu, *item);
    } else {
      const auto file = m_core.directoryStructure()->searchFile(
          item->dataRelativeFilePath().toStdWString(), nullptr);

      if (file) {
        addFileMenus(menu, *file, item->originID());
      }
    }
  }

  addCommonMenus(menu);

  menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

QMainWindow* getMainWindow(QWidget* w)
{
  QWidget* p = w;

  while (p) {
    if (auto* mw = dynamic_cast<QMainWindow*>(p)) {
      return mw;
    }

    p = p->parentWidget();
  }

  return nullptr;
}

bool FileTree::showShellMenu(QPoint pos)
{
  auto* mw = getMainWindow(m_tree);

  // menus by origin
  std::map<int, env::ShellMenu> menus;
  int totalFiles   = 0;
  bool warnOnEmpty = true;

  for (auto&& index : m_tree->selectionModel()->selectedRows()) {
    auto* item = m_model->itemFromIndex(proxiedIndex(index));
    if (!item) {
      continue;
    }

    if (item->isDirectory()) {
      warnOnEmpty = false;

      log::warn("directories do not have shell menus; '{}' selected", item->filename());

      continue;
    }

    if (item->isFromArchive()) {
      warnOnEmpty = false;

      log::warn("files from archives do not have shell menus; '{}' selected",
                item->filename());

      continue;
    }

    auto itor = menus.find(item->originID());
    if (itor == menus.end()) {
      itor = menus.emplace(item->originID(), mw).first;
    }

    if (!QFile::exists(item->realPath())) {
      log::error("{}", tr("File '%1' does not exist, you may need to refresh.")
                           .arg(item->realPath()));
    }

    itor->second.addFile(QFileInfo(item->realPath()));
    ++totalFiles;

    if (item->isConflicted()) {
      const auto file = m_core.directoryStructure()->searchFile(
          item->dataRelativeFilePath().toStdWString(), nullptr);

      if (!file) {
        log::error("file '{}' not found, data path={}, real path={}", item->filename(),
                   item->dataRelativeFilePath(), item->realPath());

        continue;
      }

      const auto alts = file->getAlternatives();
      if (alts.empty()) {
        log::warn("file '{}' has no alternative origins but is marked as conflicted",
                  item->dataRelativeFilePath());
      }

      for (auto&& alt : alts) {
        auto itor = menus.find(alt.originID());
        if (itor == menus.end()) {
          itor = menus.emplace(alt.originID(), mw).first;
        }

        const auto fullPath = file->getFullPath(alt.originID());
        if (fullPath.empty()) {
          log::error("file {} not found in origin {}", item->dataRelativeFilePath(),
                     alt.originID());

          continue;
        }

        if (!QFile::exists(QString::fromStdWString(fullPath))) {
          log::error("{}", tr("File '%1' does not exist, you may need to refresh.")
                               .arg(QString::fromStdWString(fullPath)));
        }

        itor->second.addFile(QFileInfo(QString::fromStdWString(fullPath)));
      }
    }
  }

  if (menus.empty()) {
    // don't warn if something that doesn't have a shell menu was selected, a
    // warning has already been logged above
    if (warnOnEmpty) {
      log::warn("no menus to show");
    }

    return false;
  } else if (menus.size() == 1) {
    auto& menu = menus.begin()->second;
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
  } else {
    env::ShellMenuCollection mc(mw);
    bool hasDiscrepancies = false;

    for (auto&& m : menus) {
      const auto* origin = m_core.directoryStructure()->findOriginByID(m.first);
      if (!origin) {
        log::error("origin {} not found for merged menus", m.first);
        continue;
      }

      QString caption = QString::fromStdWString(origin->getName());
      if (m.second.fileCount() < totalFiles) {
        const auto d = m.second.fileCount();
        caption += " " + tr("(only has %1 file(s))").arg(d);
        hasDiscrepancies = true;
      }

      mc.add(caption, std::move(m.second));
    }

    if (hasDiscrepancies) {
      mc.addDetails(tr("%1 file(s) selected").arg(totalFiles));
    }

    mc.exec(m_tree->viewport()->mapToGlobal(pos));
  }

  return true;
}

void FileTree::addDirectoryMenus(QMenu&, FileTreeItem&)
{
  // noop
}

void FileTree::addFileMenus(QMenu& menu, const FileEntry& file, int originID)
{
  using namespace spawn;

  addOpenMenus(menu, file);

  menu.addSeparator();
  menu.setToolTipsVisible(true);

  const QFileInfo target(QString::fromStdWString(file.getFullPath()));

  MenuItem(tr("&Add as Executable"))
      .callback([&] {
        addAsExecutable();
      })
      .hint(tr("Add this file to the executables list"))
      .disabledHint(tr("This file is not executable"))
      .enabled(getFileExecutionType(target) == FileExecutionTypes::Executable)
      .addTo(menu);

  MenuItem(tr("Reveal in E&xplorer"))
      .callback([&] {
        exploreOrigin();
      })
      .hint(tr("Opens the file in Explorer"))
      .disabledHint(tr("This file is in an archive"))
      .enabled(!file.isFromArchive())
      .addTo(menu);

  MenuItem(tr("Open &Mod Info"))
      .callback([&] {
        openModInfo();
      })
      .hint(tr("Opens the Mod Info Window"))
      .disabledHint(tr("This file is not in a managed mod"))
      .enabled(originID != 0)
      .addTo(menu);

  if (isHidden(file)) {
    MenuItem(tr("&Un-Hide"))
        .callback([&] {
          unhide();
        })
        .hint(tr("Un-hides the file"))
        .disabledHint(tr("This file is in an archive"))
        .enabled(!file.isFromArchive())
        .addTo(menu);
  } else {
    MenuItem(tr("&Hide"))
        .callback([&] {
          hide();
        })
        .hint(tr("Hides the file"))
        .disabledHint(tr("This file is in an archive"))
        .enabled(!file.isFromArchive())
        .addTo(menu);
  }
}

void FileTree::addOpenMenus(QMenu& menu, const MOShared::FileEntry& file)
{
  using namespace spawn;

  MenuItem openMenu, openHookedMenu;

  const QFileInfo target(QString::fromStdWString(file.getFullPath()));

  if (getFileExecutionType(target) == FileExecutionTypes::Executable) {
    openMenu.caption(tr("&Execute"))
        .callback([&] {
          open();
        })
        .hint(tr("Launches this program"))
        .disabledHint(tr("This file is in an archive"))
        .enabled(!file.isFromArchive());

    openHookedMenu.caption(tr("Execute with &VFS"))
        .callback([&] {
          openHooked();
        })
        .hint(tr("Launches this program hooked to the VFS"))
        .disabledHint(tr("This file is in an archive"))
        .enabled(!file.isFromArchive());
  } else {
    openMenu.caption(tr("&Open"))
        .callback([&] {
          open();
        })
        .hint(tr("Opens this file with its default handler"))
        .disabledHint(tr("This file is in an archive"))
        .enabled(!file.isFromArchive());

    openHookedMenu.caption(tr("Open with &VFS"))
        .callback([&] {
          openHooked();
        })
        .hint(tr("Opens this file with its default handler hooked to the VFS"))
        .disabledHint(tr("This file is in an archive"))
        .enabled(!file.isFromArchive());
  }

  MenuItem previewMenu(tr("&Preview"));
  previewMenu
      .callback([&] {
        preview();
      })
      .hint(tr("Previews this file within Mod Organizer"))
      .disabledHint(tr("This file is in an archive or has no preview handler "
                       "associated with it"))
      .enabled(canPreviewFile(m_plugins, file));

  if (m_core.settings().interface().doubleClicksOpenPreviews()) {
    previewMenu.addTo(menu);
    openMenu.addTo(menu);
    openHookedMenu.addTo(menu);
  } else {
    openMenu.addTo(menu);
    previewMenu.addTo(menu);
    openHookedMenu.addTo(menu);
  }

  // bold the first enabled option, only first three are considered
  for (int i = 0; i < 3; ++i) {
    if (i >= menu.actions().size()) {
      break;
    }

    auto* a = menu.actions()[i];

    if (menu.actions()[i]->isEnabled()) {
      auto f = a->font();
      f.setBold(true);
      a->setFont(f);
      break;
    }
  }
}

void FileTree::addCommonMenus(QMenu& menu)
{
  menu.addSeparator();

  MenuItem(tr("&Save Tree to Text File..."))
      .callback([&] {
        dumpToFile();
      })
      .hint(tr("Writes the list of files to a text file"))
      .addTo(menu);

  MenuItem(tr("&Refresh"))
      .callback([&] {
        refresh();
      })
      .hint(tr("Refreshes the list"))
      .addTo(menu);

  MenuItem(tr("Ex&pand All"))
      .callback([&] {
        expandAll();
      })
      .addTo(menu);

  MenuItem(tr("&Collapse All"))
      .callback([&] {
        collapseAll();
      })
      .addTo(menu);
}

QModelIndex FileTree::proxiedIndex(const QModelIndex& index)
{
  auto* model = m_tree->model();

  if (auto* proxy = dynamic_cast<QSortFilterProxyModel*>(model)) {
    return proxy->mapToSource(index);
  } else {
    return index;
  }
}

void FileTree::collapseAll()
{
  m_tree->collapseAll();
}

void FileTree::expandAll()
{
  m_model->aboutToExpandAll();
  m_tree->expandAll();
  m_model->expandedAll();
}
