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
    connect(sourceModel(), &QAbstractItemModel::rowsRemoved, this, [this]() { buildTree(); }, Qt::UniqueConnection);
    // connect(sourceModel(), &QAbstractItemModel::modelReset, this, &ModListByPriorityProxy::buildTree, Qt::UniqueConnection);
    refresh();
  }
}

void ModListByPriorityProxy::refresh()
{
  buildTree();
}

void ModListByPriorityProxy::buildTree()
{
  if (!sourceModel()) return;

  beginResetModel();

  // reset the root
  m_Root = { };
  m_IndexToItem.clear();

  TreeItem* root = &m_Root;
  std::unique_ptr<TreeItem> overwrite;
  for (auto& [priority, index] : m_Profile->getAllIndexesByPriority()) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);

    TreeItem* item;

    if (modInfo->isSeparator()) {
      m_Root.children.push_back(std::make_unique<TreeItem>(modInfo, index, &m_Root));
      item = m_Root.children.back().get();
      root = item;
    }
    else if (modInfo->isOverwrite()) {
      // do not push here, because the overwrite is usually not at the right position
      overwrite = std::make_unique<TreeItem>(modInfo, index, &m_Root);
      item = overwrite.get();
    }
    else {
      root->children.push_back(std::make_unique<TreeItem>(modInfo, index, root));
      item = root->children.back().get();
    }

    m_IndexToItem[index] = item;
  }

  m_Root.children.push_back(std::move(overwrite));

  endResetModel();

  // restore expand-state
  expandItems(QModelIndex());
}

void ModListByPriorityProxy::expandItems(const QModelIndex& index)
{
  for (int row = 0; row < rowCount(index); row++) {
    QModelIndex idx = this->index(row, 0, index);
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

bool ModListByPriorityProxy::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
  bool hasSeparator = false;
  bool firstRowSeparator = false;

  if (data->hasText()) {

    try {
      int firstRowPriority = INT_MAX;
      unsigned int firstRowIndex = -1;

      for (auto sourceRow : ModList::sourceRows(data)) {
        hasSeparator = hasSeparator || ModInfo::getByIndex(sourceRow)->isSeparator();
        if (m_Profile->getModPriority(sourceRow) < firstRowPriority) {
          firstRowIndex = sourceRow;
          firstRowPriority = m_Profile->getModPriority(sourceRow);
        }
      }

      firstRowSeparator = firstRowIndex != -1 && ModInfo::getByIndex(firstRowIndex)->isSeparator();
    }
    catch (std::exception const&) {

    }
  }

  // row = -1 and invalid parent means we're dropping onto an item, we don't want to drop
  // separators onto items
  if (hasSeparator && row == -1 && parent.isValid()) {
    return !static_cast<TreeItem*>(parent.internalPointer())->mod->isSeparator();
  }

  // first row is a separator, we can drop it anywhere
  if (firstRowSeparator) {
    return QAbstractProxyModel::canDropMimeData(data, action, row, column, parent);
  }

  // top-level drop is disabled unless it's before the first separator
  if (!parent.isValid() && row >= 0) {

    // the row may be outside of the children list if we insert at the end
    if (row >= m_Root.children.size()) {
      return false;
    }

    if (row > 0 && m_Root.children[row - 1]->mod->isSeparator()) {
      if (m_Root.children[row]->mod->isSeparator() || m_Root.children[row]->mod->isOverwrite()) {
        return false;
      }
    }
  }

  return QAbstractProxyModel::canDropMimeData(data, action, row, column, parent);
}

bool ModListByPriorityProxy::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
  // we need to fix the source model row
  int sourceRow = -1;

  if (data->hasUrls()) {
    if (parent.isValid()) {
      sourceRow = static_cast<TreeItem*>(parent.internalPointer())->index;
    }
  }
  else {
    if (row >= 0) {
      if (!parent.isValid()) {
        if (row < m_Root.children.size()) {
          sourceRow = m_Root.children[row]->index;
        }
        else {
          sourceRow = ModInfo::getNumMods();
        }
      }
      else {
        auto* item = static_cast<TreeItem*>(parent.internalPointer());

        if (row < item->children.size()) {
          sourceRow = item->children[row]->index;
        }
        else if (parent.row() + 1 < m_Root.children.size()) {
          sourceRow = m_Root.children[parent.row() + 1]->index;
        }
      }
    }
    else if (parent.isValid()) {
      // this is a drop in a separator
      sourceRow = m_Root.children[parent.row() + 1]->index;
    }
  }

  return sourceModel()->dropMimeData(data, action, sourceRow, column, QModelIndex());
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
