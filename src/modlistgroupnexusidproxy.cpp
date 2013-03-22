#include "modlistgroupnexusidproxy.h"
#include "modinfo.h"

ModListGroupNexusIDProxy::ModListGroupNexusIDProxy(Profile *profile, QObject *parent)
  : QAbstractProxyModel(parent)
{
  refreshMap(profile);
}

void ModListGroupNexusIDProxy::refreshMap(Profile *profile)
{
  int row = 0;
  for (unsigned int i = 0; i < ModInfo::getNumMods(); ++i) {
    int nexusID = ModInfo::getByIndex(i)->getNexusID();
    if (m_GroupMap.find(nexusID) == m_GroupMap.end()) {
      m_RowIdxMap[row] = i;
      m_IdxRowMap[i] = row;
      ++row;
    }
    m_GroupMap[nexusID].push_back(i);
  }

  for (auto iter = m_GroupMap.begin(); iter != m_GroupMap.end(); ++iter) {
    std::sort(iter->second.begin(), iter->second.end(),
              [profile] (unsigned int LHS, unsigned int RHS) {
                return profile->getModPriority(LHS) < profile->getModPriority(RHS);
              });
  }
}

int ModListGroupNexusIDProxy::rowCount(const QModelIndex &parent) const
{
  int res = 0;
  if (!parent.isValid()) {
    res = m_GroupMap.size();
  } else {
    ModInfo::Ptr info = ModInfo::getByIndex(parent.internalId());
    res = m_GroupMap.at(info->getNexusID()).size();
  }
  return res;
}

int ModListGroupNexusIDProxy::columnCount(const QModelIndex &parent) const
{
  return sourceModel()->columnCount(mapToSource(parent));
}

QModelIndex ModListGroupNexusIDProxy::mapToSource(const QModelIndex &proxyIndex) const
{
  auto iter = m_IndexMap.find(proxyIndex);
  if (iter != m_IndexMap.end()) {
    return iter->second;
  } else {
    return QModelIndex();
  }
}

QModelIndex ModListGroupNexusIDProxy::mapFromSource(const QModelIndex &sourceIndex) const
{
  unsigned int modID = sourceIndex.data(Qt::UserRole + 1).toInt();
  ModInfo::Ptr mod = ModInfo::getByIndex(modID);
  const std::vector<unsigned int> &subMods = m_GroupMap.at(mod->getNexusID());

  if (subMods.size() == 0) return QModelIndex();

  QModelIndex parent = QModelIndex();
  if (modID != subMods[0]) {
    parent = index(m_IdxRowMap.at(subMods[0]), 0, QModelIndex());
  }
  return index(sourceIndex.row(), sourceIndex.column(), parent);
}

QModelIndex ModListGroupNexusIDProxy::index(int row, int column, const QModelIndex &parent) const
{
  if (parent.isValid()) {
    // sub-mod
    ModInfo::Ptr parentMod = ModInfo::getByIndex(parent.internalId());
    const std::vector<unsigned int> &subMods = m_GroupMap.at(parentMod->getNexusID());
    if ((row < 0) || (row >= static_cast<int>(subMods.size()))) {
      qCritical("invalid index: %dx%d", row, column);
      return QModelIndex();
    }

    QPersistentModelIndex idx = createIndex(row, column, subMods[row]);
    m_IndexMap[idx] = QPersistentModelIndex(sourceModel()->index(subMods[row], column));

    return idx;
  } else {
    // top-level mod
    return createIndex(row, column, m_RowIdxMap.at(row));
  }
}

QModelIndex ModListGroupNexusIDProxy::parent(const QModelIndex &child) const
{
  if (!child.isValid()) return QModelIndex();

  ModInfo::Ptr mod = ModInfo::getByIndex(child.internalId());
  if (m_GroupMap.at(mod->getNexusID()).size() < 1) {
    return QModelIndex();
  }
  if (m_GroupMap.at(mod->getNexusID())[0] == child.internalId()) {
    return QModelIndex();
  } else {
    return index(m_IdxRowMap.at(m_GroupMap.at(mod->getNexusID())[0]), 0, QModelIndex());
  }
}

bool ModListGroupNexusIDProxy::hasChildren(const QModelIndex &parent) const
{
  if (!parent.isValid()) return true;
  ModInfo::Ptr mod = ModInfo::getByIndex(parent.internalId());
  return m_GroupMap.at(mod->getNexusID()).size() > 1;
}

QVariant ModListGroupNexusIDProxy::data(const QModelIndex &proxyIndex, int role) const
{
  auto iter = m_IndexMap.find(proxyIndex);
  if (iter != m_IndexMap.end()) {
    // mod
    return sourceModel()->data(iter->second, role);
  } else {
    return QVariant();
  }
}

QVariant ModListGroupNexusIDProxy::headerData(int section, Qt::Orientation orientation, int role) const
{
  return sourceModel()->headerData(section, orientation, role);
}
