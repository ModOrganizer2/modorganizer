#ifndef MODORGANIZER_FILETREEMODEL_INCLUDED
#define MODORGANIZER_FILETREEMODEL_INCLUDED

#include "filetreeitem.h"
#include "iconfetcher.h"
#include "directoryentry.h"
#include <unordered_set>

class OrganizerCore;

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

  enum Columns
  {
    FileName = 0,
    ModName,
    FileType,
    FileSize,
    LastModified,

    ColumnCount
  };

  Q_DECLARE_FLAGS(Flags, Flag);

  FileTreeModel(OrganizerCore& core, QObject* parent=nullptr);

  void setFlags(Flags f)
  {
    m_flags = f;
  }

  void refresh();
  void clear();

  QModelIndex index(int row, int col, const QModelIndex& parent={}) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent={}) const override;
  int columnCount(const QModelIndex& parent={}) const override;
  bool hasChildren(const QModelIndex& parent={}) const override;
  bool canFetchMore(const QModelIndex& parent) const override;
  void fetchMore(const QModelIndex& parent) override;
  QVariant data(const QModelIndex& index, int role=Qt::DisplayRole) const override;
  QVariant headerData(int i, Qt::Orientation ori, int role=Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  void sort(int column, Qt::SortOrder order=Qt::AscendingOrder) override;

  FileTreeItem* itemFromIndex(const QModelIndex& index) const;

private:
  enum class FillFlag
  {
    None             = 0x00,
    PruneDirectories = 0x01
  };

  struct Sort
  {
    int column = 0;
    Qt::SortOrder order = Qt::AscendingOrder;
  };

  class Range;

  Q_DECLARE_FLAGS(FillFlags, FillFlag);

  using DirectoryIterator = std::vector<MOShared::DirectoryEntry*>::const_iterator;
  OrganizerCore& m_core;
  mutable FileTreeItem m_root;
  Flags m_flags;
  mutable IconFetcher m_iconFetcher;
  mutable std::vector<QModelIndex> m_iconPending;
  mutable QTimer m_iconPendingTimer;
  Sort m_sort;

  bool showConflicts() const
  {
    return (m_flags & Conflicts);
  }

  bool showArchives() const;


  void update(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath);


  bool updateDirectories(
    FileTreeItem& parentItem, const std::wstring& path,
    const MOShared::DirectoryEntry& parentEntry, FillFlags flags);

  void removeDisappearingDirectories(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath, std::unordered_set<std::wstring_view>& seen);

  bool addNewDirectories(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath,
    const std::unordered_set<std::wstring_view>& seen);


  bool updateFiles(
    FileTreeItem& parentItem, const std::wstring& path,
    const MOShared::DirectoryEntry& parentEntry);

  void removeDisappearingFiles(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    int& firstFileRow, std::unordered_set<MOShared::FileEntry::Index>& seen);

  bool addNewFiles(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath, int firstFileRow,
    const std::unordered_set<MOShared::FileEntry::Index>& seen);


  std::unique_ptr<FileTreeItem> createDirectoryItem(
    FileTreeItem& parentItem, const std::wstring& parentPath,
    const MOShared::DirectoryEntry& d);

  std::unique_ptr<FileTreeItem> createFileItem(
    FileTreeItem& parentItem, const std::wstring& parentPath,
    const MOShared::FileEntry& file);


  QVariant displayData(const FileTreeItem* item, int column) const;
  std::wstring makeModName(const MOShared::FileEntry& file, int originID) const;

  void ensureLoaded(FileTreeItem* item) const;
  void updatePendingIcons();
  void removePendingIcons(const QModelIndex& parent, int first, int last);

  bool shouldShowFile(const MOShared::FileEntry& file) const;
  bool hasFilesAnywhere(const MOShared::DirectoryEntry& dir) const;
  QString makeTooltip(const FileTreeItem& item) const;
  QVariant makeIcon(const FileTreeItem& item, const QModelIndex& index) const;

  QModelIndex indexFromItem(FileTreeItem& item, int col=0) const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileTreeModel::Flags);
Q_DECLARE_OPERATORS_FOR_FLAGS(FileTreeItem::Flags);

#endif // MODORGANIZER_FILETREEMODEL_INCLUDED
