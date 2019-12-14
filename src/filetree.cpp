#include "filetree.h"
#include "organizercore.h"
#include "modinfodialogfwd.h"
#include <log.h>

using namespace MOShared;
using namespace MOBase;

// in mainwindow.cpp
QString UnmanagedModName();


bool canPreviewFile(const PluginContainer& pc, const FileEntry& file)
{
  return canPreviewFile(
    pc, file.isFromArchive(), QString::fromStdWString(file.getName()));
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
  return (QString::fromStdWString(file.getName()).endsWith(ModInfo::s_HiddenExt));
}

bool canExploreFile(const FileEntry& file);
bool canHideFile(const FileEntry& file);
bool canUnhideFile(const FileEntry& file);



FileTreeItem::FileTreeItem()
  : m_flags(NoFlags), m_loaded(false)
{
}

FileTreeItem::FileTreeItem(
  FileTreeItem* parent, int originID,
  std::wstring dataRelativeParentPath, std::wstring realPath, Flags flags,
  std::wstring file, std::wstring mod) :
    m_parent(parent), m_originID(originID),
    m_virtualParentPath(QString::fromStdWString(dataRelativeParentPath)),
    m_realPath(QString::fromStdWString(realPath)),
    m_flags(flags),
    m_file(QString::fromStdWString(file)),
    m_mod(QString::fromStdWString(mod)),
    m_loaded(false)
{
}

void FileTreeItem::add(std::unique_ptr<FileTreeItem> child)
{
  m_children.push_back(std::move(child));
}

const std::vector<std::unique_ptr<FileTreeItem>>& FileTreeItem::children() const
{
  return m_children;
}

FileTreeItem* FileTreeItem::parent()
{
  return m_parent;
}

int FileTreeItem::originID() const
{
  return m_originID;
}

const QString& FileTreeItem::virtualParentPath() const
{
  return m_virtualParentPath;
}

QString FileTreeItem::virtualPath() const
{
  QString s = "Data\\";

  if (!m_virtualParentPath.isEmpty()) {
    s += m_virtualParentPath + "\\";
  }

  s += m_file;

  return s;
}

QString FileTreeItem::dataRelativeParentPath() const
{
  return m_virtualParentPath;
}

QString FileTreeItem::dataRelativeFilePath() const
{
  auto path = dataRelativeParentPath();
  if (!path.isEmpty()) {
    path += "\\";
  }

  return path += m_file;
}

const QString& FileTreeItem::realPath() const
{
  return m_realPath;
}

const QString& FileTreeItem::filename() const
{
  return m_file;
}

const QString& FileTreeItem::mod() const
{
  return m_mod;
}

QFont FileTreeItem::font() const
{
  QFont f;

  if (isFromArchive()) {
    f.setItalic(true);
  } else if (isHidden()) {
    f.setStrikeOut(true);
  }

  return f;
}

QFileIconProvider::IconType FileTreeItem::icon() const
{
  if (m_flags & Directory) {
    return QFileIconProvider::Folder;
  } else {
    return QFileIconProvider::File;
  }
}

bool FileTreeItem::isDirectory() const
{
  return (m_flags & Directory);
}

bool FileTreeItem::isFromArchive() const
{
  return (m_flags & FromArchive);
}

bool FileTreeItem::isConflicted() const
{
  return (m_flags & Conflicted);
}

bool FileTreeItem::isHidden() const
{
  return m_file.endsWith(ModInfo::s_HiddenExt);
}

bool FileTreeItem::hasChildren() const
{
  if (!isDirectory()) {
    return false;
  }

  if (isLoaded() && m_children.empty()) {
    return false;
  }

  return true;
}

void FileTreeItem::setLoaded(bool b)
{
  m_loaded = b;
}

bool FileTreeItem::isLoaded() const
{
  return m_loaded;
}

QString FileTreeItem::debugName() const
{
  return QString("%1(ld=%2,cs=%3)")
    .arg(virtualPath())
    .arg(m_loaded)
    .arg(m_children.size());
}


FileTreeModel::FileTreeModel(OrganizerCore& core, QObject* parent)
  : QAbstractItemModel(parent), m_core(core), m_flags(NoFlags)
{
  connect(&m_iconPendingTimer, &QTimer::timeout, [&]{ updatePendingIcons(); });
}

void FileTreeModel::setFlags(Flags f)
{
  m_flags = f;
}

bool FileTreeModel::showConflicts() const
{
  return (m_flags & Conflicts);
}

