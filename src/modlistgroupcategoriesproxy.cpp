#include "modlistgroupcategoriesproxy.h"
#include "categories.h"
#include "modinfo.h"


ModListGroupCategoriesProxy::ModListGroupCategoriesProxy(QObject *parent)
  : QAbstractProxyModel(parent)
{
}

int ModListGroupCategoriesProxy::rowCount(const QModelIndex &parent) const
{
  if (!parent.isValid()) {
    return CategoryFactory::instance().numCategories();
  } else {
    unsigned int count = 0;
    for (int i = 0; i < sourceModel()->rowCount(); ++i) {
      if (ModInfo::getByIndex(i)->getPrimaryCategory() ==
            CategoryFactory::instance().getCategoryID(parent.row())) {
        ++count;
      }
    }
    return count;
  }
}

int ModListGroupCategoriesProxy::columnCount(const QModelIndex &parent) const
{
  return sourceModel()->columnCount(mapToSource(parent));
}

QModelIndex ModListGroupCategoriesProxy::mapToSource(const QModelIndex &proxyIndex) const
{
  auto iter = m_IndexMap.find(proxyIndex);
  if (iter != m_IndexMap.end()) {
    return iter->second;
  } else {
    return QModelIndex();
  }
}

QModelIndex ModListGroupCategoriesProxy::mapFromSource(const QModelIndex &sourceIndex) const
{
  if (!sourceIndex.isValid()) {
    return QModelIndex();
  } else {
    ModInfo::Ptr mod = ModInfo::getByIndex(sourceIndex.data(Qt::UserRole + 1).toInt());
    return index(sourceIndex.row(), sourceIndex.column(), index(mod->getPrimaryCategory(), 0, QModelIndex()));
  }
}

QModelIndex ModListGroupCategoriesProxy::index(int row, int column, const QModelIndex &parent) const
{
  if (parent.isValid()) {
    // mod
    int categoryId = CategoryFactory::instance().getCategoryID(parent.row());

    int srcRow = 0;
    int offset = row;
    for (; srcRow < sourceModel()->rowCount(); ++srcRow) {
      int modId = sourceModel()->index(srcRow, column).data(Qt::UserRole + 1).toInt();
      if (ModInfo::getByIndex(modId)->getPrimaryCategory() == categoryId) {

        if (--offset < 0) {
          break;
        }
      }
    }
    QPersistentModelIndex idx = createIndex(row, column, categoryId);
    m_IndexMap[idx] = QPersistentModelIndex(sourceModel()->index(srcRow, column));
    return idx;
  } else {
    // category
    return createIndex(row, column, -1);
  }
}

QModelIndex ModListGroupCategoriesProxy::parent(const QModelIndex &child) const
{
  if (!child.isValid()) {
    return QModelIndex();
  } else if (child.internalId() == -1) {
    return QModelIndex(); // top level
  } else {
    return index(CategoryFactory::instance().getCategoryIndex(child.internalId()), 0, QModelIndex());
  }
}

bool ModListGroupCategoriesProxy::hasChildren(const QModelIndex &parent) const
{
  // root item always has children (the categories), mods don't
  if (!parent.isValid()) return true;
  else if (parent.internalId() != -1) return false;

  for (int i = 0; i < sourceModel()->rowCount(); ++i) {
    ModInfo::Ptr mod = ModInfo::getByIndex(i);
    if (mod->getPrimaryCategory() == CategoryFactory::instance().getCategoryID(parent.row())) {
      return true;
    }
  }
  return false;
}

QVariant ModListGroupCategoriesProxy::data(const QModelIndex &proxyIndex, int role) const
{
  auto iter = m_IndexMap.find(proxyIndex);
  if (iter != m_IndexMap.end()) {
    // mod
    return sourceModel()->data(iter->second, role);
  } else {
    // category
    if ((role == Qt::DisplayRole) && (proxyIndex.column() == 0)) {
      return CategoryFactory::instance().getCategoryName(proxyIndex.row());
    } else {
      return QVariant();
    }
  }
}

QVariant ModListGroupCategoriesProxy::headerData(int section, Qt::Orientation orientation, int role) const
{
  return sourceModel()->headerData(section, orientation, role);
}
