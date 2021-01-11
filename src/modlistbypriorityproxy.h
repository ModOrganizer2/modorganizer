#ifndef MODLISBYPRIORITYPROXY_H
#define MODLISBYPRIORITYPROXY_H

#include <optional>
#include <set>
#include <vector>

#include <QAbstractProxyModel>
#include <QModelIndex>
#include <QMultiHash>
#include <QStringList>
#include <QIcon>
#include <QSet>

#include "modinfo.h"
#include "modlistview.h"

class ModList;
class ModListDropInfo;
class Profile;

class ModListByPriorityProxy : public QAbstractProxyModel
{
  Q_OBJECT

public:
  explicit ModListByPriorityProxy(Profile* profile, OrganizerCore& core, QObject* parent = nullptr);
  ~ModListByPriorityProxy();

  void setProfile(Profile* profile);
  void refresh();

  void setSourceModel(QAbstractItemModel* sourceModel) override;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& index) const override;
  bool hasChildren(const QModelIndex& parent) const override;

  bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

  QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
  QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;

public slots:

  void onDropEnter(const QMimeData* data, bool dropExpanded, ModListView::DropPosition dropPosition);

protected slots:

  void modelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles = QVector<int>());

private:

  void buildTree();

  struct TreeItem {
    ModInfo::Ptr mod;
    unsigned int index;
    std::vector<std::unique_ptr<TreeItem>> children;
    TreeItem* parent;

    std::size_t childIndex(TreeItem* child) const {
      for (std::size_t i = 0; i < children.size(); ++i) {
        if (children[i].get() == child) {
          return i;
        }
      }
      return -1;
    }

    TreeItem() : TreeItem(nullptr, -1) { }
    TreeItem(ModInfo::Ptr mod, unsigned int index, TreeItem* parent = nullptr) :
      mod(mod), index(index), parent(parent) { }
  };

  TreeItem m_Root;
  std::map<unsigned int, TreeItem*> m_IndexToItem;

private:
  OrganizerCore& m_core;
  Profile* m_profile;

  bool m_dropExpanded = false;
  ModListView::DropPosition m_dropPosition = ModListView::DropPosition::OnItem;
};

#endif //GROUPINGPROXY_H
