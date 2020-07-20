#ifndef MODORGANIZER_FILETREEITEM_INCLUDED
#define MODORGANIZER_FILETREEITEM_INCLUDED

#include "shared/fileregisterfwd.h"
#include <QFileIconProvider>

class FileTreeModel;

class FileTreeItem
{
  class Sorter;

public:
  using Ptr = std::unique_ptr<FileTreeItem>;
  using Children = std::vector<Ptr>;

  enum Flag
  {
    NoFlags     = 0x00,
    FromArchive = 0x01,
    Conflicted  = 0x02
  };


  Q_DECLARE_FLAGS(Flags, Flag);

  static Ptr createFile(
    FileTreeModel* model, FileTreeItem* parent,
    std::wstring dataRelativeParentPath, std::wstring file);

  static Ptr createDirectory(
    FileTreeModel* model, FileTreeItem* parent,
    std::wstring dataRelativeParentPath, std::wstring file);

  FileTreeItem(const FileTreeItem&) = delete;
  FileTreeItem& operator=(const FileTreeItem&) = delete;
  FileTreeItem(FileTreeItem&&) = default;
  FileTreeItem& operator=(FileTreeItem&&) = default;

  void setOrigin(
    int originID, const std::wstring& realPath,
    Flags flags, const std::wstring& mod);

  void add(Ptr child)
  {
    child->m_indexGuess = m_children.size();
    m_children.push_back(std::move(child));
  }

  void insert(Ptr child, std::size_t at);

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

  const Children& children() const
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

  void sort(int column, Qt::SortOrder order, bool force);
  void makeSortingStale();

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

  const MOShared::DirectoryEntryFileKey& key() const
  {
    return m_key;
  }

  const QString& mod() const
  {
    return m_mod;
  }

  QFont font() const;

  std::optional<uint64_t> fileSize() const;
  std::optional<QDateTime> lastModified() const;
  std::optional<QString> fileType() const;

  std::optional<uint64_t> compressedFileSize() const
  {
    return m_compressedFileSize.value;
  }

  void setFileSize(uint64_t size)
  {
    m_fileSize.override(size);
  }

  void setCompressedFileSize(uint64_t compressedSize)
  {
    m_compressedFileSize.override(compressedSize);
  }

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
    return m_isDirectory;
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
    if (m_expanded == b) {
      return;
    }

    m_expanded = b;

    if (m_expanded && m_sortingStale) {
      queueSort();
    }
  }

  bool isStrictlyExpanded() const
  {
    return m_expanded;
  }

  bool areChildrenVisible() const;

  QString debugName() const;

private:
  template <class T>
  struct Cached
  {
    std::optional<T> value;
    bool failed = false;
    bool overridden = false;

    bool empty() const
    {
      return !failed && !value;
    }

    void set(T t)
    {
      value = std::move(t);
      failed = false;
      overridden = false;
    }

    void override(T t)
    {
      value = std::move(t);
      failed = false;
      overridden = true;
    }

    void fail()
    {
      value = {};
      failed = true;
      overridden = false;
    }

    void reset()
    {
      if (!overridden) {
        value = {};
        failed = false;
      }
    }
  };

  static constexpr std::size_t NoIndexGuess =
    std::numeric_limits<std::size_t>::max();

  FileTreeModel* m_model;
  FileTreeItem* m_parent;
  mutable std::size_t m_indexGuess;

  const QString m_virtualParentPath;
  const std::wstring m_wsFile, m_wsLcFile;
  const MOShared::DirectoryEntryFileKey m_key;
  const QString m_file;
  const bool m_isDirectory;

  int m_originID;
  QString m_realPath;
  std::wstring m_wsRealPath;
  Flags m_flags;
  QString m_mod;

  mutable Cached<uint64_t> m_fileSize;
  mutable Cached<QDateTime> m_lastModified;
  mutable Cached<QString> m_fileType;
  mutable Cached<uint64_t> m_compressedFileSize;

  bool m_loaded;
  bool m_expanded;
  bool m_sortingStale;
  Children m_children;


  FileTreeItem(
    FileTreeModel* model, FileTreeItem* parent,
    std::wstring dataRelativeParentPath, bool isDirectory, std::wstring file);

  void getFileType() const;
  void queueSort();
};

#endif // MODORGANIZER_FILETREEITEM_INCLUDED