bool FileTreeModel::showArchives() const
{
  return (m_flags & Archives) && m_core.getArchiveParsing();
}

void FileTreeModel::refresh()
{
  beginResetModel();
  m_root = {nullptr, 0, L"", L"", FileTreeItem::Directory, L"Data", L"<root>"};
  fill(m_root, *m_core.directoryStructure(), L"");
  endResetModel();
}

void FileTreeModel::ensureLoaded(FileTreeItem* item) const
{
  if (!item) {
    log::error("ensureLoaded(): item is null");
    return;
  }

  if (item->isLoaded()) {
    return;
  }

  log::debug("{}: loading on demand", item->debugName());

  const auto path = item->dataRelativeFilePath();
  auto* dir = m_core.directoryStructure()->findSubDirectoryRecursive(
    path.toStdWString());

  if (!dir) {
    log::error("{}: directory '{}' not found", item->debugName(), path);
    return;
  }

  const_cast<FileTreeModel*>(this)
    ->fill(*item, *dir, item->dataRelativeParentPath().toStdWString());
}

void FileTreeModel::fill(
  FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
  const std::wstring& parentPath)
{
  std::wstring path = parentPath;

  if (!parentEntry.isTopLevel()) {
    if (!path.empty()) {
      path += L"\\";
    }

    path += parentEntry.getName();
  }

  const auto flags = FillFlag::PruneDirectories;

  std::vector<DirectoryEntry*>::const_iterator begin, end;
  parentEntry.getSubDirectories(begin, end);
  fillDirectories(parentItem, path, begin, end, flags);

  fillFiles(parentItem, path, parentEntry.getFiles(), flags);

  parentItem.setLoaded(true);
}

bool FileTreeModel::shouldShowFile(const FileEntry& file) const
{
  if (showConflicts() && (file.getAlternatives().size() == 0)) {
    return false;
  }

  bool isArchive = false;
  int originID = file.getOrigin(isArchive);
  if (!showArchives() && isArchive) {
    return false;
  }

  return true;
}

bool FileTreeModel::hasFilesAnywhere(const DirectoryEntry& dir) const
{
  bool foundFile = false;

  dir.forEachFile([&](auto&& f) {
    if (shouldShowFile(f)) {
      foundFile = true;

      // stop
      return false;
    }

    // continue
    return true;
  });

  if (foundFile) {
    return true;
  }

  std::vector<DirectoryEntry*>::const_iterator begin, end;
  dir.getSubDirectories(begin, end);

  for (auto itor=begin; itor!=end; ++itor) {
    if (hasFilesAnywhere(**itor)) {
      return true;
    }
  }

  return false;
}

void FileTreeModel::fillDirectories(
  FileTreeItem& parentItem, const std::wstring& path,
  DirectoryIterator begin, DirectoryIterator end, FillFlags flags)
{
  for (auto itor=begin; itor!=end; ++itor) {
    const auto& dir = **itor;

    if (flags & FillFlag::PruneDirectories) {
      if (!hasFilesAnywhere(dir)) {
        continue;
      }
    }

    auto child = std::make_unique<FileTreeItem>(
      &parentItem, 0, path, L"", FileTreeItem::Directory, dir.getName(), L"");

    if (dir.isEmpty()) {
      child->setLoaded(true);
    }

    parentItem.add(std::move(child));
  }
}

void FileTreeModel::fillFiles(
  FileTreeItem& parentItem, const std::wstring& path,
  const std::vector<FileEntry::Ptr>& files, FillFlags)
{
  for (auto&& file : files) {
    if (!shouldShowFile(*file)) {
      continue;
    }

    bool isArchive = false;
    int originID = file->getOrigin(isArchive);

    FileTreeItem::Flags flags = FileTreeItem::NoFlags;

    if (isArchive) {
      flags |= FileTreeItem::FromArchive;
    }

    if (!file->getAlternatives().empty()) {
      flags |= FileTreeItem::Conflicted;
    }

    parentItem.add(std::make_unique<FileTreeItem>(
      &parentItem, originID, path, file->getFullPath(), flags, file->getName(),
      makeModName(*file, originID)));
  }
}

std::wstring FileTreeModel::makeModName(const FileEntry& file, int originID) const
{
  static const std::wstring Unmanaged = UnmanagedModName().toStdWString();

  const auto origin = m_core.directoryStructure()->getOriginByID(originID);

  if (origin.getID() == 0) {
    return Unmanaged;
  }

  std::wstring name = origin.getName();

  const auto& archive = file.getArchive();
  if (!archive.first.empty()) {
    name += L" (" + archive.first + L")";
  }

  return name;
}

