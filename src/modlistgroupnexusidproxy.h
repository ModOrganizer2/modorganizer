#ifndef MODLISTGROUPNEXUSIDPROXY_H
#define MODLISTGROUPNEXUSIDPROXY_H


#include <QAbstractProxyModel>
#include <vector>
#include <map>
#include "profile.h"


class ModListGroupNexusIDProxy : public QAbstractProxyModel
{
  Q_OBJECT
public:
  explicit ModListGroupNexusIDProxy(Profile *profile, QObject *parent = 0);

  virtual int rowCount(const QModelIndex &parent) const;
  virtual int columnCount(const QModelIndex &parent) const;

  virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
  virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;

  virtual QModelIndex index(int row, int column, const QModelIndex &parent) const;
  virtual QModelIndex parent(const QModelIndex &child) const;

  virtual bool hasChildren(const QModelIndex &parent) const;

  virtual QVariant data(const QModelIndex &proxyIndex, int role) const;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
signals:
  
public slots:

private:

  void refreshMap(Profile *profile);

private:

  std::map<int, std::vector<unsigned int> > m_GroupMap;

  std::map<unsigned int, unsigned int> m_RowIdxMap; // maps row to mod id
  std::map<unsigned int, unsigned int> m_IdxRowMap; // maps mod id to row

  mutable std::map<QPersistentModelIndex, QPersistentModelIndex> m_IndexMap;

};

#endif // MODLISTGROUPNEXUSIDPROXY_H
