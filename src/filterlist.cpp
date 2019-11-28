#include "filterlist.h"
#include "ui_mainwindow.h"
#include "categories.h"
#include "categoriesdialog.h"
#include <utility.h>

using namespace MOBase;

FilterList::FilterList(Ui::MainWindow* ui, CategoryFactory& factory)
  : ui(ui), m_factory(factory)
{
  QObject::connect(
    ui->categoriesList, &QTreeWidget::customContextMenuRequested,
    [&](auto&& pos){ onContextMenu(pos); });

  QObject::connect(
    ui->categoriesList, &QTreeWidget::itemSelectionChanged,
    [&]{ onSelection(); });
}

QTreeWidgetItem* FilterList::addFilterItem(
  QTreeWidgetItem *root, const QString &name, int categoryID,
  ModListSortProxy::FilterType type)
{
  QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(name));
  item->setData(0, Qt::ToolTipRole, name);
  item->setData(0, Qt::UserRole, categoryID);
  item->setData(0, Qt::UserRole + 1, type);
  if (root != nullptr) {
    root->addChild(item);
  } else {
    ui->categoriesList->addTopLevelItem(item);
  }
  return item;
}

void FilterList::addContentFilters()
{
  for (unsigned i = 0; i < ModInfo::NUM_CONTENT_TYPES; ++i) {
    addFilterItem(nullptr, tr("<Contains %1>").arg(ModInfo::getContentTypeName(i)), i, ModListSortProxy::TYPE_CONTENT);
  }
}

void FilterList::addCategoryFilters(QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID)
{
  for (unsigned int i = 1;
    i < static_cast<unsigned int>(m_factory.numCategories()); ++i) {
    if ((m_factory.getParentID(i) == targetID)) {
      int categoryID = m_factory.getCategoryID(i);
      if (categoriesUsed.find(categoryID) != categoriesUsed.end()) {
        QTreeWidgetItem *item =
          addFilterItem(root, m_factory.getCategoryName(i),
            categoryID, ModListSortProxy::TYPE_CATEGORY);
        if (m_factory.hasChildren(i)) {
          addCategoryFilters(item, categoriesUsed, categoryID);
        }
      }
    }
  }
}

void FilterList::refresh()
{
  QItemSelection currentSelection = ui->modList->selectionModel()->selection();

  QVariant currentIndexName = ui->modList->currentIndex().data();
  ui->modList->setCurrentIndex(QModelIndex());

  QStringList selectedItems;
  for (QTreeWidgetItem *item : ui->categoriesList->selectedItems()) {
    selectedItems.append(item->text(0));
  }

  ui->categoriesList->clear();
  addFilterItem(nullptr, tr("<Checked>"), CategoryFactory::CATEGORY_SPECIAL_CHECKED, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Unchecked>"), CategoryFactory::CATEGORY_SPECIAL_UNCHECKED, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Update>"), CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Mod Backup>"), CategoryFactory::CATEGORY_SPECIAL_BACKUP, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Managed by MO>"), CategoryFactory::CATEGORY_SPECIAL_MANAGED, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Managed outside MO>"), CategoryFactory::CATEGORY_SPECIAL_UNMANAGED, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<No category>"), CategoryFactory::CATEGORY_SPECIAL_NOCATEGORY, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Conflicted>"), CategoryFactory::CATEGORY_SPECIAL_CONFLICT, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Not Endorsed>"), CategoryFactory::CATEGORY_SPECIAL_NOTENDORSED, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<No Nexus ID>"), CategoryFactory::CATEGORY_SPECIAL_NONEXUSID, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<No valid game data>"), CategoryFactory::CATEGORY_SPECIAL_NOGAMEDATA, ModListSortProxy::TYPE_SPECIAL);

  addContentFilters();
  std::set<int> categoriesUsed;
  for (unsigned int modIdx = 0; modIdx < ModInfo::getNumMods(); ++modIdx) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIdx);
    for (int categoryID : modInfo->getCategories()) {
      int currentID = categoryID;
      std::set<int> cycleTest;
      // also add parents so they show up in the tree
      while (currentID != 0) {
        categoriesUsed.insert(currentID);
        if (!cycleTest.insert(currentID).second) {
          log::warn("cycle in categories: {}", SetJoin(cycleTest, ", "));
          break;
        }
        currentID = m_factory.getParentID(m_factory.getCategoryIndex(currentID));
      }
    }
  }

  addCategoryFilters(nullptr, categoriesUsed, 0);

  for (const QString &item : selectedItems) {
    QList<QTreeWidgetItem*> matches = ui->categoriesList->findItems(item, Qt::MatchFixedString | Qt::MatchRecursive);
    if (matches.size() > 0) {
      matches.at(0)->setSelected(true);
    }
  }
  ui->modList->selectionModel()->select(currentSelection, QItemSelectionModel::Select);
  QModelIndexList matchList;
  if (currentIndexName.isValid()) {
    matchList = ui->modList->model()->match(ui->modList->model()->index(0, 0), Qt::DisplayRole, currentIndexName);
  }

  if (matchList.size() > 0) {
    ui->modList->setCurrentIndex(matchList.at(0));
  }
}

void FilterList::setSelection(std::vector<int> categories)
{
  for (int i = 0; i < ui->categoriesList->topLevelItemCount(); ++i) {
    if (ui->categoriesList->topLevelItem(i)->data(0, Qt::UserRole) == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
      ui->categoriesList->setCurrentItem(ui->categoriesList->topLevelItem(i));
      break;
    }
  }
}

void FilterList::clearSelection()
{
  ui->categoriesList->clearSelection();
}

void FilterList::onSelection()
{
  QModelIndexList indices = ui->categoriesList->selectionModel()->selectedRows();
  std::vector<int> categories;
  std::vector<int> content;
  for (const QModelIndex &index : indices) {
    int filterType = index.data(Qt::UserRole + 1).toInt();
    if ((filterType == ModListSortProxy::TYPE_CATEGORY)
      || (filterType == ModListSortProxy::TYPE_SPECIAL)) {
      int categoryId = index.data(Qt::UserRole).toInt();
      if (categoryId != CategoryFactory::CATEGORY_NONE) {
        categories.push_back(categoryId);
      }
    } else if (filterType == ModListSortProxy::TYPE_CONTENT) {
      int contentId = index.data(Qt::UserRole).toInt();
      content.push_back(contentId);
    }
  }

  emit changed(categories, content);

  ui->clickBlankButton->setEnabled(categories.size() > 0 || content.size() >0);

  if (indices.count() == 0) {
    ui->currentCategoryLabel->setText(QString("(%1)").arg(tr("<All>")));
  } else if (indices.count() > 1) {
    ui->currentCategoryLabel->setText(QString("(%1)").arg(tr("<Multiple>")));
  } else {
    ui->currentCategoryLabel->setText(QString("(%1)").arg(indices.first().data().toString()));
  }
  ui->modList->reset();
}

void FilterList::onContextMenu(const QPoint &pos)
{
  QMenu menu;
  menu.addAction(tr("Edit Categories..."), [&]{ editCategories(); });
  menu.addAction(tr("Deselect filter"), [&]{ clearSelection(); });

  menu.exec(ui->categoriesList->viewport()->mapToGlobal(pos));
}

void FilterList::editCategories()
{
  CategoriesDialog dialog(qApp->activeWindow());

  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}
