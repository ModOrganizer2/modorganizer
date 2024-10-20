#include "modinfodialogconflictsmodels.h"
#include "modinfodialog.h"
#include <utility.h>

using MOBase::naturalCompare;

ConflictItem::ConflictItem(QString before, QString relativeName, QString after,
                           MOShared::FileIndex index, QString fileName,
                           bool hasAltOrigins, QString altOrigin, bool archive)
    : m_before(std::move(before)), m_relativeName(std::move(relativeName)),
      m_after(std::move(after)), m_index(index), m_fileName(std::move(fileName)),
      m_hasAltOrigins(hasAltOrigins), m_altOrigin(std::move(altOrigin)),
      m_isArchive(archive)
{}

const QString& ConflictItem::before() const
{
  return m_before;
}

const QString& ConflictItem::relativeName() const
{
  return m_relativeName;
}

const QString& ConflictItem::after() const
{
  return m_after;
}

const QString& ConflictItem::fileName() const
{
  return m_fileName;
}

const QString& ConflictItem::altOrigin() const
{
  return m_altOrigin;
}

bool ConflictItem::hasAlts() const
{
  return m_hasAltOrigins;
}

bool ConflictItem::isArchive() const
{
  return m_isArchive;
}

MOShared::FileIndex ConflictItem::fileIndex() const
{
  return m_index;
}

bool ConflictItem::canHide() const
{
  return canHideFile(isArchive(), fileName());
}

bool ConflictItem::canUnhide() const
{
  return canUnhideFile(isArchive(), fileName());
}

bool ConflictItem::canRun() const
{
  return canRunFile(isArchive(), fileName());
}

bool ConflictItem::canOpen() const
{
  return canOpenFile(isArchive(), fileName());
}

bool ConflictItem::canPreview(PluginManager& pluginManager) const
{
  return canPreviewFile(pluginManager, isArchive(), fileName());
}

bool ConflictItem::canExplore() const
{
  return canExploreFile(isArchive(), fileName());
}

ConflictListModel::ConflictListModel(QTreeView* tree, std::vector<Column> columns)
    : m_tree(tree), m_columns(std::move(columns)), m_sortColumn(-1),
      m_sortOrder(Qt::AscendingOrder)
{
  m_tree->setModel(this);
}

void ConflictListModel::clear()
{
  beginResetModel();
  m_items.clear();
  endResetModel();
}

void ConflictListModel::reserve(std::size_t s)
{
  m_items.reserve(s);
}

QModelIndex ConflictListModel::index(int row, int col, const QModelIndex&) const
{
  return createIndex(row, col);
}

QModelIndex ConflictListModel::parent(const QModelIndex&) const
{
  return {};
}

int ConflictListModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }

  return static_cast<int>(m_items.size());
}

int ConflictListModel::columnCount(const QModelIndex&) const
{
  return static_cast<int>(m_columns.size());
}

const ConflictItem* ConflictListModel::itemFromIndex(const QModelIndex& index) const
{
  const auto row = index.row();
  if (row < 0) {
    return nullptr;
  }

  const auto i = static_cast<std::size_t>(row);
  if (i >= m_items.size()) {
    return nullptr;
  }

  return &m_items[i];
}

QModelIndex ConflictListModel::indexFromItem(const ConflictItem* item, int col)
{
  for (std::size_t i = 0; i < m_items.size(); ++i) {
    if (&m_items[i] == item) {
      return createIndex(static_cast<int>(i), col);
    }
  }

  return {};
}

QVariant ConflictListModel::data(const QModelIndex& index, int role) const
{
  if (role == Qt::DisplayRole || role == Qt::FontRole) {
    const ConflictItem* item = itemFromIndex(index);
    if (!item) {
      return {};
    }

    const auto col = index.column();
    if (col < 0) {
      return {};
    }

    const auto c = static_cast<std::size_t>(col);
    if (c >= m_columns.size()) {
      return {};
    }

    if (role == Qt::DisplayRole) {
      return (item->*m_columns[c].getText)();
    } else if (role == Qt::FontRole) {
      if (item->isArchive()) {
        QFont f = m_tree->font();
        f.setItalic(true);
        return f;
      }
    }
  }

  return {};
}

