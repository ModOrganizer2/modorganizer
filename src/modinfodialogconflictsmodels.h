#include "shared/fileentry.h"

class PluginManager;

class ConflictItem
{
public:
  ConflictItem(QString before, QString relativeName, QString after,
               MOShared::FileIndex index, QString fileName, bool hasAltOrigins,
               QString altOrigin, bool archive);

  const QString& before() const;
  const QString& relativeName() const;
  const QString& after() const;

  const QString& fileName() const;
  const QString& altOrigin() const;

  bool hasAlts() const;
  bool isArchive() const;

  MOShared::FileIndex fileIndex() const;

  bool canHide() const;
  bool canUnhide() const;
  bool canRun() const;
  bool canOpen() const;
  bool canPreview(PluginManager& pluginManager) const;
  bool canExplore() const;

private:
  QString m_before;
  QString m_relativeName;
  QString m_after;
  MOShared::FileIndex m_index;
  QString m_fileName;
  bool m_hasAltOrigins;
  QString m_altOrigin;
  bool m_isArchive;
};

class ConflictListModel : public QAbstractItemModel
{
  Q_OBJECT;

public:
  struct Column
  {
    QString caption;
    const QString& (ConflictItem::*getText)() const;
  };

  ConflictListModel(QTreeView* tree, std::vector<Column> columns);

  void clear();
  void reserve(std::size_t s);

  QModelIndex index(int row, int col, const QModelIndex& = {}) const override;
  QModelIndex parent(const QModelIndex&) const override;
  int rowCount(const QModelIndex& parent = {}) const override;
  int columnCount(const QModelIndex& = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QVariant headerData(int col, Qt::Orientation, int role) const;

  void sort(int colIndex, Qt::SortOrder order = Qt::AscendingOrder);
  void add(ConflictItem item);

  void finished();

  const ConflictItem* getItem(std::size_t row) const;

private:
  QTreeView* m_tree;
  std::vector<Column> m_columns;
  std::vector<ConflictItem> m_items;
  int m_sortColumn;
  Qt::SortOrder m_sortOrder;

  const ConflictItem* itemFromIndex(const QModelIndex& index) const;
  QModelIndex indexFromItem(const ConflictItem* item, int col);

  void doSort();
};

class OverwriteConflictListModel : public ConflictListModel
{
  Q_OBJECT;

public:
  OverwriteConflictListModel(QTreeView* tree);
};

class OverwrittenConflictListModel : public ConflictListModel
{
  Q_OBJECT;

public:
  OverwrittenConflictListModel(QTreeView* tree);
};

class NoConflictListModel : public ConflictListModel
{
  Q_OBJECT;

public:
  NoConflictListModel(QTreeView* tree);
};

class AdvancedConflictListModel : public ConflictListModel
{
  Q_OBJECT;

public:
  AdvancedConflictListModel(QTreeView* tree);
};