FileTreeItem* FileTreeModel::itemFromIndex(const QModelIndex& index) const
{
  auto* data = index.internalPointer();
  if (!data) {
    return nullptr;
  }

  return static_cast<FileTreeItem*>(data);
}

QModelIndex FileTreeModel::index(
  int row, int col, const QModelIndex& parentIndex) const
{
  FileTreeItem* parent = nullptr;

  if (!parentIndex.isValid()) {
    parent = &m_root;
  } else {
    parent = itemFromIndex(parentIndex);
  }

  if (!parent) {
    log::error("FileTreeModel::index(): parent is null");
    return {};
  }

  ensureLoaded(parent);

  if (static_cast<std::size_t>(row) >= parent->children().size()) {
    // don't warn if the tree hasn't been refreshed yet
    if (!m_root.children().empty()) {
      log::error(
        "FileTreeModel::index(): row {} is out of range for {}",
        row, parent->debugName());
    }

    return {};
  }

  if (col >= columnCount({})) {
    log::error(
      "FileTreeModel::index(): col {} is out of range for {}",
      col, parent->debugName());

    return {};
  }

  auto* item = parent->children()[static_cast<std::size_t>(row)].get();
  return createIndex(row, col, item);
}

QModelIndex FileTreeModel::parent(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return {};
  }

  auto* item = itemFromIndex(index);
  if (!item) {
    return {};
  }

  auto* parent = item->parent();
  if (!parent) {
    return {};
  }

  ensureLoaded(parent);

  int row = 0;
  for (auto&& child : parent->children()) {
    if (child.get() == item) {
      return createIndex(row, 0, parent);
    }

    ++row;
  }

  log::error(
    "FileTreeModel::parent(): item {} has no child {}",
    parent->debugName(), item->debugName());

  return {};
}

int FileTreeModel::rowCount(const QModelIndex& parent) const
{
  FileTreeItem* item = nullptr;

  if (!parent.isValid()) {
    item = &m_root;
  } else {
    item = itemFromIndex(parent);
  }

  if (!item) {
    return 0;
  }

  ensureLoaded(item);
  return static_cast<int>(item->children().size());
}

int FileTreeModel::columnCount(const QModelIndex&) const
{
  return 2;
}

bool FileTreeModel::hasChildren(const QModelIndex& parent) const
{
  const FileTreeItem* item = nullptr;

  if (!parent.isValid()) {
    item = &m_root;
  } else {
    item = itemFromIndex(parent);
  }

  if (!item) {
    return false;
  }

  return item->hasChildren();
}

QVariant FileTreeModel::data(const QModelIndex& index, int role) const
{
  switch (role)
  {
    case Qt::DisplayRole:
    {
      if (auto* item=itemFromIndex(index)) {
        if (index.column() == 0) {
          return item->filename();
        } else if (index.column() == 1) {
          return item->mod();
        }
      }

      break;
    }

    case Qt::FontRole:
    {
      if (auto* item=itemFromIndex(index)) {
        return item->font();
      }

      break;
    }

    case Qt::ToolTipRole:
    {
      if (auto* item=itemFromIndex(index)) {
        return makeTooltip(*item);
      }

      return {};
    }

    case Qt::ForegroundRole:
    {
      if (index.column() == 1) {
        if (auto* item=itemFromIndex(index)) {
          if (item->isConflicted()) {
            return QBrush(Qt::red);
          }
        }
      }

      break;
    }

    case Qt::DecorationRole:
    {
      if (index.column() == 0) {
        if (auto* item=itemFromIndex(index)) {
          return makeIcon(*item, index);
        }
      }

      break;
    }
  }

  return {};
}

