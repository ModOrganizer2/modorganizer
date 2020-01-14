#include "filetreemodel.h"
#include "organizercore.h"
#include <log.h>
#include <unordered_set>

using namespace MOBase;
using namespace MOShared;

// in mainwindow.cpp
QString UnmanagedModName();


template <class F>
void trace(F&&)
{
  //f();
}


FileTreeModel::FileTreeModel(OrganizerCore& core, QObject* parent) :
  QAbstractItemModel(parent), m_core(core),
  m_root(nullptr, 0, L"", L"", FileTreeItem::Directory, L"", L"<root>"),
  m_flags(NoFlags), m_isRefreshing(false)
{
  m_root.setExpanded(true);

  connect(&m_iconPendingTimer, &QTimer::timeout, [&]{ updatePendingIcons(); });

  connect(
    this, &QAbstractItemModel::modelAboutToBeReset,
    [&]{ m_iconPending.clear(); });

  connect(
    this, &QAbstractItemModel::rowsAboutToBeRemoved,
    [&](auto&& parent, int first, int last){
      removePendingIcons(parent, first, last);
    });
}

void FileTreeModel::refresh()
{
  m_isRefreshing = true;
  Guard g([&]{ m_isRefreshing = false; });

  if (m_root.hasChildren()) {
    TimeThis tt("FileTreeModel::update()");
    update(m_root, *m_core.directoryStructure(), L"");
  } else {
    TimeThis tt("FileTreeModel::fill()");
    beginResetModel();
    m_root.clear();
    fill(m_root, *m_core.directoryStructure(), L"");
    endResetModel();
  }
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

void FileTreeModel::ensureLoaded(FileTreeItem* item) const
{
  if (m_isRefreshing) {
    return;
  }

  if (!item) {
    log::error("ensureLoaded(): item is null");
    return;
  }

  if (item->isLoaded()) {
    return;
  }

  trace([&]{ log::debug("{}: loading on demand", item->debugName()); });

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
  trace([&]{ log::debug("filling {}", parentItem.debugName()); });

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

void FileTreeModel::update(
  FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
  const std::wstring& parentPath)
{
  trace([&]{ log::debug("updating {}", parentItem.debugName()); });

  std::wstring path = parentPath;

  if (!parentEntry.isTopLevel()) {
    if (!path.empty()) {
      path += L"\\";
    }

    path += parentEntry.getName();
  }

  const auto flags = FillFlag::PruneDirectories;

  updateDirectories(parentItem, path, parentEntry, flags);
  updateFiles(parentItem, path, parentEntry, flags);
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

void FileTreeModel::updateDirectories(
  FileTreeItem& parentItem, const std::wstring& path,
  const MOShared::DirectoryEntry& parentEntry, FillFlags flags)
{
  trace([&]{ log::debug(
    "updating directories in {} from {}",
    parentItem.debugName(), (path.empty() ? L"\\" : path));
  });

  int row = 0;
  std::list<FileTreeItem*> remove;
  std::unordered_set<std::wstring_view> seen;

  for (auto&& item : parentItem.children()) {
    if (!item->isDirectory()) {
      break;
    }

    if (auto d=parentEntry.findSubDirectory(item->filenameWsLowerCase(), true)) {
      // directory still exists
      seen.insert(item->filenameWs());

      if (item->areChildrenVisible()) {
        trace([&]{ log::debug(
          "{} still exists and is expanded", item->debugName());
        });

        // node is expanded
        update(*item, *d, path);

        if (flags & FillFlag::PruneDirectories) {
          if (item->children().empty()) {
            trace([&]{ log::debug(
              "{} is now empty, will prune", item->debugName());
            });

            remove.push_back(item.get());
          }
        }
      } else {
        if ((flags & FillFlag::PruneDirectories) && !hasFilesAnywhere(*d)) {
          trace([&]{ log::debug(
            "{} still exists but is empty; pruning",
            item->debugName());
          });

          remove.push_back(item.get());
        } else if (item->isLoaded()) {
          trace([&]{ log::debug(
            "{} still exists, is loaded, but is not expanded; unloading",
            item->debugName());
          });

          // node is not expanded, unload

          bool mustEnd = false;

          if (!item->children().empty()) {
            const auto itemIndex = indexFromItem(item.get(), row, 0);
            const int first = 0;
            const int last = static_cast<int>(item->children().size());

            beginRemoveRows(itemIndex, first, last);
            mustEnd = true;
          }

          item->unload();

          if (mustEnd) {
            endRemoveRows();
          }

          if (d->isEmpty()) {
            item->setLoaded(true);
          }
        }
      }
    } else {
      // directory is gone
      trace([&]{ log::debug("{} is gone, removing", item->debugName()); });
      remove.push_back(item.get());
    }

    ++row;
  }

  if (!remove.empty()) {
    trace([&]{ log::debug(
      "{}: removing disappearing items",
      parentItem.debugName());
    });


    QModelIndex parentIndex;
    int first = -1;
    int last = -1;

    const auto& cs = parentItem.children();
    for (std::size_t i=0; i<cs.size(); ++i) {
      if (remove.empty()) {
        break;
      }

      for (auto itor=remove.begin(); itor!=remove.end(); ++itor) {
        auto* toRemove = *itor;

        if (cs[i].get() == toRemove) {
          if (!parentIndex.isValid()) {
            parentIndex = parent(indexFromItem(
              toRemove, static_cast<int>(i), 0));
          }

          if (first == -1) {
            first = i;
            last = i;
          } else if (i == (last + 1)) {
            last = i;
          } else {
            beginRemoveRows(parentIndex, first, last);

            parentItem.remove(
              static_cast<std::size_t>(first),
              static_cast<std::size_t>(last - first + 1));

            endRemoveRows();

            first = i;
            last = i;
          }

          remove.erase(itor);
          break;
        }
      }
    }

    if (first != -1) {
      beginRemoveRows(parentIndex, first, last);

      parentItem.remove(
        static_cast<std::size_t>(first),
        static_cast<std::size_t>(last - first + 1));

      endRemoveRows();
    }
  }


  std::vector<DirectoryEntry*>::const_iterator begin, end;
  parentEntry.getSubDirectories(begin, end);

  std::size_t insertPos = 0;
  for (auto itor=begin; itor!=end; ++itor) {
    const auto& dir = **itor;

    if (!seen.contains(dir.getName())) {
      trace([&]{ log::debug(
        "{}: new directory {}",
        parentItem.debugName(), QString::fromStdWString(dir.getName()));
      });

      if (flags & FillFlag::PruneDirectories) {
        if (!hasFilesAnywhere(dir)) {
          trace([&]{ log::debug("has no files and pruning is set, skipping"); });
          continue;
        }
      }

      auto child = std::make_unique<FileTreeItem>(
        &parentItem, 0, path, L"", FileTreeItem::Directory, dir.getName(), L"");

      if (dir.isEmpty()) {
        child->setLoaded(true);
      }

      QModelIndex parentIndex;

      if (parentItem.parent()) {
        const auto& cs = parentItem.parent()->children();

        for (std::size_t i=0; i<cs.size(); ++i) {
          if (cs[i].get() == &parentItem) {
            parentIndex = indexFromItem(&parentItem, static_cast<int>(i), 0);
            break;
          }
        }
      }

      const auto first = static_cast<int>(insertPos);
      const auto last = static_cast<int>(insertPos);

      trace([&]{ log::debug(
        "{}: inserting {} at {}",
        parentItem.debugName(), child->debugName(), insertPos);
      });

      beginInsertRows(parentIndex, first, last);
      parentItem.insert(std::move(child), insertPos);
      endInsertRows();
    }

    ++insertPos;
  }
}

void FileTreeModel::updateFiles(
  FileTreeItem& parentItem, const std::wstring& path,
  const MOShared::DirectoryEntry& parentEntry, FillFlags)
{
  trace([&]{ log::debug(
    "updating files in {} from {}",
    parentItem.debugName(), (path.empty() ? L"\\" : path));
  });

  std::unordered_set<FileEntry::Index> seen;
  std::vector<FileTreeItem*> remove;

  for (auto&& item : parentItem.children()) {
    if (item->isDirectory()) {
      continue;
    }

    if (auto f=parentEntry.findFile(item->key())) {
      if (shouldShowFile(*f)) {
        // file still exists
        trace([&]{ log::debug("{} still exists", item->debugName()); });
        seen.emplace(f->getIndex());
        continue;
      }
    }

    trace([&]{ log::debug("{} is gone", item->debugName()); });

    remove.push_back(item.get());
  }


  if (!remove.empty()) {
    trace([&]{ log::debug(
      "{}: removing disappearing items", parentItem.debugName());
    });

    for (auto* toRemove : remove) {
      const auto& cs = parentItem.children();

      for (std::size_t i=0; i<cs.size(); ++i) {
        if (cs[i].get() == toRemove) {
          const auto itemIndex = indexFromItem(
            toRemove, static_cast<int>(i), 0);

          const auto parentIndex = parent(itemIndex);
          const int first = static_cast<int>(i);
          const int last = static_cast<int>(i);

          beginRemoveRows(parentIndex, first, last);
          parentItem.remove(i);
          endRemoveRows();

          break;
        }
      }
    }
  }

  std::size_t firstFile = 0;
  for (std::size_t i=0; i<parentItem.children().size(); ++i) {
    if (!parentItem.children()[i]->isDirectory()) {
      break;
    }

    ++firstFile;
  }

  trace([&]{ log::debug(
    "{}: first file index is {}", parentItem.debugName(), firstFile);
  });

  std::size_t insertPos = firstFile;

  parentEntry.forEachFileIndex([&](auto&& fileIndex) {
    if (!seen.contains(fileIndex)) {
      const auto& file = parentEntry.getFileByIndex(fileIndex);
      if (!file) {
        return true;
      }

      if (shouldShowFile(*file)) {
        trace([&]{ log::debug(
          "{}: new file {}",
          parentItem.debugName(), QString::fromStdWString(file->getName()));
        });

        bool isArchive = false;
        int originID = file->getOrigin(isArchive);

        FileTreeItem::Flags flags = FileTreeItem::NoFlags;

        if (isArchive) {
          flags |= FileTreeItem::FromArchive;
        }

        if (!file->getAlternatives().empty()) {
          flags |= FileTreeItem::Conflicted;
        }

        auto child = std::make_unique<FileTreeItem>(
          &parentItem, originID, path, file->getFullPath(), flags, file->getName(),
          makeModName(*file, originID));

        trace([&]{ log::debug(
          "{}: inserting {} at {}",
          parentItem.debugName(), child->debugName(), insertPos);
        });

        QModelIndex parentIndex;

        if (parentItem.parent()) {
          const auto& cs = parentItem.parent()->children();

          for (std::size_t i=0; i<cs.size(); ++i) {
            if (cs[i].get() == &parentItem) {
              parentIndex = indexFromItem(&parentItem, static_cast<int>(i), 0);
              break;
            }
          }
        }

        const auto first = static_cast<int>(insertPos);
        const auto last = static_cast<int>(insertPos);

        beginInsertRows(parentIndex, first, last);
        parentItem.insert(std::move(child), insertPos);
        endInsertRows();
      } else {
        ++insertPos;
      }
    } else {
      ++insertPos;
    }

    return true;
  });
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

QModelIndex FileTreeModel::indexFromItem(
  FileTreeItem* item, int row, int col) const
{
  return createIndex(row, col, item);
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
  return indexFromItem(item, row, col);
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
  std::vector<QModelIndex> v(std::move(m_iconPending));
  m_iconPending.clear();

  for (auto&& index : v) {
    emit dataChanged(index, index, {Qt::DecorationRole});
  }

  if (m_iconPending.empty()) {
    m_iconPendingTimer.stop();
  }
}

void FileTreeModel::removePendingIcons(
  const QModelIndex& parent, int first, int last)
{
  auto itor = m_iconPending.begin();

  while (itor != m_iconPending.end()) {
    if (itor->parent() == parent) {
      if (itor->row() >= first && itor->row() <= last) {
        if (auto* item=itemFromIndex(*itor)) {
          log::debug("removing pending icon {}", item->debugName());
        } else {
          log::debug("removing pending icon (can't get item)");
        }

        itor = m_iconPending.erase(itor);
        continue;
      }
    }

    ++itor;
  }
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
