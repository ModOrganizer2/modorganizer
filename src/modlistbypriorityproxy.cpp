#include "modlistbypriorityproxy.h"

#include "modinfo.h"
#include "profile.h"
#include "organizercore.h"
#include "modlist.h"
#include "modlistdropinfo.h"
#include "log.h"

ModListByPriorityProxy::ModListByPriorityProxy(Profile* profile, OrganizerCore& core, QObject* parent) :
  QAbstractProxyModel(parent), m_core(core), m_profile(profile)
{
}

ModListByPriorityProxy::~ModListByPriorityProxy()
{
}

void ModListByPriorityProxy::setSourceModel(QAbstractItemModel* model)
{
  if (sourceModel()) {
    disconnect(sourceModel(), nullptr, this, nullptr);
  }

  QAbstractProxyModel::setSourceModel(model);

  if (sourceModel()) {
    connect(sourceModel(), &QAbstractItemModel::layoutChanged, this, &ModListByPriorityProxy::buildTree, Qt::UniqueConnection);
    connect(sourceModel(), &QAbstractItemModel::rowsRemoved, this, &ModListByPriorityProxy::buildTree, Qt::UniqueConnection);
    connect(sourceModel(), &QAbstractItemModel::modelReset, this, &ModListByPriorityProxy::buildTree, Qt::UniqueConnection);
    connect(sourceModel(), &QAbstractItemModel::dataChanged, this, &ModListByPriorityProxy::modelDataChanged, Qt::UniqueConnection);
    refresh();
  }
}

void ModListByPriorityProxy::refresh()
{
  buildTree();
}

void ModListByPriorityProxy::setProfile(Profile* profile)
{
  m_profile = profile;
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
  std::vector<std::unique_ptr<TreeItem>> backups;
  for (auto& [priority, index] : m_profile->getAllIndexesByPriority()) {
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
    else if (modInfo->isBackup()) {
      backups.push_back(std::make_unique<TreeItem>(modInfo, index, &m_Root));
      item = backups.back().get();
    }
    else {
      root->children.push_back(std::make_unique<TreeItem>(modInfo, index, root));
      item = root->children.back().get();
    }

    m_IndexToItem[index] = item;
  }

  m_Root.children.insert(m_Root.children.begin(),
    std::make_move_iterator(backups.begin()), std::make_move_iterator(backups.end()));
  m_Root.children.push_back(std::move(overwrite));

  endResetModel();
}

void ModListByPriorityProxy::modelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
{
  QModelIndex proxyTopLeft = mapFromSource(topLeft);
  if (!proxyTopLeft.isValid()) {
    return;
  }

  if (topLeft == bottomRight) {
    emit dataChanged(proxyTopLeft, proxyTopLeft);
  }
  else {
    QModelIndex proxyBottomRight = mapFromSource(bottomRight);
    emit dataChanged(proxyTopLeft, proxyBottomRight);
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

  return createIndex(item->parent->parent->childIndex(item->parent), child.column(), item->parent);
}

bool ModListByPriorityProxy::hasChildren(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    return m_Root.children.size() > 0;
  }
  auto* item = static_cast<TreeItem*>(parent.internalPointer());
  return item->children.size() > 0;
}

bool ModListByPriorityProxy::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
  ModListDropInfo dropInfo(data, m_core);

  if (!dropInfo.isValid() || dropInfo.isLocalFileDrop()) {
    return QAbstractProxyModel::canDropMimeData(data, action, row, column, parent);
  }

  if (dropInfo.isModDrop()) {
    bool hasSeparator = false;
    unsigned int firstRowIndex = -1;

    int firstRowPriority = INT_MAX;
    for (auto sourceRow : dropInfo.rows()) {
      hasSeparator = hasSeparator || ModInfo::getByIndex(sourceRow)->isSeparator();
      if (m_profile->getModPriority(sourceRow) < firstRowPriority) {
        firstRowIndex = sourceRow;
        firstRowPriority = m_profile->getModPriority(sourceRow);
      }
    }

    bool firstRowSeparator = firstRowIndex != -1 && ModInfo::getByIndex(firstRowIndex)->isSeparator();

    // row = -1 and invalid parent means we're dropping onto an item, we don't want to drop
    // separators onto items or items into their own separator
    if (row == -1 && parent.isValid()) {
      auto* parentItem = static_cast<TreeItem*>(parent.internalPointer());
      if (hasSeparator) {
        return !parentItem->mod->isSeparator();
      }

      for (auto row : dropInfo.rows()) {
        auto it = m_IndexToItem.find(row);
        if (it != m_IndexToItem.end() && it->second->parent == parentItem) {
          return false;
        }
      }
    }

    // first row is a separator, we can drop it anywhere
    if (firstRowSeparator) {
      return QAbstractProxyModel::canDropMimeData(data, action, row, column, parent);
    }
  }

  // the row may be outside of the children list if we insert at the end
  if (!parent.isValid() && row >= m_Root.children.size()) {
    return false;
  }

  return QAbstractProxyModel::canDropMimeData(data, action, row, column, parent);
}

bool ModListByPriorityProxy::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
  // we need to fix the source model row
  ModListDropInfo dropInfo(data, m_core);
  int sourceRow = -1;

  if (dropInfo.isLocalFileDrop()) {
    if (parent.isValid()) {
      sourceRow = static_cast<TreeItem*>(parent.internalPointer())->index;
    }
  }
  else {

    if (row >= 0) {
      if (!parent.isValid()) {
        if (row < m_Root.children.size()) {
          sourceRow = m_Root.children[row]->index;
          if (row > 0
            && m_Root.children[row - 1]->mod->isSeparator()
            && !m_Root.children[row - 1]->children.empty()
            && m_dropExpanded
            && m_dropPosition == ModListView::DropPosition::BelowItem) {
            sourceRow = m_Root.children[row - 1]->children[0]->index;
          }
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

void ModListByPriorityProxy::onDropEnter(const QMimeData*, bool dropExpanded, ModListView::DropPosition dropPosition)
{
  m_dropExpanded = dropExpanded;
  m_dropPosition = dropPosition;
}
