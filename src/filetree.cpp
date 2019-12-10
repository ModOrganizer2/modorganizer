#include "filetree.h"
#include "organizercore.h"

using namespace MOShared;

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

const QString& FileTreeItem::filename() const
{
  return m_file;
}

const QString& FileTreeItem::mod() const
{
  return m_mod;
}

bool FileTreeItem::isFromArchive() const
{
  return (m_flags & FromArchive);
}

bool FileTreeItem::isHidden() const
{
  return m_file.endsWith(ModInfo::s_HiddenExt);
}

bool FileTreeItem::isConflicted() const
{
  return (m_flags & Conflicted);
}

QFileIconProvider::IconType FileTreeItem::icon() const
{
  if (m_flags & Directory) {
    return QFileIconProvider::Folder;
  } else {
    return QFileIconProvider::File;
  }
}

void FileTreeItem::setLoaded(bool b)
{
  m_loaded = b;
}



FileTreeModel::FileTreeModel(OrganizerCore& core, QObject* parent)
  : QAbstractItemModel(parent), m_core(core), m_flags(NoFlags)
{
  QFileIconProvider provider;
  m_fileIcon = provider.icon(QFileIconProvider::File);
  m_directoryIcon = provider.icon(QFileIconProvider::Folder);
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
  m_root = {nullptr, L"", L"", FileTreeItem::Directory, L"Data", L""};

  fill(m_root, *m_core.directoryStructure(), L"");
  m_root.setLoaded(true);
}

void FileTreeModel::fill(
  FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
  const std::wstring& parentPath)
{
  const std::wstring path = parentPath + L"\\" + parentEntry.getName();
  bool isDirectory = true;


  {
    std::vector<DirectoryEntry*>::const_iterator begin, end;
    parentEntry.getSubDirectories(begin, end);
    fillDirectories(parentItem, path, begin, end);
  }

  {
    fillFiles(parentItem, path, parentEntry.getFiles());
  }
}

void FileTreeModel::fillDirectories(
  FileTreeItem& parentItem, const std::wstring& path,
  DirectoryIterator begin, DirectoryIterator end)
{
  const bool isDirectory = true;

  for (auto itor=begin; itor!=end; ++itor) {
    const auto& dir = **itor;

    auto child = std::make_unique<FileTreeItem>(
      &parentItem, path, L"", FileTreeItem::Directory, dir.getName(), L"");

    if (dir.isEmpty()) {
      child->setLoaded(true);
    } else if (showConflicts() || !showArchives()) {
      fill(*child, dir, path);
    }

    parentItem.add(std::move(child));
  }
}

void FileTreeModel::fillFiles(
  FileTreeItem& parentItem, const std::wstring& path,
  const std::vector<FileEntry::Ptr>& files)
{
  const bool isDirectory = false;

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

QModelIndex FileTreeModel::index(int row, int col, const QModelIndex& parentIndex) const
{
  FileTreeItem* parent = nullptr;

  if (!parentIndex.isValid()) {
    parent = &m_root;
  } else {
    parent = itemFromIndex(parentIndex);
  }

  if (!parent) {
    return {};
  }

  if (static_cast<std::size_t>(row) >= parent->children().size()) {
    return {};
  }

  if (col >= columnCount({})) {
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

  int row = 0;
  for (auto&& child : parent->children()) {
    if (child.get() == item) {
      return createIndex(row, 0, parent);
    }

    ++row;
  }

  return {};
}

int FileTreeModel::rowCount(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    return static_cast<int>(m_root.children().size());
  } else {
    if (auto* item=itemFromIndex(parent)) {
      return static_cast<int>(item->children().size());
    }
  }

  return 0;
}

int FileTreeModel::columnCount(const QModelIndex&) const
{
  return 2;
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
          const auto iconType = item->icon();

          if (iconType == QFileIconProvider::File) {
            return m_fileIcon;
          } else if (iconType == QFileIconProvider::Folder) {
            return m_directoryIcon;
          }
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
