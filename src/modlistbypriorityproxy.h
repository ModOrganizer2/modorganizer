#ifndef MODLISBYPRIORITYPROXY_H
#define MODLISBYPRIORITYPROXY_H

#include <optional>
#include <vector>

#include <QAbstractProxyModel>
#include <QModelIndex>
#include <QMultiHash>
#include <QStringList>
#include <QIcon>
#include <QSet>

#include "modinfo.h"

class ModList;

class ModListByPriorityProxy : public QAbstractProxyModel
{
  Q_OBJECT

public:
  explicit ModListByPriorityProxy(ModList* modList, QObject* parent = nullptr);
  ~ModListByPriorityProxy();

  void setSourceModel(QAbstractItemModel* sourceModel) override;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& index) const override;
  bool hasChildren(const QModelIndex& parent) const override;

  QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
  QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;

private:

  void buildTree();

  struct ModInfoWithPriority {
    const ModInfo::Ptr mod;
    const int priority;
  };

  std::vector<ModInfoWithPriority> topLevelItems() const;
  std::vector<ModInfoWithPriority> childItems(int priority) const;
  std::optional<ModInfoWithPriority> separator(int priority) const;

private:

  ModList* m_ModList;

};

#endif //GROUPINGPROXY_H
