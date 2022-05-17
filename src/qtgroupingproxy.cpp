/****************************************************************************************
 * Copyright (c) 2007-2011 Bart Cerneels <bart.cerneels@kde.org> *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under * the
 *terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later * version.
 **
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details. *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with * this
 *program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

// Modifications 2013-03-27 to 2013-03-29 by Sebastian Herbord

#include "qtgroupingproxy.h"
#include <log.h>

#include <QDebug>
#include <QIcon>
#include <QInputDialog>

using namespace MOBase;

/*!
    \class QtGroupingProxy
    \brief The QtGroupingProxy class will group source model rows by adding a new top
   tree-level. The source model can be flat or tree organized, but only the original top
   level rows are used for determining the grouping. \ingroup model-view
*/

QtGroupingProxy::QtGroupingProxy(QModelIndex rootNode, int groupedColumn,
                                 int groupedRole, unsigned int flags, int aggregateRole)
    : QAbstractProxyModel(), m_rootNode(rootNode), m_groupedColumn(0),
      m_groupedRole(groupedRole), m_aggregateRole(aggregateRole), m_flags(flags)
{
  if (groupedColumn != -1) {
    setGroupedColumn(groupedColumn);
  }
}

QtGroupingProxy::~QtGroupingProxy() {}

void QtGroupingProxy::setSourceModel(QAbstractItemModel* model)
{
  if (sourceModel()) {
    disconnect(sourceModel(), nullptr, this, nullptr);
  }

  QAbstractProxyModel::setSourceModel(model);

  if (sourceModel()) {
    // signal proxies
    connect(sourceModel(), SIGNAL(rowsInserted(const QModelIndex&, int, int)),
            SLOT(modelRowsInserted(const QModelIndex&, int, int)));
    connect(sourceModel(), SIGNAL(rowsAboutToBeInserted(const QModelIndex&, int, int)),
            SLOT(modelRowsAboutToBeInserted(const QModelIndex&, int, int)));
    connect(sourceModel(), SIGNAL(rowsRemoved(const QModelIndex&, int, int)),
            SLOT(modelRowsRemoved(const QModelIndex&, int, int)));
    connect(sourceModel(), SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)),
            SLOT(modelRowsAboutToBeRemoved(QModelIndex, int, int)));
    connect(sourceModel(), SIGNAL(layoutChanged()), SLOT(buildTree()));
    connect(sourceModel(), SIGNAL(dataChanged(QModelIndex, QModelIndex)),
            SLOT(modelDataChanged(QModelIndex, QModelIndex)));
    connect(sourceModel(), SIGNAL(modelReset()), this, SLOT(resetModel()));

    buildTree();
  }
}

void QtGroupingProxy::setGroupedColumn(int groupedColumn)
{
  m_groupedColumn = groupedColumn;
  buildTree();
}

/** Maps to what groups the source row belongs by returning the data of those groups.
 *
 * @returns a list of data for the rows the argument belongs to. In common cases this
 * list will contain only one entry. An empty list means that the source item will be
 * placed in the root of this proxyModel. There is no support for hiding source items.
 *
 * Group data can be pre-loaded in the return value so it's added to the cache
 * maintained by this class. This is required if you want to have data that is not
 * present in the source model.
 */