QString FileTreeModel::makeTooltip(const FileTreeItem& item) const
{
  if (item.isDirectory()) {
    return {};
  }

  auto nowrap = [&](auto&& s) {
    return "<p style=\"white-space: pre; margin: 0; padding: 0;\">" + s + "</p>";
  };

  auto line = [&](auto&& caption, auto&& value) {
    if (value.isEmpty()) {
      return nowrap("<b>" + caption + ":</b>\n");
    } else {
      return nowrap("<b>" + caption + ":</b> " + value.toHtmlEscaped()) + "\n";
    }
  };

  static const QString ListStart =
    "<ul style=\""
      "margin-left: 20px; "
      "margin-top: 0; "
      "margin-bottom: 0; "
      "padding: 0; "
      "-qt-list-indent: 0;"
    "\">";

  static const QString ListEnd = "</ul>";


  QString s =
    line(tr("Virtual path"), item.virtualPath()) +
    line(tr("Real path"),    item.realPath()) +
    line(tr("From"),         item.mod());


  const auto file = m_core.directoryStructure()->searchFile(
    item.dataRelativeFilePath().toStdWString(), nullptr);

  if (file) {
    const auto alternatives = file->getAlternatives();
    QStringList list;

    for (auto&& alt : file->getAlternatives()) {
      const auto& origin = m_core.directoryStructure()->getOriginByID(alt.first);
      list.push_back(QString::fromStdWString(origin.getName()));
    }

    if (list.size() == 1) {
      s += line(tr("Also in"), list[0]);
    } else if (list.size() >= 2) {
      s += line(tr("Also in"), QString()) + ListStart;

      for (auto&& alt : list) {
        s += "<li>" + alt +"</li>";
      }

      s += ListEnd;
    }
  }

  return s;
}

QVariant FileTreeModel::makeIcon(
  const FileTreeItem& item, const QModelIndex& index) const
{
  if (item.isDirectory()) {
    return m_iconFetcher.genericDirectoryIcon();
  }

  auto v = m_iconFetcher.icon(item.realPath());
  if (!v.isNull()) {
    return v;
  }

  m_iconPending.push_back(index);
  m_iconPendingTimer.start(std::chrono::milliseconds(1));

  return m_iconFetcher.genericFileIcon();
}

void FileTreeModel::updatePendingIcons()
{
  for (auto&& index : m_iconPending) {
    emit dataChanged(index, index, {Qt::DecorationRole});
  }

  m_iconPending.clear();
  m_iconPendingTimer.stop();
}

QVariant FileTreeModel::headerData(int i, Qt::Orientation ori, int role) const
{
  if (role == Qt::DisplayRole) {
    if (i == 0) {
      return tr("File");
    } else if (i == 1) {
      return tr("Mod");
    }
  }

  return {};
}

Qt::ItemFlags FileTreeModel::flags(const QModelIndex& index) const
{
  auto f = QAbstractItemModel::flags(index);

  if (auto* item=itemFromIndex(index)) {
    if (!item->hasChildren()) {
      f |= Qt::ItemNeverHasChildren;
    }
  }

  return f;
}


FileTree::FileTree(OrganizerCore& core, PluginContainer& pc, QTreeView* tree)
  : m_core(core), m_plugins(pc), m_tree(tree), m_model(new FileTreeModel(core))
{
  m_tree->setModel(m_model);

  QObject::connect(
    m_tree, &QTreeWidget::customContextMenuRequested,
    [&](auto pos){ onContextMenu(pos); });
}

void FileTree::setFlags(FileTreeModel::Flags flags)
{
  m_model->setFlags(flags);
}

void FileTree::refresh()
{
  m_model->refresh();
}

FileTreeItem* FileTree::singleSelection()
{
  const auto sel = m_tree->selectionModel()->selectedRows();
  if (sel.size() == 1) {
    return m_model->itemFromIndex(sel[0]);
  }

  return nullptr;
}

void FileTree::open()
{
  if (auto* item=singleSelection()) {
    if (item->isFromArchive() || item->isDirectory()) {
      return;
    }

    const QString path = item->realPath();
    const QFileInfo targetInfo(path);

    m_core.processRunner()
      .setFromFile(m_tree->window(), targetInfo)
      .setHooked(false)
      .setWaitForCompletion(ProcessRunner::Refresh)
      .run();
  }
}

void FileTree::openHooked()
{
  if (auto* item=singleSelection()) {
    if (item->isFromArchive() || item->isDirectory()) {
      return;
    }

    const QString path = item->realPath();
    const QFileInfo targetInfo(path);

    m_core.processRunner()
      .setFromFile(m_tree->window(), targetInfo)
      .setHooked(true)
      .setWaitForCompletion(ProcessRunner::Refresh)
      .run();
  }
}

void FileTree::preview()
{
  if (auto* item=singleSelection()) {
    const QString path = item->dataRelativeFilePath();
    m_core.previewFileWithAlternatives(m_tree->window(), path);
  }
}

