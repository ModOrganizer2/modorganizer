/****************************************************************************************
 * Copyright (c) 2007-2010 Bart Cerneels <bart.cerneels@kde.org> *
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

// Modifications 2013-03-27 to 2013-03-28 by Sebastian Herbord

#ifndef GROUPINGPROXY_H
#define GROUPINGPROXY_H

#include <QAbstractProxyModel>
#include <QIcon>
#include <QModelIndex>
#include <QMultiHash>
#include <QSet>
#include <QStringList>

typedef QMap<int, QVariant> ItemData;
typedef QMap<int, ItemData> RowData;

class QtGroupingProxy : public QAbstractProxyModel
{
  Q_OBJECT

public:
  static const unsigned int FLAG_NOSINGLE    = 1;
  static const unsigned int FLAG_NOGROUPNAME = 2;

  enum EAggregateFunction
  {
    AGGR_NONE,   // no aggregation, return child elements as list
    AGGR_EMPTY,  // display nothing
    AGGR_FIRST,  // return value of the topmost item
    AGGR_MAX,    // return maximum value
    AGGR_MIN     // return minimum value
  };

public:
  explicit QtGroupingProxy(QModelIndex rootNode = QModelIndex(), int groupedColumn = -1,
                           int groupedRole = Qt::DisplayRole, unsigned int flags = 0,
                           int aggregateRole = Qt::DisplayRole);
  ~QtGroupingProxy();

  void setSourceModel(QAbstractItemModel* model) override;
  void setGroupedColumn(int groupedColumn);

  /* QAbstractProxyModel methods */
  virtual QModelIndex index(int, int c = 0,
                            const QModelIndex& parent = QModelIndex()) const;
  virtual Qt::ItemFlags flags(const QModelIndex& idx) const;
  virtual QModelIndex parent(const QModelIndex& idx) const;
  virtual int rowCount(const QModelIndex& idx = QModelIndex()) const;
  virtual int columnCount(const QModelIndex& idx) const;
  virtual QModelIndex mapToSource(const QModelIndex& idx) const;
  virtual QModelIndexList mapToSource(const QModelIndexList& list) const;
  virtual QModelIndex mapFromSource(const QModelIndex& idx) const;
  virtual QVariant data(const QModelIndex& idx, int role) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  virtual bool canFetchMore(const QModelIndex& parent) const;
  virtual void fetchMore(const QModelIndex& parent);
  virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const;
  virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                            int column, const QModelIndex& parent);

  /* QtGroupingProxy methods */
  virtual QModelIndex addEmptyGroup(const RowData& data);
  virtual bool removeGroup(const QModelIndex& idx);

protected slots:
  virtual void buildTree();

private slots:
  void modelDataChanged(const QModelIndex&, const QModelIndex&);
  void modelRowsAboutToBeInserted(const QModelIndex&, int, int);
  void modelRowsInserted(const QModelIndex&, int, int);
  void modelRowsAboutToBeRemoved(const QModelIndex&, int, int);
  void modelRowsRemoved(const QModelIndex&, int, int);
  void resetModel();

protected:
  /** Maps an item to a group.
   * The return value is a list because an item can put in multiple groups.
   * Inside the list is a 2 dimensional map.
   * Mapped to column-number is another map of role-number to QVariant.
   * This data prepolulates the group-data cache. The rest is gathered on demand
   * from the children of the group.
   */
  virtual QList<RowData> belongsTo(const QModelIndex& idx);

  /**
   * calls belongsTo(), checks cached data and adds the index to existing or new groups.
   * @returns the groups this index was added to where -1 means it was added to the
   * root.
   */
  QList<int> addSourceRow(const QModelIndex& idx);

  bool isGroup(const QModelIndex& index) const;
  bool isAGroupSelected(const QModelIndexList& list) const;

  /** Maintains the group -> sourcemodel row mapping
   * The reason a QList<int> is use instead of a QMultiHash is that the values have to
   * be reordered when rows are inserted or removed.
   * TODO:use some auto-incrementing container class (steveire's?) for the list
   */
  QMap<quint32, QList<int>> m_groupMap;
  /** The data cache of the groups.
   * This can be pre-loaded with data in belongsTo()
   */
  QList<RowData> m_groupMaps;

  /** "instuctions" how to create an item in the tree.
   * This is used by parent( QModelIndex )
   */
  struct ParentCreate
  {
    int parentCreateIndex;
    int row;
  };
  mutable QList<struct ParentCreate> m_parentCreateList;
  /** @returns index of the "instructions" to recreate the parent. Will create new if it
   * doesn't exist yet.
   */
  int indexOfParentCreate(const QModelIndex& parent) const;

  QModelIndexList m_selectedGroups;

  QModelIndex m_rootNode;
  int m_groupedColumn;

  /* debug function */
  void dumpGroups() const;

private:
  unsigned int m_flags;
  int m_groupedRole;

  int m_aggregateRole;
};

#endif  // GROUPINGPROXY_H
