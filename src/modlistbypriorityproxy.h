#ifndef MODLISBYPRIORITYPROXY_H
#define MODLISBYPRIORITYPROXY_H

#include <optional>
#include <set>
#include <vector>

#include <QAbstractProxyModel>
#include <QIcon>
#include <QModelIndex>
#include <QMultiHash>
#include <QSet>
#include <QStringList>

#include "modinfo.h"
#include "modlistview.h"

class ModList;
class ModListDropInfo;
class Profile;

class ModListByPriorityProxy : public QAbstractProxyModel
{
  Q_OBJECT

public:
  explicit ModListByPriorityProxy(Profile* profile, OrganizerCore& core,
                                  QObject* parent = nullptr);
  ~ModListByPriorityProxy();

  void setProfile(Profile* profile);

  // set the sort order but does not refresh the proxy
  //
  void setSortOrder(Qt::SortOrder order);

  void setSourceModel(QAbstractItemModel* sourceModel) override;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  QModelIndex index(int row, int column,
                    const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& index) const override;
  bool hasChildren(const QModelIndex& parent) const override;

  bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                       int column, const QModelIndex& parent) const override;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                    const QModelIndex& parent) override;

  QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
  QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;

public slots:

  void onDropEnter(const QMimeData* data, bool dropExpanded,
                   ModListView::DropPosition dropPosition);

protected slots:

  void onModelRowsRemoved(const QModelIndex& parent, int first, int last);
  void
  onModelLayoutChanged(const QList<QPersistentModelIndex>& parents = {},
                       LayoutChangeHint hint = LayoutChangeHint::NoLayoutChangeHint);
  void onModelReset();
  void onModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
                          const QVector<int>& roles = QVector<int>());

private:
  // fill the mapping from index to item (required by buildTree), should
  // only be used for full model reset because it destroys existing references
  // to tree items
  //
  void buildMapping();

  // create the actual tree by creating parent/child associations, requires
  // the mapping to be created (by a previous buildMapping call)
  //
  void buildTree();

  struct TreeItem
  {
    ModInfo::Ptr mod;
    unsigned int index;
    std::vector<TreeItem*> children;
    TreeItem* parent;

    std::size_t childIndex(TreeItem* child) const
    {
      for (std::size_t i = 0; i < children.size(); ++i) {
        if (children[i] == child) {
          return i;
        }
      }
      return -1;
    }

    TreeItem() : TreeItem(nullptr, -1) {}
    TreeItem(ModInfo::Ptr mod, unsigned int index, TreeItem* parent = nullptr)
        : mod(mod), index(index), parent(parent)
    {}
  };

  TreeItem m_Root;
  std::map<unsigned int, std::unique_ptr<TreeItem>> m_IndexToItem;

private:
  OrganizerCore& m_core;
  Profile* m_profile;

  Qt::SortOrder m_sortOrder                = Qt::AscendingOrder;
  bool m_dropExpanded                      = false;
  ModListView::DropPosition m_dropPosition = ModListView::DropPosition::OnItem;
};

#endif  // GROUPINGPROXY_H
