#ifndef MODLISTGROUPPROXY_H
#define MODLISTGROUPPROXY_H

#include <QIdentityProxyModel>
#include <tuple>

class ModListGroupCategoriesProxy : public QAbstractProxyModel
{

  Q_OBJECT

public:

  explicit ModListGroupCategoriesProxy(QObject *parent = 0);

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

  mutable std::map<QPersistentModelIndex, QPersistentModelIndex> m_IndexMap;

};

#endif // MODLISTGROUPPROXY_H
