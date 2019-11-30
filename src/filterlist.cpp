#include "filterlist.h"
#include "ui_mainwindow.h"
#include "categories.h"
#include "categoriesdialog.h"
#include <utility.h>

using namespace MOBase;

const int CategoryIDRole = Qt::UserRole;
const int CategoryTypeRole = Qt::UserRole + 1;

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
    ui->filtersSeparators, &QCheckBox::toggled,
    [&]{ onCriteriaChanged(); });

  ui->filters->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui->filters->header()->resizeSection(1, 50);
}

QTreeWidgetItem* FilterList::addCriteriaItem(
  QTreeWidgetItem *root, const QString &name, int categoryID,
  ModListSortProxy::CriteriaType type)
{
  QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(name));

  item->setData(0, Qt::ToolTipRole, name);
  item->setData(0, CategoryIDRole, categoryID);
  item->setData(0, CategoryTypeRole, type);

  if (root != nullptr) {
    root->addChild(item);
  } else {
    ui->filters->addTopLevelItem(item);
  }

  auto* w = new QWidget;
  w->setStyleSheet("background-color: rgba(0,0,0,0)");

  auto* ly = new QVBoxLayout(w);
  ly->setAlignment(Qt::AlignCenter);
  ly->setContentsMargins(0, 0, 0, 0);

  auto* cb = new QCheckBox;
  connect(cb, &QCheckBox::toggled, [&]{ onSelection(); });
  ly->addWidget(cb);

  ui->filters->setItemWidget(item, 1, w);

  return item;
}

void FilterList::addContentCriteria()
{
  for (unsigned i = 0; i < ModInfo::NUM_CONTENT_TYPES; ++i) {
    addCriteriaItem(
      nullptr, tr("<Contains %1>").arg(ModInfo::getContentTypeName(i)),
      i, ModListSortProxy::TYPE_CONTENT);
  }
}

void FilterList::addCategoryCriteria(QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID)
{
  const auto count = static_cast<unsigned int>(m_factory.numCategories());
  for (unsigned int i = 1; i < count; ++i) {
    if (m_factory.getParentID(i) == targetID) {
      int categoryID = m_factory.getCategoryID(i);
      if (categoriesUsed.find(categoryID) != categoriesUsed.end()) {
        QTreeWidgetItem *item =
          addCriteriaItem(root, m_factory.getCategoryName(i),
            categoryID, ModListSortProxy::TYPE_CATEGORY);
        if (m_factory.hasChildren(i)) {
          addCategoryCriteria(item, categoriesUsed, categoryID);
        }
      }
    }
  }
}

void FilterList::addSpecialCriteria(int type)
{
  addCriteriaItem(
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
  addSpecialCriteria(F::CATEGORY_SPECIAL_CHECKED);
  addSpecialCriteria(F::CATEGORY_SPECIAL_UNCHECKED);
  addSpecialCriteria(F::CATEGORY_SPECIAL_UPDATEAVAILABLE);
  addSpecialCriteria(F::CATEGORY_SPECIAL_BACKUP);
  addSpecialCriteria(F::CATEGORY_SPECIAL_MANAGED);
  addSpecialCriteria(F::CATEGORY_SPECIAL_UNMANAGED);
  addSpecialCriteria(F::CATEGORY_SPECIAL_NOCATEGORY);
  addSpecialCriteria(F::CATEGORY_SPECIAL_CONFLICT);
  addSpecialCriteria(F::CATEGORY_SPECIAL_NOTENDORSED);
  addSpecialCriteria(F::CATEGORY_SPECIAL_NONEXUSID);
  addSpecialCriteria(F::CATEGORY_SPECIAL_NOGAMEDATA);

  addContentCriteria();

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

  addCategoryCriteria(nullptr, categoriesUsed, 0);

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
    if (ui->filters->topLevelItem(i)->data(0, CategoryIDRole) == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
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
  const QModelIndexList indices = ui->filters->selectionModel()->selectedRows();
  std::vector<ModListSortProxy::Criteria> criteria;

  for (auto* item: ui->filters->selectedItems()) {
    const auto  type = static_cast<ModListSortProxy::CriteriaType>(
      item->data(0, CategoryTypeRole).toInt());

    const int id = item->data(0, CategoryIDRole).toInt();

    auto* cb = static_cast<const QCheckBox*>(ui->filters->itemWidget(item, 1));
    const bool inverse = cb->isChecked();

    criteria.push_back({type, id, inverse});
  }

  ui->filtersClear->setEnabled(!criteria.empty());

  emit criteriaChanged(criteria);
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

  const bool separators = ui->filtersSeparators->isChecked();

  emit optionsChanged(mode, separators);
}
