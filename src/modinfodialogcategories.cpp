#include "modinfodialogcategories.h"
#include "ui_modinfodialog.h"
#include "categories.h"
#include "modinfo.h"

CategoriesTab::CategoriesTab(QWidget*, Ui::ModInfoDialog* ui)
  : ui(ui)
{
  connect(
    ui->categoriesTree, &QTreeWidget::itemChanged,
    [&](auto* item, int col){ onCategoryChanged(item, col); });

  connect(
    ui->primaryCategoryBox,
    static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
    [&](int index){ onPrimaryChanged(index); });
}

void CategoriesTab::setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin)
{
  m_mod = mod;
}

void CategoriesTab::clear()
{
  ui->categoriesTree->clear();
  ui->primaryCategoryBox->clear();
}

void CategoriesTab::update()
{
  clear();

  add(
    CategoryFactory::instance(), m_mod->getCategories(),
    ui->categoriesTree->invisibleRootItem(), 0);

  updatePrimary();
}

void CategoriesTab::add(
  const CategoryFactory &factory, const std::set<int>& enabledCategories,
  QTreeWidgetItem* root, int rootLevel)
{
  for (int i=0; i<static_cast<int>(factory.numCategories()); ++i) {
    if (factory.getParentID(i) != rootLevel) {
      continue;
    }

    int categoryID = factory.getCategoryID(i);

    QTreeWidgetItem *newItem
      = new QTreeWidgetItem(QStringList(factory.getCategoryName(i)));

    newItem->setFlags(newItem->flags() | Qt::ItemIsUserCheckable);

    newItem->setCheckState(0, enabledCategories.find(categoryID)
      != enabledCategories.end()
      ? Qt::Checked
      : Qt::Unchecked);

    newItem->setData(0, Qt::UserRole, categoryID);

    if (factory.hasChildren(i)) {
      add(factory, enabledCategories, newItem, categoryID);
    }

    root->addChild(newItem);
  }
}

void CategoriesTab::updatePrimary()
{
  ui->primaryCategoryBox->clear();

  int primaryCategory = m_mod->getPrimaryCategory();

  addChecked(ui->categoriesTree->invisibleRootItem());

  for (int i = 0; i < ui->primaryCategoryBox->count(); ++i) {
    if (ui->primaryCategoryBox->itemData(i).toInt() == primaryCategory) {
      ui->primaryCategoryBox->setCurrentIndex(i);
      break;
    }
  }
}

void CategoriesTab::addChecked(QTreeWidgetItem* tree)
{
  for (int i = 0; i < tree->childCount(); ++i) {
    QTreeWidgetItem *child = tree->child(i);
    if (child->checkState(0) == Qt::Checked) {
      ui->primaryCategoryBox->addItem(child->text(0), child->data(0, Qt::UserRole));
      addChecked(child);
    }
  }
}

void CategoriesTab::save(QTreeWidgetItem* currentNode)
{
  for (int i = 0; i < currentNode->childCount(); ++i) {
    QTreeWidgetItem *childNode = currentNode->child(i);

    m_mod->setCategory(
      childNode->data(0, Qt::UserRole).toInt(), childNode->checkState(0));

    save(childNode);
  }
}

void CategoriesTab::onCategoryChanged(QTreeWidgetItem* item, int)
{
  QTreeWidgetItem *parent = item->parent();

  while ((parent != nullptr) && ((parent->flags() & Qt::ItemIsUserCheckable) != 0) && (parent->checkState(0) == Qt::Unchecked)) {
    parent->setCheckState(0, Qt::Checked);
    parent = parent->parent();
  }

  updatePrimary();
  save(ui->categoriesTree->invisibleRootItem());
}

void CategoriesTab::onPrimaryChanged(int index)
{
  if (index != -1) {
    m_mod->setPrimaryCategory(ui->primaryCategoryBox->itemData(index).toInt());
  }
}
