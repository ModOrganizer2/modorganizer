#include "filetreemodel.h"
#include "organizercore.h"
#include <log.h>

using namespace MOBase;
using namespace MOShared;

// in mainwindow.cpp
QString UnmanagedModName();


template <class F>
void trace(F&& f)
{
  //f();
}


FileTreeModel::FileTreeModel(OrganizerCore& core, QObject* parent) :
  QAbstractItemModel(parent), m_core(core),
  m_root(nullptr, 0, L"", L"", FileTreeItem::Directory, L"", L"<root>"),
  m_flags(NoFlags), m_isRefreshing(false)
{
  m_root.setExpanded(true);

  //connect(&m_iconPendingTimer, &QTimer::timeout, [&]{ updatePendingIcons(); });
  //
  //connect(
  //  this, &QAbstractItemModel::modelAboutToBeReset,
  //  [&]{ m_iconPending.clear(); });
  //
  //connect(
  //  this, &QAbstractItemModel::rowsAboutToBeRemoved,
  //  [&](auto&& parent, int first, int last){
  //    removePendingIcons(parent, first, last);
  //  });
}

void FileTreeModel::refresh()
{
  m_isRefreshing = true;
  Guard g([&]{ m_isRefreshing = false; });

  TimeThis tt("FileTreeModel::refresh()");
  update(m_root, *m_core.directoryStructure(), L"");
}

void FileTreeModel::clear()
{
  beginResetModel();
  m_root.clear();
  endResetModel();
}

bool FileTreeModel::showArchives() const
{
  return (m_flags & Archives) && m_core.getArchiveParsing();
}

QModelIndex FileTreeModel::index(
  int row, int col, const QModelIndex& parentIndex) const
{
  if (auto* parentItem=itemFromIndex(parentIndex)) {
    if (row < 0 || row >= parentItem->children().size()) {
      return {};
    }

    return createIndex(row, col, parentItem);
  }

  log::error("FileTreeModel::index(): parentIndex has no internal pointer");
  return {};
}

QModelIndex FileTreeModel::parent(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return {};
  }

  auto* parentItem = static_cast<FileTreeItem*>(index.internalPointer());
  if (!parentItem) {
    log::error("FileTreeModel::parent(): no internal pointer");
    return {};
  }

  return indexFromItem(*parentItem);
}

int FileTreeModel::rowCount(const QModelIndex& parent) const
{
  if (auto* item=itemFromIndex(parent)) {
    return static_cast<int>(item->children().size());
  }

  return 0;
}

int FileTreeModel::columnCount(const QModelIndex&) const
{
  return 2;
}

bool FileTreeModel::hasChildren(const QModelIndex& parent) const
{
  if (auto* item=itemFromIndex(parent)) {
    return item->hasChildren();
  }

  return false;
}

bool FileTreeModel::canFetchMore(const QModelIndex& parent) const
{
  if (auto* item=itemFromIndex(parent)) {
    return !item->isLoaded();
  }

  return false;
}

void FileTreeModel::fetchMore(const QModelIndex& parent)
{
  FileTreeItem* item = itemFromIndex(parent);
  if (!item) {
    return;
  }

  const auto path = item->dataRelativeFilePath();

  auto* parentEntry = m_core.directoryStructure()
    ->findSubDirectoryRecursive(path.toStdWString());

  if (!parentEntry) {
    log::error("FileTreeModel::fetchMore(): directory '{}' not found", path);
    return;
  }

  const auto parentPath = item->dataRelativeParentPath();

  update(*item, *parentEntry, parentPath.toStdWString());
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
  }

  return {};
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

FileTreeItem* FileTreeModel::itemFromIndex(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return &m_root;
  }

  auto* parentItem = static_cast<FileTreeItem*>(index.internalPointer());
  if (!parentItem) {
    log::error("FileTreeModel::itemFromIndex(): no internal pointer");
    return nullptr;
  }

  if (index.row() < 0 || index.row() >= parentItem->children().size()) {
    log::error(
      "FileeTreeModel::itemFromIndex(): row {} is out of range for {}",
      index.row(), parentItem->debugName());

    return nullptr;
  }

  return parentItem->children()[index.row()].get();
}

QModelIndex FileTreeModel::indexFromItem(FileTreeItem& item) const
{
  auto* parent = item.parent();
  if (!parent) {
    return {};
  }

  const int index = parent->childIndex(item);
  if (index == -1) {
    log::error(
      "FileTreeMode::indexFromItem(): item {} not found in parent",
      item.debugName());

    return {};
  }

  return createIndex(index, 0, parent);
}

