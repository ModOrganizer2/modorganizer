#include "filetreeitem.h"
#include "filetreemodel.h"
#include "modinfo.h"
#include "shared/util.h"
#include "modinfodialogfwd.h"
#include <log.h>
#include <utility.h>

using namespace MOBase;
using namespace MOShared;
namespace fs = std::filesystem;

constexpr bool AlwaysSortDirectoriesFirst = true;

const QString& directoryFileType()
{
  static const QString name = [] {
    const DWORD flags = SHGFI_TYPENAME;
    SHFILEINFOW sfi = {};

    // "." for the current directory, which should always exist
    const auto r = SHGetFileInfoW(L".", 0, &sfi, sizeof(sfi), flags);

    if (!r) {
      const auto e = GetLastError();

      log::error(
        "SHGetFileInfoW failed for folder file type, {}",
        formatSystemMessage(e));

      return QString("File folder");
    } else {
      return QString::fromWCharArray(sfi.szTypeName);
    }
  }();

  return name;
}

const QString& cachedFileTypeNoExtension()
{
  static const QString name = [] {
    const DWORD flags = SHGFI_TYPENAME;
    SHFILEINFOW sfi = {};

    // dummy filename with no extension
    const auto r = SHGetFileInfoW(L"file", 0, &sfi, sizeof(sfi), flags);

    if (!r) {
      const auto e = GetLastError();

      log::error(
        "SHGetFileInfoW failed for file without extension, {}",
        formatSystemMessage(e));

      return QString("File");
    } else {
      return QString::fromWCharArray(sfi.szTypeName);
    }
  }();

  return name;
}

const QString& cachedFileType(const std::wstring& file, bool isOnFilesystem)
{
  static std::map<std::wstring, QString, std::less<>> map;
  static std::mutex mutex;

  const auto dot = file.find_last_of(L'.');
  if (dot == std::wstring::npos) {
    return cachedFileTypeNoExtension();
  }

  std::scoped_lock lock(mutex);
  const auto sv = std::wstring_view(file.c_str() + dot, file.size() - dot);

  auto itor = map.find(sv);
  if (itor != map.end()) {
    return itor->second;
  }


  DWORD flags = SHGFI_TYPENAME;

  if (!isOnFilesystem) {
    // files from archives are not on the filesystem; this flag forces
    // SHGetFileInfoW() to only work with the filename
    flags |= SHGFI_USEFILEATTRIBUTES;
  }

  SHFILEINFOW sfi = {};
  const auto r = SHGetFileInfoW(file.c_str(), 0, &sfi, sizeof(sfi), flags);

  QString s;

  if (!r) {
    const auto e = GetLastError();

    log::error(
      "SHGetFileInfoW failed for '{}', {}",
      file, formatSystemMessage(e));

    s = cachedFileTypeNoExtension();
  } else {
    s = QString::fromWCharArray(sfi.szTypeName);
  }

  return map.emplace(sv, s).first->second;
}



FileTreeItem::FileTreeItem(
  FileTreeModel* model, FileTreeItem* parent,
  std::wstring dataRelativeParentPath, bool isDirectory, std::wstring file) :
    m_model(model), m_parent(parent), m_indexGuess(NoIndexGuess),
    m_virtualParentPath(QString::fromStdWString(dataRelativeParentPath)),
    m_wsFile(file),
    m_wsLcFile(ToLowerCopy(file)),
    m_key(m_wsLcFile),
    m_file(QString::fromStdWString(file)),
    m_isDirectory(isDirectory),
    m_originID(-1),
    m_flags(NoFlags),
    m_loaded(false),
    m_expanded(false),
    m_sortingStale(true)
{
}

FileTreeItem::Ptr FileTreeItem::createFile(
  FileTreeModel* model, FileTreeItem* parent,
  std::wstring dataRelativeParentPath, std::wstring file)
{
  return std::unique_ptr<FileTreeItem>(new FileTreeItem(
    model, parent, std::move(dataRelativeParentPath), false, std::move(file)));
}

FileTreeItem::Ptr FileTreeItem::createDirectory(
  FileTreeModel* model, FileTreeItem* parent,
  std::wstring dataRelativeParentPath, std::wstring file)
{
  return std::unique_ptr<FileTreeItem>(new FileTreeItem(
    model, parent, std::move(dataRelativeParentPath), true, std::move(file)));
}

void FileTreeItem::setOrigin(
  int originID, const std::wstring& realPath, Flags flags,
  const std::wstring& mod)
{
  m_originID = originID;
  m_wsRealPath = realPath;
  m_realPath = QString::fromStdWString(realPath);
  m_flags = flags;
  m_mod = QString::fromStdWString(mod);

  m_fileSize.reset();
  m_lastModified.reset();
  m_fileType.reset();
  m_compressedFileSize.reset();
}

void FileTreeItem::insert(FileTreeItem::Ptr child, std::size_t at)
{
  if (at > m_children.size()) {
    log::error(
      "{}: can't insert child {} at {}, out of range",
      debugName(), child->debugName(), at);

    return;
  }

  child->m_indexGuess = at;
  m_children.insert(m_children.begin() + at, std::move(child));
}

void FileTreeItem::remove(std::size_t i)
{
  if (i >= m_children.size()) {
    log::error("{}: can't remove child at {}", debugName(), i);
    return;
  }

  m_children.erase(m_children.begin() + i);
}

