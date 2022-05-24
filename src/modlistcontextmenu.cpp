#include "modlistcontextmenu.h"

#include <report.h>

#include "modlist.h"
#include "modlistview.h"
#include "modlistviewactions.h"
#include "organizercore.h"

using namespace MOBase;

ModListGlobalContextMenu::ModListGlobalContextMenu(OrganizerCore& core,
                                                   ModListView* view, QWidget* parent)
    : ModListGlobalContextMenu(core, view, QModelIndex(), parent)
{}

ModListGlobalContextMenu::ModListGlobalContextMenu(OrganizerCore& core,
                                                   ModListView* view,
                                                   const QModelIndex& index,
                                                   QWidget* parent)
    : QMenu(parent)
{
  connect(this, &QMenu::aboutToShow, [=, &core] {
    populate(core, view, index);
  });
}

void ModListGlobalContextMenu::populate(OrganizerCore& core, ModListView* view,
                                        const QModelIndex& index)
{
  clear();

  auto modIndex = index.data(ModList::IndexRole);
  if (modIndex.isValid() && view->sortColumn() == ModList::COL_PRIORITY) {
    auto info = ModInfo::getByIndex(modIndex.toInt());
    if (!info->isBackup()) {

      // the mod are not created/installed at the same position depending
      // on the clicked mod and the sort order
      QString installText = tr("Install mod above... ");
      QString createText  = tr("Create empty mod above");
      if (info->isSeparator()) {
        installText = tr("Install mod inside... ");
        createText  = tr("Create empty mod inside");
      } else if (view->sortOrder() == Qt::DescendingOrder) {
        installText = tr("Install mod below... ");
        createText  = tr("Create empty mod below");
      }

      addAction(installText, [=]() {
        view->actions().installMod("", index);
      });
      addAction(createText, [=]() {
        view->actions().createEmptyMod(index);
      });
      addAction(tr("Create separator above"), [=]() {
        view->actions().createSeparator(index);
      });
    }
  } else {
    addAction(tr("Install mod..."), [=]() {
      view->actions().installMod();
    });
    addAction(tr("Create empty mod"), [=]() {
      view->actions().createEmptyMod();
    });
    addAction(tr("Create separator"), [=]() {
      view->actions().createSeparator();
    });
  }

  if (view->hasCollapsibleSeparators()) {
    addSeparator();
    addAction(tr("Collapse all"), view, &QTreeView::collapseAll);
    addAction(tr("Expand all"), view, &QTreeView::expandAll);
  }

  addSeparator();

  QString enableTxt = tr("Enable all"), disableTxt = tr("Disable all");

  if (view->isFilterActive()) {
    enableTxt  = tr("Enable all matching mods");
    disableTxt = tr("Disable all matching mods");
  }

  addAction(enableTxt, [=] {
    view->actions().setAllMatchingModsEnabled(true);
  });
  addAction(disableTxt, [=] {
    view->actions().setAllMatchingModsEnabled(false);
  });

  addAction(tr("Check for updates"), [=]() {
    view->actions().checkModsForUpdates();
  });
  addAction(tr("Refresh"), &core, &OrganizerCore::profileRefresh);
  addAction(tr("Export to csv..."), [=]() {
    view->actions().exportModListCSV();
  });
}

ModListChangeCategoryMenu::ModListChangeCategoryMenu(CategoryFactory& categories,
                                                     ModInfo::Ptr mod, QMenu* parent)
    : QMenu(tr("Change Categories"), parent)
{
  populate(this, categories, mod);
}

std::vector<std::pair<int, bool>> ModListChangeCategoryMenu::categories() const
{
  return categories(this);
}

std::vector<std::pair<int, bool>>
ModListChangeCategoryMenu::categories(const QMenu* menu) const
{
  std::vector<std::pair<int, bool>> cats;
  for (QAction* action : menu->actions()) {
    if (action->menu() != nullptr) {
      auto pcats = categories(action->menu());
      cats.insert(cats.end(), pcats.begin(), pcats.end());
    } else {
      QWidgetAction* widgetAction = qobject_cast<QWidgetAction*>(action);
      if (widgetAction != nullptr) {
        QCheckBox* checkbox = qobject_cast<QCheckBox*>(widgetAction->defaultWidget());
        cats.emplace_back(widgetAction->data().toInt(), checkbox->isChecked());
      }
    }
  }
  return cats;
}

