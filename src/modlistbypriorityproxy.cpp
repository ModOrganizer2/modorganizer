#include "modlistbypriorityproxy.h"

#include "modinfo.h"
#include "profile.h"
#include "modlist.h"
#include "log.h"

ModListByPriorityProxy::ModListByPriorityProxy(Profile* profile, QObject* parent) :
  QAbstractProxyModel(parent), m_Profile(profile)
{
}

ModListByPriorityProxy::~ModListByPriorityProxy()
{
}

void ModListByPriorityProxy::setSourceModel(QAbstractItemModel* model)
{
  QAbstractProxyModel::setSourceModel(model);

  if (sourceModel()) {
    m_CollapsedItems.clear();
    connect(sourceModel(), &QAbstractItemModel::layoutChanged, this, &ModListByPriorityProxy::buildTree, Qt::UniqueConnection);
    connect(sourceModel(), &QAbstractItemModel::rowsRemoved, this, [this]() { buildTree(); }, Qt::UniqueConnection);
    connect(sourceModel(), &QAbstractItemModel::modelReset, this, &ModListByPriorityProxy::buildTree, Qt::UniqueConnection);
    buildTree();
  }
}

void ModListByPriorityProxy::buildTree()
{
  if (!sourceModel()) return;

  beginResetModel();

  // reset the root
  m_Root = { };
  m_IndexToItem.clear();

  TreeItem* root = &m_Root;
  for (auto& [priority, index] : m_Profile->getAllIndexesByPriority()) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);

    TreeItem* item;

    if (modInfo->isSeparator()) {
      m_Root.children.push_back(std::make_unique<TreeItem>(modInfo, index, &m_Root));
      item = m_Root.children.back().get();
      root = item;
    }
    else if (modInfo->isOverwrite()) {
      m_Root.children.push_back(std::make_unique<TreeItem>(modInfo, index, &m_Root));
      item = m_Root.children.back().get();
    }
    else {
      root->children.push_back(std::make_unique<TreeItem>(modInfo, index, root));
      item = root->children.back().get();
    }
    m_IndexToItem[index] = item;

  }

  endResetModel();

  // restore expand-state
  expandItems(QModelIndex());
}

void ModListByPriorityProxy::expandItems(const QModelIndex& index)
{
  for (int row = 0; row < rowCount(index); row++) {
    QModelIndex idx = this->index(row, 0, QModelIndex());
    if (!m_CollapsedItems.contains(idx.data(Qt::DisplayRole).toString())) {
      emit expandItem(idx);
    }
    expandItems(idx);
  }
}

QModelIndex ModListByPriorityProxy::mapFromSource(const QModelIndex& sourceIndex) const
{
  if (!sourceIndex.isValid()) {
    return QModelIndex();
  }

  auto* item = m_IndexToItem.at(sourceIndex.row());
  return createIndex(item->parent->childIndex(item), sourceIndex.column(), item);
}

QModelIndex ModListByPriorityProxy::mapToSource(const QModelIndex& proxyIndex) const
{
  if (!proxyIndex.isValid()) {
    return QModelIndex();
  }
  auto* item = static_cast<TreeItem*>(proxyIndex.internalPointer());
  return sourceModel()->index(item->index, proxyIndex.column());
}

int ModListByPriorityProxy::rowCount(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    return m_Root.children.size();
  }

  auto* item = static_cast<TreeItem*>(parent.internalPointer());

  if (item->mod->isSeparator()) {
    return item->children.size();
  }

  return 0;
}

int ModListByPriorityProxy::columnCount(const QModelIndex& index) const
{
  return sourceModel()->columnCount(mapToSource(index));
}


QModelIndex ModListByPriorityProxy::parent(const QModelIndex& child) const
{
  if (!child.isValid()) {
    return QModelIndex();
  }

  auto* item = static_cast<TreeItem*>(child.internalPointer());

  if (!item->parent || item->parent == &m_Root) {
    return QModelIndex();
  }

  return createIndex(item->parent->parent->childIndex(item->parent), 0, item->parent);
}

bool ModListByPriorityProxy::hasChildren(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    return m_Root.children.size() > 0;
  }
  auto* item = static_cast<TreeItem*>(parent.internalPointer());
  return item->children.size() > 0;
}

bool ModListByPriorityProxy::setData(const QModelIndex& index, const QVariant& value, int role)
{
  // only care about the "name" column
  if (index.column() == 0 && role == Qt::EditRole) {
    QString oldValue = data(index, role).toString();
    if (m_CollapsedItems.contains(oldValue)) {
      m_CollapsedItems.erase(oldValue);
      m_CollapsedItems.insert(value.toString());
    }
  }
  return QAbstractProxyModel::setData(index, value, role);
}


Qt::ItemFlags ModListByPriorityProxy::flags(const QModelIndex& idx) const
{
  if (!idx.isValid()) {
    return sourceModel()->flags(QModelIndex());
  }

  // we check the flags of the root node and if drop is not enabled, it
  // means we are dragging files.
  Qt::ItemFlags rootFlags = sourceModel()->flags(QModelIndex());
  if (!rootFlags.testFlag(Qt::ItemIsDropEnabled)) {
    return sourceModel()->flags(mapToSource(idx));
  }

  auto flags = sourceModel()->flags(mapToSource(idx));
  auto* item = static_cast<TreeItem*>(idx.internalPointer());

  if (item->mod->isSeparator()) {
    flags |= Qt::ItemIsDropEnabled;
  }

  return flags;
}

QModelIndex ModListByPriorityProxy::index(int row, int column, const QModelIndex& parent) const
{
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  const TreeItem* parentItem;
  if (!parent.isValid()) {
    parentItem = &m_Root;
  }
  else {
    parentItem = static_cast<TreeItem*>(parent.internalPointer());
  }
  return createIndex(row, column, parentItem->children[row].get());
}

void ModListByPriorityProxy::expanded(const QModelIndex& index)
{
  auto it = m_CollapsedItems.find(index.data(Qt::DisplayRole).toString());
  if (it != m_CollapsedItems.end()) {
    m_CollapsedItems.erase(it);
  }
}

void ModListByPriorityProxy::collapsed(const QModelIndex& index)
{
  m_CollapsedItems.insert(index.data(Qt::DisplayRole).toString());
}
