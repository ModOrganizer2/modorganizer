#ifndef MODELUTILS_H
#define MODELUTILS_H

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QColor>

namespace MOShared
{

// retrieve all the row index under the given parent
QModelIndexList flatIndex(const QAbstractItemModel* model, int column = 0,
                          const QModelIndex& parent = QModelIndex());

// retrieve all the visible index in the given view
QModelIndexList visibleIndex(QTreeView* view, int column = 0);

// convert back-and-forth through model proxies
QModelIndex indexModelToView(const QModelIndex& index, const QAbstractItemView* view);
QModelIndexList indexModelToView(const QModelIndexList& index,
                                 const QAbstractItemView* view);
QModelIndex indexViewToModel(const QModelIndex& index, const QAbstractItemModel* model);
QModelIndexList indexViewToModel(const QModelIndexList& index,
                                 const QAbstractItemModel* model);

}  // namespace MOShared

#endif