QVariant ConflictListModel::headerData(int col, Qt::Orientation, int role) const
{
  if (role == Qt::DisplayRole) {
    if (col < 0) {
      return {};
    }

    const auto i = static_cast<std::size_t>(col);
    if (i >= m_columns.size()) {
      return {};
    }

    return m_columns[i].caption;
  }

  return {};
}

void ConflictListModel::sort(int colIndex, Qt::SortOrder order)
{
  m_sortColumn = colIndex;
  m_sortOrder  = order;

  emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);

  const auto oldList = persistentIndexList();
  std::vector<std::pair<const ConflictItem*, int>> oldItems;

  const auto itemCount = oldList.size();
  oldItems.reserve(static_cast<std::size_t>(itemCount));

  for (int i = 0; i < itemCount; ++i) {
    const QModelIndex& index = oldList[i];
    oldItems.push_back({itemFromIndex(index), index.column()});
  }

  doSort();

  QModelIndexList newList;
  newList.reserve(itemCount);

  for (int i = 0; i < itemCount; ++i) {
    const auto& pair = oldItems[static_cast<std::size_t>(i)];
    newList.append(indexFromItem(pair.first, pair.second));
  }

  changePersistentIndexList(oldList, newList);

  emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

void ConflictListModel::add(ConflictItem item)
{
  m_items.emplace_back(std::move(item));
}

void ConflictListModel::finished()
{
  beginResetModel();
  endResetModel();

  sort(m_sortColumn, m_sortOrder);
}

const ConflictItem* ConflictListModel::getItem(std::size_t row) const
{
  if (row >= m_items.size()) {
    return nullptr;
  }

  return &m_items[row];
}

void ConflictListModel::doSort()
{
  if (m_items.empty()) {
    return;
  }

  if (m_sortColumn < 0) {
    return;
  }

  const auto c = static_cast<std::size_t>(m_sortColumn);
  if (c >= m_columns.size()) {
    return;
  }

  const auto& col = m_columns[c];

  // avoids branching on sort order while sorting
  auto sortAsc = [&](const auto& a, const auto& b) {
    return (naturalCompare((a.*col.getText)(), (b.*col.getText)()) < 0);
  };

  auto sortDesc = [&](const auto& a, const auto& b) {
    return (naturalCompare((a.*col.getText)(), (b.*col.getText)()) > 0);
  };

  if (m_sortOrder == Qt::AscendingOrder) {
    std::sort(m_items.begin(), m_items.end(), sortAsc);
  } else {
    std::sort(m_items.begin(), m_items.end(), sortDesc);
  }
}

OverwriteConflictListModel::OverwriteConflictListModel(QTreeView* tree)
    : ConflictListModel(tree, {{tr("File"), &ConflictItem::relativeName},
                               {tr("Overwritten Mods"), &ConflictItem::before}})
{}

OverwrittenConflictListModel::OverwrittenConflictListModel(QTreeView* tree)
    : ConflictListModel(tree, {{tr("File"), &ConflictItem::relativeName},
                               {tr("Providing Mod"), &ConflictItem::after}})
{}

NoConflictListModel::NoConflictListModel(QTreeView* tree)
    : ConflictListModel(tree, {{tr("File"), &ConflictItem::relativeName}})
{}

AdvancedConflictListModel::AdvancedConflictListModel(QTreeView* tree)
    : ConflictListModel(tree, {{tr("Overwrites"), &ConflictItem::before},
                               {tr("File"), &ConflictItem::relativeName},
                               {tr("Overwritten By"), &ConflictItem::after}})
{}