QList<RowData> QtGroupingProxy::belongsTo(const QModelIndex& idx)
{
  QList<RowData> rowDataList;

  // get all the data for this index from the model
  ItemData itemData = sourceModel()->itemData(idx);
  if (m_groupedRole != Qt::DisplayRole) {
    itemData[Qt::DisplayRole] = itemData[m_groupedRole];
  }

  // invalid value in grouped role -> ungrouped
  if (!itemData[Qt::DisplayRole].isValid()) {
    return rowDataList;
  }

  QMapIterator<int, QVariant> i(itemData);
  while (i.hasNext()) {
    i.next();
    int role         = i.key();
    QVariant variant = i.value();

    if (variant.type() == QVariant::List) {
      // a list of variants get's expanded to multiple rows
      QVariantList list = variant.toList();
      for (int i = 0; i < list.length(); i++) {
        // take an existing row data or create a new one
        RowData rowData = (rowDataList.count() > i) ? rowDataList.takeAt(i) : RowData();

        // we only gather data for the first column
        ItemData indexData = rowData.contains(0) ? rowData.take(0) : ItemData();
        indexData.insert(role, list.value(i));
        rowData.insert(0, indexData);
        // for the grouped column the data should not be gathered from the children
        // this will allow filtering on the content of this column with a
        // QSortFilterProxyModel
        rowData.insert(m_groupedColumn, indexData);
        rowDataList.insert(i, rowData);
      }
      break;
    } else if (!variant.isNull()) {
      // it's just a normal item. Copy all the data and break this loop.
      RowData rowData;
      rowData.insert(0, itemData);
      rowDataList << rowData;
      break;
    }
  }

  return rowDataList;
}

/* m_groupMap layout
 *  key : index of the group in m_groupMaps
 *  value : a QList of the original rows in sourceModel() for the children of this group
 *
 *  key = -1  contains a QList of the non-grouped indexes
 *
 * TODO: sub-groups
 */
void QtGroupingProxy::buildTree()
{
  if (!sourceModel())
    return;
  beginResetModel();

  m_groupMap.clear();
  // don't clear the data maps since most of it will probably be needed again.
  m_parentCreateList.clear();

  int max = sourceModel()->rowCount(m_rootNode);

  // WARNING: these have to be added in order because the addToGroups function is
  // optimized for modelRowsInserted(). Failure to do so will result in wrong data shown
  // in the view at best.
  for (int row = 0; row < max; row++) {
    QModelIndex idx = sourceModel()->index(row, m_groupedColumn, m_rootNode);
    addSourceRow(idx);
  }
  // dumpGroups();

  if (m_flags & FLAG_NOSINGLE) {
    // awkward: flatten single-item groups as a post-processing steps.

    int currentKey     = 0;
    quint32 quint32max = std::numeric_limits<quint32>::max();
    std::vector<int> rmgroups;

    QMap<quint32, QList<int>> temp;

    for (auto iter = m_groupMap.begin(); iter != m_groupMap.end(); ++iter) {
      if ((iter.key() == quint32max) || (iter->count() < 2)) {
        temp[quint32max].append(iter.value());
        if (iter.key() != quint32max) {
          rmgroups.push_back(iter.key());
        }
      } else {
        temp[currentKey++] = *iter;
      }
    }
    m_groupMap = temp;

    // second loop is necessary because qt containers can't be iterated from end to
    // front and removing by index from begin to end is ugly
    std::sort(rmgroups.begin(), rmgroups.end(), [](int lhs, int rhs) {
      return rhs < lhs;
    });
    for (auto iter = rmgroups.begin(); iter != rmgroups.end(); ++iter) {
      m_groupMaps.removeAt(*iter);
    }
  }

  endResetModel();
}

