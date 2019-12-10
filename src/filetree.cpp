#include "filetree.h"
#include "organizercore.h"
#include <log.h>

using namespace MOShared;
using namespace MOBase;

// in mainwindow.cpp
QString UnmanagedModName();


FileTreeItem::FileTreeItem()
  : m_flags(NoFlags), m_loaded(false)
{
}

FileTreeItem::FileTreeItem(
  FileTreeItem* parent,
  std::wstring virtualParentPath, std::wstring realPath, Flags flags,
  std::wstring file, std::wstring mod) :
    m_parent(parent),
    m_virtualParentPath(QString::fromStdWString(virtualParentPath)),
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

const QString& FileTreeItem::virtualParentPath() const
{
  return m_virtualParentPath;
}

QString FileTreeItem::virtualPath() const
{
  if (m_virtualParentPath.isEmpty()) {
    return m_file;
  } else {
    return m_virtualParentPath + "\\" + m_file;
  }
}

QString FileTreeItem::dataRelativeParentPath() const
{
  if (m_virtualParentPath == "data") {
    return "";
  }

  static const QString prefix = "data\\";

  auto path = m_virtualParentPath;
  if (path.startsWith(prefix)) {
    path = path.mid(prefix.size());
  }

  return path;
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


class FileTreeModel::IconFetcher
{
public:
  IconFetcher()
    : m_iconSize(GetSystemMetrics(SM_CXSMICON)), m_stop(false)
  {
    m_quickCache.file = getPixmapIcon(QFileIconProvider::File);
    m_quickCache.directory = getPixmapIcon(QFileIconProvider::Folder);

    m_thread = std::thread([&]{ threadFun(); });
  }

  ~IconFetcher()
  {
    stop();
    m_thread.join();
  }

  void stop()
  {
    m_stop = true;
    m_waiter.wakeUp();
  }

  QVariant icon(const QString& path) const
  {
    if (hasOwnIcon(path)) {
      return fileIcon(path);
    } else {
      const auto dot = path.lastIndexOf(".");

      if (dot == -1) {
        // no extension
        return m_quickCache.file;
      }

      return extensionIcon(path.midRef(dot));
    }
  }

  QPixmap genericFileIcon() const
  {
    return m_quickCache.file;
  }

  QPixmap genericDirectoryIcon() const
  {
    return m_quickCache.directory;
  }

private:
  struct QuickCache
  {
    QPixmap file;
    QPixmap directory;
  };

  struct Cache
  {
    std::map<QString, QPixmap, std::less<>> map;
    std::mutex mapMutex;

    std::set<QString> queue;
    std::mutex queueMutex;
  };

  struct Waiter
  {
    mutable std::mutex m_wakeUpMutex;
    std::condition_variable m_wakeUp;
    bool m_queueAvailable = false;

    void wait()
    {
      std::unique_lock lock(m_wakeUpMutex);
      m_wakeUp.wait(lock, [&]{ return m_queueAvailable; });
      m_queueAvailable = false;
    }

    void wakeUp()
    {
      {
        std::scoped_lock lock(m_wakeUpMutex);
        m_queueAvailable = true;
      }

      m_wakeUp.notify_one();
    }
  };

  const int m_iconSize;
  QFileIconProvider m_provider;
  std::thread m_thread;
  std::atomic<bool> m_stop;

  mutable QuickCache m_quickCache;
  mutable Cache m_extensionCache;
  mutable Cache m_fileCache;
  mutable Waiter m_waiter;


  bool hasOwnIcon(const QString& path) const
  {
    static const QString exe = ".exe";
    static const QString lnk = ".lnk";
    static const QString ico = ".ico";

    return
      path.endsWith(exe, Qt::CaseInsensitive) ||
      path.endsWith(lnk, Qt::CaseInsensitive) ||
      path.endsWith(ico, Qt::CaseInsensitive);
  }

  template <class T>
  QPixmap getPixmapIcon(T&& t) const
  {
    return m_provider.icon(t).pixmap({m_iconSize, m_iconSize});
  }

  void threadFun()
  {
    while (!m_stop) {
      m_waiter.wait();
      if (m_stop) {
        break;
      }

      checkCache(m_extensionCache);
      checkCache(m_fileCache);
    }
  }

  void checkCache(Cache& cache)
  {
    std::set<QString> queue;

    {
      std::scoped_lock lock(cache.queueMutex);
      queue = std::move(cache.queue);
    }

    if (queue.empty()) {
      return;
    }

    std::map<QString, QPixmap> map;
    for (auto&& ext : queue) {
      map.emplace(std::move(ext), getPixmapIcon(ext));
    }

    {
      std::scoped_lock lock(cache.mapMutex);
      for (auto&& p : map) {
        cache.map.insert(std::move(p));
      }
    }
  }

  void queue(Cache& cache, QString path) const
  {
    {
      std::scoped_lock lock(cache.queueMutex);
      cache.queue.insert(std::move(path));
    }

    m_waiter.wakeUp();
  }

  QVariant fileIcon(const QString& path) const
  {
    {
      std::scoped_lock lock(m_fileCache.mapMutex);
      auto itor = m_fileCache.map.find(path);
      if (itor != m_fileCache.map.end()) {
        return itor->second;
      }
    }

    queue(m_fileCache, path);
    return {};
  }

  QVariant extensionIcon(const QStringRef& ext) const
  {
    {
      std::scoped_lock lock(m_extensionCache.mapMutex);
      auto itor = m_extensionCache.map.find(ext);
      if (itor != m_extensionCache.map.end()) {
        return itor->second;
      }
    }

    queue(m_extensionCache, ext.toString());
    return {};
  }
};


FileTreeModel::FileTreeModel(OrganizerCore& core, QObject* parent)
  : QAbstractItemModel(parent), m_core(core), m_flags(NoFlags)
{
  m_iconFetcher.reset(new IconFetcher);
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
  return (m_flags & Archives);
}

void FileTreeModel::refresh()
{
  beginResetModel();
  m_root = {nullptr, L"", L"", FileTreeItem::Directory, L"Data", L""};
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
  const std::wstring path =
    parentPath +
    (parentPath.empty() ? L"" : L"\\") +
    parentEntry.getName();

  std::vector<DirectoryEntry*>::const_iterator begin, end;
  parentEntry.getSubDirectories(begin, end);
  fillDirectories(parentItem, path, begin, end);

  fillFiles(parentItem, path, parentEntry.getFiles());

  parentItem.setLoaded(true);
}

void FileTreeModel::fillDirectories(
  FileTreeItem& parentItem, const std::wstring& path,
  DirectoryIterator begin, DirectoryIterator end)
{
  for (auto itor=begin; itor!=end; ++itor) {
    const auto& dir = **itor;

    auto child = std::make_unique<FileTreeItem>(
      &parentItem, path, L"", FileTreeItem::Directory, dir.getName(), L"");

    if (dir.isEmpty()) {
      child->setLoaded(true);
    }

    parentItem.add(std::move(child));
  }
}

void FileTreeModel::fillFiles(
  FileTreeItem& parentItem, const std::wstring& path,
  const std::vector<FileEntry::Ptr>& files)
{
  for (auto&& file : files) {
    if (showConflicts() && (file->getAlternatives().size() == 0)) {
      continue;
    }

    bool isArchive = false;
    int originID = file->getOrigin(isArchive);
    if (!showArchives() && isArchive) {
      continue;
    }

    FileTreeItem::Flags flags = FileTreeItem::NoFlags;

    if (isArchive) {
      flags |= FileTreeItem::FromArchive;
    }

    if (!file->getAlternatives().empty()) {
      flags |= FileTreeItem::Conflicted;
    }

    parentItem.add(std::make_unique<FileTreeItem>(
      &parentItem, path, file->getFullPath(), flags, file->getName(),
      makeModName(*file, originID)));
  }
}

std::wstring FileTreeModel::makeModName(const FileEntry& file, int originID) const
{
  const auto origin = m_core.directoryStructure()->getOriginByID(originID);
  return origin.getName();

  //const auto index = ModInfo::getIndex(QString::fromStdWString(origin.getName()));
  //if (index == UINT_MAX) {
  //  return UnmanagedModName();
  //}
  //
  //std::wstring name = ModInfo::getByIndex(index)->name();
  //
  //std::pair<std::wstring, int> archive = file.getArchive();
  //if (archive.first.length() != 0) {
  //  name += L" (" + archive.first + L")";
  //}
  //
  //return name;
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
    log::error(
      "FileTreeModel::index(): row {} is out of range for {}",
      row, parent->debugName());

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
        QFont f;

        if (item->isFromArchive()) {
          f.setItalic(true);
        } else if (item->isHidden()) {
          f.setStrikeOut(true);
        }

        return f;
      }

      break;
    }

    case Qt::ToolTipRole:
    {
      /*
        const auto alternatives = file->getAlternatives();

        if (!alternatives.empty()) {
          std::wostringstream altString;
          altString << tr("Also in: <br>").toStdWString();
          for (std::vector<std::pair<int, std::pair<std::wstring, int>>>::iterator altIter = alternatives.begin();
            altIter != alternatives.end(); ++altIter) {
            if (altIter != alternatives.begin()) {
              altString << " , ";
            }
            altString << "<span style=\"white-space: nowrap;\"><i>" << m_core.directoryStructure()->getOriginByID(altIter->first).getName() << "</font></span>";
          }
          fileChild->setToolTip(1, QString("%1").arg(QString::fromStdWString(altString.str())));
          fileChild->setForeground(1, QBrush(Qt::red));
        } else {
          fileChild->setToolTip(1, tr("No conflict"));
        }
      */

      break;
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
          if (item->isDirectory()) {
            return m_iconFetcher->genericDirectoryIcon();
          } else {
            auto v = m_iconFetcher->icon(item->realPath());
            if (v.isNull()) {
              m_iconPending.push_back(index);
              m_iconPendingTimer.start(std::chrono::milliseconds(1));
              return m_iconFetcher->genericFileIcon();
            }
            return v;
          }
        }
      }

      break;
    }
  }

  return {};
}

void FileTreeModel::updatePendingIcons()
{
  log::debug("updating {} pending icons", m_iconPending.size());

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
