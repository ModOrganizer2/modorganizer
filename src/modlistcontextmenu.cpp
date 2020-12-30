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

ModListContextMenu::ModListContextMenu(OrganizerCore& core, const QModelIndexList& index, ModListView* modListView) :
  QMenu(modListView)
  , m_core(core)
  , m_index(index)
{
  // TODO: Change this.
  QModelIndex contextIdx = index.at(0);
  int contextColumn = contextIdx.column();
  int modIndex = contextIdx.data(ModList::IndexRole).toInt();

  try {
    /*
    if (modIndex == -1) {
      // no selection
      QMenu menu(this);
      initModListContextMenu(&menu);
      menu.exec(modList->viewport()->mapToGlobal(pos));
    }
    else {
      QMenu menu(this);

      QMenu* allMods = new QMenu(&menu);
      initModListContextMenu(allMods);
      allMods->setTitle(tr("All Mods"));
      menu.addMenu(allMods);

      if (ui->modList->hasCollapsibleSeparators()) {
        menu.addAction(tr("Collapse all"), ui->modList, &QTreeView::collapseAll);
        menu.addAction(tr("Expand all"), ui->modList, &QTreeView::expandAll);
      }

      menu.addSeparator();

      ModInfo::Ptr info = ModInfo::getByIndex(modIndex);
      std::vector<ModInfo::EFlag> flags = info->getFlags();

      // context menu for overwrites
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
        if (QDir(info->absolutePath()).count() > 2) {
          menu.addAction(tr("Sync to Mods..."), [=]() { m_OrganizerCore.syncOverwrite(); });
          menu.addAction(tr("Create Mod..."), [=]() { createModFromOverwrite(); });
          menu.addAction(tr("Move content to Mod..."), [=]() { moveOverwriteContentToExistingMod(); });
          menu.addAction(tr("Clear Overwrite..."), [=]() { clearOverwrite(); });
        }
        menu.addAction(tr("Open in Explorer"), [=]() { openExplorer_clicked(modIndex); });
      }

      // context menu for mod backups
      else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end()) {
        menu.addAction(tr("Restore Backup"), [=]() { restoreBackup_clicked(modIndex); });
        menu.addAction(tr("Remove Backup..."), [=]() { removeMod_clicked(modIndex); });
        menu.addSeparator();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
          menu.addAction(tr("Ignore missing data"), [=]() { ignoreMissingData_clicked(modIndex); });
        }
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_ALTERNATE_GAME) != flags.end()) {
          menu.addAction(tr("Mark as converted/working"), [=]() { markConverted_clicked(modIndex); });
        }
        menu.addSeparator();
        if (info->nexusId() > 0) {
          menu.addAction(tr("Visit on Nexus"), [=]() { visitOnNexus_clicked(modIndex); });
        }

        const auto url = info->parseCustomURL();
        if (url.isValid()) {
          menu.addAction(tr("Visit on %1").arg(url.host()), [=]() { visitWebPage_clicked(modIndex); });
        }

        menu.addAction(tr("Open in Explorer"), [=]() { openExplorer_clicked(modIndex); });
      }

      // separator
      else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end()) {
        menu.addSeparator();
        QMenu* addRemoveCategoriesMenu = new QMenu(tr("Change Categories"), &menu);
        populateMenuCategories(modIndex, addRemoveCategoriesMenu, 0);
        connect(addRemoveCategoriesMenu, &QMenu::aboutToHide, [=]() { addRemoveCategories_MenuHandler(addRemoveCategoriesMenu, modIndex, contextIdx); });
        addMenuAsPushButton(&menu, addRemoveCategoriesMenu);
        QMenu* primaryCategoryMenu = new QMenu(tr("Primary Category"), &menu);
        connect(primaryCategoryMenu, &QMenu::aboutToShow, [=]() { setPrimaryCategoryCandidates(primaryCategoryMenu, info); });
        addMenuAsPushButton(&menu, primaryCategoryMenu);
        menu.addSeparator();
        menu.addAction(tr("Rename Separator..."), [=]() { renameMod_clicked(); });
        menu.addAction(tr("Remove Separator..."), [=]() { removeMod_clicked(modIndex); });
        menu.addSeparator();
        addModSendToContextMenu(&menu);
        menu.addAction(tr("Select Color..."), [=]() { setColor_clicked(modIndex); });

        if (info->color().isValid()) {
          menu.addAction(tr("Reset Color"), [=]() { resetColor_clicked(modIndex); });
        }

        menu.addSeparator();
      }

      // foregin
      else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) != flags.end()) {
        addModSendToContextMenu(&menu);
      }

      // regular
      else {
        QMenu* addRemoveCategoriesMenu = new QMenu(tr("Change Categories"), &menu);
        populateMenuCategories(modIndex, addRemoveCategoriesMenu, 0);
        connect(addRemoveCategoriesMenu, &QMenu::aboutToHide, [=]() { addRemoveCategories_MenuHandler(addRemoveCategoriesMenu, modIndex, contextIdx); });
        addMenuAsPushButton(&menu, addRemoveCategoriesMenu);

        QMenu* primaryCategoryMenu = new QMenu(tr("Primary Category"), &menu);
        connect(primaryCategoryMenu, &QMenu::aboutToShow, [=]() { setPrimaryCategoryCandidates(primaryCategoryMenu, info); });
        addMenuAsPushButton(&menu, primaryCategoryMenu);

        menu.addSeparator();

        if (info->downgradeAvailable()) {
          menu.addAction(tr("Change versioning scheme"), [=]() { changeVersioningScheme(modIndex); });
        }

        if (info->nexusId() > 0)
          menu.addAction(tr("Force-check updates"), [=]() { checkModUpdates_clicked(modIndex); });
        if (info->updateIgnored()) {
          menu.addAction(tr("Un-ignore update"), [=]() { unignoreUpdate(modIndex); });
        }
        else {
          if (info->updateAvailable() || info->downgradeAvailable()) {
            menu.addAction(tr("Ignore update"), [=]() { ignoreUpdate(modIndex); });
          }
        }
        menu.addSeparator();

        menu.addAction(tr("Enable selected"), [=]() { enableSelectedMods_clicked(); });
        menu.addAction(tr("Disable selected"), [=]() { disableSelectedMods_clicked(); });

        menu.addSeparator();

        addModSendToContextMenu(&menu);

        menu.addAction(tr("Rename Mod..."), [=]() { renameMod_clicked(); });
        menu.addAction(tr("Reinstall Mod"), [=]() { reinstallMod_clicked(modIndex); });
        menu.addAction(tr("Remove Mod..."), [=]() { removeMod_clicked(modIndex); });
        menu.addAction(tr("Create Backup"), [=]() { backupMod_clicked(modIndex); });

        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_HIDDEN_FILES) != flags.end()) {
          menu.addAction(tr("Restore hidden files"), [=]() { restoreHiddenFiles_clicked(modIndex); });
        }

        menu.addSeparator();

        if (contextColumn == ModList::COL_NOTES) {
          menu.addAction(tr("Select Color..."), [=]() { setColor_clicked(modIndex); });
          if (info->color().isValid()) {
            menu.addAction(tr("Reset Color"), [=]() { resetColor_clicked(modIndex); });
          }
          menu.addSeparator();
        }

        if (info->nexusId() > 0 && Settings::instance().nexus().endorsementIntegration()) {
          switch (info->endorsedState()) {
          case EndorsedState::ENDORSED_TRUE: {
            menu.addAction(tr("Un-Endorse"), [=]() { unendorse_clicked(); });
          } break;
          case EndorsedState::ENDORSED_FALSE: {
            menu.addAction(tr("Endorse"), [=]() { endorse_clicked(); });
            menu.addAction(tr("Won't endorse"), [=]() { dontendorse_clicked(modIndex); });
          } break;
          case EndorsedState::ENDORSED_NEVER: {
            menu.addAction(tr("Endorse"), [=]() { endorse_clicked(); });
          } break;
          default: {
            QAction* action = new QAction(tr("Endorsement state unknown"), &menu);
            action->setEnabled(false);
            menu.addAction(action);
          } break;
          }
        }

        if (info->nexusId() > 0 && Settings::instance().nexus().trackedIntegration()) {
          switch (info->trackedState()) {
          case TrackedState::TRACKED_FALSE: {
            menu.addAction(tr("Start tracking"), [=]() { track_clicked(); });
          } break;
          case TrackedState::TRACKED_TRUE: {
            menu.addAction(tr("Stop tracking"), [=]() { untrack_clicked(); });
          } break;
          default: {
            QAction* action = new QAction(tr("Tracked state unknown"), &menu);
            action->setEnabled(false);
            menu.addAction(action);
          } break;
          }
        }

        menu.addSeparator();

        std::vector<ModInfo::EFlag> flags = info->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
          menu.addAction(tr("Ignore missing data"), [=]() { ignoreMissingData_clicked(modIndex); });
        }

        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_ALTERNATE_GAME) != flags.end()) {
          menu.addAction(tr("Mark as converted/working"), [=]() { markConverted_clicked(modIndex); });
        }

        menu.addSeparator();

        if (info->nexusId() > 0) {
          menu.addAction(tr("Visit on Nexus"), [=]() { visitOnNexus_clicked(modIndex); });
        }

        const auto url = info->parseCustomURL();
        if (url.isValid()) {
          menu.addAction(tr("Visit on %1").arg(url.host()), [=]() { visitWebPage_clicked(modIndex); });
        }

        menu.addAction(tr("Open in Explorer"), [&, modIndex]() { openExplorer_clicked(modIndex); });
      }

      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) == flags.end()) {
        QAction* infoAction = menu.addAction(tr("Information..."), [=]() { information_clicked(modIndex); });
        menu.setDefaultAction(infoAction);
      }
    }
    */
  }
  catch (const std::exception& e) {
    reportError(tr("Exception: ").arg(e.what()));
  }
  catch (...) {
    reportError(tr("Unknown exception"));
  }
}