QList<int> QtGroupingProxy::addSourceRow(const QModelIndex& idx)
{
  QList<int> updatedGroups;
  QList<RowData> groupData = belongsTo(idx);

  // an empty list here means it's supposed to go in root.
  if (groupData.isEmpty()) {
    updatedGroups << -1;
    if (!m_groupMap.keys().contains(std::numeric_limits<quint32>::max()))
      m_groupMap.insert(std::numeric_limits<quint32>::max(),
                        QList<int>());  // add an empty placeholder
  }

  // an item can be in multiple groups
  foreach (RowData data, groupData) {
    int updatedGroup = -1;
    if (!data.isEmpty()) {
      foreach (const RowData& cachedData, m_groupMaps) {
        // when this matches the index belongs to an existing group
        if (data[0][Qt::DisplayRole] == cachedData[0][Qt::DisplayRole]) {
          data = cachedData;
          break;
        }
      }

      updatedGroup = m_groupMaps.indexOf(data);
      //-1 means not found
      if (updatedGroup == -1) {
        // new groups are added to the end of the existing list
        m_groupMaps << data;
        updatedGroup = m_groupMaps.count() - 1;
      }

      if (!m_groupMap.keys().contains(updatedGroup))
        m_groupMap.insert(updatedGroup, QList<int>());  // add an empty placeholder
    }

    if (!updatedGroups.contains(updatedGroup))
      updatedGroups << updatedGroup;
  }

  // update m_groupMap to the new source-model layout (one row added)
  QMutableMapIterator<quint32, QList<int>> i(m_groupMap);
  while (i.hasNext()) {
    i.next();
    QList<int>& groupList = i.value();
    int insertedProxyRow  = groupList.count();
    for (; insertedProxyRow > 0; insertedProxyRow--) {
      int& rowValue = groupList[insertedProxyRow - 1];
      if (idx.row() <= rowValue) {
        // increment the rows that come after the new row since they moved one place up.
        rowValue++;
      } else {
        break;
      }
    }

    if (updatedGroups.contains(i.key())) {
      // the row needs to be added to this group
      groupList.insert(insertedProxyRow, idx.row());
    }
  }

  return updatedGroups;
}

/** Each ModelIndex has in it's internalId a position in the parentCreateList.
 * struct ParentCreate are the instructions to recreate the parent index.
 * It contains the proxy row number of the parent and the postion in this list of the
 * grandfather. This function creates the ParentCreate structs and saves them in a list.
 */
int QtGroupingProxy::indexOfParentCreate(const QModelIndex& parent) const
{
  if (!parent.isValid())
    return -1;

  struct ParentCreate pc;
  for (int i = 0; i < m_parentCreateList.size(); i++) {
    pc = m_parentCreateList[i];
    if (pc.parentCreateIndex == parent.internalId() && pc.row == parent.row())
      return i;
  }
  // there is no parentCreate yet for this index, so let's create one.
  pc.parentCreateIndex = parent.internalId();
  pc.row               = parent.row();
  m_parentCreateList << pc;

  return m_parentCreateList.size() - 1;
}

QModelIndex QtGroupingProxy::index(int row, int column, const QModelIndex& parent) const
{
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  if (parent.column() > 0) {
    return QModelIndex();
  }

  /* We save the instructions to make the parent of the index in a struct.
   * The place of the struct in the list is stored in the internalId
   */
  int parentCreateIndex = indexOfParentCreate(parent);

  return createIndex(row, column, parentCreateIndex);
}

QModelIndex QtGroupingProxy::parent(const QModelIndex& index) const
{
  if (!index.isValid())
    return QModelIndex();

  int parentCreateIndex = index.internalId();
  if (parentCreateIndex == -1 || parentCreateIndex >= m_parentCreateList.count())
    return QModelIndex();

  struct ParentCreate pc = m_parentCreateList[parentCreateIndex];

  // only items at column 0 have children
  return createIndex(pc.row, 0, pc.parentCreateIndex);
}

int QtGroupingProxy::rowCount(const QModelIndex& index) const
{
  if (!index.isValid()) {
    // the number of top level groups + the number of non-grouped items
    int rows = m_groupMaps.count() +
               m_groupMap.value(std::numeric_limits<quint32>::max()).count();
    return rows;
  }

  // TODO:group in group support.
  if (isGroup(index)) {
    qint64 groupIndex = index.row();
    int rows          = m_groupMap.value(groupIndex).count();
    return rows;
  } else {
    QModelIndex originalIndex = mapToSource(index);
    int rowCount              = sourceModel()->rowCount(originalIndex);
    return rowCount;
  }
}

int QtGroupingProxy::columnCount(const QModelIndex& index) const
{
  if (!index.isValid())
    return sourceModel()->columnCount(m_rootNode);

  if (index.column() != 0)
    return 0;

  return sourceModel()->columnCount(mapToSource(index));
}

