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
  void criteriaChanged(std::vector<ModListSortProxy::Criteria> criteria);
  void optionsChanged(ModListSortProxy::FilterMode mode, bool separators);

private:
  class CriteriaItem;

  Ui::MainWindow* ui;
  CategoryFactory& m_factory;

  void onContextMenu(const QPoint &pos);
  void onSelection();
  void onCriteriaChanged();

  void editCategories();
  void toggleInverted(bool b);

  QTreeWidgetItem* addCriteriaItem(
    QTreeWidgetItem *root, const QString &name, int categoryID,
    ModListSortProxy::CriteriaType type);

  void addContentCriteria();
  void addCategoryCriteria(
    QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID);
  void addSpecialCriteria(int type);

};

#endif // MODORGANIZER_CATEGORIESLIST_INCLUDED