bool ModListChangeCategoryMenu::populate(QMenu* menu, CategoryFactory& factory,
                                         ModInfo::Ptr mod, int targetId)
{
  const std::set<int>& categories = mod->getCategories();

  bool childEnabled = false;

  for (unsigned int i = 1; i < factory.numCategories(); ++i) {
    if (factory.getParentID(i) == targetId) {
      QMenu* targetMenu = menu;
      if (factory.hasChildren(i)) {
        targetMenu = menu->addMenu(factory.getCategoryName(i).replace('&', "&&"));
      }

      int id              = factory.getCategoryID(i);
      QCheckBox* checkBox = new QCheckBox(targetMenu);
      bool enabled        = categories.find(id) != categories.end();
      checkBox->setText(factory.getCategoryName(i).replace('&', "&&"));
      if (enabled) {
        childEnabled = true;
      }
      checkBox->setChecked(enabled ? Qt::Checked : Qt::Unchecked);

      QWidgetAction* checkableAction = new QWidgetAction(targetMenu);
      checkableAction->setDefaultWidget(checkBox);
      checkableAction->setData(id);
      targetMenu->addAction(checkableAction);

      if (factory.hasChildren(i)) {
        if (populate(targetMenu, factory, mod, factory.getCategoryID(i)) || enabled) {
          targetMenu->setIcon(QIcon(":/MO/gui/resources/check.png"));
        }
      }
    }
  }
  return childEnabled;
}

ModListPrimaryCategoryMenu::ModListPrimaryCategoryMenu(CategoryFactory& categories,
                                                       ModInfo::Ptr mod, QMenu* parent)
    : QMenu(tr("Primary Category"), parent)
{
  connect(this, &QMenu::aboutToShow, [=]() {
    populate(categories, mod);
  });
}

void ModListPrimaryCategoryMenu::populate(const CategoryFactory& factory,
                                          ModInfo::Ptr mod)
{
  clear();
  const std::set<int>& categories = mod->getCategories();
  for (int categoryID : categories) {
    int catIdx            = factory.getCategoryIndex(categoryID);
    QWidgetAction* action = new QWidgetAction(this);
    try {
      QRadioButton* categoryBox =
          new QRadioButton(factory.getCategoryName(catIdx).replace('&', "&&"), this);
      categoryBox->setChecked(categoryID == mod->primaryCategory());
      action->setDefaultWidget(categoryBox);
      action->setData(categoryID);
    } catch (const std::exception& e) {
      log::error("failed to create category checkbox: {}", e.what());
    }

    action->setData(categoryID);
    addAction(action);
  }
}

int ModListPrimaryCategoryMenu::primaryCategory() const
{
  for (QAction* action : actions()) {
    QWidgetAction* widgetAction = qobject_cast<QWidgetAction*>(action);
    if (widgetAction) {
      QRadioButton* button = qobject_cast<QRadioButton*>(widgetAction->defaultWidget());
      if (button && button->isChecked()) {
        return widgetAction->data().toInt();
      }
    }
  }
  return -1;
}