static bool variantLess(const QVariant& LHS, const QVariant& RHS)
{
  if ((LHS.type() == RHS.type()) &&
      ((LHS.type() == QVariant::Int) || (LHS.type() == QVariant::UInt))) {
    return LHS.toInt() < RHS.toInt();
  }

  // this should always work (comparing empty strings in the worst case) but
  // the results may be wrong
  return LHS.toString() < RHS.toString();
}

static QVariant variantMax(const QVariantList& variants)
{
  QVariant result = variants.first();
  foreach (const QVariant& iter, variants) {
    if (variantLess(result, iter)) {
      result = iter;
    }
  }
  return result;
}

static QVariant variantMin(const QVariantList& variants)
{
  QVariant result = variants.first();
  foreach (const QVariant& iter, variants) {
    if (variantLess(iter, result)) {
      result = iter;
    }
  }
  return result;
}

QVariant QtGroupingProxy::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  int row    = index.row();
  int column = index.column();
  if (isGroup(index)) {
    if ((role != Qt::DisplayRole) && (role != Qt::EditRole)) {
      switch (role) {
      case Qt::ForegroundRole: {
        return QBrush(Qt::gray);
      } break;
      case Qt::FontRole: {
        QFont font(m_groupMaps[row][column].value(Qt::FontRole).value<QFont>());
        font.setItalic(true);
        return font;
      } break;
      case Qt::TextAlignmentRole: {
        return Qt::AlignHCenter;
      } break;
      case Qt::UserRole: {
        return m_groupMaps[row][column].value(Qt::DisplayRole).toString();
      } break;
      case Qt::CheckStateRole: {
        if (column != 0)
          return QVariant();
        int childCount          = m_groupMap.value(row).count();
        int checked             = 0;
        QModelIndex parentIndex = this->index(row, 0, index.parent());
        for (int childRow = 0; childRow < childCount; ++childRow) {
          QModelIndex childIndex = this->index(childRow, 0, parentIndex);
          QVariant data          = mapToSource(childIndex).data(Qt::CheckStateRole);
          if (data.toInt() == 2)
            ++checked;
        }
        if (checked == childCount)
          return Qt::Checked;
        else if (checked == 0)
          return Qt::Unchecked;
        else
          return Qt::PartiallyChecked;
      } break;
      default: {
        QModelIndex parentIndex = this->index(row, 0, index.parent());
        if (m_groupMap.value(row).count() > 0) {
          return this->index(0, column, parentIndex).data(role);
        } else {
          return QVariant();
        }
        //                return m_groupMaps[row][column].value( role );
      } break;
      }
    }

    // use cached or precalculated data
    if (m_groupMaps[row][column].contains(Qt::DisplayRole)) {
      if ((m_flags & FLAG_NOGROUPNAME) != 0) {
        QModelIndex parentIndex = this->index(row, 0, index.parent());
        QModelIndex childIndex  = this->index(0, column, parentIndex);
        return childIndex.data(role).toString();
      } else {
        return m_groupMaps[row][column].value(role).toString();
      }
    }

    // for column 0 we gather data from the grouped column instead
    if (column == 0)
      column = m_groupedColumn;

    // map all data from children to columns of group to allow grouping one level up
    QVariantList variantsOfChildren;
    int childCount = m_groupMap.value(row).count();
    if (childCount == 0)
      return QVariant();

    int function = AGGR_NONE;
    if (m_aggregateRole >= Qt::UserRole) {
      QModelIndex parentIndex = this->index(row, 0, index.parent());
      QModelIndex childIndex  = this->index(0, column, parentIndex);
      function                = mapToSource(childIndex).data(m_aggregateRole).toInt();
    }

    // Need a parentIndex with column == 0 because only those have children.
    QModelIndex parentIndex = this->index(row, 0, index.parent());
    for (int childRow = 0; childRow < childCount; childRow++) {
      QModelIndex childIndex = this->index(childRow, column, parentIndex);
      QVariant data          = mapToSource(childIndex).data(role);

      if (data.isValid() && !variantsOfChildren.contains(data))
        variantsOfChildren << data;
    }

    // saving in cache
    ItemData roleMap = m_groupMaps[row].value(column);
    foreach (const QVariant& variant, variantsOfChildren) {
      if (roleMap[role] != variant) {
        roleMap.insert(role, variantsOfChildren);
      }
    }

    if (variantsOfChildren.count() == 0)
      return QVariant();

    // only one unique variant? No need to return a list
    switch (function) {
    case AGGR_EMPTY:
      return QVariant();
    case AGGR_FIRST:
      return variantsOfChildren.first();
    case AGGR_MAX:
      return variantMax(variantsOfChildren);
    case AGGR_MIN:
      return variantMin(variantsOfChildren);
    default: {
      if (variantsOfChildren.count() == 1)
        return variantsOfChildren.first();

      return variantsOfChildren;
    } break;
    }
  }

  return mapToSource(index).data(role);
}

