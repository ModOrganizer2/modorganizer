#include "filterlist.h"
#include "ui_mainwindow.h"
#include "categories.h"
#include "categoriesdialog.h"
#include <utility.h>

using namespace MOBase;

FilterList::FilterList(Ui::MainWindow* ui, CategoryFactory& factory)
  : ui(ui), m_factory(factory)
{
  connect(
    ui->filters, &QTreeWidget::customContextMenuRequested,
    [&](auto&& pos){ onContextMenu(pos); });

  connect(
    ui->filters, &QTreeWidget::itemSelectionChanged,
    [&]{ onSelection(); });

  connect(
    ui->filtersClear, &QPushButton::clicked,
    [&]{ clearSelection(); });

  connect(
    ui->filtersAnd, &QCheckBox::toggled,
    [&]{ onCriteriaChanged(); });

  connect(
    ui->filtersOr, &QCheckBox::toggled,
    [&]{ onCriteriaChanged(); });

  connect(
    ui->filtersNot, &QCheckBox::toggled,
    [&]{ onCriteriaChanged(); });

  connect(
    ui->filtersSeparators, &QCheckBox::toggled,
    [&]{ onCriteriaChanged(); });
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
    ui->filters->addTopLevelItem(item);
  }
  return item;
}

void FilterList::addContentFilters()
{
  for (unsigned i = 0; i < ModInfo::NUM_CONTENT_TYPES; ++i) {
    addFilterItem(
      nullptr, tr("<Contains %1>").arg(ModInfo::getContentTypeName(i)),
      i, ModListSortProxy::TYPE_CONTENT);
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

void FilterList::addSpecialFilterItem(int type)
{
  addFilterItem(
    nullptr, m_factory.getSpecialCategoryName(type),
    type, ModListSortProxy::TYPE_SPECIAL);
}

void FilterList::refresh()
{
  QStringList selectedItems;
  for (QTreeWidgetItem *item : ui->filters->selectedItems()) {
    selectedItems.append(item->text(0));
  }

  ui->filters->clear();

  using F = CategoryFactory;
  addSpecialFilterItem(F::CATEGORY_SPECIAL_CHECKED);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_UNCHECKED);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_UPDATEAVAILABLE);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_BACKUP);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_MANAGED);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_UNMANAGED);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_NOCATEGORY);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_CONFLICT);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_NOTENDORSED);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_NONEXUSID);
  addSpecialFilterItem(F::CATEGORY_SPECIAL_NOGAMEDATA);

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
    QList<QTreeWidgetItem*> matches = ui->filters->findItems(
      item, Qt::MatchFixedString | Qt::MatchRecursive);

    if (matches.size() > 0) {
      matches.at(0)->setSelected(true);
    }
  }
}

void FilterList::setSelection(std::vector<int> categories)
{
  for (int i = 0; i < ui->filters->topLevelItemCount(); ++i) {
    if (ui->filters->topLevelItem(i)->data(0, Qt::UserRole) == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
      ui->filters->setCurrentItem(ui->filters->topLevelItem(i));
      break;
    }
  }
}

void FilterList::clearSelection()
{
  ui->filters->clearSelection();
}

void FilterList::onSelection()
{
  QModelIndexList indices = ui->filters->selectionModel()->selectedRows();
  std::vector<int> categories;
  std::vector<int> content;

  for (const QModelIndex &index : indices) {
    const int filterType = index.data(Qt::UserRole + 1).toInt();

    if ((filterType == ModListSortProxy::TYPE_CATEGORY) || (filterType == ModListSortProxy::TYPE_SPECIAL)) {
      const int categoryId = index.data(Qt::UserRole).toInt();
      if (categoryId != CategoryFactory::CATEGORY_NONE) {
        categories.push_back(categoryId);
      }
    } else if (filterType == ModListSortProxy::TYPE_CONTENT) {
      const int contentId = index.data(Qt::UserRole).toInt();
      content.push_back(contentId);
    }
  }

  ui->filtersClear->setEnabled(categories.size() > 0 || content.size() >0);

  emit filtersChanged(categories, content);
}

void FilterList::onContextMenu(const QPoint &pos)
{
  QMenu menu;
  menu.addAction(tr("Edit Categories..."), [&]{ editCategories(); });
  menu.addAction(tr("Deselect filter"), [&]{ clearSelection(); });

  menu.exec(ui->filters->viewport()->mapToGlobal(pos));
}

void FilterList::editCategories()
{
  CategoriesDialog dialog(qApp->activeWindow());

  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}

void FilterList::onCriteriaChanged()
{
  const auto mode = ui->filtersAnd->isChecked() ?
    ModListSortProxy::FILTER_AND : ModListSortProxy::FILTER_OR;

  const bool inverse = ui->filtersNot->isChecked();
  const bool separators = ui->filtersSeparators->isChecked();

  emit criteriaChanged(mode, inverse, separators);
}