void FileTreeModel::update(
  FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
  const std::wstring& parentPath)
{
  trace([&]{ log::debug("updating {}", parentItem.debugName()); });

  auto path = parentPath;
  if (!parentEntry.isTopLevel()) {
    if (!path.empty()) {
      path += L"\\";
    }

    path += parentEntry.getName();
  }

  updateDirectories(parentItem, path, parentEntry, FillFlag::None);
  updateFiles(parentItem, path, parentEntry);

  parentItem.setLoaded(true);
}


void FileTreeModel::updateDirectories(
  FileTreeItem& parentItem, const std::wstring& parentPath,
  const MOShared::DirectoryEntry& parentEntry, FillFlags flags)
{
  // removeDisappearingDirectories() will add directories that are in the
  // tree and still on the filesystem to this set; addNewDirectories() will
  // use this to figure out if a directory is new or not
  std::unordered_set<std::wstring_view> seen;

  removeDisappearingDirectories(parentItem, parentEntry, parentPath, seen);
  addNewDirectories(parentItem, parentEntry, parentPath, seen);
}

void FileTreeModel::removeDisappearingDirectories(
  FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
  const std::wstring& parentPath, std::unordered_set<std::wstring_view>& seen)
{
  auto& children = parentItem.children();
  auto itor = children.begin();

  // keeps track of the contiguous directories that need to be removed to
  // avoid calling beginRemoveRows(), etc. for each item
  int removeStart = -1;
  int row = 0;

  // for each item in this tree item
  while (itor != children.end()) {
    const auto& item = *itor;

    if (!item->isDirectory()) {
      // directories are always first, no point continuing once a file has
      // been seen
      break;
    }

    auto d = parentEntry.findSubDirectory(item->filenameWsLowerCase(), true);

    if (d) {
      trace([&]{ log::debug("dir {} still there", item->filename()); });

      // directory is still there
      seen.emplace(item->filenameWs());

      // if there were directories before this row that need to be removed,
      // do it now
      if (removeStart != -1) {
        removeRange(parentItem, removeStart, row - 1);

        // adjust current row to account for those that were just removed
        row -= (row - removeStart);
        itor = children.begin() + row;

        removeStart = -1;
      }

      if (item->areChildrenVisible()) {
        update(*item, *d, parentPath);
      }
    } else {
      // directory is gone from the parent entry
      trace([&]{ log::debug("dir {} is gone", item->filename()); });

      if (removeStart == -1) {
        // start a new contiguous sequence
        removeStart = row;
      }
    }

    ++row;
    ++itor;
  }

  // remove the last directory range, if any
  if (removeStart != -1) {
    removeRange(parentItem, removeStart, row -1 );
  }
}

void FileTreeModel::addNewDirectories(
  FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
  const std::wstring& parentPath,
  const std::unordered_set<std::wstring_view>& seen)
{
  // keeps track of the contiguous directories that need to be added to
  // avoid calling beginAddRows(), etc. for each item
  std::vector<std::unique_ptr<FileTreeItem>> toAdd;
  int addStart = -1;
  int row = 0;

  // for each directory on the filesystem
  for (auto&& d : parentEntry.getSubDirectories()) {
    if (seen.contains(d->getName())) {
      // already seen in the parent item

      // if there were directories before this row that need to be added,
      // do it now
      if (addStart != -1) {
        addRange(parentItem, addStart, toAdd);
        toAdd.clear();
        addStart = -1;
      }
    } else {
      // this is a new directory
      trace([&]{ log::debug("new dir {}", QString::fromStdWString(d->getName())); });

      auto item = std::make_unique<FileTreeItem>(
        &parentItem, 0, parentPath, L"", FileTreeItem::Directory,
        d->getName(), L"");

      if (d->isEmpty()) {
        // if this directory is empty, mark the item as loaded so the expand
        // arrow doesn't show
        item->setLoaded(true);
      }

      toAdd.push_back(std::move(item));

      if (addStart == -1) {
        // start a new contiguous sequence
        addStart = row;
      }
    }

    ++row;
  }

  // add the last directory range, if any
  if (addStart != -1) {
    addRange(parentItem, addStart, toAdd);
  }
}

void FileTreeModel::removeRange(FileTreeItem& parentItem, int first, int last)
{
  const auto parentIndex = indexFromItem(parentItem);

  beginRemoveRows(parentIndex, first, last);

  parentItem.remove(
    static_cast<std::size_t>(first),
    static_cast<std::size_t>(last - first + 1));

  endRemoveRows();
}