bool QtGroupingProxy::setData(const QModelIndex& idx, const QVariant& value, int role)
{
  if (!idx.isValid())
    return false;

  // no need to set data to exactly the same value
  if (idx.data(role) == value)
    return false;

  if (isGroup(idx)) {
    ItemData columnData = m_groupMaps[idx.row()][idx.column()];

    columnData.insert(role, value);
    // QItemDelegate will always use Qt::EditRole
    if (role == Qt::EditRole)
      columnData.insert(Qt::DisplayRole, value);

    // and make sure it's stored in the map
    m_groupMaps[idx.row()].insert(idx.column(), columnData);

    int columnToChange = idx.column() ? idx.column() : m_groupedColumn;
    foreach (int originalRow, m_groupMap.value(idx.row())) {
      QModelIndex childIdx =
          sourceModel()->index(originalRow, columnToChange, m_rootNode);
      if (childIdx.isValid())
        sourceModel()->setData(childIdx, value, role);
    }
    // TODO: we might need to reload the data from the children at this point

    emit dataChanged(idx, idx);
    return true;
  }

  return sourceModel()->setData(mapToSource(idx), value, role);
}

bool QtGroupingProxy::isGroup(const QModelIndex& index) const
{
  int parentCreateIndex = index.internalId();
  if (parentCreateIndex == -1 && index.row() < m_groupMaps.count())
    return true;
  return false;
}

QModelIndex QtGroupingProxy::mapToSource(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return m_rootNode;
  }

  if (isGroup(index)) {
    return m_rootNode;
  }

  QModelIndex proxyParent    = index.parent();
  QModelIndex originalParent = mapToSource(proxyParent);

  int originalRow = index.row();
  if (originalParent == m_rootNode) {
    int indexInGroup = index.row();
    if (!proxyParent.isValid())
      indexInGroup -= m_groupMaps.count();

    QList<int> childRows = m_groupMap.value(proxyParent.row());
    if (childRows.isEmpty() || indexInGroup >= childRows.count() || indexInGroup < 0)
      return QModelIndex();

    originalRow = childRows.at(indexInGroup);
  }
  return sourceModel()->index(originalRow, index.column(), originalParent);
}

QModelIndexList QtGroupingProxy::mapToSource(const QModelIndexList& list) const
{
  QModelIndexList originalList;
  foreach (const QModelIndex& index, list) {
    QModelIndex originalIndex = mapToSource(index);
    if (originalIndex.isValid())
      originalList << originalIndex;
  }
  return originalList;
}

