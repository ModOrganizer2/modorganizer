#include "savestab.h"
#include "activatemodsdialog.h"
#include "organizercore.h"
#include "ui_mainwindow.h"
#include <iplugingame.h>
#include <localsavegames.h>
#include <isavegameinfowidget.h>

using namespace MOBase;

SavesTab::SavesTab(QWidget* window, OrganizerCore& core, Ui::MainWindow* mwui)
    : m_window(window), m_core(core), m_CurrentSaveView(nullptr),
      ui{mwui->tabWidget, mwui->savesTab, mwui->savegameList}
{
  m_SavesWatcherTimer.setSingleShot(true);
  m_SavesWatcherTimer.setInterval(500);

  ui.list->installEventFilter(this);
  ui.list->setMouseTracking(true);

  connect(&m_SavesWatcher, &QFileSystemWatcher::directoryChanged, [&] {
    m_SavesWatcherTimer.start();
  });

  connect(&m_SavesWatcherTimer, &QTimer::timeout, [&] {
    refreshSavesIfOpen();
  });

  connect(ui.list, &QWidget::customContextMenuRequested, [&](auto pos) {
    onContextMenu(pos);
  });

  connect(ui.list, &QTreeWidget::itemEntered, [&](auto* item) {
    saveSelectionChanged(item);
  });
}

bool SavesTab::eventFilter(QObject* object, QEvent* e)
{
  if (object == ui.list) {
    if (e->type() == QEvent::Leave || e->type() == QEvent::WindowDeactivate) {
      hideSaveGameInfo();
    } else if (e->type() == QEvent::KeyPress) {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
      if (keyEvent->key() == Qt::Key_Delete) {
        deleteSavegame();
      }
    }
  }

  return false;
}

void SavesTab::displaySaveGameInfo(QTreeWidgetItem* newItem)
{
  // don't display the widget if the main window doesn't have focus
  //
  // this goes against the standard behaviour for tooltips, which are displayed
  // on hover regardless of focus, but this widget is so large and busy that
  // it's probably better this way
  if (!m_window->isActiveWindow()) {
    return;
  }

  if (m_CurrentSaveView == nullptr) {
    auto info = m_core.gameFeatures().gameFeature<SaveGameInfo>();

    if (info != nullptr) {
      m_CurrentSaveView = info->getSaveGameWidget(m_window);
    }

    if (m_CurrentSaveView == nullptr) {
      return;
    }
  }

  m_CurrentSaveView->setSave(*m_SaveGames[ui.list->indexOfTopLevelItem(newItem)]);

  QWindow* window = m_CurrentSaveView->window()->windowHandle();
  QRect screenRect;
  if (window == nullptr)
    screenRect = QGuiApplication::primaryScreen()->geometry();
  else
    screenRect = window->screen()->geometry();

  QPoint pos = QCursor::pos();
  if (pos.x() + m_CurrentSaveView->width() > screenRect.right()) {
    pos.rx() -= (m_CurrentSaveView->width() + 2);
  } else {
    pos.rx() += 5;
  }

  if (pos.y() + m_CurrentSaveView->height() > screenRect.bottom()) {
    pos.ry() -= (m_CurrentSaveView->height() + 10);
  } else {
    pos.ry() += 20;
  }
  m_CurrentSaveView->move(pos);

  m_CurrentSaveView->show();
  m_CurrentSaveView->setProperty("displayItem",
                                 QVariant::fromValue(static_cast<void*>(newItem)));
}

void SavesTab::saveSelectionChanged(QTreeWidgetItem* newItem)
{
  if (newItem == nullptr) {
    hideSaveGameInfo();
  } else if (m_CurrentSaveView == nullptr ||
             newItem != m_CurrentSaveView->property("displayItem").value<void*>()) {
    displaySaveGameInfo(newItem);
  }
}

void SavesTab::hideSaveGameInfo()
{
  if (m_CurrentSaveView != nullptr) {
    m_CurrentSaveView->deleteLater();
    m_CurrentSaveView = nullptr;
  }
}

void SavesTab::refreshSavesIfOpen()
{
  if (ui.mainTabs->currentWidget() == ui.tab) {
    refreshSaveList();
  }
}

QDir SavesTab::currentSavesDir() const
{
  QDir savesDir;
  if (m_core.currentProfile()->localSavesEnabled()) {
    savesDir.setPath(m_core.currentProfile()->savePath());
  } else {
    auto iniFiles = m_core.managedGame()->iniFiles();

    if (iniFiles.isEmpty()) {
      return m_core.managedGame()->savesDirectory();
    }

    QString iniPath = m_core.currentProfile()->absoluteIniFilePath(iniFiles[0]);

    wchar_t path[MAX_PATH];
    if (::GetPrivateProfileStringW(L"General", L"SLocalSavePath", L"", path, MAX_PATH,
                                   iniPath.toStdWString().c_str())) {
      savesDir.setPath(m_core.managedGame()->documentsDirectory().absoluteFilePath(
          QString::fromWCharArray(path)));
    } else {
      savesDir = m_core.managedGame()->savesDirectory();
    }
  }

  return savesDir;
}

