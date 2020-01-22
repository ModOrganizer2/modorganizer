#include "filetreeitem.h"
#include "filetreemodel.h"
#include "modinfo.h"
#include "util.h"
#include "modinfodialogfwd.h"
#include <log.h>

using namespace MOBase;
using namespace MOShared;


FileTreeItem::FileTreeItem(
  FileTreeItem* parent, int originID,
  std::wstring dataRelativeParentPath, std::wstring realPath, Flags flags,
  std::wstring file, std::wstring mod) :
    m_parent(parent), m_indexGuess(NoIndexGuess),
    m_originID(originID),
    m_virtualParentPath(QString::fromStdWString(dataRelativeParentPath)),
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

void FileTreeItem::insert(std::unique_ptr<FileTreeItem> child, std::size_t at)
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
        return naturalCompare(a->meta().type, b->meta().type);

      case FileTreeModel::FileSize:
        return threeWayCompare(a->meta().size, b->meta().size);

      case FileTreeModel::LastModified:
        return threeWayCompare(a->meta().lastModified, b->meta().lastModified);

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

const FileTreeItem::Meta& FileTreeItem::meta() const
{
  if (!m_meta) {
    QFileInfo fi(m_realPath);

    SHFILEINFOW sfi = {};
    const auto r = SHGetFileInfoW(
      m_realPath.toStdWString().c_str(), 0, &sfi, sizeof(sfi),
      SHGFI_TYPENAME);

    if (!r) {
      const auto e = GetLastError();

      log::error(
        "SHGetFileInfoW failed for '{}', {}",
        m_realPath, e);

      sfi = {};
    }

    m_meta = {
      static_cast<uint64_t>(fi.size()),
      fi.lastModified(),
      QString::fromWCharArray(sfi.szTypeName)
    };
  }

  return *m_meta;
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
