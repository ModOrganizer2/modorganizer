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
  enum States : int
  {
    FirstState = 0,

    Inactive = FirstState,
    Active,
    Inverted,

    LastState = Inverted
  };

  CriteriaItem(FilterList* list, QString name, CriteriaType type, int id)
    : QTreeWidgetItem({"", name}), m_list(list), m_state(Inactive)
  {
    setData(0, Qt::ToolTipRole, name);
    setData(0, TypeRole, type);
    setData(0, IDRole, id);
  }

  CriteriaType type() const
  {
    return static_cast<CriteriaType>(data(0, TypeRole).toInt());
  }

  int id() const
  {
    return data(0, IDRole).toInt();
  }

  States state() const
  {
    return m_state;
  }

  void setState(States s)
  {
    if (m_state != s) {
      m_state = s;
      updateState();
    }
  }

  void nextState()
  {
    m_state = static_cast<States>(m_state + 1);
    if (m_state > LastState) {
      m_state = FirstState;
    }

    updateState();
  }

  void previousState()
  {
    m_state = static_cast<States>(m_state - 1);
    if (m_state < FirstState) {
      m_state = LastState;
    }

    updateState();
  }

private:
  const int IDRole = Qt::UserRole;
  const int TypeRole = Qt::UserRole + 1;

  FilterList* m_list;
  States m_state;

  void updateState()
  {
    QString s;

    switch (m_state)
    {
      case Inactive:
      {
        break;
      }

      case Active:
      {
        // U+2713 CHECK MARK
        s = QString::fromUtf8("\xe2\x9c\x93");
        break;
      }

      case Inverted:
      {
        s = tr("Not");
        break;
      }
    }

    setText(0, s);
  }
};


class ClickFilter : public QObject
{
public:
  ClickFilter(std::function<bool (QMouseEvent*)> f)
    : m_f(std::move(f))
  {
  }

  bool eventFilter(QObject* o, QEvent* e) override
  {
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonDblClick) {
      if (m_f) {
        return m_f(static_cast<QMouseEvent*>(e));
      }
    }

    return QObject::eventFilter(o, e);;
  }

private:
  std::function<bool (QMouseEvent*)> m_f;
};


FilterList::FilterList(Ui::MainWindow* ui, CategoryFactory& factory)
  : ui(ui), m_factory(factory)
{
  ui->filters->viewport()->installEventFilter(
    new ClickFilter([&](auto* e){ return onClick(e); }));

  connect(
    ui->filtersClear, &QPushButton::clicked,
    [&]{ clearSelection(); });

  connect(
    ui->filtersEdit, &QPushButton::clicked,
    [&]{ editCategories(); });

  connect(
    ui->filtersAnd, &QCheckBox::toggled,
    [&]{ onOptionsChanged(); });

  connect(
    ui->filtersOr, &QCheckBox::toggled,
    [&]{ onOptionsChanged(); });

  connect(
    ui->filtersSeparators, &QCheckBox::toggled,
    [&]{ onOptionsChanged(); });

  ui->filters->header()->setMinimumSectionSize(0);
  ui->filters->header()->setSectionResizeMode(0, QHeaderView::Fixed);
  ui->filters->header()->resizeSection(0, 30);
  ui->categoriesSplitter->setCollapsible(0, false);
  ui->categoriesSplitter->setCollapsible(1, false);
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

  item->setTextAlignment(0, Qt::AlignCenter);

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
  const auto sc = static_cast<CategoryFactory::SpecialCategories>(type);

  addCriteriaItem(
    nullptr, m_factory.getSpecialCategoryName(sc),
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
  addSpecialCriteria(F::Checked);
  addSpecialCriteria(F::UpdateAvailable);
  addSpecialCriteria(F::Backup);
  addSpecialCriteria(F::Managed);
  addSpecialCriteria(F::HasNoCategory);
  addSpecialCriteria(F::Conflict);
  addSpecialCriteria(F::NotEndorsed);
  addSpecialCriteria(F::NoNexusID);
  addSpecialCriteria(F::NoGameData);

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

void FilterList::setSelection(const std::vector<Criteria>& criteria)
{
  for (int i = 0; i < ui->filters->topLevelItemCount(); ++i) {
    const auto* item = dynamic_cast<CriteriaItem*>(
      ui->filters->topLevelItem(i));

    if (!item) {
      continue;
    }

    for (auto&& c : criteria) {
      if (item->type() == c.type && item->id() == c.id) {
        ui->filters->setCurrentItem(ui->filters->topLevelItem(i));
        break;
      }
    }
  }
}

void FilterList::clearSelection()
{
  for (int i=0; i<ui->filters->topLevelItemCount(); ++i) {
    auto* ci = dynamic_cast<CriteriaItem*>(ui->filters->topLevelItem(i));
    if (!ci) {
      continue;
    }

    ci->setState(CriteriaItem::Inactive);
  }

  checkCriteria();
}

bool FilterList::onClick(QMouseEvent* e)
{
  auto* item = ui->filters->itemAt(e->pos());
  if (!item) {
    return false;
  }

  auto* ci = dynamic_cast<CriteriaItem*>(item);
  if (!ci) {
    return false;
  }

  if (e->button() == Qt::LeftButton) {
    ci->nextState();
  } else if (e->button() == Qt::RightButton) {
    ci->previousState();
  } else {
    return false;
  }

  checkCriteria();
  return true;
}

void FilterList::checkCriteria()
{
  std::vector<Criteria> criteria;

  for (int i=0; i<ui->filters->topLevelItemCount(); ++i) {
    const auto* ci = dynamic_cast<CriteriaItem*>(ui->filters->topLevelItem(i));
    if (!ci) {
      continue;
    }

    if (ci->state() != CriteriaItem::Inactive) {
      criteria.push_back({
        ci->type(), ci->id(), (ci->state() == CriteriaItem::Inverted)
      });
    }
  }

  emit criteriaChanged(criteria);
}

void FilterList::editCategories()
{
  CategoriesDialog dialog(qApp->activeWindow());

  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}

void FilterList::onOptionsChanged()
{
  const auto mode = ui->filtersAnd->isChecked() ?
    ModListSortProxy::FILTER_AND : ModListSortProxy::FILTER_OR;

  const bool separators = ui->filtersSeparators->isChecked();

  emit optionsChanged(mode, separators);
}
