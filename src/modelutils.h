#ifndef MODELUTILS_H
#define MODELUTILS_H

#include <QAbstractItemView>
#include <QAbstractItemModel>

// retrieve all the row index under the given parent
QModelIndexList flatIndex(
  const QAbstractItemModel* model, int column = 0, const QModelIndex& parent = QModelIndex());

// convert back-and-forth through model proxies
QModelIndex indexModelToView(const QModelIndex& index, const QAbstractItemView* view);
QModelIndexList indexModelToView(const QModelIndexList& index, const QAbstractItemView* view);
QModelIndex indexViewToModel(const QModelIndex& index, const QAbstractItemModel* model);
QModelIndexList indexViewToModel(const QModelIndexList& index, const QAbstractItemModel* model);


#endif