void FileTree::addAsExecutable()
{
  auto* item = singleSelection();
  if (!item) {
    return;
  }

  const QString path = item->realPath();
  const QFileInfo target(path);
  const auto fec = spawn::getFileExecutionContext(m_tree->window(), target);

  switch (fec.type)
  {
    case spawn::FileExecutionTypes::Executable:
    {
      const QString name = QInputDialog::getText(
        m_tree->window(), QObject::tr("Enter Name"),
        QObject::tr("Enter a name for the executable"),
        QLineEdit::Normal,
        target.completeBaseName());

      if (!name.isEmpty()) {
        //Note: If this already exists, you'll lose custom settings
        m_core.executablesList()->setExecutable(Executable()
          .title(name)
          .binaryInfo(fec.binary)
          .arguments(fec.arguments)
          .workingDirectory(target.absolutePath()));

        // todo
        //emit executablesChanged();
      }

      break;
    }

    case spawn::FileExecutionTypes::Other:  // fall-through
    default:
    {
      QMessageBox::information(
        m_tree->window(), QObject::tr("Not an executable"),
        QObject::tr("This is not a recognized executable."));

      break;
    }
  }
}

void FileTree::exploreOrigin()
{
}

void FileTree::openModInfo()
{
}

void FileTree::toggleVisibility()
{
}

void FileTree::hide()
{
}

void FileTree::unhide()
{
}

void FileTree::dumpToFile()
{
}

void FileTree::onContextMenu(const QPoint &pos)
{
  QMenu menu;

  if (auto* item=singleSelection()) {
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

void FileTree::addDirectoryMenus(QMenu&, FileTreeItem&)
{
  // noop
}

void FileTree::addFileMenus(QMenu& menu, const FileEntry& file, int originID)
{
  using namespace spawn;

  addOpenMenus(menu, file);

  menu.addSeparator();

  const QFileInfo target(QString::fromStdWString(file.getFullPath()));
  if (getFileExecutionType(target) == FileExecutionTypes::Executable) {
    menu.addAction(QObject::tr("&Add as Executable"), [&]{ addAsExecutable(); });
  }

  if (!file.isFromArchive()) {
    menu.addAction(QObject::tr("Open Origin in Explorer"), [&]{ exploreOrigin(); });
  }

  if (originID != 0) {
    menu.addAction(QObject::tr("Open Mod Info"), [&]{ openModInfo(); });
  }

  // offer to hide/unhide file, but not for files from archives
  if (!file.isFromArchive()) {
    if (isHidden(file)) {
      menu.addAction(QObject::tr("Un-Hide"), [&]{ hide(); });
    } else {
      menu.addAction(QObject::tr("Hide"), [&]{ unhide(); });
    }
  }
}

void FileTree::addOpenMenus(QMenu& menu, const MOShared::FileEntry& file)
{
  QAction* openMenu = nullptr;
  QAction* openHookedMenu = nullptr;

  if (canRunFile(file)) {
    openMenu = new QAction(QObject::tr("&Execute"), m_tree);
    openHookedMenu = new QAction(QObject::tr("Execute with &VFS"), m_tree);
  } else if (canOpenFile(file)) {
    openMenu = new QAction(QObject::tr("&Open"), m_tree);
    openHookedMenu = new QAction(QObject::tr("Open with &VFS"), m_tree);
  }

  QAction* previewMenu = nullptr;
  if (canPreviewFile(m_plugins, file)) {
    previewMenu = new QAction(QObject::tr("Preview"), m_tree);
  }

  if (openMenu && previewMenu) {
    if (m_core.settings().interface().doubleClicksOpenPreviews()) {
      menu.addAction(previewMenu);
      menu.addAction(openMenu);
    } else {
      menu.addAction(openMenu);
      menu.addAction(previewMenu);
    }
  } else {
    if (openMenu) {
      menu.addAction(openMenu);
    }

    if (previewMenu) {
      menu.addAction(previewMenu);
    }
  }

  if (openHookedMenu) {
    menu.addAction(openHookedMenu);
  }


  if (openMenu) {
    QObject::connect(openMenu, &QAction::triggered, [&]{ open(); });
  }

  if (openHookedMenu) {
    QObject::connect(openHookedMenu, &QAction::triggered, [&]{ openHooked(); });
  }

  if (previewMenu) {
    QObject::connect(previewMenu, &QAction::triggered, [&]{ preview(); });
  }


  // bold the first option
  auto* top = menu.actions()[0];
  auto f = top->font();
  f.setBold(true);
  top->setFont(f);
}

void FileTree::addCommonMenus(QMenu& menu)
{
  menu.addAction(QObject::tr("Write To File..."), [&]{ dumpToFile(); });
  menu.addAction(QObject::tr("Refresh"), [&]{ refresh(); });
}
