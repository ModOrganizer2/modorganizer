#include "modelutils.h"

#include <QAbstractProxyModel>

namespace MOShared
{

QModelIndexList flatIndex(const QAbstractItemModel* model, int column,
                          const QModelIndex& parent)
{
  QModelIndexList index;
  for (std::size_t i = 0; i < model->rowCount(parent); ++i) {
    index.append(model->index(i, column, parent));
    index.append(flatIndex(model, column, index.back()));
  }
  return index;
}

static QModelIndexList visibleIndexImpl(QTreeView* view, int column,
                                        const QModelIndex& parent)
{
  if (parent.isValid() && !view->isExpanded(parent)) {
    return {};
  }

  auto* model = view->model();
  QModelIndexList index;
  for (std::size_t i = 0; i < model->rowCount(parent); ++i) {
    index.append(model->index(i, column, parent));
    index.append(visibleIndexImpl(view, column, index.back()));
  }
  return index;
}

QModelIndexList visibleIndex(QTreeView* view, int column)
{
  return visibleIndexImpl(view, column, QModelIndex());
}

QModelIndex indexModelToView(const QModelIndex& index, const QAbstractItemView* view)
{
  // we need to stack the proxy
  std::vector<QAbstractProxyModel*> proxies;
  {
    auto* currentModel = view->model();
    while (auto* proxy = qobject_cast<QAbstractProxyModel*>(currentModel)) {
      proxies.push_back(proxy);
      currentModel = proxy->sourceModel();
    }
  }

  if (proxies.empty() || proxies.back()->sourceModel() != index.model()) {
    return QModelIndex();
  }

  auto qindex = index;
  for (auto rit = proxies.rbegin(); rit != proxies.rend(); ++rit) {
    qindex = (*rit)->mapFromSource(qindex);
  }

  return qindex;
}

QModelIndexList indexModelToView(const QModelIndexList& index,
                                 const QAbstractItemView* view)
{
  QModelIndexList result;
  for (auto& idx : index) {
    result.append(indexModelToView(idx, view));
  }
  return result;
}

QModelIndex indexViewToModel(const QModelIndex& index, const QAbstractItemModel* model)
{
  if (index.model() == model) {
    return index;
  } else if (auto* proxy = qobject_cast<const QAbstractProxyModel*>(index.model())) {
    return indexViewToModel(proxy->mapToSource(index), model);
  } else {
    return QModelIndex();
  }
}

QModelIndexList indexViewToModel(const QModelIndexList& index,
                                 const QAbstractItemModel* model)
{
  QModelIndexList result;
  for (auto& idx : index) {
    result.append(indexViewToModel(idx, model));
  }
  return result;
}

}  // namespace MOShared
