#include "directoryentry.h"
#include "iconfetcher.h"
#include <QAbstractItemModel>

class OrganizerCore;

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
  const std::vector<std::unique_ptr<FileTreeItem>>& children() const;

  FileTreeItem* parent();
  int originID() const;
  const QString& virtualParentPath() const;
  QString virtualPath() const;
  const QString& filename() const;
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

  QString debugName() const;

private:
  FileTreeItem* m_parent;
  int m_originID;
  QString m_virtualParentPath;
  QString m_realPath;
  Flags m_flags;
  QString m_file;
  QString m_mod;
  bool m_loaded;
  std::vector<std::unique_ptr<FileTreeItem>> m_children;
};


class FileTreeModel : public QAbstractItemModel
{
  Q_OBJECT;

public:
  enum Flag
  {
    NoFlags   = 0x00,
    Conflicts = 0x01,
    Archives  = 0x02
  };

  Q_DECLARE_FLAGS(Flags, Flag);

  FileTreeModel(OrganizerCore& core, QObject* parent=nullptr);

  void setFlags(Flags f);
  void refresh();

  QModelIndex index(int row, int col, const QModelIndex& parent={}) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent={}) const override;
  int columnCount(const QModelIndex& parent={}) const override;
  bool hasChildren(const QModelIndex& parent={}) const override;
  QVariant data(const QModelIndex& index, int role=Qt::DisplayRole) const override;
  QVariant headerData(int i, Qt::Orientation ori, int role=Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
  enum class FillFlag
  {
    None             = 0x00,
    PruneDirectories = 0x01
  };

  Q_DECLARE_FLAGS(FillFlags, FillFlag);

  using DirectoryIterator = std::vector<MOShared::DirectoryEntry*>::const_iterator;
  OrganizerCore& m_core;
  mutable FileTreeItem m_root;
  Flags m_flags;
  mutable IconFetcher m_iconFetcher;
  mutable std::vector<QModelIndex> m_iconPending;
  mutable QTimer m_iconPendingTimer;

  bool showConflicts() const;
  bool showArchives() const;

  void fill(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath);

  void fillDirectories(
    FileTreeItem& parentItem, const std::wstring& path,
    DirectoryIterator begin, DirectoryIterator end, FillFlags flags);

  void fillFiles(
    FileTreeItem& parentItem, const std::wstring& path,
    const std::vector<MOShared::FileEntry::Ptr>& files, FillFlags flags);

  std::wstring makeModName(const MOShared::FileEntry& file, int originID) const;

  FileTreeItem* itemFromIndex(const QModelIndex& index) const;
  void ensureLoaded(FileTreeItem* item) const;
  void updatePendingIcons();

  bool shouldShowFile(const MOShared::FileEntry& file) const;
  bool hasFilesAnywhere(const MOShared::DirectoryEntry& dir) const;
  QString makeTooltip(const FileTreeItem& item) const;
  QVariant makeIcon(const FileTreeItem& item, const QModelIndex& index) const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileTreeModel::Flags);
Q_DECLARE_OPERATORS_FOR_FLAGS(FileTreeItem::Flags);
