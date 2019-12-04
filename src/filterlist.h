#ifndef MODORGANIZER_CATEGORIESLIST_INCLUDED
#define MODORGANIZER_CATEGORIESLIST_INCLUDED

#include "modlistsortproxy.h"
#include <QTreeWidgetItem>

namespace Ui { class MainWindow; };
class CategoryFactory;
class Settings;

class FilterList : public QObject
{
  Q_OBJECT;

public:
  FilterList(Ui::MainWindow* ui, CategoryFactory& factory);

  void restoreState(const Settings& s);
  void saveState(Settings& s) const;

  void setSelection(const std::vector<ModListSortProxy::Criteria>& criteria);
  void clearSelection();
  void refresh();

signals:
  void criteriaChanged(std::vector<ModListSortProxy::Criteria> criteria);
  void optionsChanged(
    ModListSortProxy::FilterMode mode, ModListSortProxy::SeparatorsMode sep);

private:
  class CriteriaItem;

  Ui::MainWindow* ui;
  CategoryFactory& m_factory;

  bool onClick(QMouseEvent* e);
  void onItemActivated(QTreeWidgetItem* item);
  void onOptionsChanged();

  void editCategories();
  void checkCriteria();
  bool cycleItem(QTreeWidgetItem* item, int direction);

  QTreeWidgetItem* addCriteriaItem(
    QTreeWidgetItem *root, const QString &name, int categoryID,
    ModListSortProxy::CriteriaType type);

  void addContentCriteria();
  void addCategoryCriteria(
    QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID);
  void addSpecialCriteria(int type);
};

#endif // MODORGANIZER_CATEGORIESLIST_INCLUDED
