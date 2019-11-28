#ifndef MODORGANIZER_CATEGORIESLIST_INCLUDED
#define MODORGANIZER_CATEGORIESLIST_INCLUDED

#include "modlistsortproxy.h"
#include <QTreeWidgetItem>

namespace Ui { class MainWindow; };
class CategoryFactory;

class FilterList : public QObject
{
  Q_OBJECT;

public:
  FilterList(Ui::MainWindow* ui, CategoryFactory& factory);

  void setSelection(std::vector<int> categories);
  void clearSelection();
  void refresh();

signals:
  void changed(std::vector<int> categories, std::vector<int> content);

private:
  Ui::MainWindow* ui;
  CategoryFactory& m_factory;

  void onContextMenu(const QPoint &pos);
  void onSelection();

  void editCategories();

  QTreeWidgetItem* addFilterItem(
    QTreeWidgetItem *root, const QString &name, int categoryID,
    ModListSortProxy::FilterType type);

  void addContentFilters();
  void addCategoryFilters(
    QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID);
};

#endif // MODORGANIZER_CATEGORIESLIST_INCLUDED