void SavesTab::startMonitorSaves()
{
  stopMonitorSaves();

  QDir savesDir = currentSavesDir();

  m_SavesWatcher.addPath(savesDir.absolutePath());
}

void SavesTab::stopMonitorSaves()
{
  if (m_SavesWatcher.directories().length() > 0) {
    m_SavesWatcher.removePaths(m_SavesWatcher.directories());
  }
}

void SavesTab::refreshSaveList()
{
  TimeThis tt("MainWindow::refreshSaveList()");

  startMonitorSaves();  // re-starts monitoring

  try {
    QDir savesDir = currentSavesDir();
    MOBase::log::debug("reading save games from {}", savesDir.absolutePath());
    m_SaveGames = m_core.managedGame()->listSaves(savesDir);
    std::sort(m_SaveGames.begin(), m_SaveGames.end(),
              [](auto const& lhs, auto const& rhs) {
                return lhs->getCreationTime() > rhs->getCreationTime();
              });

    ui.list->clear();
    for (auto& save : m_SaveGames) {
      auto relpath = savesDir.relativeFilePath(save->getFilepath());
      auto display = save->getName();
      ui.list->addTopLevelItem(new QTreeWidgetItem(ui.list, {display, relpath}));
    }
  } catch (std::exception& e) {
    // listSaves() can throw
    log::error("{}", e.what());
  }
}

void SavesTab::deleteSavegame()
{
  auto info = m_core.gameFeatures().gameFeature<SaveGameInfo>();

  QString savesMsgLabel;
  QStringList deleteFiles;

  int count = 0;

  for (const QModelIndex& idx : ui.list->selectionModel()->selectedRows()) {

    auto& saveGame = m_SaveGames[idx.row()];

    if (count < 10) {
      savesMsgLabel +=
          "<li>" + QFileInfo(saveGame->getFilepath()).completeBaseName() + "</li>";
    }
    ++count;

    deleteFiles += saveGame->allFiles();
  }

  if (count > 10) {
    savesMsgLabel += "<li><i>... " + tr("%1 more").arg(count - 10) + "</i></li>";
  }

  if (QMessageBox::question(
          m_window, tr("Confirm"),
          tr("Are you sure you want to remove the following %n save(s)?<br>"
             "<ul>%1</ul><br>"
             "Removed saves will be sent to the Recycle Bin.",
             "", count)
              .arg(savesMsgLabel),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    shellDelete(deleteFiles, true);  // recycle bin delete.
    refreshSaveList();
  }
}

void SavesTab::onContextMenu(const QPoint& pos)
{
  QItemSelectionModel* selection = ui.list->selectionModel();

  if (!selection->hasSelection()) {
    return;
  }

  QMenu menu;

  auto info = m_core.gameFeatures().gameFeature<SaveGameInfo>();
  if (info != nullptr) {
    QAction* action = menu.addAction(tr("Fix enabled mods..."));
    action->setEnabled(false);
    if (selection->selectedRows().count() == 1) {
      auto& save = m_SaveGames[selection->selectedRows()[0].row()];
      SaveGameInfo::MissingAssets missing = info->getMissingAssets(*save);
      if (missing.size() != 0) {
        connect(action, &QAction::triggered, this, [this, missing] {
          fixMods(missing);
        });
        action->setEnabled(true);
      }
    }
  }

  QString deleteMenuLabel =
      tr("Delete %n save(s)", "", selection->selectedRows().count());
  menu.addAction(deleteMenuLabel, [&] {
    deleteSavegame();
  });

  menu.addAction(tr("Open in Explorer..."), [&] {
    openInExplorer();
  });

  menu.exec(ui.list->viewport()->mapToGlobal(pos));
}

void SavesTab::fixMods(SaveGameInfo::MissingAssets const& missingAssets)
{
  ActivateModsDialog dialog(missingAssets, m_window);
  if (dialog.exec() == QDialog::Accepted) {
    // activate the required mods, then enable all esps
    std::set<QString> modsToActivate = dialog.getModsToActivate();
    for (std::set<QString>::iterator iter = modsToActivate.begin();
         iter != modsToActivate.end(); ++iter) {
      if ((*iter != "<data>") && (*iter != "<overwrite>")) {
        unsigned int modIndex = ModInfo::getIndex(*iter);
        m_core.currentProfile()->setModEnabled(modIndex, true);
      }
    }

    m_core.currentProfile()->writeModlist();
    m_core.refreshLists();

    std::set<QString> espsToActivate = dialog.getESPsToActivate();
    for (std::set<QString>::iterator iter = espsToActivate.begin();
         iter != espsToActivate.end(); ++iter) {
      m_core.pluginList()->enableESP(*iter);
    }

    m_core.saveCurrentLists();
  }
}

void SavesTab::openInExplorer()
{
  auto info = m_core.gameFeatures().gameFeature<SaveGameInfo>();

  const auto sel = ui.list->selectionModel()->selectedRows();
  if (sel.empty()) {
    return;
  }

  auto& saveGame = m_SaveGames[sel[0].row()];
  shell::Explore(saveGame->getFilepath());
}