ModListContextMenu::ModListContextMenu(const QModelIndex& index, OrganizerCore& core,
                                       CategoryFactory& categories, ModListView* view)
    : QMenu(view), m_core(core), m_categories(categories),
      m_index(index.model() == view->model() ? view->indexViewToModel(index) : index),
      m_view(view), m_actions(view->actions())
{
  if (view->selectionModel()->hasSelection()) {
    m_selected = view->indexViewToModel(view->selectionModel()->selectedRows());
  } else {
    m_selected = {index};
  }

  ModInfo::Ptr info = ModInfo::getByIndex(index.data(ModList::IndexRole).toInt());

  QMenu* allMods =
      new ModListGlobalContextMenu(core, view, m_index, view->topLevelWidget());
  allMods->setTitle(tr("All Mods"));
  addMenu(allMods);

  auto viewIndex = view->indexModelToView(m_index);
  if (view->model()->hasChildren(viewIndex)) {
    bool expanded = view->isExpanded(viewIndex);
    addSeparator();
    addAction(tr("Collapse all"), view, &QTreeView::collapseAll);
    addAction(tr("Collapse others"), [=]() {
      m_view->collapseAll();
      m_view->setExpanded(viewIndex, expanded);
    });
    addAction(tr("Expand all"), view, &QTreeView::expandAll);
  }

  addSeparator();

  // Add type-specific items
  if (info->isOverwrite()) {
    addOverwriteActions(info);
  } else if (info->isBackup()) {
    addBackupActions(info);
  } else if (info->isSeparator()) {
    addSeparatorActions(info);
  } else if (info->isForeign()) {
    addForeignActions(info);
  } else {
    addRegularActions(info);
  }

  // add information for all except foreign
  if (!info->isForeign()) {
    QAction* infoAction = addAction(tr("Information..."), [=]() {
      view->actions().displayModInformation(m_index.data(ModList::IndexRole).toInt());
    });
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

void ModListContextMenu::addSendToContextMenu()
{
  static const std::vector overwritten_flags{
      ModInfo::EConflictFlag::FLAG_CONFLICT_MIXED,
      ModInfo::EConflictFlag::FLAG_CONFLICT_OVERWRITTEN,
      ModInfo::EConflictFlag::FLAG_CONFLICT_REDUNDANT};

  static const std::vector overwrite_flags{
      ModInfo::EConflictFlag::FLAG_CONFLICT_MIXED,
      ModInfo::EConflictFlag::FLAG_CONFLICT_OVERWRITE};

  bool overwrite = false, overwritten = false;
  for (auto& idx : m_selected) {
    auto index = idx.data(ModList::IndexRole);
    if (index.isValid()) {
      auto info  = ModInfo::getByIndex(index.toInt());
      auto flags = info->getConflictFlags();
      if (std::find_first_of(flags.begin(), flags.end(), overwritten_flags.begin(),
                             overwritten_flags.end()) != flags.end()) {
        overwritten = true;
      }
      if (std::find_first_of(flags.begin(), flags.end(), overwrite_flags.begin(),
                             overwrite_flags.end()) != flags.end()) {
        overwrite = true;
      }
    }
  }

  QMenu* menu = new QMenu(m_view);
  menu->setTitle(tr("Send to... "));
  menu->addAction(tr("Lowest priority"), [this] {
    m_actions.sendModsToTop(m_selected);
  });
  menu->addAction(tr("Highest priority"), [this] {
    m_actions.sendModsToBottom(m_selected);
  });
  menu->addAction(tr("Priority..."), [this] {
    m_actions.sendModsToPriority(m_selected);
  });
  menu->addAction(tr("Separator..."), [this] {
    m_actions.sendModsToSeparator(m_selected);
  });
  if (overwrite) {
    menu->addAction(tr("First conflict"), [this] {
      m_actions.sendModsToFirstConflict(m_selected);
    });
  }
  if (overwritten) {
    menu->addAction(tr("Last conflict"), [this] {
      m_actions.sendModsToLastConflict(m_selected);
    });
  }
  addMenu(menu);
}

void ModListContextMenu::addCategoryContextMenus(ModInfo::Ptr mod)
{
  ModListChangeCategoryMenu* categoriesMenu =
      new ModListChangeCategoryMenu(m_categories, mod, this);
  connect(categoriesMenu, &QMenu::aboutToHide, [=]() {
    m_actions.setCategories(m_selected, m_index, categoriesMenu->categories());
  });
  addMenuAsPushButton(categoriesMenu);

  ModListPrimaryCategoryMenu* primaryCategoryMenu =
      new ModListPrimaryCategoryMenu(m_categories, mod, this);
  connect(primaryCategoryMenu, &QMenu::aboutToHide, [=]() {
    int category = primaryCategoryMenu->primaryCategory();
    if (category != -1) {
      m_actions.setPrimaryCategory(m_selected, category);
    }
  });
  addMenuAsPushButton(primaryCategoryMenu);
}

void ModListContextMenu::addOverwriteActions(ModInfo::Ptr mod)
{
  if (QDir(mod->absolutePath()).count() > 2) {
    addAction(tr("Sync to Mods..."), [=]() {
      m_core.syncOverwrite();
    });
    addAction(tr("Create Mod..."), [=]() {
      m_actions.createModFromOverwrite();
    });
    addAction(tr("Move content to Mod..."), [=]() {
      m_actions.moveOverwriteContentToExistingMod();
    });
    addAction(tr("Clear Overwrite..."), [=]() {
      m_actions.clearOverwrite();
    });
  }
  addAction(tr("Open in Explorer"), [=]() {
    m_actions.openExplorer(m_selected);
  });
}

void ModListContextMenu::addSeparatorActions(ModInfo::Ptr mod)
{
  addCategoryContextMenus(mod);
  addSeparator();

  addAction(tr("Rename Separator..."), [=]() {
    m_actions.renameMod(m_index);
  });
  addAction(tr("Remove Separator..."), [=]() {
    m_actions.removeMods(m_selected);
  });
  addSeparator();

  if (m_view->sortColumn() == ModList::COL_PRIORITY) {
    addSendToContextMenu();
    addSeparator();
  }
  addAction(tr("Select Color..."), [=]() {
    m_actions.setColor(m_selected, m_index);
  });

  if (mod->color().isValid()) {
    addAction(tr("Reset Color"), [=]() {
      m_actions.resetColor(m_selected);
    });
  }

  addSeparator();
}

void ModListContextMenu::addForeignActions(ModInfo::Ptr mod)
{
  if (m_view->sortColumn() == ModList::COL_PRIORITY) {
    addSendToContextMenu();
  }
}

void ModListContextMenu::addBackupActions(ModInfo::Ptr mod)
{
  auto flags = mod->getFlags();
  addAction(tr("Restore Backup"), [=]() {
    m_actions.restoreBackup(m_index);
  });
  addAction(tr("Remove Backup..."), [=]() {
    m_actions.removeMods(m_selected);
  });
  addSeparator();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
    addAction(tr("Ignore missing data"), [=]() {
      m_actions.ignoreMissingData(m_selected);
    });
  }
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_ALTERNATE_GAME) !=
      flags.end()) {
    addAction(tr("Mark as converted/working"), [=]() {
      m_actions.markConverted(m_selected);
    });
  }
  addSeparator();
  if (mod->nexusId() > 0) {
    addAction(tr("Visit on Nexus"), [=]() {
      m_actions.visitOnNexus(m_selected);
    });
  }

  const auto url = mod->parseCustomURL();
  if (url.isValid()) {
    addAction(tr("Visit on %1").arg(url.host()), [=]() {
      m_actions.visitWebPage(m_selected);
    });
  }

  addAction(tr("Open in Explorer"), [=]() {
    m_actions.openExplorer(m_selected);
  });
}

