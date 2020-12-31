#include "modlistcontextmenu.h"

#include <report.h>

#include "modlist.h"
#include "modlistview.h"
#include "modlistviewactions.h"
#include "organizercore.h"

using namespace MOBase;

ModListGlobalContextMenu::ModListGlobalContextMenu(OrganizerCore& core, ModListView* view, QWidget* parent)
  : QMenu(parent)
{
  addAction(tr("Install Mod..."), [=]() { view->actions().installMod(); });
  addAction(tr("Create empty mod"), [=]() { view->actions().createEmptyMod(-1); });
  addAction(tr("Create Separator"), [=]() { view->actions().createSeparator(-1); });

  if (view->hasCollapsibleSeparators()) {
    addSeparator();
    addAction(tr("Collapse all"), view, &QTreeView::collapseAll);
    addAction(tr("Expand all"), view, &QTreeView::expandAll);
  }

  addSeparator();

  addAction(tr("Enable all visible"), [=]() {
    if (QMessageBox::question(view, tr("Confirm"), tr("Really enable all visible mods?"),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      view->enableAllVisible();
    }
    });
  addAction(tr("Disable all visible"), [=]() {
    if (QMessageBox::question(view, tr("Confirm"), tr("Really disable all visible mods?"),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      view->disableAllVisible();
    }
    });
  addAction(tr("Check for updates"), [=]() { view->actions().checkModsForUpdates(); });
  addAction(tr("Refresh"), &core, &OrganizerCore::profileRefresh);
  addAction(tr("Export to csv..."), [=]() { view->actions().exportModListCSV(); });
}


ModListChangeCategoryMenu::ModListChangeCategoryMenu(CategoryFactory& categories, ModInfo::Ptr mod, QMenu* parent)
  : QMenu(tr("Change Categories"), parent)
{
  populate(this, categories, mod);
}

std::vector<std::pair<int, bool>> ModListChangeCategoryMenu::categories() const
{
  return categories(this);
}

std::vector<std::pair<int, bool>> ModListChangeCategoryMenu::categories(const QMenu* menu) const
{
  std::vector<std::pair<int, bool>> cats;
  for (QAction* action : menu->actions()) {
    if (action->menu() != nullptr) {
      auto pcats = categories(action->menu());
      cats.insert(cats.end(), pcats.begin(), pcats.end());
    }
    else {
      QWidgetAction* widgetAction = qobject_cast<QWidgetAction*>(action);
      if (widgetAction != nullptr) {
        QCheckBox* checkbox = qobject_cast<QCheckBox*>(widgetAction->defaultWidget());
        cats.emplace_back(widgetAction->data().toInt(), checkbox->isChecked());
      }
    }
  }
  return cats;
}

bool ModListChangeCategoryMenu::populate(QMenu* menu, CategoryFactory& factory, ModInfo::Ptr mod, int targetId)
{
  const std::set<int>& categories = mod->getCategories();

  bool childEnabled = false;

  for (unsigned int i = 1; i < factory.numCategories(); ++i) {
    if (factory.getParentID(i) == targetId) {
      QMenu* targetMenu = menu;
      if (factory.hasChildren(i)) {
        targetMenu = menu->addMenu(factory.getCategoryName(i).replace('&', "&&"));
      }

      int id = factory.getCategoryID(i);
      QScopedPointer<QCheckBox> checkBox(new QCheckBox(targetMenu));
      bool enabled = categories.find(id) != categories.end();
      checkBox->setText(factory.getCategoryName(i).replace('&', "&&"));
      if (enabled) {
        childEnabled = true;
      }
      checkBox->setChecked(enabled ? Qt::Checked : Qt::Unchecked);

      QScopedPointer<QWidgetAction> checkableAction(new QWidgetAction(targetMenu));
      checkableAction->setDefaultWidget(checkBox.take());
      checkableAction->setData(id);
      targetMenu->addAction(checkableAction.take());

      if (factory.hasChildren(i)) {
        if (populate(targetMenu, factory, mod, factory.getCategoryID(i)) || enabled) {
          targetMenu->setIcon(QIcon(":/MO/gui/resources/check.png"));
        }
      }
    }
  }
  return childEnabled;
}

ModListPrimaryCategoryMenu::ModListPrimaryCategoryMenu(CategoryFactory& categories, ModInfo::Ptr mod, QMenu* parent)
  : QMenu(tr("Primary Category"), parent)
{
  connect(this, &QMenu::aboutToShow, [=]() { populate(categories, mod); });
}

void ModListPrimaryCategoryMenu::populate(const CategoryFactory& factory, ModInfo::Ptr mod)
{
  clear();
  const std::set<int>& categories = mod->getCategories();
  for (int categoryID : categories) {
    int catIdx = factory.getCategoryIndex(categoryID);
    QWidgetAction* action = new QWidgetAction(this);
    try {
      QRadioButton* categoryBox = new QRadioButton(
        factory.getCategoryName(catIdx).replace('&', "&&"),
        this);
      connect(categoryBox, &QRadioButton::toggled, [mod, categoryID](bool enable) {
        if (enable) {
          mod->setPrimaryCategory(categoryID);
        }
      });
      categoryBox->setChecked(categoryID == mod->primaryCategory());
      action->setDefaultWidget(categoryBox);
    }
    catch (const std::exception& e) {
      log::error("failed to create category checkbox: {}", e.what());
    }

    action->setData(categoryID);
    addAction(action);
  }
}

ModListContextMenu::ModListContextMenu(
  const QModelIndex& index, OrganizerCore& core, CategoryFactory& categories, ModListView* view) :
  QMenu(view)
  , m_core(core)
  , m_categories(categories)
  , m_index(index.model() == view->model() ? view->indexViewToModel(index) : index)
  , m_view(view)
  , m_actions(view->actions())
{
  if (view->selectionModel()->hasSelection()) {
    m_selected = view->indexViewToModel(view->selectionModel()->selectedRows());
  }
  else {
    m_selected = { index };
  }

  QMenu* allMods = new ModListGlobalContextMenu(core, view, view);
  allMods->setTitle(tr("All Mods"));
  addMenu(allMods);

  if (view->hasCollapsibleSeparators()) {
    addAction(tr("Collapse all"), view, &QTreeView::collapseAll);
    addAction(tr("Expand all"), view, &QTreeView::expandAll);
  }

  addSeparator();

  // Add type-specific items
  ModInfo::Ptr info = ModInfo::getByIndex(index.data(ModList::IndexRole).toInt());

  // TODO:
  // - Don't forget to check for the sort priority for "Send To... "

  if (info->isOverwrite()) {
    addOverwriteActions(info);
  }
  else if (info->isBackup()) {
    addBackupActions(info);
  }
  else if (info->isSeparator()) {
    addSeparatorActions(info);
  }
  else if (info->isForeign()) {
    addForeignActions(info);
  }
  else {
    addRegularActions(info);
  }

  // add information for all except foreign
  if (!info->isForeign()) {
    QAction* infoAction = addAction(tr("Information..."), [=]() { view->actions().displayModInformation(m_index.data(ModList::IndexRole).toInt()); });
    setDefaultAction(infoAction);
  }
}

void ModListContextMenu::addMenuAsPushButton(QMenu* menu)
{
  QPushButton* pushBtn = new QPushButton(menu->title());
  pushBtn->setMenu(menu);
  QWidgetAction* action = new QWidgetAction(this);
  action->setDefaultWidget(pushBtn);
  addAction(action);
}

QMenu* ModListContextMenu::createSendToContextMenu()
{
  QMenu* menu = new QMenu(m_view);
  menu->setTitle(tr("Send to... "));
  menu->addAction(tr("Top"), [=]() { m_actions.sendModsToTop(m_selected); });
  menu->addAction(tr("Bottom"), [=]() { m_actions.sendModsToBottom(m_selected); });
  menu->addAction(tr("Priority..."), [=]() { m_actions.sendModsToPriority(m_selected); });
  menu->addAction(tr("Separator..."), [=]() { m_actions.sendModsToSeparator(m_selected); });
  return menu;
}

void ModListContextMenu::addOverwriteActions(ModInfo::Ptr mod)
{
  if (QDir(mod->absolutePath()).count() > 2) {
    addAction(tr("Sync to Mods..."), [=]() { m_core.syncOverwrite(); });
    addAction(tr("Create Mod..."), [=]() { m_actions.createModFromOverwrite(); });
    addAction(tr("Move content to Mod..."), [=]() { m_actions.moveOverwriteContentToExistingMod(); });
    addAction(tr("Clear Overwrite..."), [=]() { m_actions.clearOverwrite(); });
  }
  addAction(tr("Open in Explorer"), [=]() { m_actions.openExplorer(m_selected); });
}

void ModListContextMenu::addSeparatorActions(ModInfo::Ptr mod)
{
  addSeparator();

  // categories
  ModListChangeCategoryMenu* categoriesMenu = new ModListChangeCategoryMenu(m_categories, mod, this);
  connect(categoriesMenu, &QMenu::aboutToHide, [=]() {
    m_actions.setCategories(m_selected, m_index, categoriesMenu->categories());
  });
  addMenuAsPushButton(categoriesMenu);

  ModListPrimaryCategoryMenu* primaryCategoryMenu = new ModListPrimaryCategoryMenu(m_categories, mod, this);
  addMenuAsPushButton(primaryCategoryMenu);
  addSeparator();


  addAction(tr("Rename Separator..."), [=]() { m_actions.renameMod(m_index); });
  addAction(tr("Remove Separator..."), [=]() { m_actions.removeMods(m_selected); });
  addSeparator();

  if (m_view->sortColumn() == ModList::COL_PRIORITY) {
    addMenu(createSendToContextMenu());
    addSeparator();
  }
  addAction(tr("Select Color..."), [=]() { m_actions.setColor(m_selected, m_index); });

  if (mod->color().isValid()) {
    addAction(tr("Reset Color"), [=]() { m_actions.resetColor(m_selected, m_index); });
  }

  addSeparator();
}

void ModListContextMenu::addForeignActions(ModInfo::Ptr mod)
{
  if (m_view->sortColumn() == ModList::COL_PRIORITY) {
    addMenu(createSendToContextMenu());
  }
}

void ModListContextMenu::addBackupActions(ModInfo::Ptr mod)
{
  auto flags = mod->getFlags();
  addAction(tr("Restore Backup"), [=]() { m_actions.restoreBackup(m_index); });
  addAction(tr("Remove Backup..."), [=]() { m_actions.removeMods(m_selected); });
  addSeparator();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
    addAction(tr("Ignore missing data"), [=]() { m_actions.ignoreMissingData(m_selected); });
  }
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_ALTERNATE_GAME) != flags.end()) {
    addAction(tr("Mark as converted/working"), [=]() { m_actions.markConverted(m_selected); });
  }
  addSeparator();
  if (mod->nexusId() > 0) {
    addAction(tr("Visit on Nexus"), [=]() { m_actions.visitOnNexus(m_selected); });
  }

  const auto url = mod->parseCustomURL();
  if (url.isValid()) {
    addAction(tr("Visit on %1").arg(url.host()), [=]() { m_actions.visitWebPage(m_selected); });
  }

  addAction(tr("Open in Explorer"), [=]() { m_actions.openExplorer(m_selected); });
}

void ModListContextMenu::addRegularActions(ModInfo::Ptr mod)
{

}
