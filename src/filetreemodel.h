#ifndef MODORGANIZER_FILETREEMODEL_INCLUDED
#define MODORGANIZER_FILETREEMODEL_INCLUDED

#include "filetreeitem.h"
#include "iconfetcher.h"
#include "shared/fileregisterfwd.h"
#include <unordered_set>

class OrganizerCore;

class FileTreeModel : public QAbstractItemModel
{
  Q_OBJECT;

public:
  enum Flag
  {
    NoFlags          = 0x00,
    ConflictsOnly    = 0x01,
    Archives         = 0x02,
    PruneDirectories = 0x04
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

  struct SortInfo
  {
    int column = 0;
    Qt::SortOrder order = Qt::AscendingOrder;
  };


  FileTreeModel(OrganizerCore& core, QObject* parent=nullptr);

  void setFlags(Flags f)
  {
    m_flags = f;
  }

  void refresh();
  void clear();

  bool fullyLoaded() const
  {
    return m_fullyLoaded;
  }

  void ensureFullyLoaded();

  bool enabled() const;
  void setEnabled(bool b);

  void aboutToExpandAll();
  void expandedAll();


  const SortInfo& sortInfo() const;

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
  void sortItem(FileTreeItem& item, bool force);
  void queueSortItem(FileTreeItem* item);

private:
  class Range;

  using DirectoryIterator = std::vector<MOShared::DirectoryEntry*>::const_iterator;

  OrganizerCore& m_core;
  bool m_enabled;
  mutable FileTreeItem::Ptr m_root;
  Flags m_flags;
  mutable IconFetcher m_iconFetcher;
  mutable std::vector<QModelIndex> m_iconPending;
  mutable QTimer m_iconPendingTimer;
  SortInfo m_sort;
  bool m_fullyLoaded;
  bool m_sortingEnabled;

  // see top of filetreemodel.cpp
  std::vector<FileTreeItem*> m_removeItems;
  std::vector<FileTreeItem*> m_sortItems;
  QTimer m_removeTimer;
  QTimer m_sortTimer;


  bool showConflictsOnly() const
  {
    return (m_flags & ConflictsOnly);
  }

  bool showArchives() const;


  // for `forFetching`, see top of filetreemodel.cpp
  void update(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath, bool forFetching);

  void doFetchMore(const QModelIndex& parent, bool forFetch, bool doSort);

  void queueRemoveItem(FileTreeItem* item);
  void removeItems();

  void sortItems();


  // for `forFetching`, see top of filetreemodel.cpp
  bool updateDirectories(
    FileTreeItem& parentItem, const std::wstring& path,
    const MOShared::DirectoryEntry& parentEntry, bool forFetching);

  // for `forFetching`, see top of filetreemodel.cpp
  void removeDisappearingDirectories(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath, std::unordered_set<std::wstring_view>& seen,
    bool forFetching);

  bool addNewDirectories(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath,
    const std::unordered_set<std::wstring_view>& seen);


  bool updateFiles(
    FileTreeItem& parentItem, const std::wstring& path,
    const MOShared::DirectoryEntry& parentEntry);

  void removeDisappearingFiles(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    int& firstFileRow, std::unordered_set<MOShared::FileIndex>& seen);

  bool addNewFiles(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath, int firstFileRow,
    const std::unordered_set<MOShared::FileIndex>& seen);


  FileTreeItem::Ptr createDirectoryItem(
    FileTreeItem& parentItem, const std::wstring& parentPath,
    const MOShared::DirectoryEntry& d);

  FileTreeItem::Ptr createFileItem(
    FileTreeItem& parentItem, const std::wstring& parentPath,
    const MOShared::FileEntry& file);

  void updateFileItem(FileTreeItem& item, const MOShared::FileEntry& file);


  QVariant displayData(const FileTreeItem* item, int column) const;
  std::wstring makeModName(const MOShared::FileEntry& file, int originID) const;

  void ensureLoaded(FileTreeItem* item) const;
  void updatePendingIcons();
  void removePendingIcons(const QModelIndex& parent, int first, int last);

  bool shouldShowFile(const MOShared::FileEntry& file) const;
  bool shouldShowFolder(const MOShared::DirectoryEntry& dir, const FileTreeItem* item) const;
  QString makeTooltip(const FileTreeItem& item) const;
  QVariant makeIcon(const FileTreeItem& item, const QModelIndex& index) const;

  QModelIndex indexFromItem(FileTreeItem& item, int col=0) const;
  void recursiveFetchMore(const QModelIndex& m);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileTreeModel::Flags);
Q_DECLARE_OPERATORS_FOR_FLAGS(FileTreeItem::Flags);

#endif // MODORGANIZER_FILETREEMODEL_INCLUDED
