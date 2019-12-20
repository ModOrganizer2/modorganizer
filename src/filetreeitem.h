#ifndef MODORGANIZER_FILETREEITEM_INCLUDED
#define MODORGANIZER_FILETREEITEM_INCLUDED

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

  FileTreeItem();
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
    m_children.push_back(std::move(child));
  }

  void insert(std::unique_ptr<FileTreeItem> child, std::size_t at);
  void remove(std::size_t i);

  const std::vector<std::unique_ptr<FileTreeItem>>& children() const
  {
    return m_children;
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
  FileTreeItem* m_parent;
  int m_originID;
  QString m_virtualParentPath;
  QString m_realPath;
  Flags m_flags;
  std::wstring m_wsFile, m_wsLcFile;
  QString m_file;
  QString m_mod;
  bool m_loaded;
  bool m_expanded;
  std::vector<std::unique_ptr<FileTreeItem>> m_children;
};

#endif // MODORGANIZER_FILETREEITEM_INCLUDED
