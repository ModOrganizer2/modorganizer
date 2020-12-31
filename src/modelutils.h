#ifndef MODELUTILS_H
#define MODELUTILS_H

#include <QAbstractItemView>
#include <QAbstractItemModel>

// convert back-and-forth through model proxies
QModelIndex indexModelToView(const QModelIndex& index, const QAbstractItemView* view);
QModelIndexList indexModelToView(const QModelIndexList& index, const QAbstractItemView* view);
QModelIndex indexViewToModel(const QModelIndex& index, const QAbstractItemModel* model);
QModelIndexList indexViewToModel(const QModelIndexList& index, const QAbstractItemModel* model);


#endif
