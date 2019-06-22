#include "modinfodialogtab.h"

class CategoryFactory;

class CategoriesTab : public ModInfoDialogTab
{
public:
  CategoriesTab(QWidget* parent, Ui::ModInfoDialog* ui);

  void clear() override;
  void setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin) override;
  void update() override;

private:
  Ui::ModInfoDialog* ui;
  ModInfo::Ptr m_mod;

  void add(
    const CategoryFactory& factory, const std::set<int>& enabledCategories,
    QTreeWidgetItem* root, int rootLevel);

  void updatePrimary();
  void addChecked(QTreeWidgetItem* tree);

  void save(QTreeWidgetItem* currentNode);

  void onCategoryChanged(QTreeWidgetItem* item, int col);
  void onPrimaryChanged(int index);
};