QModelIndex QtGroupingProxy::mapFromSource(const QModelIndex& idx) const
{
  if (!idx.isValid())
    return QModelIndex();

  QModelIndex proxyParent;
  QModelIndex sourceParent = idx.parent();

  int proxyRow  = idx.row();
  int sourceRow = idx.row();

  if (sourceParent.isValid() && (sourceParent != m_rootNode)) {
    // idx is a child of one of the items in the source model
    proxyParent = mapFromSource(sourceParent);
  } else {
    // idx is an item in the top level of the source model (child of the rootnode)
    int groupRow = -1;
    QMapIterator<quint32, QList<int>> iterator(m_groupMap);
    while (iterator.hasNext()) {
      iterator.next();
      if (iterator.value().contains(sourceRow)) {
        groupRow = iterator.key();
        break;
      }
    }

    if (groupRow != -1)  // it's in a group, let's find the correct row.
    {
      proxyParent = this->index(groupRow, 0, QModelIndex());
      proxyRow    = m_groupMap.value(groupRow).indexOf(sourceRow);
    } else {
      proxyParent = QModelIndex();
      // if the proxy item is not in a group it will be below the groups.
      int groupLength = m_groupMaps.count();
      int i = m_groupMap.value(std::numeric_limits<quint32>::max()).indexOf(sourceRow);

      proxyRow = groupLength + i;
    }
  }

  return this->index(proxyRow, idx.column(), proxyParent);
}

Qt::ItemFlags QtGroupingProxy::flags(const QModelIndex& idx) const
{
  if (!idx.isValid()) {
    Qt::ItemFlags rootFlags = sourceModel()->flags(m_rootNode);
    if (rootFlags.testFlag(Qt::ItemIsDropEnabled))
      return Qt::ItemFlags(Qt::ItemIsDropEnabled);

    return Qt::ItemFlags(0);
  }

  // only if the grouped column has the editable flag set allow the
  // actions leading to setData on the source (edit & drop)
  if (isGroup(idx)) {
    //        dumpGroups();
    Qt::ItemFlags defaultFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    // Qt::ItemFlags defaultFlags(Qt::ItemIsEnabled);
    bool groupIsEditable = true;

    if (idx.column() == 0) {
      bool checkable = true;
      foreach (int originalRow, m_groupMap.value(idx.row())) {
        QModelIndex originalIdx =
            sourceModel()->index(originalRow, 0, m_rootNode.parent());
        if ((originalIdx.flags() & Qt::ItemIsUserCheckable) == 0) {
          checkable = false;
        }
      }

      if (checkable) {
        defaultFlags |= Qt::ItemIsUserCheckable;
      }
    }

    // it's possible to have empty groups
    if (m_groupMap.value(idx.row()).count() == 0) {
      // check the flags of this column with the root node
      QModelIndex originalRootNode =
          sourceModel()->index(m_rootNode.row(), m_groupedColumn, m_rootNode.parent());
      groupIsEditable = originalRootNode.flags().testFlag(Qt::ItemIsEditable);
    } else {
      foreach (int originalRow, m_groupMap.value(idx.row())) {
        QModelIndex originalIdx =
            sourceModel()->index(originalRow, m_groupedColumn, m_rootNode);

        groupIsEditable =
            groupIsEditable ? originalIdx.flags().testFlag(Qt::ItemIsEditable) : false;
        if (!groupIsEditable)  // all children need to have an editable grouped column
          break;
      }
    }
    if (groupIsEditable)
      return (defaultFlags | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
    return defaultFlags;
  }

  QModelIndex originalIdx         = mapToSource(idx);
  Qt::ItemFlags originalItemFlags = sourceModel()->flags(originalIdx);

  // check the source model to see if the grouped column is editable;
  QModelIndex groupedColumnIndex =
      sourceModel()->index(originalIdx.row(), m_groupedColumn, originalIdx.parent());
  bool groupIsEditable =
      sourceModel()->flags(groupedColumnIndex).testFlag(Qt::ItemIsEditable);

  if (groupIsEditable)
    return originalItemFlags | Qt::ItemIsDragEnabled;
  return originalItemFlags;
}

QVariant QtGroupingProxy::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
  return sourceModel()->headerData(section, orientation, role);
}

