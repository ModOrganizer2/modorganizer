#include "modlistbypriorityproxy.h"

#include "log.h"
#include "modinfo.h"
#include "modlist.h"
#include "modlistdropinfo.h"
#include "organizercore.h"
#include "profile.h"

ModListByPriorityProxy::ModListByPriorityProxy(Profile* profile, OrganizerCore& core,
                                               QObject* parent)
    : QAbstractProxyModel(parent), m_core(core), m_profile(profile)
{}

ModListByPriorityProxy::~ModListByPriorityProxy() {}

void ModListByPriorityProxy::setSourceModel(QAbstractItemModel* model)
{
  if (sourceModel()) {
    disconnect(sourceModel(), nullptr, this, nullptr);
  }

  QAbstractProxyModel::setSourceModel(model);

  if (sourceModel()) {
    connect(sourceModel(), &QAbstractItemModel::layoutChanged, this,
            &ModListByPriorityProxy::onModelLayoutChanged, Qt::UniqueConnection);
    connect(sourceModel(), &QAbstractItemModel::rowsRemoved, this,
            &ModListByPriorityProxy::onModelRowsRemoved, Qt::UniqueConnection);
    connect(sourceModel(), &QAbstractItemModel::modelReset, this,
            &ModListByPriorityProxy::onModelReset, Qt::UniqueConnection);
    connect(sourceModel(), &QAbstractItemModel::dataChanged, this,
            &ModListByPriorityProxy::onModelDataChanged, Qt::UniqueConnection);

    onModelReset();
  }
}

void ModListByPriorityProxy::setProfile(Profile* profile)
{
  m_profile = profile;
}

void ModListByPriorityProxy::setSortOrder(Qt::SortOrder order)
{
  if (m_sortOrder != order) {
    m_sortOrder = order;
    onModelLayoutChanged();
  }
}

void ModListByPriorityProxy::buildMapping()
{
  m_IndexToItem.clear();
  for (unsigned int index = 0; index < ModInfo::getNumMods(); ++index) {
    m_IndexToItem[index] =
        std::make_unique<TreeItem>(ModInfo::getByIndex(index), index);
  }
}

void ModListByPriorityProxy::buildTree()
{
  if (!sourceModel())
    return;

  // reset the root
  m_Root = {};

  // clear all children
  for (auto& [index, item] : m_IndexToItem) {
    item->children.clear();
  }

  TreeItem* root = &m_Root;
  TreeItem* overwrite;
  std::vector<TreeItem*> backups;

  auto fn = [&](const auto& p) {
    auto& [priority, index] = p;
    ModInfo::Ptr modInfo    = ModInfo::getByIndex(index);
    TreeItem* item          = m_IndexToItem[index].get();

    if (modInfo->isSeparator()) {
      item->parent = &m_Root;
      m_Root.children.push_back(item);
      root = item;
    } else if (modInfo->isOverwrite()) {
      // do not push here, because the overwrite is usually not at the right position
      item->parent = &m_Root;
      overwrite    = item;
    } else if (modInfo->isBackup()) {
      // do not push here, because backups are usually not at the right position
      item->parent = &m_Root;
      backups.push_back(item);
    } else {
      item->parent = root;
      root->children.push_back(item);
    }
  };

  auto& ibp = m_profile->getAllIndexesByPriority();
  if (m_sortOrder == Qt::AscendingOrder) {
    std::for_each(ibp.begin(), ibp.end(), fn);
    m_Root.children.insert(m_Root.children.begin(),
                           std::make_move_iterator(backups.begin()),
                           std::make_move_iterator(backups.end()));
    m_Root.children.push_back(std::move(overwrite));
  } else {
    std::for_each(ibp.rbegin(), ibp.rend(), fn);
    m_Root.children.insert(m_Root.children.begin(), std::move(overwrite));
    m_Root.children.insert(m_Root.children.end(),
                           std::make_move_iterator(backups.begin()),
                           std::make_move_iterator(backups.end()));
  }
}

