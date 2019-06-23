#include "modinfodialogtab.h"

class CategoryFactory;

class CategoriesTab : public ModInfoDialogTab
{
public:
  CategoriesTab(
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui);

  void clear() override;
  void update() override;

private:
  void add(
    const CategoryFactory& factory, const std::set<int>& enabledCategories,
    QTreeWidgetItem* root, int rootLevel);

  void updatePrimary();
  void addChecked(QTreeWidgetItem* tree);

  void save(QTreeWidgetItem* currentNode);

  void onCategoryChanged(QTreeWidgetItem* item, int col);
  void onPrimaryChanged(int index);
};
