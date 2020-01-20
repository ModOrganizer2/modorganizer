#ifndef MODORGANIZER_FILETREEITEM_INCLUDED
#define MODORGANIZER_FILETREEITEM_INCLUDED

#include "directoryentry.h"
#include <QFileIconProvider>

class FileTreeItem
{
public:
  enum Flag
  {
    NoFlags     = 0x00,
    Directory   = 0x01,
    FromArchive = 0x02,
    Conflicted  = 0x04
  };

  Q_DECLARE_FLAGS(Flags, Flag);

  FileTreeItem(
    FileTreeItem* parent, int originID,
    std::wstring dataRelativeParentPath, std::wstring realPath, Flags flags,
    std::wstring file, std::wstring mod);

  FileTreeItem(const FileTreeItem&) = delete;
  FileTreeItem& operator=(const FileTreeItem&) = delete;
  FileTreeItem(FileTreeItem&&) = default;
  FileTreeItem& operator=(FileTreeItem&&) = default;

  void add(std::unique_ptr<FileTreeItem> child)
  {
    child->m_indexGuess = m_children.size();
    m_children.push_back(std::move(child));
  }

  void insert(std::unique_ptr<FileTreeItem> child, std::size_t at);

  template <class Itor>
  void insert(Itor begin, Itor end, std::size_t at)
  {
    std::size_t nextRowGuess = m_children.size();
    for (auto itor=begin; itor!=end; ++itor) {
      (*itor)->m_indexGuess = nextRowGuess++;
    }

    m_children.insert(m_children.begin() + at, begin, end);
  }

  void remove(std::size_t i);
  void remove(std::size_t from, std::size_t n);

  void clear()
  {
    m_children.clear();
    m_loaded = false;
  }

  const std::vector<std::unique_ptr<FileTreeItem>>& children() const
  {
    return m_children;
  }

  int childIndex(const FileTreeItem& item) const
  {
    if (item.m_indexGuess < m_children.size()) {
      if (m_children[item.m_indexGuess].get() == &item) {
        return static_cast<int>(item.m_indexGuess);
      }
    }

    for (std::size_t i=0; i<m_children.size(); ++i) {
      if (m_children[i].get() == &item) {
        item.m_indexGuess = i;
        return static_cast<int>(i);
      }
    }

    return -1;
  }

  FileTreeItem* parent()
  {
    return m_parent;
  }

  int originID() const
  {
    return m_originID;
  }

  const QString& virtualParentPath() const
  {
    return m_virtualParentPath;
  }

  QString virtualPath() const;

  const QString& filename() const
  {
    return m_file;
  }

  const std::wstring& filenameWs() const
  {
    return m_wsFile;
  }

  const std::wstring& filenameWsLowerCase() const
  {
    return m_wsLcFile;
  }

  const MOShared::DirectoryEntry::FileKey& key() const
  {
    return m_key;
  }

  const QString& mod() const
  {
    return m_mod;
  }

  QFont font() const;

  const QString& realPath() const
  {
    return m_realPath;
  }

  const QString& dataRelativeParentPath() const
  {
    return m_virtualParentPath;
  }

  QString dataRelativeFilePath() const;

  QFileIconProvider::IconType icon() const;

  bool isDirectory() const
  {
    return (m_flags & Directory);
  }

  bool isFromArchive() const
  {
    return (m_flags & FromArchive);
  }

  bool isConflicted() const
  {
    return (m_flags & Conflicted);
  }

  bool isHidden() const;

  bool hasChildren() const
  {
    if (!isDirectory()) {
      return false;
    }

    if (isLoaded() && m_children.empty()) {
      return false;
    }

    return true;
  }


  void setLoaded(bool b)
  {
    m_loaded = b;
  }

  bool isLoaded() const
  {
    return m_loaded;
  }

  void unload();

  void setExpanded(bool b)
  {
    m_expanded = b;
  }

  bool isStrictlyExpanded() const
  {
    return m_expanded;
  }

  bool areChildrenVisible() const;

  QString debugName() const;

private:
  static constexpr std::size_t NoIndexGuess =
    std::numeric_limits<std::size_t>::max();

  FileTreeItem* m_parent;
  mutable std::size_t m_indexGuess;

  const int m_originID;
  const QString m_virtualParentPath;
  const QString m_realPath;
  const Flags m_flags;
  const std::wstring m_wsFile, m_wsLcFile;
  const MOShared::DirectoryEntry::FileKey m_key;
  const QString m_file;
  const QString m_mod;

  bool m_loaded;
  bool m_expanded;
  std::vector<std::unique_ptr<FileTreeItem>> m_children;
};

#endif // MODORGANIZER_FILETREEITEM_INCLUDED