void ModListByPriorityProxy::onModelRowsRemoved(const QModelIndex& parent, int first,
                                                int last)
{
  onModelReset();
}

void ModListByPriorityProxy::onModelLayoutChanged(const QList<QPersistentModelIndex>&,
                                                  LayoutChangeHint hint)
{
  emit layoutAboutToBeChanged();
  auto persistent = persistentIndexList();
  buildTree();

  QModelIndexList toPersistent;
  for (auto& idx : persistent) {
    // we can still access the TreeItem* because we did not destroy them
    auto* item = static_cast<TreeItem*>(idx.internalPointer());
    toPersistent.append(createIndex(static_cast<int>(item->parent->childIndex(item)),
                                    idx.column(), item));
  }
  changePersistentIndexList(persistent, toPersistent);

  emit layoutChanged({}, hint);
}

void ModListByPriorityProxy::onModelReset()
{
  beginResetModel();
  buildMapping();
  buildTree();
  endResetModel();
}

void ModListByPriorityProxy::onModelDataChanged(const QModelIndex& topLeft,
                                                const QModelIndex& bottomRight,
                                                const QVector<int>& roles)
{
  QModelIndex proxyTopLeft = mapFromSource(topLeft);
  if (!proxyTopLeft.isValid()) {
    return;
  }

  if (topLeft == bottomRight) {
    emit dataChanged(proxyTopLeft, proxyTopLeft);
  } else {
    QModelIndex proxyBottomRight = mapFromSource(bottomRight);
    emit dataChanged(proxyTopLeft, proxyBottomRight);
  }
}

QModelIndex ModListByPriorityProxy::mapFromSource(const QModelIndex& sourceIndex) const
{
  if (!sourceIndex.isValid()) {
    return QModelIndex();
  }

  auto* item = m_IndexToItem.at(sourceIndex.row()).get();
  return createIndex(static_cast<int>(item->parent->childIndex(item)),
                     sourceIndex.column(), item);
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
    return static_cast<int>(m_Root.children.size());
  }

  auto* item = static_cast<TreeItem*>(parent.internalPointer());

  if (item->mod->isSeparator()) {
    return static_cast<int>(item->children.size());
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

  return createIndex(static_cast<int>(item->parent->parent->childIndex(item->parent)),
                     0, item->parent);
}

bool ModListByPriorityProxy::hasChildren(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    return m_Root.children.size() > 0;
  }
  auto* item = static_cast<TreeItem*>(parent.internalPointer());
  return item->children.size() > 0;
}

