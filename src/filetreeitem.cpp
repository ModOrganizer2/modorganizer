#include "filetreeitem.h"
#include "modinfo.h"
#include <log.h>

using namespace MOBase;

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
  m_loaded(false),
  m_expanded(false)
{
}

void FileTreeItem::add(std::unique_ptr<FileTreeItem> child)
{
  m_children.push_back(std::move(child));
}

void FileTreeItem::insert(std::unique_ptr<FileTreeItem> child, std::size_t at)
{
  if (at > m_children.size()) {
    log::error(
      "{}: can't insert child {} at {}, out of range",
      debugName(), child->debugName(), at);

    return;
  }

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

void FileTreeItem::unload()
{
  if (!m_loaded) {
    return;
  }

  m_loaded = false;
  m_children.clear();
}

void FileTreeItem::setExpanded(bool b)
{
  m_expanded = b;
}

bool FileTreeItem::isStrictlyExpanded() const
{
  return m_expanded;
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