void FileTreeItem::remove(std::size_t from, std::size_t n)
{
  if ((from + n) > m_children.size()) {
    log::error("{}: can't remove children from {} n={}", debugName(), from, n);
    return;
  }

  auto begin = m_children.begin() + from;
  auto end = begin + n;

  m_children.erase(begin, end);
}


template <class T>
int threeWayCompare(T&& a, T&& b)
{
  if (a < b) {
    return -1;
  }

  if (a > b) {
    return 1;
  }

  return 0;
}

class FileTreeItem::Sorter
{
public:
  static int compare(int column, const FileTreeItem* a, const FileTreeItem* b)
  {
    switch (column)
    {
      case FileTreeModel::FileName:
        return naturalCompare(a->m_file, b->m_file);

      case FileTreeModel::ModName:
        return naturalCompare(a->m_mod, b->m_mod);

      case FileTreeModel::FileType:
        return naturalCompare(
          a->fileType().value_or(QString()),
          b->fileType().value_or(QString()));

      case FileTreeModel::FileSize:
        return threeWayCompare(
          a->fileSize().value_or(0),
          b->fileSize().value_or(0));

      case FileTreeModel::LastModified:
        return threeWayCompare(
          a->lastModified().value_or(QDateTime()),
          b->lastModified().value_or(QDateTime()));

      default:
        return 0;
    }
  }
};

void FileTreeItem::queueSort()
{
  if (!m_children.empty()) {
    m_model->queueSortItem(this);
  }
}

void FileTreeItem::makeSortingStale()
{
  m_sortingStale = true;

  for (auto& c : m_children) {
    c->makeSortingStale();
  }
}

void FileTreeItem::sort(int column, Qt::SortOrder order, bool force)
{
  if (!m_expanded) {
    m_sortingStale = true;
    return;
  }

  if (m_sortingStale || force) {
    //log::debug("sorting is stale for {}, sorting now", debugName());
    m_sortingStale = false;

    std::sort(m_children.begin(), m_children.end(), [&](auto&& a, auto&& b) {
      int r = 0;

      if (a->isDirectory() && !b->isDirectory()) {
        if constexpr (AlwaysSortDirectoriesFirst) {
          return true;
        } else {
          r = -1;
        }
      } else if (!a->isDirectory() && b->isDirectory()) {
        if constexpr (AlwaysSortDirectoriesFirst) {
          return false;
        } else {
          r = 1;
        }
      } else {
        r = FileTreeItem::Sorter::compare(column, a.get(), b.get());
      }

      if (order == Qt::AscendingOrder) {
        return (r < 0);
      } else {
        return (r > 0);
      }
    });
  }

  for (auto& child : m_children) {
    child->sort(column, order, force);
  }
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

QString FileTreeItem::dataRelativeFilePath() const
{
  auto path = dataRelativeParentPath();
  if (!path.isEmpty()) {
    path += "\\";
  }

  return path += m_file;
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

std::optional<uint64_t> FileTreeItem::fileSize() const
{
  if (m_fileSize.empty() && !m_isDirectory) {
    std::error_code ec;
    const auto size = fs::file_size(fs::path(m_wsRealPath), ec);

    if (ec) {
      log::error("can't get file size for '{}', {}", m_realPath, ec.message());
      m_fileSize.fail();
    } else {
      m_fileSize.set(size);
    }
  }

  return m_fileSize.value;
}

std::optional<QDateTime> FileTreeItem::lastModified() const
{
  if (m_lastModified.empty()) {
    if (m_realPath.isEmpty()) {
      // this is a virtual directory
      m_lastModified.set({});
    } else if (isFromArchive()) {
      // can't get last modified date for files in archives
      m_lastModified.set({});
    } else {
      // looks like a regular file on the filesystem
      const QFileInfo fi(m_realPath);
      const auto d = fi.lastModified();

      if (!d.isValid()) {
        log::error("can't get last modified date for '{}'", m_realPath);
        m_lastModified.fail();
      } else {
        m_lastModified.set(d);
      }
    }
  }

  return m_lastModified.value;
}

std::optional<QString> FileTreeItem::fileType() const
{
  if (m_fileType.empty()) {
    getFileType();
  }

  return m_fileType.value;
}

void FileTreeItem::getFileType() const
{
  if (isDirectory()) {
    m_fileType.set(directoryFileType());
    return;
  }

  const auto& t = cachedFileType(m_wsRealPath, !isFromArchive());
  if (t.isEmpty()) {
    m_fileType.fail();
  } else {
    m_fileType.set(t);
  }
}

QFileIconProvider::IconType FileTreeItem::icon() const
{
  if (m_isDirectory) {
    return QFileIconProvider::Folder;
  } else {
    return QFileIconProvider::File;
  }
}

bool FileTreeItem::isHidden() const
{
  return m_file.endsWith(ModInfo::s_HiddenExt, Qt::CaseInsensitive);
}

void FileTreeItem::unload()
{
  if (!m_loaded) {
    return;
  }

  clear();
}

bool FileTreeItem::areChildrenVisible() const
{
  if (m_expanded) {
    if (m_parent) {
      return m_parent->areChildrenVisible();
    } else {
      return true;
    }
  }

  return false;
}

QString FileTreeItem::debugName() const
{
  return QString("%1(ld=%2,cs=%3)")
    .arg(virtualPath())
    .arg(m_loaded)
    .arg(m_children.size());
}