void ModListContextMenu::addRegularActions(ModInfo::Ptr mod)
{
  auto flags = mod->getFlags();

  addCategoryContextMenus(mod);
  addSeparator();

  if (mod->downgradeAvailable()) {
    addAction(tr("Change versioning scheme"), [=]() {
      m_actions.changeVersioningScheme(m_index);
    });
  }

  if (mod->nexusId() > 0)
    addAction(tr("Force-check updates"), [=]() {
      m_actions.checkModsForUpdates(m_selected);
    });
  if (mod->updateIgnored()) {
    addAction(tr("Un-ignore update"), [=]() {
      m_actions.setIgnoreUpdate(m_selected, false);
    });
  } else {
    if (mod->updateAvailable() || mod->downgradeAvailable()) {
      addAction(tr("Ignore update"), [=]() {
        m_actions.setIgnoreUpdate(m_selected, true);
      });
    }
  }
  addSeparator();

  addAction(tr("Enable selected"), [=]() {
    m_core.modList()->setActive(m_selected, true);
  });
  addAction(tr("Disable selected"), [=]() {
    m_core.modList()->setActive(m_selected, false);
  });

  addSeparator();

  if (m_view->sortColumn() == ModList::COL_PRIORITY) {
    addSendToContextMenu();
    addSeparator();
  }

  addAction(tr("Rename Mod..."), [=]() {
    m_actions.renameMod(m_index);
  });
  addAction(tr("Reinstall Mod"), [=]() {
    m_actions.reinstallMod(m_index);
  });
  addAction(tr("Remove Mod..."), [=]() {
    m_actions.removeMods(m_selected);
  });
  addAction(tr("Create Backup"), [=]() {
    m_actions.createBackup(m_index);
  });

  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_HIDDEN_FILES) !=
      flags.end()) {
    addAction(tr("Restore hidden files"), [=]() {
      m_actions.restoreHiddenFiles(m_selected);
    });
  }

  addSeparator();

  if (m_index.column() == ModList::COL_NOTES) {
    addAction(tr("Select Color..."), [=]() {
      m_actions.setColor(m_selected, m_index);
    });
    if (mod->color().isValid()) {
      addAction(tr("Reset Color"), [=]() {
        m_actions.resetColor(m_selected);
      });
    }
    addSeparator();
  }

  if (mod->nexusId() > 0 && Settings::instance().nexus().endorsementIntegration()) {
    switch (mod->endorsedState()) {
    case EndorsedState::ENDORSED_TRUE: {
      addAction(tr("Un-Endorse"), [=]() {
        m_actions.setEndorsed(m_selected, false);
      });
    } break;
    case EndorsedState::ENDORSED_FALSE: {
      addAction(tr("Endorse"), [=]() {
        m_actions.setEndorsed(m_selected, true);
      });
      addAction(tr("Won't endorse"), [=]() {
        m_actions.willNotEndorsed(m_selected);
      });
    } break;
    case EndorsedState::ENDORSED_NEVER: {
      addAction(tr("Endorse"), [=]() {
        m_actions.setEndorsed(m_selected, true);
      });
    } break;
    default: {
      QAction* action = new QAction(tr("Endorsement state unknown"), this);
      action->setEnabled(false);
      addAction(action);
    } break;
    }
  }

  if (mod->nexusId() > 0 && Settings::instance().nexus().trackedIntegration()) {
    switch (mod->trackedState()) {
    case TrackedState::TRACKED_FALSE: {
      addAction(tr("Start tracking"), [=]() {
        m_actions.setTracked(m_selected, true);
      });
    } break;
    case TrackedState::TRACKED_TRUE: {
      addAction(tr("Stop tracking"), [=]() {
        m_actions.setTracked(m_selected, false);
      });
    } break;
    default: {
      QAction* action = new QAction(tr("Tracked state unknown"), this);
      action->setEnabled(false);
      addAction(action);
    } break;
    }
  }

  addSeparator();

  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
    addAction(tr("Ignore missing data"), [=]() {
      m_actions.ignoreMissingData(m_selected);
    });
  }

  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_ALTERNATE_GAME) !=
      flags.end()) {
    addAction(tr("Mark as converted/working"), [=]() {
      m_actions.markConverted(m_selected);
    });
  }

  addSeparator();

  if (mod->nexusId() > 0) {
    addAction(tr("Visit on Nexus"), [=]() {
      m_actions.visitOnNexus(m_selected);
    });
  }

  const auto url = mod->parseCustomURL();
  if (url.isValid()) {
    addAction(tr("Visit on %1").arg(url.host()), [=]() {
      m_actions.visitWebPage(m_selected);
    });
  }

  addAction(tr("Open in Explorer"), [=]() {
    m_actions.openExplorer(m_selected);
  });
}