bool QtGroupingProxy::canFetchMore(const QModelIndex& parent) const
{
  if (!parent.isValid())
    return false;

  if (isGroup(parent))
    return false;

  return sourceModel()->canFetchMore(mapToSource(parent));
}

void QtGroupingProxy::fetchMore(const QModelIndex& parent)
{
  if (!parent.isValid())
    return;

  if (isGroup(parent))
    return;

  return sourceModel()->fetchMore(mapToSource(parent));
}

QModelIndex QtGroupingProxy::addEmptyGroup(const RowData& data)
{
  int newRow = m_groupMaps.count();
  beginInsertRows(QModelIndex(), newRow, newRow);
  m_groupMaps << data;
  endInsertRows();
  return index(newRow, 0, QModelIndex());
}

bool QtGroupingProxy::removeGroup(const QModelIndex& idx)
{
  beginRemoveRows(idx.parent(), idx.row(), idx.row());
  m_groupMap.remove(idx.row());
  m_groupMaps.removeAt(idx.row());
  m_parentCreateList.removeAt(idx.internalId());
  endRemoveRows();

  // TODO: only true if all data could be unset.
  return true;
}

bool QtGroupingProxy::hasChildren(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    return true;
  }

  if (isGroup(parent)) {
    return !m_groupMap.value(parent.row()).isEmpty();
  }

  return sourceModel()->hasChildren(mapToSource(parent));
}

bool QtGroupingProxy::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                   int row, int column, const QModelIndex& parent)
{
  QModelIndex idx = index(row, column, parent);
  if (isGroup(idx)) {
    QList<int> childRows = m_groupMap.value(idx.row());
    int max              = *std::max_element(childRows.begin(), childRows.end());

    QModelIndex newIdx = mapToSource(index(max, column, idx));
    return sourceModel()->dropMimeData(data, action, max, column, newIdx);
  } else {
    if (row == -1) {
      return sourceModel()->dropMimeData(data, action, -1, -1, mapToSource(parent));
    } else {
      QModelIndex idx = mapToSource(index(row, column, parent));
      return sourceModel()->dropMimeData(data, action, idx.row(), idx.column(),
                                         idx.parent());
    }
  }
}

void QtGroupingProxy::modelRowsAboutToBeInserted(const QModelIndex& parent, int start,
                                                 int end)
{
  if (parent != m_rootNode) {
    // an item will be added to an original index, remap and pass it on
    QModelIndex proxyParent = mapFromSource(parent);
    beginInsertRows(proxyParent, start, end);
  }
}

void QtGroupingProxy::modelRowsInserted(const QModelIndex& parent, int start, int end)
{
  if (parent == m_rootNode) {
    // top level of the model changed, these new rows need to be put in groups
    for (int modelRow = start; modelRow <= end; modelRow++) {
      addSourceRow(sourceModel()->index(modelRow, m_groupedColumn, m_rootNode));
    }
  } else {
    // an item was added to an original index, remap and pass it on
    QModelIndex proxyParent = mapFromSource(parent);

    QString s;
    QDebug debug(&s);
    debug << proxyParent;
    log::debug("{}", s);

    // beginInsertRows had to be called in modelRowsAboutToBeInserted()
    endInsertRows();
  }
}

