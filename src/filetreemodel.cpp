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


// tracks a contiguous range in the model to avoid calling begin*Rows(), etc.
// for every single item that's added/removed
//
class FileTreeModel::Range
{
public:
  // note that file ranges can start from an index higher than 0 if there are
  // directories
  //
  Range(FileTreeModel* model, FileTreeItem& parentItem, int start=0)
    : m_model(model), m_parentItem(parentItem), m_first(-1), m_current(start)
  {
  }

  // includes the current index in the range
  //
  void includeCurrent()
  {
    // just remember the start of the range, m_current will be used in add()
    // or remove() to figure out the actual range
    if (m_first == -1) {
      m_first = m_current;
    }
  }

  // moves to the next row
  //
  void next()
  {
    ++m_current;
  }

  // returns the current row
  //
  int current() const
  {
    return m_current;
  }

  // adds the given items to this range
  //
  void add(FileTreeItem::Children toAdd)
  {
    if (m_first == -1) {
      // nothing to add
      Q_ASSERT(toAdd.empty());
      return;
    }

    const auto last = m_current - 1;
    const auto parentIndex = m_model->indexFromItem(m_parentItem);

    // make sure the number of items is the same as the size of this range
    Q_ASSERT(static_cast<int>(toAdd.size()) == (last - m_first));

    m_model->beginInsertRows(parentIndex, m_first, last);

    m_parentItem.insert(
      std::make_move_iterator(toAdd.begin()),
      std::make_move_iterator(toAdd.end()),
      static_cast<std::size_t>(m_first));

    m_model->endInsertRows();

    // reset
    m_first = -1;
  }

  // removes the item in this range, returns an iterator to first item passed
  // this range once removed, which can be end()
  //
  FileTreeItem::Children::const_iterator remove()
  {
    if (m_first == -1) {
      // nothing to remove
      return m_parentItem.children().begin() + m_current + 1;
    }

    const auto last = m_current - 1;
    const auto parentIndex = m_model->indexFromItem(m_parentItem);

    m_model->beginRemoveRows(parentIndex, m_first, last);

    m_parentItem.remove(
      static_cast<std::size_t>(m_first),
      static_cast<std::size_t>(last - m_first + 1));

    m_model->endRemoveRows();

    // adjust current row to account for those that were just removed
    m_current -= (m_current - m_first);

    // reset
    m_first = -1;

    return m_parentItem.children().begin() + m_current + 1;
  }

private:
  FileTreeModel* m_model;
  FileTreeItem& m_parentItem;
  int m_first;
  int m_current;
};


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
  Range range(this, parentItem);

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
      itor = range.remove();

      if (item->areChildrenVisible()) {
        update(*item, *d, parentPath);
      }
    } else {
      // directory is gone from the parent entry
      trace([&]{ log::debug("dir {} is gone", item->filename()); });

      range.includeCurrent();
      ++itor;
    }

    range.next();
  }

  // remove the last directory range, if any
  range.remove();
}

void FileTreeModel::addNewDirectories(
  FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
  const std::wstring& parentPath,
  const std::unordered_set<std::wstring_view>& seen)
{
  // keeps track of the contiguous directories that need to be added to
  // avoid calling beginAddRows(), etc. for each item
  Range range(this, parentItem);
  std::vector<std::unique_ptr<FileTreeItem>> toAdd;

  // for each directory on the filesystem
  for (auto&& d : parentEntry.getSubDirectories()) {
    if (seen.contains(d->getName())) {
      // already seen in the parent item

      // if there were directories before this row that need to be added,
      // do it now
      range.add(std::move(toAdd));
      toAdd.clear();
    } else {
      // this is a new directory
      trace([&]{ log::debug("new dir {}", QString::fromStdWString(d->getName())); });

      toAdd.push_back(createDirectoryItem(parentItem, parentPath, *d));
      range.includeCurrent();
    }

    range.next();
  }

  // add the last directory range, if any
  range.add(std::move(toAdd));
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

  firstFileRow = -1;

  // keeps track of the contiguous directories that need to be removed to
  // avoid calling beginRemoveRows(), etc. for each item
  Range range(this, parentItem);

  // for each item in this tree item
  while (itor != children.end()) {
    const auto& item = *itor;

    if (!item->isDirectory()) {
      if (firstFileRow == -1) {
        firstFileRow = range.current();
      }

      auto f = parentEntry.findFile(item->key());

      if (f) {
        trace([&]{ log::debug("file {} still there", item->filename()); });

        // file is still there
        seen.emplace(f->getIndex());

        // if there were files before this row that need to be removed,
        // do it now
        itor = range.remove();
      } else {
        // file is gone from the parent entry
        trace([&]{ log::debug("file {} is gone", item->filename()); });

        range.includeCurrent();
        ++itor;
      }
    } else {
      ++itor;
    }

    range.next();
  }

  // remove the last file range, if any
  range.remove();

  if (firstFileRow == -1) {
    firstFileRow = static_cast<int>(children.size());
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
  Range range(this, parentItem, firstFileRow);

  // for each directory on the filesystem
  parentEntry.forEachFileIndex([&](auto&& fileIndex) {
    if (seen.contains(fileIndex)) {
      // already seen in the parent item

      // if there were directories before this row that need to be added,
      // do it now
      range.add(std::move(toAdd));
      toAdd.clear();
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

      toAdd.push_back(createFileItem(parentItem, parentPath, *file));
      range.includeCurrent();
    }

    range.next();

    return true;
  });

  // add the last file range, if any
  range.add(std::move(toAdd));
}

std::unique_ptr<FileTreeItem> FileTreeModel::createDirectoryItem(
  FileTreeItem& parentItem, const std::wstring& parentPath,
  const DirectoryEntry& d)
{
  auto item = std::make_unique<FileTreeItem>(
    &parentItem, 0, parentPath, L"", FileTreeItem::Directory,
    d.getName(), L"");

  if (d.isEmpty()) {
    // if this directory is empty, mark the item as loaded so the expand
    // arrow doesn't show
    item->setLoaded(true);
  }

  return item;
}

std::unique_ptr<FileTreeItem> FileTreeModel::createFileItem(
  FileTreeItem& parentItem, const std::wstring& parentPath,
  const FileEntry& file)
{
  bool isArchive = false;
  int originID = file.getOrigin(isArchive);

  FileTreeItem::Flags flags = FileTreeItem::NoFlags;

  if (isArchive) {
    flags |= FileTreeItem::FromArchive;
  }

  if (!file.getAlternatives().empty()) {
    flags |= FileTreeItem::Conflicted;
  }

  auto item = std::make_unique<FileTreeItem>(
    &parentItem, originID, parentPath, file.getFullPath(), flags,
    file.getName(), makeModName(file, originID));

  item->setLoaded(true);

  return item;
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

QString FileTreeModel::makeTooltip(const FileTreeItem& item) const
{
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


  if (item.isDirectory()) {
    return
      line(tr("Directory"), item.filename()) +
      line(tr("Virtual path"), item.virtualPath());
  }


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
  return {};
}
