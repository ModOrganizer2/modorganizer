#include "modinfodialogcategories.h"
#include "categories.h"
#include "modinfo.h"
#include "ui_modinfodialog.h"

CategoriesTab::CategoriesTab(ModInfoDialogTabContext cx)
    : ModInfoDialogTab(std::move(cx))
{
  connect(ui->categories, &QTreeWidget::itemChanged, [&](auto* item, int col) {
    onCategoryChanged(item, col);
  });

  connect(ui->primaryCategories,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          [&](int index) {
            onPrimaryChanged(index);
          });
}

void CategoriesTab::clear()
{
  ui->categories->clear();
  ui->primaryCategories->clear();
  setHasData(false);
}

void CategoriesTab::update()
{
  clear();

  add(CategoryFactory::instance(), mod().getCategories(),
      ui->categories->invisibleRootItem(), 0);

  updatePrimary();
}

bool CategoriesTab::canHandleSeparators() const
{
  return true;
}

bool CategoriesTab::usesOriginFiles() const
{
  return false;
}

void CategoriesTab::add(const CategoryFactory* factory,
                        const std::set<int>& enabledCategories, QTreeWidgetItem* root,
                        int rootLevel)
{
  for (int i = 0; i < static_cast<int>(factory->numCategories()); ++i) {
    if (factory->getParentID(i) != rootLevel) {
      continue;
    }

    int categoryID = factory->getCategoryID(i);

    QTreeWidgetItem* newItem =
        new QTreeWidgetItem(QStringList(factory->getCategoryName(i)));

    newItem->setFlags(newItem->flags() | Qt::ItemIsUserCheckable);

    newItem->setCheckState(0,
                           enabledCategories.find(categoryID) != enabledCategories.end()
                               ? Qt::Checked
                               : Qt::Unchecked);

    newItem->setData(0, Qt::UserRole, categoryID);

    if (factory->hasChildren(i)) {
      add(factory, enabledCategories, newItem, categoryID);
    }

    root->addChild(newItem);
  }
}

void CategoriesTab::updatePrimary()
{
  ui->primaryCategories->clear();

  int primaryCategory = mod().primaryCategory();

  addChecked(ui->categories->invisibleRootItem());

  for (int i = 0; i < ui->primaryCategories->count(); ++i) {
    if (ui->primaryCategories->itemData(i).toInt() == primaryCategory) {
      ui->primaryCategories->setCurrentIndex(i);
      break;
    }
  }

  setHasData(ui->primaryCategories->count() > 0);
}

void CategoriesTab::addChecked(QTreeWidgetItem* tree)
{
  for (int i = 0; i < tree->childCount(); ++i) {
    QTreeWidgetItem* child = tree->child(i);
    if (child->checkState(0) == Qt::Checked) {
      ui->primaryCategories->addItem(child->text(0), child->data(0, Qt::UserRole));
      addChecked(child);
    }
  }
}

void CategoriesTab::save(QTreeWidgetItem* currentNode)
{
  for (int i = 0; i < currentNode->childCount(); ++i) {
    QTreeWidgetItem* childNode = currentNode->child(i);

    mod().setCategory(childNode->data(0, Qt::UserRole).toInt(),
                      childNode->checkState(0));

    save(childNode);
  }
}

void CategoriesTab::onCategoryChanged(QTreeWidgetItem* item, int)
{
  QTreeWidgetItem* parent = item->parent();

  while ((parent != nullptr) && ((parent->flags() & Qt::ItemIsUserCheckable) != 0) &&
         (parent->checkState(0) == Qt::Unchecked)) {
    parent->setCheckState(0, Qt::Checked);
    parent = parent->parent();
  }

  updatePrimary();
  save(ui->categories->invisibleRootItem());
}

void CategoriesTab::onPrimaryChanged(int index)
{
  if (index != -1) {
    mod().setPrimaryCategory(ui->primaryCategories->itemData(index).toInt());
  }
}
