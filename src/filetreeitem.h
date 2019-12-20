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

  void add(std::unique_ptr<FileTreeItem> child);
  void insert(std::unique_ptr<FileTreeItem> child, std::size_t at);
  void remove(std::size_t i);
  const std::vector<std::unique_ptr<FileTreeItem>>& children() const;

  FileTreeItem* parent();
  int originID() const;
  const QString& virtualParentPath() const;
  QString virtualPath() const;
  const QString& filename() const;
  const std::wstring& filenameWs() const;
  const std::wstring& filenameWsLowerCase() const;
  const QString& mod() const;
  QFont font() const;

  const QString& realPath() const;
  QString dataRelativeParentPath() const;
  QString dataRelativeFilePath() const;

  QFileIconProvider::IconType icon() const;

  bool isDirectory() const;
  bool isFromArchive() const;
  bool isConflicted() const;
  bool isHidden() const;
  bool hasChildren() const;

  void setLoaded(bool b);
  bool isLoaded() const;
  void unload();

  void setExpanded(bool b);
  bool isStrictlyExpanded() const;
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