void QtGroupingProxy::modelRowsAboutToBeRemoved(const QModelIndex& parent, int start,
                                                int end)
{
  if (parent == m_rootNode) {
    QMap<quint32, QList<int>>::const_iterator i;
    // HACK, we are going to call beginRemoveRows() multiple times without
    // endRemoveRows() if a source index is in multiple groups.
    // This can be a problem for some views/proxies, but Q*Views can handle it.
    // TODO: investigate a queue for applying proxy model changes in the correct order
    for (i = m_groupMap.constBegin(); i != m_groupMap.constEnd(); ++i) {
      int groupIndex              = i.key();
      const QList<int>& groupList = i.value();
      QModelIndex proxyParent     = index(groupIndex, 0);
      foreach (int originalRow, groupList) {
        if (originalRow >= start && originalRow <= end) {
          int proxyRow = groupList.indexOf(originalRow);
          if (groupIndex == -1)  // adjust for non-grouped (root level) original items
            proxyRow += m_groupMaps.count();
          // TODO: optimize for continues original rows in the same group
          beginRemoveRows(proxyParent, proxyRow, proxyRow);
        }
      }
    }
  } else {
    // child item(s) of an original item will be removed, remap and pass it on
    QModelIndex proxyParent = mapFromSource(parent);
    beginRemoveRows(proxyParent, start, end);
  }
}

void QtGroupingProxy::modelRowsRemoved(const QModelIndex& parent, int start, int end)
{
  if (parent == m_rootNode) {
    // TODO: can be optimised by iterating over m_groupMap and checking start <= r < end

    // rather than increasing i we change the stored sourceRows in-place and reuse
    // argument start X-times (where X = end - start).
    for (int i = start; i <= end; i++) {
      // HACK: we are going to iterate the hash in reverse so calls to endRemoveRows()
      // are matched up with the beginRemoveRows() in modelRowsAboutToBeRemoved()
      // NOTE: easier to do reverse with java style iterator
      QMutableMapIterator<quint32, QList<int>> iter(m_groupMap);
      iter.toBack();
      while (iter.hasPrevious()) {
        iter.previous();
        int groupIndex = iter.key();
        // has to be a modifiable reference for remove and replace operations
        QList<int>& groupList = iter.value();
        int rowIndex          = groupList.indexOf(start);
        if (rowIndex != -1) {
          QModelIndex proxyParent = index(groupIndex, 0);
          groupList.removeAt(rowIndex);
        }
        // Now decrement all source rows that are after the removed row
        for (int j = 0; j < groupList.count(); j++) {
          int sourceRow = groupList.at(j);
          if (sourceRow > start)
            groupList.replace(j, sourceRow - 1);
        }
        if (rowIndex != -1)
          endRemoveRows();  // end remove operation only after group was updated.
      }
    }

    return;
  }

  // beginRemoveRows had to be called in modelRowsAboutToBeRemoved();
  endRemoveRows();
}

void QtGroupingProxy::resetModel()
{
  buildTree();
}

void QtGroupingProxy::modelDataChanged(const QModelIndex& topLeft,
                                       const QModelIndex& bottomRight)
{
  // TODO: need to look in the groupedColumn and see if it changed and changed grouping
  // accordingly
  QModelIndex proxyTopLeft = mapFromSource(topLeft);
  if (!proxyTopLeft.isValid())
    return;

  if (topLeft == bottomRight) {
    emit dataChanged(proxyTopLeft, proxyTopLeft);
  } else {
    QModelIndex proxyBottomRight = mapFromSource(bottomRight);
    emit dataChanged(proxyTopLeft, proxyBottomRight);
  }
}

bool QtGroupingProxy::isAGroupSelected(const QModelIndexList& list) const
{
  foreach (const QModelIndex& index, list) {
    if (isGroup(index))
      return true;
  }
  return false;
}

void QtGroupingProxy::dumpGroups() const
{
  QString s;
  QDebug debug(&s);

  debug << "m_groupMap:\n";
  for (int groupIndex = -1; groupIndex < m_groupMap.keys().count() - 1; groupIndex++) {
    debug << groupIndex << " : " << m_groupMap.value(groupIndex) << "\n";
  }

  debug << "m_groupMaps:\n";
  for (int groupIndex = 0; groupIndex < m_groupMaps.count(); groupIndex++) {
    debug << m_groupMaps[groupIndex] << ": " << m_groupMap.value(groupIndex) << "\n";
  }

  debug << m_groupMap.value(std::numeric_limits<quint32>::max());

  log::debug("{}", s);
}
