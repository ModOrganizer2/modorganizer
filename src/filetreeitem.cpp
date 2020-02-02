#include "filetreeitem.h"
#include "filetreemodel.h"
#include "modinfo.h"
#include "util.h"
#include "modinfodialogfwd.h"
#include <log.h>
#include <utility.h>

using namespace MOBase;
using namespace MOShared;
namespace fs = std::filesystem;

const QString& directoryFileType()
{
  static QString name;

  if (name.isEmpty()) {
    const DWORD flags = SHGFI_TYPENAME;
    SHFILEINFOW sfi = {};

    // "." for the current directory, which should always exist
    const auto r = SHGetFileInfoW(L".", 0, &sfi, sizeof(sfi), flags);

    if (!r) {
      const auto e = GetLastError();

      log::error(
        "SHGetFileInfoW failed for folder file type, {}",
        formatSystemMessage(e));

      name = "File folder";
    } else {
      name = QString::fromWCharArray(sfi.szTypeName);
    }
  }

  return name;
}


FileTreeItem::FileTreeItem(
  FileTreeItem* parent, int originID,
  std::wstring dataRelativeParentPath, std::wstring realPath, Flags flags,
  std::wstring file, std::wstring mod) :
    m_parent(parent), m_indexGuess(NoIndexGuess),
    m_originID(originID),
    m_virtualParentPath(QString::fromStdWString(dataRelativeParentPath)),
    m_wsRealPath(realPath),
    m_realPath(QString::fromStdWString(realPath)),
    m_flags(flags),
    m_wsFile(file),
    m_wsLcFile(ToLowerCopy(file)),
    m_key(m_wsLcFile),
    m_file(QString::fromStdWString(file)),
    m_mod(QString::fromStdWString(mod)),
    m_loaded(false),
    m_expanded(false)
{
}

FileTreeItem::Ptr FileTreeItem::create(
  FileTreeItem* parent, int originID,
  std::wstring dataRelativeParentPath, std::wstring realPath, Flags flags,
  std::wstring file, std::wstring mod)
{
  return std::unique_ptr<FileTreeItem>(new FileTreeItem(
    parent, originID, std::move(dataRelativeParentPath), std::move(realPath),
    flags, std::move(file), std::move(mod)));
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


void FileTreeItem::sort(int column, Qt::SortOrder order)
{
  std::sort(m_children.begin(), m_children.end(), [&](auto&& a, auto&& b) {
    int r = 0;

    if (a->isDirectory() && !b->isDirectory()) {
      r = -1;
    } else if (!a->isDirectory() && b->isDirectory()) {
      r = 1;
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
  if (m_fileSize.empty()) {
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

  DWORD flags = SHGFI_TYPENAME;

  if (isFromArchive()) {
    // files from archives are not on the filesystem; this flag forces
    // SHGetFileInfoW() to only work with the filename
    flags |= SHGFI_USEFILEATTRIBUTES;
  }

  SHFILEINFOW sfi = {};
  const auto r = SHGetFileInfoW(
    m_wsRealPath.c_str(), 0, &sfi, sizeof(sfi), flags);

  if (!r) {
    const auto e = GetLastError();

    log::error(
      "SHGetFileInfoW failed for '{}', {}",
      m_realPath, formatSystemMessage(e));

    m_fileType.fail();
  } else {
    m_fileType.set(QString::fromWCharArray(sfi.szTypeName));
  }
}

QFileIconProvider::IconType FileTreeItem::icon() const
{
  if (m_flags & Directory) {
    return QFileIconProvider::Folder;
  } else {
    return QFileIconProvider::File;
  }
}

bool FileTreeItem::isHidden() const
{
  return m_file.endsWith(ModInfo::s_HiddenExt);
}

void FileTreeItem::unload()
{
  if (!m_loaded) {
    return;
  }

  m_loaded = false;
  m_children.clear();
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