void FileTreeModel::addRange(
  FileTreeItem& parentItem, int at,
  std::vector<std::unique_ptr<FileTreeItem>>& items)
{
  const auto parentIndex = indexFromItem(parentItem);
  beginInsertRows(parentIndex, at, at + static_cast<int>(items.size()) - 1);

  parentItem.insert(
    std::make_move_iterator(items.begin()),
    std::make_move_iterator(items.end()),
    static_cast<std::size_t>(at));

  endInsertRows();
}


void FileTreeModel::updateFiles(
  FileTreeItem& parentItem, const std::wstring& parentPath,
  const MOShared::DirectoryEntry& parentEntry)
{
  // removeDisappearingFiles() will add files that are in the tree and still on
  // the filesystem to this set; addNewFiless() will use this to figure out if
  // a file is new or not
  std::unordered_set<FileEntry::Index> seen;

  int firstFileRow = 0;

  removeDisappearingFiles(parentItem, parentEntry, firstFileRow, seen);
  addNewFiles(parentItem, parentEntry, parentPath, firstFileRow, seen);
}

void FileTreeModel::removeDisappearingFiles(
  FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
  int& firstFileRow, std::unordered_set<FileEntry::Index>& seen)
{
  auto& children = parentItem.children();
  auto itor = children.begin();

  // keeps track of the contiguous directories that need to be removed to
  // avoid calling beginRemoveRows(), etc. for each item
  int removeStart = -1;
  firstFileRow = 0;

  // directories are always first, so find the first file item
  while (itor != children.end()) {
    const auto& item = *itor;

    if (!item->isDirectory()) {
      break;
    }

    ++firstFileRow;
    ++itor;
  }

  if (itor == children.end()) {
    // no file items
    return;
  }

  int row = firstFileRow;

  // for each item in this tree item
  while (itor != children.end()) {
    const auto& item = *itor;

    auto f = parentEntry.findFile(item->key());

    if (f) {
      trace([&]{ log::debug("file {} still there", item->filename()); });

      // file is still there
      seen.emplace(f->getIndex());

      // if there were files before this row that need to be removed,
      // do it now
      if (removeStart != -1) {
        removeRange(parentItem, removeStart, row - 1);

        // adjust current row to account for those that were just removed
        row -= (row - removeStart);
        itor = children.begin() + row;

        removeStart = -1;
      }
    } else {
      // file is gone from the parent entry
      trace([&]{ log::debug("file {} is gone", item->filename()); });

      if (removeStart == -1) {
        // start a new contiguous sequence
        removeStart = row;
      }
    }

    ++row;
    ++itor;
  }

  // remove the last file range, if any
  if (removeStart != -1) {
    removeRange(parentItem, removeStart, row -1 );
  }
}

void FileTreeModel::addNewFiles(
  FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
  const std::wstring& parentPath, const int firstFileRow,
  const std::unordered_set<FileEntry::Index>& seen)
{
  // keeps track of the contiguous files that need to be added to
  // avoid calling beginAddRows(), etc. for each item
  std::vector<std::unique_ptr<FileTreeItem>> toAdd;
  int addStart = -1;
  int row = firstFileRow;

  // for each directory on the filesystem
  parentEntry.forEachFileIndex([&](auto&& fileIndex) {
    if (seen.contains(fileIndex)) {
      // already seen in the parent item

      // if there were directories before this row that need to be added,
      // do it now
      if (addStart != -1) {
        addRange(parentItem, addStart, toAdd);
        toAdd.clear();
        addStart = -1;
      }
    } else {
      const auto file = parentEntry.getFileByIndex(fileIndex);

      if (!file) {
        log::error(
          "FileTreeModel::addNewFiles(): file index {} in path {} not found",
          fileIndex, parentPath);

        return true;
      }

      // this is a new file
      trace([&]{ log::debug("new file {}", QString::fromStdWString(file->getName())); });

      bool isArchive = false;
      int originID = file->getOrigin(isArchive);

      FileTreeItem::Flags flags = FileTreeItem::NoFlags;

      if (isArchive) {
        flags |= FileTreeItem::FromArchive;
      }

      if (!file->getAlternatives().empty()) {
        flags |= FileTreeItem::Conflicted;
      }

      auto item = std::make_unique<FileTreeItem>(
        &parentItem, originID, parentPath, file->getFullPath(), flags,
        file->getName(), makeModName(*file, originID));

      item->setLoaded(true);

      toAdd.push_back(std::move(item));

      if (addStart == -1) {
        // start a new contiguous sequence
        addStart = row;
      }
    }

    ++row;

    return true;
  });

  // add the last file range, if any
  if (addStart != -1) {
    addRange(parentItem, addStart, toAdd);
  }
}

std::wstring FileTreeModel::makeModName(
  const MOShared::FileEntry& file, int originID) const
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
