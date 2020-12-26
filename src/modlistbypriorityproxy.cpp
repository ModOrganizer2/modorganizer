#include "modlistbypriorityproxy.h"

#include "modinfo.h"
#include "modlist.h"

ModListByPriorityProxy::ModListByPriorityProxy(ModList* modList, QObject* parent) :
  QAbstractProxyModel(parent), m_ModList(modList)
{
  setSourceModel(modList);
}

ModListByPriorityProxy::~ModListByPriorityProxy()
{
}

void ModListByPriorityProxy::setSourceModel(QAbstractItemModel* model)
{
  QAbstractProxyModel::setSourceModel(model);
  // connect(sourceModel(), &QAbstractItemModel::layoutChanged, this, &ModListByPriorityProxy::buildTree);
}

void ModListByPriorityProxy::buildTree()
{
  if (!sourceModel()) return;

  beginResetModel();

  endResetModel();

}

std::vector<ModListByPriorityProxy::ModInfoWithPriority> ModListByPriorityProxy::topLevelItems() const
{
  std::vector<ModListByPriorityProxy::ModInfoWithPriority> items;
  bool separator = false;
  for (auto& [priority, index] : m_ModList->m_Profile->getAllIndexesByPriority()) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
    if (modInfo->isSeparator()) {
      items.emplace_back(modInfo, priority);
      separator = true;
    }
    else if (modInfo->isOverwrite() || !separator) {
      items.emplace_back(modInfo, priority);
    }
  }

  return items;
}

std::vector<ModListByPriorityProxy::ModInfoWithPriority> ModListByPriorityProxy::childItems(int priority) const
{
  std::vector<ModListByPriorityProxy::ModInfoWithPriority> children;
  for (auto& [p, index] : m_ModList->m_Profile->getAllIndexesByPriority()) {
    if (p > priority) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
      if (modInfo->isSeparator()) {
        break;
      }
      children.emplace_back(modInfo, p);
    }
  }
  return children;
}

std::optional<ModListByPriorityProxy::ModInfoWithPriority> ModListByPriorityProxy::separator(int priority) const
{
  // overwrites
  if (priority == ULONG_MAX) {
    return {};
  }

  auto& indexByPriority = m_ModList->m_Profile->getAllIndexesByPriority();

  auto it = indexByPriority.find(priority);
  if (it == std::end(indexByPriority)) {
    return {};
  }

  {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(it->second);
    if (modInfo->isSeparator() || modInfo->isOverwrite()) {
      return {};
    }
  }

  auto rit = std::reverse_iterator{ it };
  for (; rit != std::rend(indexByPriority); ++rit) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(rit->second);
    if (modInfo->isSeparator()) {
      return ModInfoWithPriority{ modInfo, rit->first };
    }
  }

  return {};
}

QModelIndex ModListByPriorityProxy::mapFromSource(const QModelIndex& sourceIndex) const
{
  if (!sourceIndex.isValid()) {
    return QModelIndex();
  }

  auto topItems = topLevelItems();
  ModInfo::Ptr modInfo = ModInfo::getByIndex(sourceIndex.row());

  for (std::size_t i = 0; i < topItems.size(); ++i) {
    if (topItems[i].mod == modInfo) {
      return createIndex(i, sourceIndex.column(), modInfo.get());
    }
  }

  auto sep = separator(m_ModList->priority(modInfo->name()));
  auto children = childItems(sep->priority);
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (children[i].mod == modInfo) {
      return createIndex(i, sourceIndex.column(), modInfo.get());
    }
  }

  return QModelIndex();
}

QModelIndex ModListByPriorityProxy::mapToSource(const QModelIndex& proxyIndex) const
{
  if (!proxyIndex.isValid()) {
    return QModelIndex();
  }
  auto topItems = topLevelItems();
  ModInfo::Ptr modInfo;
  if (proxyIndex.parent().isValid()) {
    ModInfo::Ptr parentInfo = topItems[proxyIndex.parent().row()].mod;
    modInfo = childItems(m_ModList->priority(parentInfo->name()))[proxyIndex.row()].mod;
  }
  else {
    modInfo = topItems[proxyIndex.row()].mod;
  }
  return sourceModel()->index(ModInfo::getIndex(modInfo->name()), proxyIndex.column(), mapToSource(proxyIndex.parent()));
}

int ModListByPriorityProxy::rowCount(const QModelIndex& parent) const
{
  auto topItems = topLevelItems();
  if (!parent.isValid()) {
    return topItems.size();
  }

  ModInfo::Ptr modInfo = topItems[parent.row()].mod;
  if (!modInfo->isSeparator()) {
    return 0;
  }

  auto priority = m_ModList->priority(modInfo->name());
  return childItems(priority).size();
}

int ModListByPriorityProxy::columnCount(const QModelIndex& index) const
{
  return sourceModel()->columnCount(mapToSource(index));
}


QModelIndex ModListByPriorityProxy::parent(const QModelIndex& child) const
{

  auto topItems = topLevelItems();
  ModInfo::Ptr modInfo;
  if (child.parent().isValid()) {
    ModInfo::Ptr parentInfo = topItems[child.parent().row()].mod;
    modInfo = childItems(m_ModList->priority(parentInfo->name()))[child.row()].mod;
  }
  else {
    modInfo = topItems[child.row()].mod;
  }

  auto sep = separator(m_ModList->priority(modInfo->name()));

  if (!sep) {
    return QModelIndex();
  }

  for (std::size_t i = 0; i < topItems.size(); ++i) {
    if (topItems[i].mod == sep->mod) {
      return createIndex(i, child.column(), sep->mod.get());
    }
  }

  return QModelIndex();
}

bool ModListByPriorityProxy::hasChildren(const QModelIndex& parent) const
{
  if (!parent.isValid()) {
    return false;
  }
  ModInfo* modInfo = static_cast<ModInfo*>(parent.internalPointer());
  for (auto& item : topLevelItems()) {
    if (modInfo == item.mod) {
      return modInfo->isSeparator() && !childItems(item.priority).empty();
    }
  }
  return false;
}

QModelIndex ModListByPriorityProxy::index(int row, int column, const QModelIndex& parent) const
{
  auto topItems = topLevelItems();
  if (!parent.isValid()) {
    return createIndex(row, column, topItems[row].mod.get());
  }
  else {
    auto children = childItems(topItems[parent.row()].priority);
    return createIndex(row, column, children[row].mod.get());
  }
}
