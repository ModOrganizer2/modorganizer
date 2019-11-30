#include "filterlist.h"
#include "ui_mainwindow.h"
#include "categories.h"
#include "categoriesdialog.h"
#include <utility.h>

using namespace MOBase;
using CriteriaType = ModListSortProxy::CriteriaType;
using Criteria = ModListSortProxy::Criteria;

class FilterList::CriteriaItem : public QTreeWidgetItem
{
public:
  CriteriaItem(FilterList* list, QString name, CriteriaType type, int id)
    : QTreeWidgetItem({name}), m_list(list), m_widget(nullptr), m_checkbox(nullptr)
  {
    setData(0, Qt::ToolTipRole, name);
    setData(0, TypeRole, type);
    setData(0, IDRole, id);

    m_widget = new QWidget;
    m_widget->setStyleSheet("background-color: rgba(0,0,0,0)");

    auto* ly = new QVBoxLayout(m_widget);
    ly->setAlignment(Qt::AlignCenter);
    ly->setContentsMargins(0, 0, 0, 0);

    m_checkbox = new QCheckBox;
    QObject::connect(m_checkbox, &QCheckBox::toggled, [&]{ m_list->onSelection(); });
    ly->addWidget(m_checkbox);
  }

  QWidget* widget()
  {
    return m_widget;
  }

  CriteriaType type() const
  {
    return static_cast<CriteriaType>(data(0, TypeRole).toInt());
  }

  int id() const
  {
    return data(0, IDRole).toInt();
  }

  bool inverse() const
  {
    return m_checkbox->isChecked();
  }

  void setInverted(bool b)
  {
    m_checkbox->setChecked(b);
  }

private:
  const int IDRole = Qt::UserRole;
  const int TypeRole = Qt::UserRole + 1;

  FilterList* m_list;
  QWidget* m_widget;
  QCheckBox* m_checkbox;
};


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
  CriteriaType type)
{
  auto* item = new CriteriaItem(this, name, type, categoryID);

  if (root != nullptr) {
    root->addChild(item);
  } else {
    ui->filters->addTopLevelItem(item);
  }

  ui->filters->setItemWidget(item, 1, item->widget());

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
    const auto* item = dynamic_cast<CriteriaItem*>(
      ui->filters->topLevelItem(i));

    if (!item) {
      continue;
    }

    if (item->id() == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
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
  std::vector<Criteria> criteria;

  for (auto* item : ui->filters->selectedItems()) {
    const auto* ci = dynamic_cast<CriteriaItem*>(item);
    if (!ci) {
      continue;
    }

    criteria.push_back({
      ci->type(), ci->id(), ci->inverse()
    });
  }

  ui->filtersClear->setEnabled(!criteria.empty());

  emit criteriaChanged(criteria);
}

void FilterList::onContextMenu(const QPoint &pos)
{
  QMenu menu;
  menu.addAction(tr("Deselect filters"), [&]{ clearSelection(); });
  menu.addAction(tr("Set inverted"), [&]{ toggleInverted(true); });
  menu.addAction(tr("Unset inverted"), [&]{ toggleInverted(false); });
  menu.addSeparator();
  menu.addAction(tr("Edit Categories..."), [&]{ editCategories(); });

  menu.exec(ui->filters->viewport()->mapToGlobal(pos));
}

void FilterList::editCategories()
{
  CategoriesDialog dialog(qApp->activeWindow());

  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}

void FilterList::toggleInverted(bool b)
{
  bool changed = false;

  for (auto* item : ui->filters->selectedItems()) {
    auto* ci = dynamic_cast<CriteriaItem*>(item);
    if (!ci) {
      continue;
    }

    if (ci->inverse() != b) {
      ci->setInverted(b);
      changed = true;
    }
  }

  if (changed) {
    onSelection();
  }
}

void FilterList::onCriteriaChanged()
{
  const auto mode = ui->filtersAnd->isChecked() ?
    ModListSortProxy::FILTER_AND : ModListSortProxy::FILTER_OR;

  const bool separators = ui->filtersSeparators->isChecked();

  emit optionsChanged(mode, separators);
}