bool ModListByPriorityProxy::canDropMimeData(const QMimeData* data,
                                             Qt::DropAction action, int row, int column,
                                             const QModelIndex& parent) const
{
  ModListDropInfo dropInfo(data, m_core);

  if (!dropInfo.isValid() || dropInfo.isLocalFileDrop()) {
    return QAbstractProxyModel::canDropMimeData(data, action, row, column, parent);
  }

  if (dropInfo.isModDrop()) {
    bool hasSeparator          = false;
    unsigned int firstRowIndex = -1;

    int firstRowPriority = Profile::MaximumPriority;
    for (auto sourceRow : dropInfo.rows()) {
      hasSeparator = hasSeparator || ModInfo::getByIndex(sourceRow)->isSeparator();
      if (m_sortOrder == Qt::AscendingOrder &&
              m_profile->getModPriority(sourceRow) < firstRowPriority ||
          m_sortOrder == Qt::DescendingOrder &&
              m_profile->getModPriority(sourceRow) > firstRowPriority) {
        firstRowIndex    = sourceRow;
        firstRowPriority = m_profile->getModPriority(sourceRow);
      }
    }

    bool firstRowSeparator =
        firstRowIndex != -1 && ModInfo::getByIndex(firstRowIndex)->isSeparator();

    // row = -1 and valid parent means we're dropping onto an item, we don't want to
    // drop separators onto items or items into their own separator
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

bool ModListByPriorityProxy::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                          int row, int column,
                                          const QModelIndex& parent)
{
  // we need to fix the source model row if we are dropping at a
  // given priority (not a local file)
  const ModListDropInfo dropInfo(data, m_core);
  int sourceRow = -1;

  if (dropInfo.isLocalFileDrop()) {
    if (parent.isValid()) {
      sourceRow = static_cast<TreeItem*>(parent.internalPointer())->index;
    }
  } else {

    if (row >= 0) {
      if (!parent.isValid()) {
        if (row < m_Root.children.size()) {

          if (m_sortOrder == Qt::AscendingOrder) {
            sourceRow = m_Root.children[row]->index;

            // fix bug when dropping a mod just below an expanded separator
            //
            // by default, Qt consider it's a drop at the end of that separator
            // but we want to make it a drop at the beginning
            if (row > 0 && m_sortOrder == Qt::AscendingOrder &&
                m_Root.children[row - 1]->mod->isSeparator() &&
                !m_Root.children[row - 1]->children.empty() && m_dropExpanded &&
                m_dropPosition == ModListView::DropPosition::BelowItem) {
              sourceRow = m_Root.children[row - 1]->children[0]->index;
            }
          } else {
            sourceRow = m_Root.children[row - 1]->index;

            // fix drop below a collapsed separator or at the end of an expanded
            // separator, above the next item
            if (row > 0 && m_sortOrder == Qt::DescendingOrder &&
                m_Root.children[row - 1]->mod->isSeparator() &&
                !m_Root.children[row - 1]->children.empty() &&
                (!m_dropExpanded ||
                 m_dropPosition == ModListView::DropPosition::AboveItem)) {
              sourceRow = m_Root.children[row - 1]->children.back()->index;
            }
          }
        } else {
          sourceRow = ModInfo::getNumMods();
        }
      }

      // the parent is valid, we are dropping in a separator
      else {
        auto* item = static_cast<TreeItem*>(parent.internalPointer());

        // we usually need to decrement the row in descending priority, but if
        // it's the first row, we need to go back to the separator itself
        if (m_sortOrder == Qt::DescendingOrder && row == 0 &&
            m_dropPosition == ModListView::DropPosition::AboveItem) {
          sourceRow = item->index;
        } else {

          // in descending priority, we decrement the row to fix the drop position
          // because this is not done by the sort proxy for us
          if (m_sortOrder == Qt::DescendingOrder) {
            row--;
          }

          if (row < item->children.size()) {
            sourceRow = item->children[row]->index;
          } else if (parent.row() + 1 < m_Root.children.size()) {
            sourceRow = m_Root.children[parent.row() + 1]->index;
          }
        }
      }
    }

    // row < 0 and valid parent means we are dropping into an item,
    // which can only be a separator since dropping into non-separators
    // is disabled
    //
    // we want to drop at the end of the separator, so we need to find
    // the right priority
    else if (parent.isValid()) {

      // in ascending priority, we take the priority of the next top-level
      // item, which can be a separator or the overwrite mod, but is guaranteed
      // to exist
      if (m_sortOrder == Qt::AscendingOrder) {
        sourceRow = m_Root.children[parent.row() + 1]->index;
      }

      // in descending priority, we take the separator itself if it's empty or
      // its last children
      else {
        auto* item = m_Root.children[parent.row()];
        sourceRow = item->children.empty() ? item->index : item->children.back()->index;
      }
    }
  }

  return sourceModel()->dropMimeData(data, action, sourceRow, column, QModelIndex());
}

QModelIndex ModListByPriorityProxy::index(int row, int column,
                                          const QModelIndex& parent) const
{
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  const TreeItem* parentItem;
  if (!parent.isValid()) {
    parentItem = &m_Root;
  } else {
    parentItem = static_cast<TreeItem*>(parent.internalPointer());
  }

  return createIndex(row, column, parentItem->children[row]);
}

void ModListByPriorityProxy::onDropEnter(const QMimeData*, bool dropExpanded,
                                         ModListView::DropPosition dropPosition)
{
  m_dropExpanded = dropExpanded;
  m_dropPosition = dropPosition;
}
