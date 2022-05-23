/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "aboutdialog.h"
#include "browserdialog.h"
#include "categories.h"
#include "categoriesdialog.h"
#include "datatab.h"
#include "downloadlist.h"
#include "downloadstab.h"
#include "editexecutablesdialog.h"
#include "envshortcut.h"
#include "eventfilter.h"
#include "executableinfo.h"
#include "executableslist.h"
#include "filedialogmemory.h"
#include "filterlist.h"
#include "guessedvalue.h"
#include "imodinterface.h"
#include "installationmanager.h"
#include "instancemanager.h"
#include "instancemanagerdialog.h"
#include "iplugindiagnose.h"
#include "iplugingame.h"
#include "isavegame.h"
#include "isavegameinfowidget.h"
#include "listdialog.h"
#include "localsavegames.h"
#include "messagedialog.h"
#include "modlist.h"
#include "modlistcontextmenu.h"
#include "modlistviewactions.h"
#include "motddialog.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "organizercore.h"
#include "overwriteinfodialog.h"
#include "pluginlist.h"
#include "previewdialog.h"
#include "previewgenerator.h"
#include "problemsdialog.h"
#include "profile.h"
#include "profilesdialog.h"
#include "savestab.h"
#include "selectiondialog.h"
#include "serverinfo.h"
#include "settingsdialog.h"
#include "shared/appconfig.h"
#include "spawn.h"
#include "statusbar.h"
#include <bsainvalidation.h>
#include <dataarchives.h>
#include <safewritefile.h>
#include <scopeguard.h>
#include <taskprogressmanager.h>
#include <uibase/game_features/savegameinfo.h>
#include <uibase/report.h>
#include <uibase/tutorialmanager.h>
#include <uibase/utility.h>
#include <uibase/versioninfo.h>
#include <usvfs/usvfs.h>

#include "directoryrefresher.h"
#include "shared/directoryentry.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"

#include <QAbstractItemDelegate>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QColorDialog>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFIleIconProvider>
#include <QFileDialog>
#include <QFont>
#include <QFuture>
#include <QHash>
#include <QIODevice>
#include <QIcon>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValueRef>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QModelIndex>
#include <QNetworkProxyFactory>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRect>
#include <QResizeEvent>
#include <QScopedPointer>
#include <QShortcut>
#include <QSize>
#include <QSizePolicy>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVariantList>
#include <QVersionNumber>
#include <QWebEngineProfile>
#include <QWhatsThis>
#include <QWidgetAction>

#include <QDebug>
#include <QtGlobal>

#ifndef Q_MOC_RUN
#include <boost/algorithm/string.hpp>
#include <boost/assign.hpp>
#include <boost/bind/bind.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/thread.hpp>
#endif

#include <shlobj.h>

#include <exception>
#include <functional>
#include <limits.h>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "gameplugins.h"

#ifdef TEST_MODELS
#include "modeltest.h"
#endif  // TEST_MODELS

#pragma warning(disable : 4428)

using namespace MOBase;
using namespace MOShared;

const QSize SmallToolbarSize(24, 24);
const QSize MediumToolbarSize(32, 32);
const QSize LargeToolbarSize(42, 36);

QString UnmanagedModName()
{
  return QObject::tr("<Unmanaged>");
}

bool runLoot(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList);

void setFilterShortcuts(QWidget* widget, QLineEdit* edit)
{
  auto activate = [=] {
    edit->setFocus();
    edit->selectAll();
  };

  auto reset = [=] {
    edit->clear();
    widget->setFocus();
  };

  auto hookActivate = [activate](auto* w) {
    auto* s = new QShortcut(QKeySequence::Find, w);
    s->setAutoRepeat(false);
    s->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(s, &QShortcut::activated, activate);
  };

  auto hookReset = [reset](auto* w) {
    auto* s = new QShortcut(QKeySequence(Qt::Key_Escape), w);
    s->setAutoRepeat(false);
    s->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(s, &QShortcut::activated, reset);
  };

  hookActivate(widget);
  hookReset(widget);

  hookActivate(edit);
  hookReset(edit);
}

MainWindow::MainWindow(Settings& settings, OrganizerCore& organizerCore,
                       PluginContainer& pluginContainer, ThemeManager& themeManager,
                       TranslationManager& translationManager, QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_WasVisible(false),
      m_FirstPaint(true), m_linksSeparator(nullptr), m_Tutorial(this, "MainWindow"),
      m_OldProfileIndex(-1), m_OldExecutableIndex(-1),
      m_CategoryFactory(CategoryFactory::instance()), m_OrganizerCore(organizerCore),
      m_PluginContainer(pluginContainer), m_ThemeManager(themeManager),
      m_TranslationManager(translationManager),
      m_ArchiveListWriter(std::bind(&MainWindow::saveArchiveList, this)),
      m_LinkToolbar(nullptr), m_LinkDesktop(nullptr), m_LinkStartMenu(nullptr),
      m_NumberOfProblems(0), m_ProblemsCheckRequired(false)
{
  // disables incredibly slow menu fade in effect that looks and feels like crap.
  // this was only happening to users with the windows
  // "Fade or slide menus into view" effect enabled.
  // maybe in the future the effects will be better at this moment they aren't.
  QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
  QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
  QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
  QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
  QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);

  QWebEngineProfile::defaultProfile()->setPersistentCookiesPolicy(
      QWebEngineProfile::NoPersistentCookies);
  QWebEngineProfile::defaultProfile()->setHttpCacheMaximumSize(52428800);
  QWebEngineProfile::defaultProfile()->setCachePath(settings.paths().cache());
  QWebEngineProfile::defaultProfile()->setPersistentStoragePath(
      settings.paths().cache());

  // qt resets the thread name somewhere within the QWebEngineProfile calls
  // above
  MOShared::SetThisThreadName("main");

  ui->setupUi(this);
  onLanguageChanged(settings.interface().language());
  ui->statusBar->setup(ui, settings);

  {
    auto& ni = NexusInterface::instance();

    // there are two ways to get here:
    //  1) the user just started MO, and
    //  2) the user has changed some setting that required a restart
    //
    // "restarting" MO doesn't actually re-execute the binary, it just basically
    // executes most of main() again, so a bunch of things are actually not
    // reset
    //
    // one of these things is the api status, which will have fired its events
    // long before the execution gets here because stuff is still cached and no
    // real request to nexus is actually done
    //
    // therefore, when the user starts MO normally, the user account and stats
    // will be empty (which is fine) and populated later on when the api key
    // check has finished
    //
    // in the rare case where the user restarts MO through the settings, this
    // will correctly pick up the previous values
    updateWindowTitle(ni.getAPIUserAccount());
    ui->statusBar->setAPI(ni.getAPIStats(), ni.getAPIUserAccount());
  }

  m_CategoryFactory.loadCategories();

  ui->logList->setCore(m_OrganizerCore);

  setupToolbar();
  toggleMO2EndorseState();
  toggleUpdateAction();

  TaskProgressManager::instance().tryCreateTaskbar();

  setupModList();
  ui->espList->setup(m_OrganizerCore, this, ui);
  ui->bsaList->setLocalMoveOnly(true);
  ui->bsaList->setHeaderHidden(true);

  const bool pluginListAdjusted =
      settings.geometry().restoreState(ui->espList->header());

  // data tab
  m_DataTab.reset(new DataTab(m_OrganizerCore, m_PluginContainer, this, ui));
  m_DataTab->restoreState(settings);

  connect(m_DataTab.get(), &DataTab::executablesChanged, [&] {
    refreshExecutablesList();
  });

  connect(m_DataTab.get(), &DataTab::originModified, [&](int id) {
    originModified(id);
  });

  connect(m_DataTab.get(), &DataTab::displayModInformation,
          [&](auto&& m, auto&& i, auto&& tab) {
            displayModInformation(m, i, tab);
          });

  // downloads tab
  m_DownloadsTab.reset(new DownloadsTab(m_OrganizerCore, ui));

  // saves tab
  m_SavesTab.reset(new SavesTab(this, m_OrganizerCore, ui));

  // Hide stuff we do not need:
  auto& features = m_OrganizerCore.gameFeatures();
  if (!features.gameFeature<GamePlugins>()) {
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->espTab));
  }
  if (!features.gameFeature<DataArchives>()) {
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->bsaTab));
  }

  settings.geometry().restoreState(ui->downloadView->header());
  settings.geometry().restoreState(ui->savegameList->header());

  ui->splitter->setStretchFactor(0, 3);
  ui->splitter->setStretchFactor(1, 2);

  resizeLists(pluginListAdjusted);

  QMenu* linkMenu = new QMenu(this);
  m_LinkToolbar   = linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Toolbar and Menu"),
                                        this, SLOT(linkToolbar()));
  m_LinkDesktop   = linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Desktop"), this,
                                        SLOT(linkDesktop()));
  m_LinkStartMenu = linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Start Menu"), this,
                                        SLOT(linkMenu()));
  ui->linkButton->setMenu(linkMenu);

  ui->listOptionsBtn->setMenu(
      new ModListGlobalContextMenu(m_OrganizerCore, ui->modList, this));

  ui->openFolderMenu->setMenu(openFolderMenu());

  // don't allow mouse wheel to switch grouping, too many people accidentally
  // turn on grouping and then don't understand what happened
  EventFilter* noWheel = new EventFilter(this, [](QObject*, QEvent* event) -> bool {
    return event->type() == QEvent::Wheel;
  });

  ui->groupCombo->installEventFilter(noWheel);
  ui->profileBox->installEventFilter(noWheel);

  updateSortButton();

  connect(&m_PluginContainer, SIGNAL(diagnosisUpdate()), this,
          SLOT(scheduleCheckForProblems()));

  connect(&m_OrganizerCore, &OrganizerCore::directoryStructureReady, this,
          &MainWindow::onDirectoryStructureChanged);
  connect(m_OrganizerCore.directoryRefresher(),
          SIGNAL(progress(const DirectoryRefreshProgress*)), this,
          SLOT(refresherProgress(const DirectoryRefreshProgress*)));
  connect(m_OrganizerCore.directoryRefresher(), SIGNAL(error(QString)), this,
          SLOT(showError(QString)));

  connect(&m_OrganizerCore.settings(), &Settings::languageChanged, this,
          &MainWindow::onLanguageChanged);
  connect(&m_OrganizerCore.settings(), &Settings::themeChanged, this,
          &MainWindow::themeChanged);

  connect(m_OrganizerCore.updater(), SIGNAL(restart()), this, SLOT(close()));
  connect(m_OrganizerCore.updater(), SIGNAL(updateAvailable()), this,
          SLOT(updateAvailable()));
  connect(m_OrganizerCore.updater(), SIGNAL(motdAvailable(QString)), this,
          SLOT(motdReceived(QString)));
  connect(&m_OrganizerCore, &OrganizerCore::refreshTriggered, this, [this]() {
    updateSortButton();
  });

  connect(&NexusInterface::instance(), SIGNAL(requestNXMDownload(QString)),
          &m_OrganizerCore, SLOT(downloadRequestedNXM(QString)));
  connect(&NexusInterface::instance(),
          SIGNAL(nxmDownloadURLsAvailable(QString, int, int, QVariant, QVariant, int)),
          this, SLOT(nxmDownloadURLs(QString, int, int, QVariant, QVariant, int)));
  connect(&NexusInterface::instance(), SIGNAL(needLogin()), &m_OrganizerCore,
          SLOT(nexusApi()));

  connect(NexusInterface::instance().getAccessManager(),
          &NXMAccessManager::credentialsReceived, this, &MainWindow::updateWindowTitle);
  connect(&NexusInterface::instance(), &NexusInterface::requestsChanged, ui->statusBar,
          &StatusBar::setAPI);

  connect(&TutorialManager::instance(), SIGNAL(windowTutorialFinished(QString)), this,
          SLOT(windowTutorialFinished(QString)));
  connect(ui->tabWidget, SIGNAL(currentChanged(int)), &TutorialManager::instance(),
          SIGNAL(tabChanged(int)));
  connect(ui->toolBar, SIGNAL(customContextMenuRequested(QPoint)), this,
          SLOT(toolBar_customContextMenuRequested(QPoint)));
  connect(ui->menuToolbars, &QMenu::aboutToShow, [&] {
    updateToolbarMenu();
  });
  connect(ui->menuView, &QMenu::aboutToShow, [&] {
    updateViewMenu();
  });
  connect(ui->actionTool->menu(), &QMenu::aboutToShow, [&] {
    updateToolMenu();
  });
  connect(&m_PluginContainer, &PluginContainer::pluginEnabled, this,
          [this](IPlugin* plugin) {
            if (m_PluginContainer.implementInterface<IPluginModPage>(plugin)) {
              updateModPageMenu();
            }
          });
  connect(&m_PluginContainer, &PluginContainer::pluginDisabled, this,
          [this](IPlugin* plugin) {
            if (m_PluginContainer.implementInterface<IPluginModPage>(plugin)) {
              updateModPageMenu();
            }
          });
  connect(&m_PluginContainer, &PluginContainer::pluginRegistered, this,
          &MainWindow::onPluginRegistrationChanged);
  connect(&m_PluginContainer, &PluginContainer::pluginUnregistered, this,
          &MainWindow::onPluginRegistrationChanged);

  connect(&m_OrganizerCore, &OrganizerCore::modInstalled, this,
          &MainWindow::modInstalled);

  connect(&m_CategoryFactory, SIGNAL(nexusCategoryRefresh(CategoriesDialog*)), this,
          SLOT(refreshNexusCategories(CategoriesDialog*)));
  connect(&m_CategoryFactory, SIGNAL(categoriesSaved()), this, SLOT(categoriesSaved()));

  m_CheckBSATimer.setSingleShot(true);
  connect(&m_CheckBSATimer, SIGNAL(timeout()), this, SLOT(checkBSAList()));

  setFilterShortcuts(ui->modList, ui->modFilterEdit);
  setFilterShortcuts(ui->espList, ui->espFilterEdit);
  setFilterShortcuts(ui->downloadView, ui->downloadFilterEdit);

  m_UpdateProblemsTimer.setSingleShot(true);
  connect(&m_UpdateProblemsTimer, &QTimer::timeout, this,
          &MainWindow::checkForProblemsAsync);
  connect(this, &MainWindow::checkForProblemsDone, this,
          &MainWindow::updateProblemsButton, Qt::ConnectionType::QueuedConnection);

  m_SaveMetaTimer.setSingleShot(false);
  connect(&m_SaveMetaTimer, SIGNAL(timeout()), this, SLOT(saveModMetas()));
  m_SaveMetaTimer.start(5000);

  FileDialogMemory::restore(settings);

  fixCategories();

  m_StartTime = QTime::currentTime();

  m_Tutorial.expose("modList", m_OrganizerCore.modList());
  m_Tutorial.expose("espList", m_OrganizerCore.pluginList());

  m_OrganizerCore.setUserInterface(this);
  connect(m_OrganizerCore.modList(), &ModList::showMessage, [=](auto&& message) {
    showMessage(message);
  });
  connect(m_OrganizerCore.modList(), &ModList::modRenamed,
          [=](auto&& oldName, auto&& newName) {
            modRenamed(oldName, newName);
          });
  connect(m_OrganizerCore.modList(), &ModList::modUninstalled, [=](auto&& name) {
    modRemoved(name);
  });
  connect(m_OrganizerCore.modList(), &ModList::fileMoved, [=](auto&&... args) {
    fileMoved(args...);
  });
  connect(m_OrganizerCore.installationManager(), &InstallationManager::modReplaced,
          [=](auto&& name) {
            modRemoved(name);
          });
  connect(m_OrganizerCore.downloadManager(), &DownloadManager::showMessage,
          [=](auto&& message) {
            showMessage(message);
          });

  updateModPageMenu();

  // refresh profiles so the current profile can be activated
  refreshProfiles(false);

  ui->profileBox->setCurrentText(m_OrganizerCore.currentProfile()->name());

  if (settings.archiveParsing()) {
    ui->dataTabShowFromArchives->setCheckState(Qt::Checked);
    ui->dataTabShowFromArchives->setEnabled(true);
  } else {
    ui->dataTabShowFromArchives->setCheckState(Qt::Unchecked);
    ui->dataTabShowFromArchives->setEnabled(false);
  }

  QApplication::instance()->installEventFilter(this);

  scheduleCheckForProblems();
  refreshExecutablesList();
  updatePinnedExecutables();
  resetActionIcons();
  processUpdates();

  ui->modList->updateModCount();
  ui->espList->updatePluginCount();
  ui->statusBar->updateNormalMessage(m_OrganizerCore);
}

void MainWindow::setupModList()
{
  ui->modList->setup(m_OrganizerCore, m_CategoryFactory, this, ui);

  connect(&ui->modList->actions(), &ModListViewActions::overwriteCleared, [=]() {
    scheduleCheckForProblems();
  });
  connect(&ui->modList->actions(), &ModListViewActions::originModified, this,
          &MainWindow::originModified);
  connect(&ui->modList->actions(), &ModListViewActions::modInfoDisplayed, this,
          &MainWindow::modInfoDisplayed);

  connect(m_OrganizerCore.modList(), &ModList::modPrioritiesChanged, [&]() {
    m_ArchiveListWriter.write();
  });
}

void MainWindow::resetActionIcons()
{
  // this is a bit of a hack
  //
  // the .qss files have historically set qproperty-icon by id and these ids
  // correspond to the QActions created in the .ui file
  //
  // the problem is that QActions do not support having their icon property
  // set from a .qss because they're not widgets (they don't inherit from
  // QWidget), and styling only works on widget
  //
  // a QAction _does_ have an associated icon, it just can't be set from a .qss
  // file
  //
  // so here, a dummy QToolButton widget is created for each QAction and is
  // given the same name as the action, which makes it pick up the icon
  // specified in the .qss file
  //
  // that icon is then given to the widget used by the QAction (if it's some
  // sort of button, which typically happens on the toolbar) _and_ to the
  // QAction itself, which is used in the menu bar

  // clearing the notification, will be set below if the stylesheet has set
  // anything for it
  m_originalNotificationIcon = {};

  // QActions created from the .ui file are children of the main window
  for (QAction* action : findChildren<QAction*>()) {
    // creating a dummy button
    auto dummy = std::make_unique<QToolButton>();

    // reusing the action name
    dummy->setObjectName(action->objectName());

    // styling the button, this has to be done manually because the button is
    // never added anywhere
    style()->polish(dummy.get());

    // the button's icon may be null if it wasn't specified in the .qss file,
    // which can happen if the stylesheet just doesn't override icons, or for
    // other actions like the pinned custom executables
    const auto icon = dummy->icon();
    if (icon.isNull()) {
      continue;
    }

    // button associated with the action on the toolbar
    QWidget* actionWidget = ui->toolBar->widgetForAction(action);

    if (auto* actionButton = dynamic_cast<QAbstractButton*>(actionWidget)) {
      actionButton->setIcon(icon);
    }

    // the action's icon is used by the menu bar
    action->setIcon(icon);

    if (action == ui->actionNotifications) {
      // if the stylesheet has set a notification icon, remember it here so it
      // can be used in updateProblemsButton()
      m_originalNotificationIcon = icon;
    }
  }

  // update the button for the potentially new icon
  updateProblemsButton();
}

MainWindow::~MainWindow()
{
  try {
    cleanup();

    m_OrganizerCore.setUserInterface(nullptr);

    if (m_IntegratedBrowser) {
      m_IntegratedBrowser->close();
      m_IntegratedBrowser.reset();
    }

    delete ui;
  } catch (std::exception& e) {
    QMessageBox::critical(
        nullptr, tr("Crash on exit"),
        tr("MO crashed while exiting.  Some settings may not be saved.\n\nError: %1")
            .arg(e.what()),
        QMessageBox::Ok);
  }
}

void MainWindow::updateWindowTitle(const APIUserAccount& user)
{
  //"\xe2\x80\x93" is an "em dash", a longer "-"
  QString title =
      QString("%1 \xe2\x80\x93 Mod Organizer v%2")
          .arg(m_OrganizerCore.managedGame()->displayGameName(),
               m_OrganizerCore.getVersion().string(Version::FormatCondensed));

  if (!user.name().isEmpty()) {
    const QString premium = (user.type() == APIUserAccountTypes::Premium ? "*" : "");
    title.append(QString(" (%1%2)").arg(user.name(), premium));
  }

  this->setWindowTitle(title);
}

void MainWindow::resizeLists(bool pluginListCustom)
{
  // ensure the columns aren't so small you can't see them any more
  for (int i = 0; i < ui->modList->header()->count(); ++i) {
    if (ui->modList->header()->sectionSize(i) < 10) {
      ui->modList->header()->resizeSection(i, 10);
    }
  }

  if (!pluginListCustom) {
    // resize plugin list to fit content
    for (int i = 0; i < ui->espList->header()->count(); ++i) {
      ui->espList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    ui->espList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  }
}

void MainWindow::allowListResize()
{
  // allow resize on mod list
  for (int i = 0; i < ui->modList->header()->count(); ++i) {
    ui->modList->header()->setSectionResizeMode(i, QHeaderView::Interactive);
  }
  ui->modList->header()->setStretchLastSection(true);

  // allow resize on plugin list
  for (int i = 0; i < ui->espList->header()->count(); ++i) {
    ui->espList->header()->setSectionResizeMode(i, QHeaderView::Interactive);
  }
  ui->espList->header()->setStretchLastSection(true);
}

void MainWindow::updateStyle(const QString&)
{
  resetActionIcons();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
  m_Tutorial.resize(event->size());
  QMainWindow::resizeEvent(event);
}

void MainWindow::setupToolbar()
{
  setupActionMenu(ui->actionModPage);
  setupActionMenu(ui->actionTool);
  setupActionMenu(ui->actionHelp);
  setupActionMenu(ui->actionEndorseMO);

  createHelpMenu();
  createEndorseMenu();

  // find last separator, add a spacer just before it so the icons are
  // right-aligned
  m_linksSeparator = nullptr;
  for (auto* a : ui->toolBar->actions()) {
    if (a->isSeparator()) {
      m_linksSeparator = a;
    }
  }

  if (m_linksSeparator) {
    auto* spacer = new QWidget(ui->toolBar);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    ui->toolBar->insertWidget(m_linksSeparator, spacer);

  } else {
    log::warn("no separator found on the toolbar, icons won't be right-aligned");
  }

  if (!InstanceManager::singleton().allowedToChangeInstance()) {
    ui->actionChange_Game->setVisible(false);
  }
}

void MainWindow::setupActionMenu(QAction* a)
{
  a->setMenu(new QMenu(this));

  auto* w = ui->toolBar->widgetForAction(a);
  if (auto* tb = dynamic_cast<QToolButton*>(w))
    tb->setPopupMode(QToolButton::InstantPopup);
}

void MainWindow::updatePinnedExecutables()
{
  for (auto* a : ui->toolBar->actions()) {
    if (a->objectName().startsWith("custom__")) {
      ui->toolBar->removeAction(a);
      a->deleteLater();
    }
  }

  ui->menuRun->clear();

  bool hasLinks = false;

  for (const auto& exe : *m_OrganizerCore.executablesList()) {
    if (!exe.hide() && exe.isShownOnToolbar()) {
      hasLinks = true;

      QAction* exeAction =
          new QAction(iconForExecutable(exe.binaryInfo().filePath()), exe.title());

      exeAction->setObjectName(QString("custom__") + exe.title());
      exeAction->setStatusTip(exe.binaryInfo().filePath());

      if (!connect(exeAction, SIGNAL(triggered()), this, SLOT(startExeAction()))) {
        log::debug("failed to connect trigger?");
      }

      if (m_linksSeparator) {
        ui->toolBar->insertAction(m_linksSeparator, exeAction);
      } else {
        // separator wasn't found, add it to the end
        ui->toolBar->addAction(exeAction);
      }

      ui->menuRun->addAction(exeAction);
    }
  }

  // don't show the menu if there are no links
  ui->menuRun->menuAction()->setVisible(hasLinks);
}

void MainWindow::updateToolbarMenu()
{
  ui->actionMainMenuToggle->setChecked(ui->menuBar->isVisible());
  ui->actionToolBarMainToggle->setChecked(ui->toolBar->isVisible());
  ui->actionStatusBarToggle->setChecked(ui->statusBar->isVisible());

  ui->actionToolBarSmallIcons->setChecked(ui->toolBar->iconSize() == SmallToolbarSize);
  ui->actionToolBarMediumIcons->setChecked(ui->toolBar->iconSize() ==
                                           MediumToolbarSize);
  ui->actionToolBarLargeIcons->setChecked(ui->toolBar->iconSize() == LargeToolbarSize);

  ui->actionToolBarIconsOnly->setChecked(ui->toolBar->toolButtonStyle() ==
                                         Qt::ToolButtonIconOnly);
  ui->actionToolBarTextOnly->setChecked(ui->toolBar->toolButtonStyle() ==
                                        Qt::ToolButtonTextOnly);
  ui->actionToolBarIconsAndText->setChecked(ui->toolBar->toolButtonStyle() ==
                                            Qt::ToolButtonTextUnderIcon);
}

void MainWindow::updateViewMenu()
{
  ui->actionViewLog->setChecked(ui->logDock->isVisible());
}

QMenu* MainWindow::createPopupMenu()
{
  auto* m = new QMenu;

  // add all the actions from the toolbars menu
  for (auto* a : ui->menuToolbars->actions()) {
    m->addAction(a);
  }

  m->addSeparator();

  // other actions
  m->addAction(ui->actionViewLog);

  // make sure the actions are updated
  updateToolbarMenu();
  updateViewMenu();

  return m;
}

void MainWindow::on_actionMainMenuToggle_triggered()
{
  ui->menuBar->setVisible(!ui->menuBar->isVisible());
}

void MainWindow::on_actionToolBarMainToggle_triggered()
{
  ui->toolBar->setVisible(!ui->toolBar->isVisible());
}

void MainWindow::on_actionStatusBarToggle_triggered()
{
  ui->statusBar->setVisible(!ui->statusBar->isVisible());
}

void MainWindow::on_actionToolBarSmallIcons_triggered()
{
  setToolbarSize(SmallToolbarSize);
}

void MainWindow::on_actionToolBarMediumIcons_triggered()
{
  setToolbarSize(MediumToolbarSize);
}

void MainWindow::on_actionToolBarLargeIcons_triggered()
{
  setToolbarSize(LargeToolbarSize);
}

void MainWindow::on_actionToolBarIconsOnly_triggered()
{
  setToolbarButtonStyle(Qt::ToolButtonIconOnly);
}

void MainWindow::on_actionToolBarTextOnly_triggered()
{
  setToolbarButtonStyle(Qt::ToolButtonTextOnly);
}

void MainWindow::on_actionToolBarIconsAndText_triggered()
{
  setToolbarButtonStyle(Qt::ToolButtonTextUnderIcon);
}

void MainWindow::on_actionViewLog_triggered()
{
  ui->logDock->setVisible(!ui->logDock->isVisible());
}

void MainWindow::setToolbarSize(const QSize& s)
{
  for (auto* tb : findChildren<QToolBar*>()) {
    tb->setIconSize(s);
  }
}

void MainWindow::setToolbarButtonStyle(Qt::ToolButtonStyle s)
{
  for (auto* tb : findChildren<QToolBar*>()) {
    tb->setToolButtonStyle(s);
  }
}

void MainWindow::on_centralWidget_customContextMenuRequested(const QPoint& pos)
{
  // this allows for getting the context menu even if both the menubar and all
  // the toolbars are hidden; an alternative is the Alt key handled in
  // keyPressEvent() below

  // the custom context menu event bubbles up to here if widgets don't actually
  // process this, which would show the menu when right-clicking button, labels,
  // etc.
  //
  // only show the context menu when right-clicking on the central widget
  // itself, which is basically just the outer edges of the main window
  auto* w = childAt(pos);
  if (w != ui->centralWidget) {
    return;
  }

  createPopupMenu()->exec(ui->centralWidget->mapToGlobal(pos));
}

void MainWindow::scheduleCheckForProblems()
{
  if (!m_UpdateProblemsTimer.isActive()) {
    m_UpdateProblemsTimer.start(500);
  }
}

void MainWindow::updateProblemsButton()
{
  // if the current stylesheet doesn't provide an icon, this is used instead
  const char* DefaultIconName = ":/MO/gui/warning";

  const std::size_t numProblems = m_NumberOfProblems;

  // original icon without a count painted on it
  const QIcon original = m_originalNotificationIcon.isNull()
                             ? QIcon(DefaultIconName)
                             : m_originalNotificationIcon;

  // final icon
  QIcon final;

  if (numProblems > 0) {
    ui->actionNotifications->setToolTip(tr("There are notifications to read"));

    // will contain the original icon, plus a notification count; this also
    // makes sure the pixmap is exactly 64x64 by requesting the icon that's
    // as close to 64x64 as possible, and then scaling it up if it's too small
    QPixmap merged = original.pixmap(64, 64).scaled(64, 64);

    {
      QPainter painter(&merged);

      const std::string badgeName =
          std::string(":/MO/gui/badge_") +
          (numProblems < 10 ? std::to_string(static_cast<long long>(numProblems))
                            : "more");

      painter.drawPixmap(32, 32, 32, 32, QPixmap(badgeName.c_str()));
    }

    final = QIcon(merged);
  } else {
    ui->actionNotifications->setToolTip(tr("There are no notifications"));

    // no change
    final = original;
  }

  ui->actionNotifications->setEnabled(numProblems > 0);

  // setting the icon on the action (shown on the menu)
  ui->actionNotifications->setIcon(final);

  // setting the icon on the toolbar button
  if (auto* actionWidget = ui->toolBar->widgetForAction(ui->actionNotifications)) {
    if (auto* button = dynamic_cast<QAbstractButton*>(actionWidget)) {
      button->setIcon(final);
    }
  }

  // updating the status bar, may be null very early when MO is starting
  if (ui->statusBar) {
    ui->statusBar->setNotifications(numProblems > 0);
  }
}

bool MainWindow::errorReported(QString& logFile)
{
  QDir dir(qApp->property("dataPath").toString() + "/" +
           QString::fromStdWString(AppConfig::logPath()));
  QFileInfoList files =
      dir.entryInfoList(QStringList("ModOrganizer_??_??_??_??_??.log"), QDir::Files,
                        QDir::Name | QDir::Reversed);

  if (files.count() > 0) {
    logFile = files.at(0).absoluteFilePath();
    QFile file(logFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      char buffer[1024];
      int line = 0;
      while (!file.atEnd()) {
        file.readLine(buffer, 1024);
        if (strncmp(buffer, "ERROR", 5) == 0) {
          return true;
        }

        // prevent this function from taking forever
        if (line++ >= 50000) {
          break;
        }
      }
    }
  }

  return false;
}

QFuture<void> MainWindow::checkForProblemsAsync()
{
  return QtConcurrent::run([this]() {
    checkForProblemsImpl();
  });
}

void MainWindow::checkForProblemsImpl()
{
  m_ProblemsCheckRequired = true;

  std::scoped_lock lk(m_CheckForProblemsMutex);

  // another thread might already have checked while this one was waiting on the lock
  if (m_ProblemsCheckRequired) {
    m_ProblemsCheckRequired = false;
    TimeThis tt("MainWindow::checkForProblemsImpl()");
    size_t numProblems = 0;
    for (QObject* pluginObj : m_PluginContainer.plugins<QObject>()) {
      IPlugin* plugin = qobject_cast<IPlugin*>(pluginObj);
      if (plugin == nullptr || m_PluginContainer.isEnabled(plugin)) {
        IPluginDiagnose* diagnose = qobject_cast<IPluginDiagnose*>(pluginObj);
        if (diagnose != nullptr)
          numProblems += diagnose->activeProblems().size();
      }
    }
    m_NumberOfProblems = numProblems;
    emit checkForProblemsDone();
  }
}

void MainWindow::about()
{
  AboutDialog(m_OrganizerCore.getVersion().string(Version::FormatCondensed), this)
      .exec();
}

void MainWindow::createEndorseMenu()
{
  auto* menu = ui->actionEndorseMO->menu();
  if (!menu) {
    // shouldn't happen
    return;
  }

  menu->clear();

  QAction* endorseAction = new QAction(tr("Endorse"), menu);
  connect(endorseAction, SIGNAL(triggered()), this, SLOT(actionEndorseMO()));
  menu->addAction(endorseAction);

  QAction* wontEndorseAction = new QAction(tr("Won't Endorse"), menu);
  connect(wontEndorseAction, SIGNAL(triggered()), this, SLOT(actionWontEndorseMO()));
  menu->addAction(wontEndorseAction);
}

void MainWindow::createHelpMenu()
{
  //: Translation strings for tutorial names
  static std::map<QString, const char*> translate = {
      {"First Steps", QT_TR_NOOP("First Steps")},
      {"Conflict Resolution", QT_TR_NOOP("Conflict Resolution")},
      {"Overview", QT_TR_NOOP("Overview")}};

  auto* menu = ui->actionHelp->menu();
  if (!menu) {
    // this happens on startup because languageChanged() (which calls this) is
    // called before the menus are actually created
    return;
  }

  menu->clear();

  QAction* helpAction = new QAction(tr("Help on UI"), menu);
  connect(helpAction, SIGNAL(triggered()), this, SLOT(helpTriggered()));
  menu->addAction(helpAction);

  QAction* wikiAction = new QAction(tr("Documentation"), menu);
  connect(wikiAction, SIGNAL(triggered()), this, SLOT(wikiTriggered()));
  menu->addAction(wikiAction);

  if (!m_OrganizerCore.managedGame()->getSupportURL().isEmpty()) {
    QAction* gameSupportAction = new QAction(tr("Game Support Wiki"), menu);
    connect(gameSupportAction, SIGNAL(triggered()), this, SLOT(gameSupportTriggered()));
    menu->addAction(gameSupportAction);
  }

  QAction* discordAction = new QAction(tr("Chat on Discord"), menu);
  connect(discordAction, SIGNAL(triggered()), this, SLOT(discordTriggered()));
  menu->addAction(discordAction);

  QAction* issueAction = new QAction(tr("Report Issue"), menu);
  connect(issueAction, SIGNAL(triggered()), this, SLOT(issueTriggered()));
  menu->addAction(issueAction);

  QMenu* tutorialMenu = new QMenu(tr("Tutorials"), menu);

  typedef std::vector<std::pair<int, QAction*>> ActionList;

  ActionList tutorials;

  QDirIterator dirIter(QApplication::applicationDirPath() + "/tutorials",
                       QStringList("*.js"), QDir::Files);
  while (dirIter.hasNext()) {
    dirIter.next();
    QString fileName = dirIter.fileName();

    QFile file(dirIter.filePath());
    if (!file.open(QIODevice::ReadOnly)) {
      log::error("Failed to open {}", fileName);
      continue;
    }
    QString firstLine = QString::fromUtf8(file.readLine());
    if (firstLine.startsWith("//TL")) {
      QStringList params = firstLine.mid(4).trimmed().split('#');
      if (params.size() != 2) {
        log::error("invalid header line for tutorial {}, expected 2 parameters",
                   fileName);
        continue;
      }
      QAction* tutAction = new QAction(tr(translate[params.at(0)]), tutorialMenu);
      tutAction->setData(fileName);
      tutorials.push_back(std::make_pair(params.at(1).toInt(), tutAction));
    }
  }

  std::sort(tutorials.begin(), tutorials.end(),
            [](const ActionList::value_type& LHS, const ActionList::value_type& RHS) {
              return LHS.first < RHS.first;
            });

  for (auto iter = tutorials.begin(); iter != tutorials.end(); ++iter) {
    connect(iter->second, SIGNAL(triggered()), this, SLOT(tutorialTriggered()));
    tutorialMenu->addAction(iter->second);
  }

  menu->addMenu(tutorialMenu);
  menu->addAction(tr("About"), this, SLOT(about()));
  menu->addAction(tr("About Qt"), qApp, SLOT(aboutQt()));
}

bool MainWindow::addProfile()
{
  QComboBox* profileBox = findChild<QComboBox*>("profileBox");
  bool okClicked        = false;

  QString name = QInputDialog::getText(this, tr("Name"),
                                       tr("Please enter a name for the new profile"),
                                       QLineEdit::Normal, QString(), &okClicked);
  if (okClicked && (name.size() > 0)) {
    try {
      profileBox->addItem(name);
      profileBox->setCurrentIndex(profileBox->count() - 1);
      return true;
    } catch (const std::exception& e) {
      reportError(tr("failed to create profile: %1").arg(e.what()));
      return false;
    }
  } else {
    return false;
  }
}

void MainWindow::hookUpWindowTutorials()
{
  QDirIterator dirIter(QApplication::applicationDirPath() + "/tutorials",
                       QStringList("*.js"), QDir::Files);
  while (dirIter.hasNext()) {
    dirIter.next();
    QString fileName = dirIter.fileName();
    QFile file(dirIter.filePath());
    if (!file.open(QIODevice::ReadOnly)) {
      log::error("Failed to open {}", fileName);
      continue;
    }
    QString firstLine = QString::fromUtf8(file.readLine());
    if (firstLine.startsWith("//WIN")) {
      QString windowName = firstLine.mid(6).trimmed();
      if (!m_OrganizerCore.settings().interface().isTutorialCompleted(windowName)) {
        TutorialManager::instance().activateTutorial(windowName, fileName);
      }
    }
  }
}

bool MainWindow::shouldStartTutorial() const
{
  if (GlobalSettings::hideTutorialQuestion()) {
    return false;
  }

  QMessageBox dlg(
      QMessageBox::Question, tr("Show tutorial?"),
      tr("You are starting Mod Organizer for the first time. "
         "Do you want to show a tutorial of its basic features? If you choose "
         "no you can always start the tutorial from the \"Help\" menu."),
      QMessageBox::Yes | QMessageBox::No);

  dlg.setCheckBox(new QCheckBox(tr("Never ask to show tutorials")));

  const auto r = dlg.exec();

  if (dlg.checkBox()->isChecked()) {
    GlobalSettings::setHideTutorialQuestion(true);
  }

  return (r == QMessageBox::Yes);
}

void MainWindow::showEvent(QShowEvent* event)
{
  QMainWindow::showEvent(event);

  if (!m_WasVisible) {
    ui->modList->refreshFilters();
    readSettings();

    // this needs to be connected here instead of in the constructor because the
    // actual changing of the stylesheet is done by MOApplication, which
    // connects its signal in runApplication() (in main.cpp), and that happens
    // _after_ the MainWindow is constructed, but _before_ it is shown
    //
    // by connecting the event here, changing the style setting will first be
    // handled by MOApplication, and then in updateStyle(), at which point the
    // stylesheet has already been set correctly
    connect(this, &MainWindow::themeChanged, this, &MainWindow::updateStyle);

    // only the first time the window becomes visible
    m_Tutorial.registerControl();

    hookUpWindowTutorials();

    if (m_OrganizerCore.settings().firstStart()) {
      QString firstStepsTutorial = ToQString(AppConfig::firstStepsTutorial());
      if (TutorialManager::instance().hasTutorial(firstStepsTutorial)) {
        if (shouldStartTutorial()) {
          TutorialManager::instance().activateTutorial("MainWindow",
                                                       firstStepsTutorial);
        }
      } else {
        log::error("{} missing", firstStepsTutorial);
        QPoint pos = ui->toolBar->mapToGlobal(QPoint());
        pos.rx() += ui->toolBar->width() / 2;
        pos.ry() += ui->toolBar->height();
        QWhatsThis::showText(pos,
                             QObject::tr("Please use \"Help\" from the toolbar to get "
                                         "usage instructions to all elements"));
      }

      if (!m_OrganizerCore.managedGame()->getSupportURL().isEmpty()) {
        QMessageBox::information(this, tr("Game Support Wiki"),
                                 tr("Do you know how to mod this game? Do you need to "
                                    "learn? There's a game support wiki available! "
                                    "Click OK to open the wiki. In the future, you can "
                                    "access this link from the \"Help\" menu."),
                                 QMessageBox::Ok);
        gameSupportTriggered();
      }

      QMessageBox newCatDialog;
      newCatDialog.setWindowTitle(tr("Category Setup"));
      newCatDialog.setText(
          tr("Please choose how to handle the default category setup.\n\n"
             "If you've already connected to Nexus, you can automatically import Nexus "
             "categories for this game (if applicable). Otherwise, use the old Mod "
             "Organizer default category structure, or leave the categories blank (for "
             "manual setup)."));
      QPushButton importBtn(tr("&Import Nexus Categories"));
      QPushButton defaultBtn(tr("Use &Old Category Defaults"));
      QPushButton cancelBtn(tr("Do &Nothing"));
      if (NexusInterface::instance().getAccessManager()->validated()) {
        newCatDialog.addButton(&importBtn, QMessageBox::ButtonRole::AcceptRole);
      }
      newCatDialog.addButton(&defaultBtn, QMessageBox::ButtonRole::AcceptRole);
      newCatDialog.addButton(&cancelBtn, QMessageBox::ButtonRole::RejectRole);
      newCatDialog.exec();
      if (newCatDialog.clickedButton() == &importBtn) {
        importCategories(false);
      } else if (newCatDialog.clickedButton() == &cancelBtn) {
        m_CategoryFactory.reset();
      } else if (newCatDialog.clickedButton() == &defaultBtn) {
        m_CategoryFactory.loadCategories();
      }
      m_CategoryFactory.saveCategories();

      m_OrganizerCore.settings().setFirstStart(false);
    } else {
      auto& settings = m_OrganizerCore.settings();
      if (m_LastVersion < QVersionNumber(2, 5) &&
          !GlobalSettings::hideCategoryReminder()) {
        QMessageBox migrateCatDialog;
        migrateCatDialog.setWindowTitle("Category Migration");
        migrateCatDialog.setText(
            tr("This is your first time running version 2.5 or higher with an old MO2 "
               "instance. The category system now relies on an updated system to map "
               "Nexus categories.\n\n"
               "In order to assign Nexus categories automatically, you will need to "
               "import the Nexus categories for the currently managed game and map "
               "them to your preferred category structure.\n\n"
               "You can either manually open the category editor, via the Settings "
               "dialog or the category filter sidebar, and set up the mappings as you "
               "see fit, or you can automatically import and map the categories as "
               "defined on Nexus.\n\n"
               "As a final option, you can disable Nexus category mapping altogether, "
               "which can be changed at any time in the Settings dialog."));
        QPushButton importBtn(tr("&Import Nexus Categories"));
        QPushButton openSettingsBtn(tr("&Open Categories Dialog"));
        QPushButton disableBtn(tr("&Disable Nexus Mappings"));
        QPushButton closeBtn(tr("&Close"));
        QCheckBox dontShow(tr("&Don't show this again"));
        if (NexusInterface::instance().getAccessManager()->validated()) {
          migrateCatDialog.addButton(&importBtn, QMessageBox::ButtonRole::AcceptRole);
        }
        migrateCatDialog.addButton(&openSettingsBtn,
                                   QMessageBox::ButtonRole::ActionRole);
        migrateCatDialog.addButton(&disableBtn,
                                   QMessageBox::ButtonRole::DestructiveRole);
        migrateCatDialog.addButton(&closeBtn, QMessageBox::ButtonRole::RejectRole);
        migrateCatDialog.setCheckBox(&dontShow);
        migrateCatDialog.exec();
        if (migrateCatDialog.clickedButton() == &importBtn) {
          importCategories(dontShow.isChecked());
        } else if (migrateCatDialog.clickedButton() == &openSettingsBtn) {
          this->ui->filtersEdit->click();
        } else if (migrateCatDialog.clickedButton() == &disableBtn) {
          Settings::instance().nexus().setCategoryMappings(false);
        }
        if (dontShow.isChecked()) {
          GlobalSettings::setHideCategoryReminder(true);
        }
      }
    }

    m_OrganizerCore.settings().widgets().restoreIndex(ui->groupCombo);

    m_OrganizerCore.settings().nexus().registerAsNXMHandler(false);
    m_WasVisible = true;
    updateProblemsButton();

    // notify plugins that the MO2 is ready
    m_PluginContainer.startPlugins(this);

    // forces a log list refresh to display startup logs
    //
    // since the log list is not visible until this point, the automatic
    // resize of columns seems to break the log list (since Qt 5.15.1 or
    // 5.15.2), an make the list empty on startup (in debug the list is not
    // empty because some logs are added after the log list becomes visible)
    //
    // the reset() forces a re-computation of the column size, thus properly
    // the logs that are already in the log model
    //
    ui->logList->reset();
    ui->logList->scrollToBottom();
  }
}

void MainWindow::paintEvent(QPaintEvent* event)
{
  if (m_FirstPaint) {
    allowListResize();
    m_FirstPaint = false;
  }

  QMainWindow::paintEvent(event);
}

void MainWindow::onBeforeClose()
{
  storeSettings();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
  if (isVisible()) {
    // this is messy
    //
    // the main problem this is solving is when closing MO, then getting the
    // lock overlay because processes are still running, then pressing the X
    // again
    //
    // in this case, closeEvent() is _not_ called for the second event and the
    // window is immediately hidden
    //
    // this always saves the settings here; in the event where a lock overlay
    // is then shown, it might save settings multiple times, but it's harmless
    onBeforeClose();
  }

  // this happens for two reasons:
  //  1) the user requested to close the window, such as clicking the X
  //  2) close() is called in runApplication() after application.exec()
  //     returns, which happens when qApp->exit() is called
  //
  // the window must never actually close for 1), because settings haven't been
  // saved yet: the state of many widgets is saved to the ini, which relies on
  // the window still being onscreen (or else everything is considered hidden)
  //
  // for 2), the settings have been saved and the window can just close

  if (ModOrganizerCanCloseNow()) {
    // the user has confirmed if necessary and all settings have been saved,
    // just close it
    QMainWindow::closeEvent(event);
    return;
  }

  if (UILocker::instance().locked()) {
    // don't bother asking the user to confirm if the ui is already locked
    event->ignore();
    ExitModOrganizer(Exit::Force);
    return;
  }

  if (ModOrganizerExiting()) {
    // ignore repeated attempts
    event->ignore();
    return;
  }

  // never close the window because settings might need to be changed
  event->ignore();

  // start the process of exiting, which may require confirmation by calling
  // canExit(), among other things
  ExitModOrganizer();
}

bool MainWindow::canExit()
{
  if (m_OrganizerCore.downloadManager()->downloadsInProgressNoPause()) {
    if (QMessageBox::question(
            this, tr("Downloads in progress"),
            tr("There are still downloads in progress, do you really want to quit?"),
            QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Cancel) {
      return false;
    } else {
      m_OrganizerCore.downloadManager()->pauseAll();
    }
  }

  const auto r = m_OrganizerCore.waitForAllUSVFSProcesses();
  if (r == ProcessRunner::Cancelled) {
    return false;
  }

  setCursor(Qt::WaitCursor);
  return true;
}

void MainWindow::cleanup()
{
  QWebEngineProfile::defaultProfile()->clearAllVisitedLinks();

  if (m_IntegratedBrowser) {
    m_IntegratedBrowser->close();
    m_IntegratedBrowser = {};
  }

  m_SaveMetaTimer.stop();
  m_MetaSave.waitForFinished();
}

bool MainWindow::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::StatusTip && object != this) {
    QMainWindow::event(event);
    return true;
  }

  return false;
}

void MainWindow::registerPluginTool(IPluginTool* tool, QString name, QMenu* menu)
{
  if (!menu) {
    menu = ui->actionTool->menu();
  }

  if (name.isEmpty())
    name = tool->displayName();

  QAction* action = new QAction(tool->icon(), name, menu);
  action->setToolTip(tool->tooltip());
  tool->setParentWidget(this);
  connect(
      action, &QAction::triggered, this,
      [this, tool]() {
        try {
          tool->display();
        } catch (const std::exception& e) {
          reportError(
              tr("Plugin \"%1\" failed: %2").arg(tool->localizedName()).arg(e.what()));
        } catch (...) {
          reportError(tr("Plugin \"%1\" failed").arg(tool->localizedName()));
        }
      },
      Qt::QueuedConnection);

  menu->addAction(action);
}

void MainWindow::updateToolMenu()
{
  // Clear the menu:
  ui->actionTool->menu()->clear();

  std::vector<IPluginTool*> toolPlugins = m_PluginContainer.plugins<IPluginTool>();

  // Sort the plugins by display name
  std::sort(std::begin(toolPlugins), std::end(toolPlugins),
            [](IPluginTool* left, IPluginTool* right) {
              return left->displayName().toLower() < right->displayName().toLower();
            });

  // Remove disabled plugins:
  toolPlugins.erase(std::remove_if(std::begin(toolPlugins), std::end(toolPlugins),
                                   [&](auto* tool) {
                                     return !m_PluginContainer.isEnabled(tool);
                                   }),
                    toolPlugins.end());

  // Group the plugins into submenus
  QMap<QString, QList<QPair<QString, IPluginTool*>>> submenuMap;
  for (auto toolPlugin : toolPlugins) {
    QStringList toolName = toolPlugin->displayName().split("/");
    QString submenu      = toolName[0];
    toolName.pop_front();
    submenuMap[submenu].append(
        QPair<QString, IPluginTool*>(toolName.join("/"), toolPlugin));
  }

  // Start registering plugins
  for (auto submenuKey : submenuMap.keys()) {
    if (submenuMap[submenuKey].length() > 1) {
      QMenu* submenu = new QMenu(submenuKey, this);
      for (auto info : submenuMap[submenuKey]) {
        registerPluginTool(info.second, info.first, submenu);
      }
      ui->actionTool->menu()->addMenu(submenu);
    } else {
      registerPluginTool(submenuMap[submenuKey].front().second);
    }
  }
}

void MainWindow::registerModPage(IPluginModPage* modPage)
{
  QAction* action = new QAction(modPage->icon(), modPage->displayName(), this);
  connect(
      action, &QAction::triggered, this,
      [this, modPage]() {
        if (modPage->useIntegratedBrowser()) {

          if (!m_IntegratedBrowser) {
            m_IntegratedBrowser.reset(new BrowserDialog);

            connect(m_IntegratedBrowser.get(),
                    SIGNAL(requestDownload(QUrl, QNetworkReply*)), &m_OrganizerCore,
                    SLOT(requestDownload(QUrl, QNetworkReply*)));
          }

          m_IntegratedBrowser->setWindowTitle(modPage->displayName());
          m_IntegratedBrowser->openUrl(modPage->pageURL());
        } else {
          shell::Open(QUrl(modPage->pageURL()));
        }
      },
      Qt::QueuedConnection);

  ui->actionModPage->menu()->addAction(action);
}

bool MainWindow::registerNexusPage(const QString& gameName)
{
  // Get the plugin
  IPluginGame* plugin = m_OrganizerCore.getGame(gameName);
  if (plugin == nullptr)
    return false;

  // Get the gameURL
  QString gameURL = NexusInterface::instance().getGameURL(gameName);
  if (gameURL.isEmpty())
    return false;

  // Create an action
  QAction* action = new QAction(plugin->gameIcon(),
                                QObject::tr("Visit %1 on Nexus").arg(gameName), this);

  // Bind the action
  connect(
      action, &QAction::triggered, this,
      [this, gameURL]() {
        shell::Open(QUrl(gameURL));
      },
      Qt::QueuedConnection);

  // Add the action
  ui->actionModPage->menu()->addAction(action);

  return true;
}

void MainWindow::updateModPageMenu()
{
  // Clear the menu:
  ui->actionModPage->menu()->clear();

  // Determine the loaded mod page plugins
  std::vector<IPluginModPage*> modPagePlugins =
      m_PluginContainer.plugins<IPluginModPage>();

  // Sort the plugins by display name
  std::sort(std::begin(modPagePlugins), std::end(modPagePlugins),
            [](IPluginModPage* left, IPluginModPage* right) {
              return left->displayName().toLower() < right->displayName().toLower();
            });

  // Remove disabled plugins
  modPagePlugins.erase(std::remove_if(std::begin(modPagePlugins),
                                      std::end(modPagePlugins),
                                      [&](auto* tool) {
                                        return !m_PluginContainer.isEnabled(tool);
                                      }),
                       modPagePlugins.end());

  for (auto* modPagePlugin : modPagePlugins) {
    registerModPage(modPagePlugin);
  }

  QStringList registeredSources;

  // Add the primary game
  QString gameShortName = m_OrganizerCore.managedGame()->gameShortName();
  if (registerNexusPage(gameShortName))
    registeredSources << gameShortName;

  // Add the primary sources
  for (auto gameName : m_OrganizerCore.managedGame()->primarySources()) {
    if (!registeredSources.contains(gameName) && registerNexusPage(gameName))
      registeredSources << gameName;
  }

  // Add a separator if needed
  if (registeredSources.length() > 0)
    ui->actionModPage->menu()->addSeparator();

  // Add the secondary games (sorted)
  QStringList secondaryGames = m_OrganizerCore.managedGame()->validShortNames();
  secondaryGames.sort(Qt::CaseInsensitive);
  for (auto gameName : secondaryGames) {
    if (!registeredSources.contains(gameName) && registerNexusPage(gameName))
      registeredSources << gameName;
  }

  // No mod page plugin and the menu was visible
  bool keepOriginalAction =
      modPagePlugins.size() == 0 && registeredSources.length() <= 1;
  if (keepOriginalAction) {
    ui->toolBar->insertAction(ui->actionAdd_Profile, ui->actionNexus);
  } else {
    ui->toolBar->removeAction(ui->actionNexus);
  }
  ui->actionModPage->setVisible(!keepOriginalAction);
}

void MainWindow::startExeAction()
{
  QAction* action = qobject_cast<QAction*>(sender());

  if (action == nullptr) {
    log::error("not an action?");
    return;
  }

  const auto& list = *m_OrganizerCore.executablesList();

  const auto title = action->text();
  auto itor        = list.find(title);

  if (itor == list.end()) {
    log::warn("startExeAction(): executable '{}' not found", title);
    return;
  }

  action->setEnabled(false);
  Guard g([&] {
    action->setEnabled(true);
  });

  m_OrganizerCore.processRunner()
      .setFromExecutable(*itor)
      .setWaitForCompletion(ProcessRunner::TriggerRefresh)
      .run();
}

void MainWindow::activateSelectedProfile()
{
  m_OrganizerCore.setCurrentProfile(ui->profileBox->currentText());

  m_SavesTab->refreshSaveList();
  m_OrganizerCore.refresh();
  ui->modList->updateModCount();
  ui->espList->updatePluginCount();
  ui->statusBar->updateNormalMessage(m_OrganizerCore);
}

void MainWindow::on_profileBox_currentIndexChanged(int index)
{
  if (!ui->profileBox->isEnabled()) {
    return;
  }

  int previousIndex = m_OldProfileIndex;
  m_OldProfileIndex = index;

  // select has changed, save stuff
  if ((previousIndex != -1) && (m_OrganizerCore.currentProfile() != nullptr) &&
      m_OrganizerCore.currentProfile()->exists()) {
    m_OrganizerCore.saveCurrentLists();
  }

  // Avoid doing any refresh if currentProfile is already set but previous
  // index was -1 as it means that this is happening during initialization so
  // everything has already been set.
  if (previousIndex == -1 && m_OrganizerCore.currentProfile() != nullptr &&
      m_OrganizerCore.currentProfile()->exists() &&
      ui->profileBox->currentText() == m_OrganizerCore.currentProfile()->name()) {
    return;
  }

  // ensure the new index is valid
  if (index < 0 || index >= ui->profileBox->count()) {
    log::debug("invalid profile index, using last profile");
    ui->profileBox->setCurrentIndex(ui->profileBox->count() - 1);
  }

  // handle <manage> item
  if (ui->profileBox->currentIndex() == 0) {
    // remember the profile name that was selected before, previousIndex can't
    // be used again because adding/deleting profiles will change the order
    // in the list
    const QString previousName = ui->profileBox->itemText(previousIndex);

    // show the dialog
    ProfilesDialog dlg(previousName, m_OrganizerCore, this);
    dlg.exec();

    // check if the user clicked 'select' to select another profile
    std::optional<QString> newSelection = dlg.selectedProfile();

    // refresh the profile box; this loops until there is at least one profile
    // available, which shouldn't really happen because the dialog won't allow
    // it
    //
    // the `false` to refreshProfiles() is so it doesn't try to select the
    // profile in the list because 1) it's done just below, and 2) it might be
    // wrong profile if there's something in newSelection
    while (!refreshProfiles(false)) {
      ProfilesDialog dlg(previousName, m_OrganizerCore, this);
      dlg.exec();
      newSelection = dlg.selectedProfile();
    }

    // note that setCurrentText() is recursive, it will re-execute this function
    if (newSelection) {
      ui->profileBox->setCurrentText(*newSelection);
    } else {
      ui->profileBox->setCurrentText(previousName);
    }

    // nothing else to do because setCurrentText() is recursive and will
    // have re-executed on_profileBox_currentIndexChanged() again, doing all
    // the stuff below for the new selection
    return;
  }

  activateSelectedProfile();

  auto saveGames = m_OrganizerCore.gameFeatures().gameFeature<LocalSavegames>();
  if (saveGames != nullptr) {
    if (saveGames->prepareProfile(m_OrganizerCore.currentProfile())) {
      m_SavesTab->refreshSaveList();
    }
  }

  auto invalidation = m_OrganizerCore.gameFeatures().gameFeature<BSAInvalidation>();
  if (invalidation != nullptr) {
    if (invalidation->prepareProfile(m_OrganizerCore.currentProfile())) {
      QTimer::singleShot(5, [this] {
        m_OrganizerCore.refresh();
      });
    }
  }
}

bool MainWindow::refreshProfiles(bool selectProfile, QString newProfile)
{
  QComboBox* profileBox = findChild<QComboBox*>("profileBox");

  QString currentProfileName = profileBox->currentText();

  profileBox->blockSignals(true);
  profileBox->clear();
  profileBox->addItem(QObject::tr("<Manage...>"));

  QDir profilesDir(Settings::instance().paths().profiles());
  profilesDir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);

  QDirIterator profileIter(profilesDir);

  while (profileIter.hasNext()) {
    profileIter.next();
    try {
      profileBox->addItem(profileIter.fileName());
    } catch (const std::runtime_error& error) {
      reportError(QObject::tr("failed to parse profile %1: %2")
                      .arg(profileIter.fileName())
                      .arg(error.what()));
    }
  }

  // now select one of the profiles, preferably the one that was selected before
  profileBox->blockSignals(false);

  if (selectProfile) {
    if (profileBox->count() > 1) {
      if (newProfile.isEmpty()) {
        profileBox->setCurrentText(currentProfileName);
      } else {
        profileBox->setCurrentText(newProfile);
      }
      if (profileBox->currentIndex() == 0) {
        profileBox->setCurrentIndex(1);
      }
    }
  }
  return profileBox->count() > 1;
}

void MainWindow::refreshExecutablesList()
{
  QAbstractItemModel* model = ui->executablesListBox->model();

  auto add = [&](const QString& title, const QFileInfo& binary) {
    QIcon icon;
    if (!binary.fileName().isEmpty()) {
      icon = iconForExecutable(binary.filePath());
    }

    ui->executablesListBox->addItem(icon, title);

    const auto i = ui->executablesListBox->count() - 1;

    model->setData(model->index(i, 0),
                   QSize(0, ui->executablesListBox->iconSize().height() + 4),
                   Qt::SizeHintRole);
  };

  ui->executablesListBox->setEnabled(false);
  ui->executablesListBox->clear();

  add(tr("<Edit...>"), {});

  for (const auto& exe : *m_OrganizerCore.executablesList()) {
    if (!exe.hide()) {
      add(exe.title(), exe.binaryInfo());
    }
  }

  if (ui->executablesListBox->count() == 1) {
    // all executables are hidden, add an empty one to at least be able to
    // switch to edit
    add(tr("(no executables)"), QFileInfo(":badfile"));
  }

  ui->executablesListBox->setCurrentIndex(1);
  ui->executablesListBox->setEnabled(true);
}

static bool BySortValue(const std::pair<UINT32, QTreeWidgetItem*>& LHS,
                        const std::pair<UINT32, QTreeWidgetItem*>& RHS)
{
  return LHS.first < RHS.first;
}

template <typename InputIterator>
static QStringList toStringList(InputIterator current, InputIterator end)
{
  QStringList result;
  for (; current != end; ++current) {
    result.append(*current);
  }
  return result;
}

void MainWindow::updateBSAList(const QStringList& defaultArchives,
                               const QStringList& activeArchives)
{
  m_DefaultArchives = defaultArchives;
  ui->bsaList->clear();
  ui->bsaList->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  std::vector<std::pair<UINT32, QTreeWidgetItem*>> items;

  auto invalidation = m_OrganizerCore.gameFeatures().gameFeature<BSAInvalidation>();
  std::vector<FileEntryPtr> files = m_OrganizerCore.directoryStructure()->getFiles();

  QStringList plugins =
      m_OrganizerCore.findFiles("", [](const QString& fileName) -> bool {
        return fileName.endsWith(".esp", Qt::CaseInsensitive) ||
               fileName.endsWith(".esm", Qt::CaseInsensitive) ||
               fileName.endsWith(".esl", Qt::CaseInsensitive);
      });

  auto hasAssociatedPlugin = [&](const QString& bsaName) -> bool {
    for (const QString& pluginName : plugins) {
      QFileInfo pluginInfo(pluginName);
      if (bsaName.startsWith(QFileInfo(pluginName).completeBaseName(),
                             Qt::CaseInsensitive) &&
          (m_OrganizerCore.pluginList()->state(pluginInfo.fileName()) ==
           IPluginList::STATE_ACTIVE)) {
        return true;
      }
    }
    return false;
  };

  for (FileEntryPtr current : files) {
    QFileInfo fileInfo(ToQString(current->getName()));

    if (fileInfo.suffix().toLower() == "bsa" || fileInfo.suffix().toLower() == "ba2") {
      int index = activeArchives.indexOf(fileInfo.fileName());
      if (index == -1) {
        index = 0xFFFF;
      } else {
        index += 2;
      }

      if ((invalidation != nullptr) &&
          invalidation->isInvalidationBSA(fileInfo.fileName())) {
        index = 1;
      }

      int originId = current->getOrigin();
      FilesOrigin& origin =
          m_OrganizerCore.directoryStructure()->getOriginByID(originId);

      QTreeWidgetItem* newItem = new QTreeWidgetItem(
          QStringList() << fileInfo.fileName() << ToQString(origin.getName()));
      newItem->setData(0, Qt::UserRole, index);
      newItem->setData(1, Qt::UserRole, originId);
      newItem->setFlags(newItem->flags() &
                        ~(Qt::ItemIsDropEnabled | Qt::ItemIsUserCheckable));
      newItem->setCheckState(0, (index != -1) ? Qt::Checked : Qt::Unchecked);
      newItem->setData(0, Qt::UserRole, false);
      if (m_OrganizerCore.settings().game().forceEnableCoreFiles() &&
          defaultArchives.contains(fileInfo.fileName())) {
        newItem->setCheckState(0, Qt::Checked);
        newItem->setDisabled(true);
        newItem->setData(0, Qt::UserRole, true);
      } else if (fileInfo.fileName().compare("update.bsa", Qt::CaseInsensitive) == 0) {
        newItem->setCheckState(0, Qt::Checked);
        newItem->setDisabled(true);
      } else if (hasAssociatedPlugin(fileInfo.fileName())) {
        newItem->setCheckState(0, Qt::Checked);
        newItem->setDisabled(true);
      } else {
        newItem->setCheckState(0, Qt::Unchecked);
        newItem->setDisabled(true);
      }
      if (index < 0)
        index = 0;

      UINT32 sortValue = ((origin.getPriority() & 0xFFFF) << 16) | (index & 0xFFFF);
      items.push_back(std::make_pair(sortValue, newItem));
    }
  }
  std::sort(items.begin(), items.end(), BySortValue);

  for (auto iter = items.begin(); iter != items.end(); ++iter) {
    int originID = iter->second->data(1, Qt::UserRole).toInt();

    const FilesOrigin& origin =
        m_OrganizerCore.directoryStructure()->getOriginByID(originID);

    QString modName;
    const unsigned int modIndex = ModInfo::getIndex(ToQString(origin.getName()));

    if (modIndex == UINT_MAX) {
      modName = UnmanagedModName();
    } else {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
      modName              = modInfo->name();
    }

    QList<QTreeWidgetItem*> items =
        ui->bsaList->findItems(modName, Qt::MatchFixedString);
    QTreeWidgetItem* subItem = nullptr;
    if (items.length() > 0) {
      subItem = items.at(0);
    } else {
      subItem = new QTreeWidgetItem(QStringList(modName));
      subItem->setFlags(subItem->flags() & ~Qt::ItemIsDragEnabled);
      ui->bsaList->addTopLevelItem(subItem);
    }
    subItem->addChild(iter->second);
    subItem->setExpanded(true);
  }
  checkBSAList();
}

void MainWindow::checkBSAList()
{
  auto archives = m_OrganizerCore.gameFeatures().gameFeature<DataArchives>();

  if (archives != nullptr) {
    ui->bsaList->blockSignals(true);
    ON_BLOCK_EXIT([&]() {
      ui->bsaList->blockSignals(false);
    });

    QStringList defaultArchives = archives->archives(m_OrganizerCore.currentProfile());

    bool warning = false;

    for (int i = 0; i < ui->bsaList->topLevelItemCount(); ++i) {
      bool modWarning         = false;
      QTreeWidgetItem* tlItem = ui->bsaList->topLevelItem(i);
      for (int j = 0; j < tlItem->childCount(); ++j) {
        QTreeWidgetItem* item = tlItem->child(j);
        QString filename      = item->text(0);
        item->setIcon(0, QIcon());
        item->setToolTip(0, QString());

        if (item->checkState(0) == Qt::Unchecked) {
          if (defaultArchives.contains(filename)) {
            item->setIcon(0, QIcon(":/MO/gui/warning"));
            item->setToolTip(
                0, tr("This bsa is enabled in the ini file so it may be required!"));
            modWarning = true;
          }
        }
      }
      if (modWarning) {
        ui->bsaList->expandItem(ui->bsaList->topLevelItem(i));
        warning = true;
      }
    }
    if (warning) {
      ui->tabWidget->setTabIcon(1, QIcon(":/MO/gui/warning"));
    } else {
      ui->tabWidget->setTabIcon(1, QIcon());
    }
  }
}

void MainWindow::saveModMetas()
{
  if (m_MetaSave.isFinished()) {
    m_MetaSave = QtConcurrent::run([this]() {
      for (unsigned int i = 0; i < ModInfo::getNumMods(); ++i) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
        modInfo->saveMeta();
      }
    });
  }
}

void MainWindow::fixCategories()
{
  for (unsigned int i = 0; i < ModInfo::getNumMods(); ++i) {
    ModInfo::Ptr modInfo     = ModInfo::getByIndex(i);
    std::set<int> categories = modInfo->getCategories();
    for (std::set<int>::iterator iter = categories.begin(); iter != categories.end();
         ++iter) {
      if (!m_CategoryFactory.categoryExists(*iter)) {
        modInfo->setCategory(*iter, false);
      }
    }
  }
}

void MainWindow::setupNetworkProxy(bool activate)
{
  QNetworkProxyFactory::setUseSystemConfiguration(activate);
}

void MainWindow::activateProxy(bool activate)
{
  QProgressDialog busyDialog(tr("Activating Network Proxy"), QString(), 0, 0,
                             parentWidget());
  busyDialog.setWindowFlags(busyDialog.windowFlags() &
                            ~Qt::WindowContextHelpButtonHint);
  busyDialog.setWindowModality(Qt::WindowModal);
  busyDialog.show();

  QFutureWatcher<void> futureWatcher;
  QEventLoop loop;
  connect(&futureWatcher, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit,
          Qt::QueuedConnection);

  futureWatcher.setFuture(QtConcurrent::run(MainWindow::setupNetworkProxy, activate));

  // wait for setupNetworkProxy while keeping ui responsive
  loop.exec();

  busyDialog.hide();
}

void MainWindow::readSettings()
{
  const auto& s = m_OrganizerCore.settings();

  if (!s.geometry().restoreGeometry(this)) {
    resize(1300, 800);
  }

  s.geometry().restoreState(this);
  s.geometry().restoreDocks(this);
  s.geometry().restoreToolbars(this);
  s.geometry().restoreState(ui->splitter);
  s.geometry().restoreState(ui->categoriesSplitter);
  s.geometry().restoreVisibility(ui->menuBar);
  s.geometry().restoreVisibility(ui->statusBar);

  FilterWidget::setOptions(s.interface().filterOptions());

  {
    // special case in case someone puts 0 in the INI
    auto v = s.widgets().index(ui->executablesListBox);
    if (!v || v == 0) {
      v = 1;
    }

    ui->executablesListBox->setCurrentIndex(*v);
  }

  s.widgets().restoreIndex(ui->tabWidget);

  ui->modList->restoreState(s);

  {
    s.geometry().restoreVisibility(ui->categoriesGroup, false);
    const auto v = ui->categoriesGroup->isVisible();
    setCategoryListVisible(v);
    ui->displayCategoriesBtn->setChecked(v);
  }

  if (s.network().useProxy()) {
    activateProxy(true);
  }
}

void MainWindow::processUpdates()
{
  auto& settings      = m_OrganizerCore.settings();
  const auto earliest = QVersionNumber::fromString("2.1.2").normalized();

  const auto lastVersion = settings.version().value_or(earliest);
  const auto currentVersion =
      QVersionNumber::fromString(m_OrganizerCore.getVersion().string()).normalized();

  m_LastVersion = lastVersion;

  settings.processUpdates(currentVersion, lastVersion);

  if (!settings.firstStart()) {
    if (lastVersion < QVersionNumber(2, 1, 6)) {
      ui->modList->header()->setSectionHidden(ModList::COL_NOTES, true);
    }

    if (lastVersion < QVersionNumber(2, 2, 1)) {
      // hide new columns by default
      for (int i = DownloadList::COL_MODNAME; i < DownloadList::COL_COUNT; ++i) {
        ui->downloadView->header()->hideSection(i);
      }
    }

    if (lastVersion < QVersionNumber(2, 3)) {
      for (int i = 1; i < ui->dataTree->header()->count(); ++i)
        ui->dataTree->setColumnWidth(i, 150);
    }
  }

  if (currentVersion < lastVersion) {
    const auto text =
        tr("Notice: Your current MO version (%1) is lower than the previously used one "
           "(%2). "
           "The GUI may not downgrade gracefully, so you may experience oddities. "
           "However, there should be no serious issues.")
            .arg(currentVersion.toString())
            .arg(lastVersion.toString());

    log::warn("{}", text);
  }
}

void MainWindow::storeSettings()
{
  auto& s = m_OrganizerCore.settings();

  s.geometry().saveState(this);
  s.geometry().saveGeometry(this);
  s.geometry().saveDocks(this);

  s.geometry().saveVisibility(ui->menuBar);
  s.geometry().saveVisibility(ui->statusBar);
  s.geometry().saveToolbars(this);
  s.geometry().saveState(ui->splitter);
  s.geometry().saveState(ui->categoriesSplitter);
  s.geometry().saveMainWindowMonitor(this);
  s.geometry().saveVisibility(ui->categoriesGroup);

  s.geometry().saveState(ui->espList->header());
  s.geometry().saveState(ui->downloadView->header());
  s.geometry().saveState(ui->savegameList->header());

  s.widgets().saveIndex(ui->executablesListBox);
  s.widgets().saveIndex(ui->tabWidget);

  m_DataTab->saveState(s);
  ui->modList->saveState(s);

  s.interface().setFilterOptions(FilterWidget::options());
}

QMainWindow* MainWindow::mainWindow()
{
  return this;
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
  QWidget* currentWidget = ui->tabWidget->widget(index);
  if (currentWidget == ui->espTab) {
    m_OrganizerCore.refreshESPList();
  } else if (currentWidget == ui->bsaTab) {
    m_OrganizerCore.refreshBSAList();
  } else if (currentWidget == ui->dataTab) {
    m_DataTab->activated();
  } else if (currentWidget == ui->savesTab) {
    m_SavesTab->refreshSaveList();
  }
}

void MainWindow::on_startButton_clicked()
{
  const Executable* selectedExecutable = getSelectedExecutable();
  if (!selectedExecutable) {
    return;
  }

  ui->startButton->setEnabled(false);
  Guard g([&] {
    ui->startButton->setEnabled(true);
  });

  m_OrganizerCore.processRunner()
      .setFromExecutable(*selectedExecutable)
      .setWaitForCompletion(ProcessRunner::TriggerRefresh)
      .run();
}

bool MainWindow::modifyExecutablesDialog(int selection)
{
  bool result = false;

  try {
    EditExecutablesDialog dialog(m_OrganizerCore, selection, this);

    result = (dialog.exec() == QDialog::Accepted);

    refreshExecutablesList();
    updatePinnedExecutables();
  } catch (const std::exception& e) {
    reportError(e.what());
  }

  return result;
}

void MainWindow::on_executablesListBox_currentIndexChanged(int index)
{
  if (!ui->executablesListBox->isEnabled()) {
    return;
  }

  const int previousIndex = (m_OldExecutableIndex > 0 ? m_OldExecutableIndex : 1);

  m_OldExecutableIndex = index;

  if (index == 0) {
    modifyExecutablesDialog(previousIndex - 1);
    const auto newCount = ui->executablesListBox->count();

    if (previousIndex >= 0 && previousIndex < newCount) {
      ui->executablesListBox->setCurrentIndex(previousIndex);
    } else {
      ui->executablesListBox->setCurrentIndex(newCount - 1);
    }
  }
}

void MainWindow::helpTriggered()
{
  QWhatsThis::enterWhatsThisMode();
}

void MainWindow::wikiTriggered()
{
  shell::Open(QUrl("https://modorganizer2.github.io/"));
}

void MainWindow::gameSupportTriggered()
{
  shell::Open(QUrl(m_OrganizerCore.managedGame()->getSupportURL()));
}

void MainWindow::discordTriggered()
{
  shell::Open(QUrl("https://discord.gg/ewUVAqyrQX"));
}

void MainWindow::issueTriggered()
{
  shell::Open(QUrl("https://github.com/Modorganizer2/modorganizer/issues"));
}

void MainWindow::tutorialTriggered()
{
  QAction* tutorialAction = qobject_cast<QAction*>(sender());
  if (tutorialAction != nullptr) {
    TutorialManager::instance().activateTutorial("MainWindow",
                                                 tutorialAction->data().toString());
  }
}

void MainWindow::on_actionInstallMod_triggered()
{
  ui->modList->actions().installMod();
}

void MainWindow::on_action_Refresh_triggered()
{
  refreshProfile_activated();
}

void MainWindow::on_actionAdd_Profile_triggered()
{
  for (;;) {
    ProfilesDialog profilesDialog(m_OrganizerCore.currentProfile()->name(),
                                  m_OrganizerCore, this);

    // workaround: need to disable monitoring of the saves directory, otherwise the
    // active profile directory is locked
    m_SavesTab->stopMonitorSaves();
    profilesDialog.exec();
    m_SavesTab->refreshSaveList();  // since the save list may now be outdated we have
                                    // to refresh it completely

    if (refreshProfiles(true, profilesDialog.selectedProfile().value_or("")) &&
        !profilesDialog.failed()) {
      break;
    }
  }

  auto saveGames = m_OrganizerCore.gameFeatures().gameFeature<LocalSavegames>();
  if (saveGames != nullptr) {
    if (saveGames->prepareProfile(m_OrganizerCore.currentProfile())) {
      m_SavesTab->refreshSaveList();
    }
  }

  auto invalidation = m_OrganizerCore.gameFeatures().gameFeature<BSAInvalidation>();
  if (invalidation != nullptr) {
    if (invalidation->prepareProfile(m_OrganizerCore.currentProfile())) {
      QTimer::singleShot(5, [this] {
        m_OrganizerCore.refresh();
      });
    }
  }
}

void MainWindow::on_actionModify_Executables_triggered()
{
  const auto sel = (m_OldExecutableIndex > 0 ? m_OldExecutableIndex - 1 : 0);

  if (modifyExecutablesDialog(sel)) {
    const auto newCount = ui->executablesListBox->count();
    if (m_OldExecutableIndex >= 0 && m_OldExecutableIndex < newCount) {
      ui->executablesListBox->setCurrentIndex(m_OldExecutableIndex);
    } else {
      ui->executablesListBox->setCurrentIndex(newCount - 1);
    }
  }
}

void MainWindow::refresherProgress(const DirectoryRefreshProgress* p)
{
  if (p->finished()) {
    setEnabled(true);
    ui->statusBar->setProgress(100);
  } else {
    setEnabled(false);
    ui->statusBar->setProgress(p->percentDone());
  }
}

void MainWindow::onDirectoryStructureChanged()
{
  // some problem-reports may rely on the virtual directory tree so they need to be
  // updated now
  scheduleCheckForProblems();
  m_DataTab->updateTree();
}

void MainWindow::modInstalled(const QString& modName)
{
  if (!m_OrganizerCore.settings().interface().checkUpdateAfterInstallation()) {
    return;
  }

  unsigned int index = ModInfo::getIndex(modName);

  if (index == UINT_MAX) {
    return;
  }

  // force an update to happen
  ui->modList->actions().checkModsForUpdates(
      {m_OrganizerCore.modList()->index(index, 0)});
}

void MainWindow::importCategories(bool)
{
  NexusInterface& nexus = NexusInterface::instance();
  nexus.setPluginContainer(&m_OrganizerCore.pluginContainer());
  nexus.requestGameInfo(Settings::instance().game().plugin()->gameShortName(), this,
                        QVariant(), QString());
}

void MainWindow::showMessage(const QString& message)
{
  MessageDialog::showMessage(message, this);
}

void MainWindow::showError(const QString& message)
{
  reportError(message);
}

void MainWindow::modRenamed(const QString& oldName, const QString& newName)
{
  Profile::renameModInAllProfiles(oldName, newName);

  // immediately refresh the active profile because the data in memory is invalid
  m_OrganizerCore.currentProfile()->refreshModStatus();

  // also fix the directory structure
  try {
    if (m_OrganizerCore.directoryStructure()->originExists(ToWString(oldName))) {
      FilesOrigin& origin =
          m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(oldName));
      origin.setName(ToWString(newName));
    } else {
    }
  } catch (const std::exception& e) {
    reportError(tr("failed to change origin name: %1").arg(e.what()));
  }
}

void MainWindow::fileMoved(const QString& filePath, const QString& oldOriginName,
                           const QString& newOriginName)
{
  const FileEntryPtr filePtr =
      m_OrganizerCore.directoryStructure()->findFile(ToWString(filePath));
  if (filePtr.get() != nullptr) {
    try {
      if (m_OrganizerCore.directoryStructure()->originExists(
              ToWString(newOriginName))) {
        FilesOrigin& newOrigin = m_OrganizerCore.directoryStructure()->getOriginByName(
            ToWString(newOriginName));

        QString fullNewPath = ToQString(newOrigin.getPath()) + "\\" + filePath;
        WIN32_FIND_DATAW findData;
        HANDLE hFind;
        hFind = ::FindFirstFileW(ToWString(fullNewPath).c_str(), &findData);
        filePtr->addOrigin(newOrigin.getID(), findData.ftCreationTime, L"", -1);
        FindClose(hFind);
      }
      if (m_OrganizerCore.directoryStructure()->originExists(
              ToWString(oldOriginName))) {
        FilesOrigin& oldOrigin = m_OrganizerCore.directoryStructure()->getOriginByName(
            ToWString(oldOriginName));
        filePtr->removeOrigin(oldOrigin.getID());
      }
    } catch (const std::exception& e) {
      reportError(tr("failed to move \"%1\" from mod \"%2\" to \"%3\": %4")
                      .arg(filePath)
                      .arg(oldOriginName)
                      .arg(newOriginName)
                      .arg(e.what()));
    }
  } else {
    // this is probably not an error, the specified path is likely a directory
  }
}

void MainWindow::modRemoved(const QString& fileName)
{
  if (!fileName.isEmpty() && !QFileInfo(fileName).isAbsolute()) {
    m_OrganizerCore.downloadManager()->markUninstalled(fileName);
  }
}

void MainWindow::windowTutorialFinished(const QString& windowName)
{
  m_OrganizerCore.settings().interface().setTutorialCompleted(windowName);
}

void MainWindow::displayModInformation(ModInfo::Ptr modInfo, unsigned int modIndex,
                                       ModInfoTabIDs tabID)
{
  ui->modList->actions().displayModInformation(modInfo, modIndex, tabID);
}

bool MainWindow::closeWindow()
{
  return close();
}

void MainWindow::setWindowEnabled(bool enabled)
{
  setEnabled(enabled);
}

void MainWindow::refreshProfile_activated()
{
  m_OrganizerCore.refresh();
}

void MainWindow::saveArchiveList()
{
  if (m_OrganizerCore.isArchivesInit()) {
    SafeWriteFile archiveFile(m_OrganizerCore.currentProfile()->getArchivesFileName());
    for (int i = 0; i < ui->bsaList->topLevelItemCount(); ++i) {
      QTreeWidgetItem* tlItem = ui->bsaList->topLevelItem(i);
      for (int j = 0; j < tlItem->childCount(); ++j) {
        QTreeWidgetItem* item = tlItem->child(j);
        if (item->checkState(0) == Qt::Checked) {
          archiveFile->write(item->text(0).toUtf8().append("\r\n"));
        }
      }
    }
    archiveFile.commitIfDifferent(m_ArchiveListHash);
  } else {
    log::debug("archive list not initialised");
  }
}

void MainWindow::openInstanceFolder()
{
  QString dataPath = qApp->property("dataPath").toString();
  shell::Explore(dataPath);
}

void MainWindow::openInstallFolder()
{
  shell::Explore(qApp->applicationDirPath());
}

void MainWindow::openPluginsFolder()
{
  QString pluginsPath =
      QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::pluginPath());
  shell::Explore(pluginsPath);
}

void MainWindow::openStylesheetsFolder()
{
  QString ssPath = QCoreApplication::applicationDirPath() + "/" +
                   ToQString(AppConfig::stylesheetsPath());
  shell::Explore(ssPath);
}

void MainWindow::openProfileFolder()
{
  shell::Explore(m_OrganizerCore.currentProfile()->absolutePath());
}

void MainWindow::openIniFolder()
{
  if (m_OrganizerCore.currentProfile()->localSettingsEnabled()) {
    shell::Explore(m_OrganizerCore.currentProfile()->absolutePath());
  } else {
    shell::Explore(m_OrganizerCore.managedGame()->documentsDirectory());
  }
}

void MainWindow::openDownloadsFolder()
{
  shell::Explore(m_OrganizerCore.settings().paths().downloads());
}

void MainWindow::openModsFolder()
{
  shell::Explore(m_OrganizerCore.settings().paths().mods());
}

void MainWindow::openGameFolder()
{
  shell::Explore(m_OrganizerCore.managedGame()->gameDirectory());
}

void MainWindow::openMyGamesFolder()
{
  shell::Explore(m_OrganizerCore.managedGame()->documentsDirectory());
}

QMenu* MainWindow::openFolderMenu()
{
  QMenu* FolderMenu = new QMenu(this);

  // game folders that are not necessarily MO-specific
  FolderMenu->addAction(tr("Open Game folder"), this, SLOT(openGameFolder()));
  FolderMenu->addAction(tr("Open MyGames folder"), this, SLOT(openMyGamesFolder()));
  FolderMenu->addAction(tr("Open INIs folder"), this, SLOT(openIniFolder()));

  FolderMenu->addSeparator();

  // MO-specific folders that are related to modding the game
  FolderMenu->addAction(tr("Open Instance folder"), this, SLOT(openInstanceFolder()));
  FolderMenu->addAction(tr("Open Mods folder"), this, SLOT(openModsFolder()));
  FolderMenu->addAction(tr("Open Profile folder"), this, SLOT(openProfileFolder()));
  FolderMenu->addAction(tr("Open Downloads folder"), this, SLOT(openDownloadsFolder()));

  FolderMenu->addSeparator();

  // MO-specific folders that are not directly related to modding and are either
  // in the installation folder or the instance
  FolderMenu->addAction(tr("Open MO2 Install folder"), this, SLOT(openInstallFolder()));
  FolderMenu->addAction(tr("Open MO2 Plugins folder"), this, SLOT(openPluginsFolder()));
  FolderMenu->addAction(tr("Open MO2 Stylesheets folder"), this,
                        SLOT(openStylesheetsFolder()));
  FolderMenu->addAction(tr("Open MO2 Logs folder"), [=] {
    ui->logList->openLogsFolder();
  });

  return FolderMenu;
}

void MainWindow::linkToolbar()
{
  Executable* exe = getSelectedExecutable();
  if (!exe) {
    return;
  }

  exe->setShownOnToolbar(!exe->isShownOnToolbar());
  updatePinnedExecutables();
}

void MainWindow::linkDesktop()
{
  if (auto* exe = getSelectedExecutable()) {
    env::Shortcut(*exe).toggle(env::Shortcut::Desktop);
  }
}

void MainWindow::linkMenu()
{
  if (auto* exe = getSelectedExecutable()) {
    env::Shortcut(*exe).toggle(env::Shortcut::StartMenu);
  }
}

void MainWindow::on_linkButton_pressed()
{
  const Executable* exe = getSelectedExecutable();
  if (!exe) {
    return;
  }

  const QIcon addIcon(":/MO/gui/link");
  const QIcon removeIcon(":/MO/gui/remove");

  env::Shortcut shortcut(*exe);

  m_LinkToolbar->setIcon(exe->isShownOnToolbar() ? removeIcon : addIcon);

  m_LinkDesktop->setIcon(shortcut.exists(env::Shortcut::Desktop) ? removeIcon
                                                                 : addIcon);

  m_LinkStartMenu->setIcon(shortcut.exists(env::Shortcut::StartMenu) ? removeIcon
                                                                     : addIcon);
}

void MainWindow::on_actionSettings_triggered()
{
  Settings& settings = m_OrganizerCore.settings();

  QString oldModDirectory(settings.paths().mods());
  QString oldCacheDirectory(settings.paths().cache());
  QString oldProfilesDirectory(settings.paths().profiles());
  QString oldManagedGameDirectory(settings.game().directory().value_or(""));
  bool oldDisplayForeign(settings.interface().displayForeign());
  bool oldArchiveParsing(settings.archiveParsing());
  bool proxy                    = settings.network().useProxy();
  DownloadManager* dlManager    = m_OrganizerCore.downloadManager();
  const bool oldCheckForUpdates = settings.checkForUpdates();
  const int oldMaxDumps         = settings.diagnostics().maxCoreDumps();

  SettingsDialog dialog(&m_PluginContainer, m_ThemeManager, m_TranslationManager,
                        settings, this);
  dialog.exec();

  auto e = dialog.exitNeeded();

  if (oldManagedGameDirectory != settings.game().directory()) {
    e |= Exit::Restart;
  }

  if (e.testFlag(Exit::Restart)) {
    const auto r =
        MOBase::TaskDialog(this)
            .title(tr("Restart Mod Organizer"))
            .main("Restart Mod Organizer")
            .content(tr("Mod Organizer must restart to finish configuration changes"))
            .icon(QMessageBox::Question)
            .button({tr("Restart"), QMessageBox::Yes})
            .button(
                {tr("Continue"), tr("Some things might be weird."), QMessageBox::No})
            .exec();

    if (r == QMessageBox::Yes) {
      ExitModOrganizer(e);
    }
  }

  InstallationManager* instManager = m_OrganizerCore.installationManager();
  instManager->setModsDirectory(settings.paths().mods());
  instManager->setDownloadDirectory(settings.paths().downloads());

  // Schedule a problem check since diagnose plugins may have been enabled / disabled.
  scheduleCheckForProblems();

  fixCategories();
  ui->modList->refreshFilters();
  ui->modList->refresh();

  m_OrganizerCore.refreshLists();

  updateSortButton();

  if (settings.paths().profiles() != oldProfilesDirectory) {
    refreshProfiles();
  }

  if (dlManager->getOutputDirectory() != settings.paths().downloads()) {
    if (dlManager->downloadsInProgress()) {
      MessageDialog::showMessage(tr("Can't change download directory while "
                                    "downloads are in progress!"),
                                 this);
    } else {
      dlManager->setOutputDirectory(settings.paths().downloads());
    }
  }

  if ((settings.paths().mods() != oldModDirectory) ||
      (settings.interface().displayForeign() != oldDisplayForeign)) {
    m_OrganizerCore.refresh();
  }

  const auto state = settings.archiveParsing();
  if (state != oldArchiveParsing) {
    if (!state) {
      ui->dataTabShowFromArchives->setCheckState(Qt::Unchecked);
      ui->dataTabShowFromArchives->setEnabled(false);
    } else {
      ui->dataTabShowFromArchives->setCheckState(Qt::Checked);
      ui->dataTabShowFromArchives->setEnabled(true);
    }
    m_OrganizerCore.refresh();
  }

  if (settings.paths().cache() != oldCacheDirectory) {
    NexusInterface::instance().setCacheDirectory(settings.paths().cache());
  }

  if (proxy != settings.network().useProxy()) {
    activateProxy(settings.network().useProxy());
  }

  ui->statusBar->checkSettings(m_OrganizerCore.settings());
  m_DownloadsTab->update();

  m_OrganizerCore.setLogLevel(settings.diagnostics().logLevel());

  if (settings.diagnostics().maxCoreDumps() != oldMaxDumps) {
    m_OrganizerCore.cycleDiagnostics();
  }

  toggleMO2EndorseState();

  if (oldCheckForUpdates != settings.checkForUpdates()) {
    toggleUpdateAction();

    if (settings.checkForUpdates()) {
      m_OrganizerCore.checkForUpdates();
    }
  }
}

void MainWindow::onPluginRegistrationChanged()
{
  updateModPageMenu();
  scheduleCheckForProblems();
  m_DownloadsTab->update();
}

void MainWindow::refreshNexusCategories(CategoriesDialog* dialog)
{
  NexusInterface& nexus = NexusInterface::instance();
  nexus.setPluginContainer(&m_PluginContainer);
  if (!Settings::instance().game().plugin()->primarySources().isEmpty()) {
    nexus.requestGameInfo(
        Settings::instance().game().plugin()->primarySources().first(), dialog,
        QVariant(), QString());
  } else {
    nexus.requestGameInfo(Settings::instance().game().plugin()->gameShortName(), dialog,
                          QVariant(), QString());
  }
}

void MainWindow::categoriesSaved()
{
  for (auto modName : m_OrganizerCore.modList()->allMods()) {
    auto mod = ModInfo::getByName(modName);
    for (auto category : mod->getCategories()) {
      if (!m_CategoryFactory.categoryExists(category))
        mod->setCategory(category, false);
    }
  }
}

void MainWindow::on_actionNexus_triggered()
{
  const IPluginGame* game = m_OrganizerCore.managedGame();
  QString gameName        = game->gameShortName();
  if (game->gameNexusName().isEmpty() && game->primarySources().count())
    gameName = game->primarySources()[0];
  shell::Open(QUrl(NexusInterface::instance().getGameURL(gameName)));
}

void MainWindow::onLanguageChanged(const QString& newLanguage)
{
  m_TranslationManager.load(newLanguage.toStdString());

  ui->retranslateUi(this);
  log::debug("loaded language {}", newLanguage);

  ui->profileBox->setItemText(0, QObject::tr("<Manage...>"));

  createHelpMenu();

  if (m_DownloadsTab) {
    m_DownloadsTab->update();
  }

  ui->listOptionsBtn->setMenu(
      new ModListGlobalContextMenu(m_OrganizerCore, ui->modList, this));
  ui->openFolderMenu->setMenu(openFolderMenu());
}

void MainWindow::originModified(int originID)
{
  FilesOrigin& origin = m_OrganizerCore.directoryStructure()->getOriginByID(originID);
  origin.enable(false);

  DirectoryStats dummy;
  m_OrganizerCore.directoryStructure()->addFromOrigin(
      origin.getName(), origin.getPath(), origin.getPriority(), dummy);

  DirectoryRefresher::cleanStructure(m_OrganizerCore.directoryStructure());
}

void MainWindow::updateAvailable()
{
  ui->actionUpdate->setEnabled(true);
  ui->actionUpdate->setToolTip(tr("Update available"));
  ui->statusBar->setUpdateAvailable(true);
}

void MainWindow::motdReceived(const QString& motd)
{
  // don't show motd after 5 seconds, may be annoying. Hopefully the user's
  // internet connection is faster next time
  if (m_StartTime.secsTo(QTime::currentTime()) < 5) {
    unsigned int hash = static_cast<unsigned int>(qHash(motd));
    if (hash != m_OrganizerCore.settings().motdHash()) {
      MotDDialog dialog(motd);
      dialog.exec();
      m_OrganizerCore.settings().setMotdHash(hash);
    }
  }
}

void MainWindow::on_actionUpdate_triggered()
{
  m_OrganizerCore.startMOUpdate();
}

void MainWindow::on_actionExit_triggered()
{
  ExitModOrganizer();
}

void MainWindow::actionEndorseMO()
{
  // Normally this would be the managed game but MO2 is only uploaded to the Skyrim SE
  // site right now
  IPluginGame* game = m_OrganizerCore.getGame("skyrimse");
  if (!game)
    return;

  if (QMessageBox::question(
          this, tr("Endorse Mod Organizer"),
          tr("Do you want to endorse Mod Organizer on %1 now?")
              .arg(NexusInterface::instance().getGameURL(game->gameShortName())),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    NexusInterface::instance().requestToggleEndorsement(
        game->gameShortName(), game->nexusModOrganizerID(),
        m_OrganizerCore.getVersion().string(), true, this, QVariant(), QString());
  }
}

void MainWindow::actionWontEndorseMO()
{
  // Normally this would be the managed game but MO2 is only uploaded to the Skyrim SE
  // site right now
  IPluginGame* game = m_OrganizerCore.getGame("skyrimse");
  if (!game)
    return;

  if (QMessageBox::question(
          this, tr("Abstain from Endorsing Mod Organizer"),
          tr("Are you sure you want to abstain from endorsing Mod Organizer 2?\n"
             "You will have to visit the mod page on the %1 Nexus site to change your "
             "mind.")
              .arg(NexusInterface::instance().getGameURL(game->gameShortName())),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    NexusInterface::instance().requestToggleEndorsement(
        game->gameShortName(), game->nexusModOrganizerID(),
        m_OrganizerCore.getVersion().string(), false, this, QVariant(), QString());
  }
}

void MainWindow::toggleMO2EndorseState()
{
  const auto& s = m_OrganizerCore.settings();

  if (!s.nexus().endorsementIntegration()) {
    ui->actionEndorseMO->setVisible(false);
    return;
  }

  ui->actionEndorseMO->setVisible(true);

  bool enabled = false;
  QString text;

  switch (s.nexus().endorsementState()) {
  case EndorsementState::Accepted: {
    text = tr("Thank you for endorsing MO2! :)");
    break;
  }

  case EndorsementState::Refused: {
    text = tr("Please reconsider endorsing MO2 on Nexus!");
    break;
  }

  case EndorsementState::NoDecision: {
    enabled = true;
    break;
  }
  }

  ui->actionEndorseMO->menu()->setEnabled(enabled);
  ui->actionEndorseMO->setToolTip(text);
  ui->actionEndorseMO->setStatusTip(text);
}

void MainWindow::toggleUpdateAction()
{
  const auto& s = m_OrganizerCore.settings();
  ui->actionUpdate->setVisible(s.checkForUpdates());
}

void MainWindow::updateSortButton()
{
  if (m_OrganizerCore.managedGame()->sortMechanism() !=
      IPluginGame::SortMechanism::NONE) {
    ui->sortButton->setEnabled(true);
    ui->sortButton->setToolTip(tr("Sort the plugins using LOOT."));
  } else {
    ui->sortButton->setDisabled(true);
    ui->sortButton->setToolTip(tr("There is no supported sort mechanism for this game. "
                                  "You will probably have to use a third-party tool."));
  }
}

void MainWindow::nxmEndorsementsAvailable(QVariant userData, QVariant resultData, int)
{
  QVariantList data = resultData.toList();
  std::multimap<QString, std::pair<int, QString>> sorted;
  QStringList games = m_OrganizerCore.managedGame()->validShortNames();
  games += m_OrganizerCore.managedGame()->gameShortName();
  bool searchedMO2NexusGame = false;
  for (auto endorsementData : data) {
    QVariantMap endorsement      = endorsementData.toMap();
    std::pair<int, QString> data = std::make_pair<int, QString>(
        endorsement["mod_id"].toInt(), endorsement["status"].toString());
    sorted.insert(std::pair<QString, std::pair<int, QString>>(
        endorsement["domain_name"].toString(), data));
  }
  for (auto game : games) {
    IPluginGame* gamePlugin = m_OrganizerCore.getGame(game);
    if (gamePlugin != nullptr &&
        gamePlugin->gameShortName().compare("SkyrimSE", Qt::CaseInsensitive) == 0)
      searchedMO2NexusGame = true;
    auto iter = sorted.equal_range(gamePlugin->gameNexusName());
    for (auto result = iter.first; result != iter.second; ++result) {
      std::vector<ModInfo::Ptr> modsList =
          ModInfo::getByModID(result->first, result->second.first);

      for (auto mod : modsList) {
        if (result->second.second == "Endorsed")
          mod->setIsEndorsed(true);
        else if (result->second.second == "Abstained")
          mod->setNeverEndorse();
        else
          mod->setIsEndorsed(false);
      }

      if (Settings::instance().nexus().endorsementIntegration()) {
        if (result->first == "skyrimspecialedition" &&
            result->second.first == gamePlugin->nexusModOrganizerID()) {
          m_OrganizerCore.settings().nexus().setEndorsementState(
              endorsementStateFromString(result->second.second));

          toggleMO2EndorseState();
        }
      }
    }
  }

  if (!searchedMO2NexusGame && Settings::instance().nexus().endorsementIntegration()) {
    auto gamePlugin = m_OrganizerCore.getGame("SkyrimSE");
    if (gamePlugin) {
      auto iter = sorted.equal_range(gamePlugin->gameNexusName());
      for (auto result = iter.first; result != iter.second; ++result) {
        if (result->second.first == gamePlugin->nexusModOrganizerID()) {
          m_OrganizerCore.settings().nexus().setEndorsementState(
              endorsementStateFromString(result->second.second));

          toggleMO2EndorseState();
          break;
        }
      }
    }
  }
}

void MainWindow::nxmUpdateInfoAvailable(QString gameName, QVariant userData,
                                        QVariant resultData, int)
{
  QString gameNameReal;
  for (IPluginGame* game : m_PluginContainer.plugins<IPluginGame>()) {
    if (game->gameNexusName() == gameName) {
      gameNameReal = game->gameShortName();
      break;
    }
  }
  QVariantList resultList = resultData.toList();

  auto* watcher = new QFutureWatcher<NxmUpdateInfoData>();
  QObject::connect(watcher, &QFutureWatcher<NxmUpdateInfoData>::finished,
                   [this, watcher]() {
                     finishUpdateInfo(watcher->result());
                     watcher->deleteLater();
                   });
  auto future = QtConcurrent::run([=]() {
    return NxmUpdateInfoData{
        gameNameReal,
        ModInfo::filteredMods(gameNameReal, resultList, userData.toBool(), true)};
  });
  watcher->setFuture(future);
  ui->modList->invalidateFilter();
}

void MainWindow::finishUpdateInfo(const NxmUpdateInfoData& data)
{
  if (data.finalMods.empty()) {
    log::info("{}", tr("None of your %1 mods appear to have had recent file updates.")
                        .arg(data.game));
  }

  std::set<std::pair<QString, int>> organizedGames;
  for (auto& mod : data.finalMods) {
    if (mod->canBeUpdated()) {
      organizedGames.insert(
          std::make_pair<QString, int>(mod->gameName().toLower(), mod->nexusId()));
    }
  }

  if (!data.finalMods.empty() && organizedGames.empty())
    log::warn("{}", tr("All of your mods have been checked recently. We restrict "
                       "update checks to help preserve your available API requests."));

  for (const auto& game : organizedGames) {
    NexusInterface::instance().requestUpdates(game.second, this, QVariant(), game.first,
                                              QString());
  }
}

void MainWindow::nxmUpdatesAvailable(QString gameName, int modID, QVariant userData,
                                     QVariant resultData, int requestID)
{
  QVariantMap resultInfo = resultData.toMap();
  QList files            = resultInfo["files"].toList();
  QList fileUpdates      = resultInfo["file_updates"].toList();
  QString gameNameReal;

  for (IPluginGame* game : m_PluginContainer.plugins<IPluginGame>()) {
    if (game->gameNexusName() == gameName) {
      gameNameReal = game->gameShortName();
      break;
    }
  }

  std::vector<ModInfo::Ptr> modsList = ModInfo::getByModID(gameNameReal, modID);

  bool requiresInfo = false;

  for (auto mod : modsList) {
    QString validNewVersion;
    int newModStatus      = -1;
    QString installedFile = QFileInfo(mod->installationFile()).fileName();

    if (!installedFile.isEmpty()) {
      QVariantMap foundFileData;

      // update the file status
      for (auto& file : files) {
        QVariantMap fileData = file.toMap();

        if (fileData["file_name"].toString().compare(installedFile,
                                                     Qt::CaseInsensitive) == 0) {
          foundFileData = fileData;
          newModStatus  = foundFileData["category_id"].toInt();

          if (newModStatus != NexusInterface::FileStatus::OLD_VERSION &&
              newModStatus != NexusInterface::FileStatus::REMOVED &&
              newModStatus != NexusInterface::FileStatus::ARCHIVED) {

            // since the file is still active if there are no updates for it, use this
            // as current version
            validNewVersion = foundFileData["version"].toString();
          }
          break;
        }
      }

      if (foundFileData.isEmpty()) {
        // The file was not listed, the file is likely archived and archived files are
        // being hidden on the mod
        newModStatus = NexusInterface::FileStatus::ARCHIVED_HIDDEN;
      }

      // look for updates of the file
      int currentUpdateId = -1;

      // find installed file ID from the updates list since old filenames are not
      // guaranteed to be unique
      for (auto& updateEntry : fileUpdates) {
        const QVariantMap& updateData = updateEntry.toMap();

        if (installedFile.compare(updateData["old_file_name"].toString(),
                                  Qt::CaseInsensitive) == 0) {
          currentUpdateId = updateData["old_file_id"].toInt();
          break;
        }
      }

      bool foundActiveUpdate = false;

      // there is at least one update
      if (currentUpdateId > 0) {
        bool lookForMoreUpdates = true;

        // follow the update chain until there are no more updates
        while (lookForMoreUpdates) {
          lookForMoreUpdates = false;

          for (auto& updateEntry : fileUpdates) {
            const QVariantMap& updateData = updateEntry.toMap();

            if (currentUpdateId == updateData["old_file_id"].toInt()) {
              currentUpdateId = updateData["new_file_id"].toInt();

              // check if the new file is still active
              for (auto& file : files) {
                const QVariantMap& fileData = file.toMap();

                if (currentUpdateId == fileData["file_id"].toInt()) {
                  int updateStatus = fileData["category_id"].toInt();

                  if (updateStatus != NexusInterface::FileStatus::OLD_VERSION &&
                      updateStatus != NexusInterface::FileStatus::REMOVED &&
                      updateStatus != NexusInterface::FileStatus::ARCHIVED) {

                    // new version is active, so record it
                    validNewVersion   = fileData["version"].toString();
                    foundActiveUpdate = true;
                  }
                  break;
                }
              }

              lookForMoreUpdates = true;
              break;
            }
          }
        }
      }

      // if there were no active direct file updates for the installedFile
      if (!foundActiveUpdate) {
        // get the global mod version in case the file isn't an optional
        if (newModStatus != NexusInterface::FileStatus::OPTIONAL_FILE &&
            newModStatus != NexusInterface::FileStatus::MISCELLANEOUS) {
          requiresInfo = true;
        }
      }
    } else {
      // No installedFile means we don't know what to look at for a version so
      // just get the global mod version
      requiresInfo = true;
    }

    if (newModStatus > 0) {
      mod->setNexusFileStatus(newModStatus);
    }

    if (!validNewVersion.isEmpty()) {
      mod->setNewestVersion(validNewVersion);
      mod->setLastNexusUpdate(QDateTime::currentDateTimeUtc());
    }
  }

  // invalidate the filter to display mods with an update
  ui->modList->invalidateFilter();

  if (requiresInfo) {
    NexusInterface::instance().requestModInfo(gameNameReal, modID, this, QVariant(),
                                              QString());
  }
}

void MainWindow::nxmModInfoAvailable(QString gameName, int modID, QVariant userData,
                                     QVariant resultData, int requestID)
{
  QVariantMap result = resultData.toMap();
  QString gameNameReal;
  bool foundUpdate = false;

  for (IPluginGame* game : m_PluginContainer.plugins<IPluginGame>()) {
    if (game->gameNexusName() == gameName) {
      gameNameReal = game->gameShortName();
      break;
    }
  }

  std::vector<ModInfo::Ptr> modsList = ModInfo::getByModID(gameNameReal, modID);

  for (auto mod : modsList) {
    QDateTime now          = QDateTime::currentDateTimeUtc();
    QDateTime updateTarget = mod->getExpires();

    // if file is still listed as optional or miscellaneous don't update the version as
    // often optional files are left with an older version than the main mod version.
    if (!result["version"].toString().isEmpty() &&
        mod->getNexusFileStatus() != NexusInterface::FileStatus::OPTIONAL_FILE &&
        mod->getNexusFileStatus() != NexusInterface::FileStatus::MISCELLANEOUS) {

      mod->setNewestVersion(result["version"].toString());
      foundUpdate = true;
    }

    // update the LastNexusUpdate time in any case since we did perform the check.
    mod->setLastNexusUpdate(QDateTime::currentDateTimeUtc());

    mod->setNexusDescription(result["description"].toString());

    mod->setNexusCategory(result["category_id"].toInt());

    if ((mod->endorsedState() != EndorsedState::ENDORSED_NEVER) &&
        (result.contains("endorsement"))) {
      QVariantMap endorsement   = result["endorsement"].toMap();
      QString endorsementStatus = endorsement["endorse_status"].toString();

      if (endorsementStatus.compare("Endorsed") == 00)
        mod->setIsEndorsed(true);
      else if (endorsementStatus.compare("Abstained") == 00)
        mod->setNeverEndorse();
      else
        mod->setIsEndorsed(false);
    }

    mod->setLastNexusQuery(QDateTime::currentDateTimeUtc());
    mod->setNexusLastModified(
        QDateTime::fromSecsSinceEpoch(result["updated_timestamp"].toInt(), Qt::UTC));

    m_OrganizerCore.modList()->notifyChange(ModInfo::getIndex(mod->name()));
  }

  if (foundUpdate) {
    // invalidate the filter to display mods with an update
    ui->modList->invalidateFilter();
  }
}

void MainWindow::nxmEndorsementToggled(QString, int, QVariant, QVariant resultData, int)
{
  const QMap results = resultData.toMap();

  auto itor = results.find("status");
  if (itor == results.end()) {
    log::error("endorsement response has no status");
    return;
  }

  const auto s = endorsementStateFromString(itor->toString());

  switch (s) {
  case EndorsementState::Accepted: {
    QMessageBox::information(this, tr("Thank you!"),
                             tr("Thank you for your endorsement!"));
    break;
  }

  case EndorsementState::Refused: {
    // don't spam message boxes if the user doesn't want to endorse
    log::info(
        "Mod Organizer will not be endorsed and will no longer ask you to endorse.");
    break;
  }

  case EndorsementState::NoDecision: {
    log::error("bad status '{}' in endorsement response", itor->toString());
    return;
  }
  }

  m_OrganizerCore.settings().nexus().setEndorsementState(s);
  toggleMO2EndorseState();

  if (!disconnect(sender(),
                  SIGNAL(nxmEndorsementToggled(QString, int, QVariant, QVariant, int)),
                  this,
                  SLOT(nxmEndorsementToggled(QString, int, QVariant, QVariant, int)))) {
    log::error("failed to disconnect endorsement slot");
  }
}

void MainWindow::nxmTrackedModsAvailable(QVariant userData, QVariant resultData, int)
{
  QMap<QString, QString> gameNames;
  for (auto game : m_PluginContainer.plugins<IPluginGame>()) {
    gameNames[game->gameNexusName()] = game->gameShortName();
  }

  for (unsigned int i = 0; i < ModInfo::getNumMods(); i++) {
    auto modInfo = ModInfo::getByIndex(i);
    if (modInfo->nexusId() <= 0)
      continue;

    bool found       = false;
    auto resultsList = resultData.toList();
    for (auto item : resultsList) {
      auto results = item.toMap();
      if ((gameNames[results["domain_name"].toString()].compare(
               modInfo->gameName(), Qt::CaseInsensitive) == 0) &&
          (results["mod_id"].toInt() == modInfo->nexusId())) {
        found = true;
        break;
      }
    }

    modInfo->setIsTracked(found);
    modInfo->saveMeta();
  }
}

void MainWindow::nxmDownloadURLs(QString, int, int, QVariant, QVariant resultData, int)
{
  auto servers = m_OrganizerCore.settings().network().servers();

  for (const QVariant& var : resultData.toList()) {
    const QVariantMap map = var.toMap();

    const auto name = map["short_name"].toString();
    const auto isPremium =
        map["name"].toString().contains("Premium", Qt::CaseInsensitive);
    const auto isCDN =
        map["short_name"].toString().contains("CDN", Qt::CaseInsensitive);

    bool found = false;

    for (auto& server : servers) {
      if (server.name() == name) {
        // already exists, update
        server.setPremium(isPremium);
        server.updateLastSeen();
        found = true;
        break;
      }
    }

    if (!found) {
      // new server
      ServerInfo server(name, isPremium, QDate::currentDate(), isCDN ? 1 : 0, {});
      servers.add(std::move(server));
    }
  }

  m_OrganizerCore.settings().network().updateServers(servers);
}

void MainWindow::nxmGameInfoAvailable(QString gameName, QVariant, QVariant resultData,
                                      int)
{
  QVariantMap result          = resultData.toMap();
  QVariantList categories     = result["categories"].toList();
  CategoryFactory& catFactory = CategoryFactory::instance();
  catFactory.reset();
  for (auto category : categories) {
    auto catMap = category.toMap();
    std::vector<CategoryFactory::NexusCategory> nexusCat;
    nexusCat.push_back(CategoryFactory::NexusCategory(catMap["name"].toString(),
                                                      catMap["category_id"].toInt()));
    catFactory.addCategory(catMap["name"].toString(), nexusCat, 0);
  }
}

void MainWindow::nxmRequestFailed(QString gameName, int modID, int, QVariant, int,
                                  int errorCode, const QString& errorString)
{
  if (errorCode == QNetworkReply::ContentAccessDenied ||
      errorCode == QNetworkReply::ContentNotFoundError) {
    log::debug("{}",
               tr("Mod ID %1 no longer seems to be available on Nexus.").arg(modID));

    // update last checked timestamp on orphaned mods as well to avoid repeating
    // requests
    QString gameNameReal;
    for (IPluginGame* game : m_PluginContainer.plugins<IPluginGame>()) {
      if (game->gameNexusName() == gameName) {
        gameNameReal = game->gameShortName();
        break;
      }
    }
    auto orphanedMods = ModInfo::getByModID(gameNameReal, modID);
    for (auto mod : orphanedMods) {
      mod->setLastNexusUpdate(QDateTime::currentDateTimeUtc());
      mod->setLastNexusQuery(QDateTime::currentDateTimeUtc());
    }
  } else {
    MessageDialog::showMessage(
        tr("Error %1: Request to Nexus failed: %2").arg(errorCode).arg(errorString),
        this);
  }
}

BSA::EErrorCode MainWindow::extractBSA(BSA::Archive& archive, BSA::Folder::Ptr folder,
                                       const QString& destination,
                                       QProgressDialog& progress)
{
  QDir().mkdir(destination);
  BSA::EErrorCode result = BSA::ERROR_NONE;
  QString errorFile;

  for (unsigned int i = 0; i < folder->getNumFiles(); ++i) {
    BSA::File::Ptr file = folder->getFile(i);
    BSA::EErrorCode res = archive.extract(file, qUtf8Printable(destination));
    if (res != BSA::ERROR_NONE) {
      reportError(tr("failed to read %1: %2").arg(file->getName().c_str()).arg(res));
      result = res;
    }
    progress.setLabelText(file->getName().c_str());
    progress.setValue(progress.value() + 1);
    QCoreApplication::processEvents();
    if (progress.wasCanceled()) {
      result = BSA::ERROR_CANCELED;
    }
  }

  if (result != BSA::ERROR_NONE) {
    if (QMessageBox::critical(
            this, tr("Error"),
            tr("failed to extract %1 (errorcode %2)").arg(errorFile).arg(result),
            QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
      return result;
    }
  }

  for (unsigned int i = 0; i < folder->getNumSubFolders(); ++i) {
    BSA::Folder::Ptr subFolder = folder->getSubFolder(i);
    BSA::EErrorCode res        = extractBSA(
        archive, subFolder,
        destination.mid(0).append("/").append(subFolder->getName().c_str()), progress);
    if (res != BSA::ERROR_NONE) {
      return res;
    }
  }
  return BSA::ERROR_NONE;
}

bool MainWindow::extractProgress(QProgressDialog& progress, int percentage,
                                 std::string fileName)
{
  progress.setLabelText(fileName.c_str());
  progress.setValue(percentage);
  QCoreApplication::processEvents();
  return !progress.wasCanceled();
}

void MainWindow::extractBSATriggered(QTreeWidgetItem* item)
{
  using namespace boost::placeholders;

  QString origin;

  QString targetFolder =
      FileDialogMemory::getExistingDirectory("extractBSA", this, tr("Extract BSA"));
  QStringList archives = {};
  if (!targetFolder.isEmpty()) {
    if (!item->parent()) {
      for (int i = 0; i < item->childCount(); ++i) {
        archives.append(item->child(i)->text(0));
      }
      origin = QDir::fromNativeSeparators(
          ToQString(m_OrganizerCore.directoryStructure()
                        ->getOriginByName(ToWString(item->text(0)))
                        .getPath()));
    } else {
      origin = QDir::fromNativeSeparators(
          ToQString(m_OrganizerCore.directoryStructure()
                        ->getOriginByName(ToWString(item->text(1)))
                        .getPath()));
      archives = QStringList({item->text(0)});
    }

    for (auto archiveName : archives) {
      BSA::Archive archive;
      QString archivePath = QString("%1\\%2").arg(origin).arg(archiveName);
      BSA::EErrorCode result =
          archive.read(archivePath.toLocal8Bit().constData(), true);
      if ((result != BSA::ERROR_NONE) && (result != BSA::ERROR_INVALIDHASHES)) {
        reportError(tr("failed to read %1: %2").arg(archivePath).arg(result));
        return;
      }

      QProgressDialog progress(this);
      progress.setMaximum(100);
      progress.setValue(0);
      progress.show();
      archive.extractAll(
          QDir::toNativeSeparators(targetFolder).toLocal8Bit().constData(),
          boost::bind(&MainWindow::extractProgress, this, boost::ref(progress), _1,
                      _2));
      if (result == BSA::ERROR_INVALIDHASHES) {
        reportError(
            tr("This archive contains invalid hashes. Some files may be broken."));
      }
      archive.close();
    }
  }
}

void MainWindow::on_bsaList_customContextMenuRequested(const QPoint& pos)
{
  QMenu menu;
  menu.addAction(tr("Extract..."), [=, item = ui->bsaList->itemAt(pos)]() {
    extractBSATriggered(item);
  });

  menu.exec(ui->bsaList->viewport()->mapToGlobal(pos));
}

void MainWindow::on_bsaList_itemChanged(QTreeWidgetItem*, int)
{
  m_ArchiveListWriter.write();
  m_CheckBSATimer.start(500);
}

void MainWindow::on_actionNotifications_triggered()
{
  auto future = checkForProblemsAsync();

  future.waitForFinished();

  ProblemsDialog problems(m_PluginContainer, this);
  problems.exec();

  scheduleCheckForProblems();
}

void MainWindow::on_actionChange_Game_triggered()
{
  InstanceManagerDialog dlg(m_PluginContainer, this);
  dlg.exec();
}

void MainWindow::setCategoryListVisible(bool visible)
{
  using namespace std::literals;

  if (visible) {
    ui->categoriesGroup->show();
    ui->displayCategoriesBtn->setText(ToQString(L"\u00ab"sv));
  } else {
    ui->categoriesGroup->hide();
    ui->displayCategoriesBtn->setText(ToQString(L"\u00bb"sv));
  }
}

void MainWindow::on_displayCategoriesBtn_toggled(bool checked)
{
  setCategoryListVisible(checked);
}

void MainWindow::removeFromToolbar(QAction* action)
{
  const auto& title = action->text();
  auto& list        = *m_OrganizerCore.executablesList();

  auto itor = list.find(title);
  if (itor == list.end()) {
    log::warn("removeFromToolbar(): executable '{}' not found", title);
    return;
  }

  itor->setShownOnToolbar(false);
  updatePinnedExecutables();
}

void MainWindow::toolBar_customContextMenuRequested(const QPoint& point)
{
  QAction* action = ui->toolBar->actionAt(point);

  if (action != nullptr) {
    if (action->objectName().startsWith("custom_")) {
      QMenu menu;
      menu.addAction(tr("Remove '%1' from the toolbar").arg(action->text()),
                     [&, action]() {
                       removeFromToolbar(action);
                     });
      menu.exec(ui->toolBar->mapToGlobal(point));
      return;
    }
  }

  // did not click a link button, show the default context menu
  auto* m = createPopupMenu();
  m->exec(ui->toolBar->mapToGlobal(point));
}

Executable* MainWindow::getSelectedExecutable()
{
  const QString name =
      ui->executablesListBox->itemText(ui->executablesListBox->currentIndex());

  try {
    return &m_OrganizerCore.executablesList()->get(name);
  } catch (std::runtime_error&) {
    return nullptr;
  }
}

void MainWindow::on_showHiddenBox_toggled(bool checked)
{
  m_OrganizerCore.downloadManager()->setShowHidden(checked);
}

const char* MainWindow::PATTERN_BACKUP_GLOB = ".????_??_??_??_??_??";
const char* MainWindow::PATTERN_BACKUP_REGEX =
    "\\.(\\d\\d\\d\\d_\\d\\d_\\d\\d_\\d\\d_\\d\\d_\\d\\d)";
const char* MainWindow::PATTERN_BACKUP_DATE = "yyyy_MM_dd_hh_mm_ss";

bool MainWindow::createBackup(const QString& filePath, const QDateTime& time)
{
  QString outPath = filePath + "." + time.toString(PATTERN_BACKUP_DATE);
  if (shellCopy(QStringList(filePath), QStringList(outPath), this)) {
    QFileInfo fileInfo(filePath);
    removeOldFiles(fileInfo.absolutePath(), fileInfo.fileName() + PATTERN_BACKUP_GLOB,
                   10, QDir::Name);
    return true;
  } else {
    return false;
  }
}

void MainWindow::on_saveButton_clicked()
{
  m_OrganizerCore.savePluginList();
  QDateTime now = QDateTime::currentDateTime();
  if (createBackup(m_OrganizerCore.currentProfile()->getPluginsFileName(), now) &&
      createBackup(m_OrganizerCore.currentProfile()->getLoadOrderFileName(), now) &&
      createBackup(m_OrganizerCore.currentProfile()->getLockedOrderFileName(), now)) {
    MessageDialog::showMessage(tr("Backup of load order created"), this);
  }
}

QString MainWindow::queryRestore(const QString& filePath)
{
  QFileInfo pluginFileInfo(filePath);
  QString pattern     = pluginFileInfo.fileName() + ".*";
  QFileInfoList files = pluginFileInfo.absoluteDir().entryInfoList(
      QStringList(pattern), QDir::Files, QDir::Name);

  SelectionDialog dialog(tr("Choose backup to restore"), this);
  QRegularExpression exp(QRegularExpression::anchoredPattern(pluginFileInfo.fileName() +
                                                             PATTERN_BACKUP_REGEX));
  QRegularExpression exp2(
      QRegularExpression::anchoredPattern(pluginFileInfo.fileName() + "\\.(.*)"));
  for (const QFileInfo& info : boost::adaptors::reverse(files)) {
    auto match  = exp.match(info.fileName());
    auto match2 = exp2.match(info.fileName());
    if (match.hasMatch()) {
      QDateTime time = QDateTime::fromString(match.captured(1), PATTERN_BACKUP_DATE);
      dialog.addChoice(time.toString(), "", match.captured(1));
    } else if (match2.hasMatch()) {
      dialog.addChoice(match2.captured(1), "", match2.captured(1));
    }
  }

  if (dialog.numChoices() == 0) {
    QMessageBox::information(this, tr("No Backups"),
                             tr("There are no backups to restore"));
    return QString();
  }

  if (dialog.exec() == QDialog::Accepted) {
    return dialog.getChoiceData().toString();
  } else {
    return QString();
  }
}

void MainWindow::on_restoreButton_clicked()
{
  QString pluginName = m_OrganizerCore.currentProfile()->getPluginsFileName();
  QString choice     = queryRestore(pluginName);
  if (!choice.isEmpty()) {
    QString loadOrderName = m_OrganizerCore.currentProfile()->getLoadOrderFileName();
    QString lockedName    = m_OrganizerCore.currentProfile()->getLockedOrderFileName();
    if (!shellCopy(pluginName + "." + choice, pluginName, true, this) ||
        !shellCopy(loadOrderName + "." + choice, loadOrderName, true, this) ||
        !shellCopy(lockedName + "." + choice, lockedName, true, this)) {

      const auto e = GetLastError();

      QMessageBox::critical(this, tr("Restore failed"),
                            tr("Failed to restore the backup. Errorcode: %1")
                                .arg(QString::fromStdWString(formatSystemMessage(e))));
    }
    m_OrganizerCore.refreshESPList(true);
  }
}

void MainWindow::on_saveModsButton_clicked()
{
  m_OrganizerCore.currentProfile()->writeModlistNow(true);
  QDateTime now = QDateTime::currentDateTime();
  if (createBackup(m_OrganizerCore.currentProfile()->getModlistFileName(), now)) {
    MessageDialog::showMessage(tr("Backup of mod list created"), this);
  }
}

void MainWindow::on_restoreModsButton_clicked()
{
  QString modlistName = m_OrganizerCore.currentProfile()->getModlistFileName();
  QString choice      = queryRestore(modlistName);
  if (!choice.isEmpty()) {
    if (!shellCopy(modlistName + "." + choice, modlistName, true, this)) {
      const auto e = GetLastError();
      QMessageBox::critical(this, tr("Restore failed"),
                            tr("Failed to restore the backup. Errorcode: %1")
                                .arg(formatSystemMessage(e)));
    }
    m_OrganizerCore.refresh(false);
  }
}

void MainWindow::on_managedArchiveLabel_linkHovered(const QString&)
{
  QToolTip::showText(QCursor::pos(), ui->managedArchiveLabel->toolTip());
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
  // Accept copy or move drags to the download window. Link drags are not
  // meaningful (Well, they are - we could drop a link in the download folder,
  // but you need privileges to do that).
  if (ui->downloadTab->isVisible() &&
      (event->proposedAction() == Qt::CopyAction ||
       event->proposedAction() == Qt::MoveAction) &&
      event->answerRect().intersects(ui->downloadTab->rect())) {

    // If I read the documentation right, this won't work under a motif windows
    // manager and the check needs to be done at the drop. However, that means
    // the user might be allowed to drop things which we can't sanely process
    QMimeData const* data = event->mimeData();

    if (data->hasUrls()) {
      QStringList extensions =
          m_OrganizerCore.installationManager()->getSupportedExtensions();

      // This is probably OK - scan to see if these are moderately sane archive
      // types
      QList<QUrl> urls = data->urls();
      bool ok          = true;
      for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
          QString local = url.toLocalFile();
          bool fok      = false;
          for (auto ext : extensions) {
            if (local.endsWith(ext, Qt::CaseInsensitive)) {
              fok = true;
              break;
            }
          }
          if (!fok) {
            ok = false;
            break;
          }
        }
      }
      if (ok) {
        event->accept();
      }
    }
  }
}

void MainWindow::dropLocalFile(const QUrl& url, const QString& outputDir, bool move)
{
  QFileInfo file(url.toLocalFile());
  if (!file.exists()) {
    log::warn("invalid source file: {}", file.absoluteFilePath());
    return;
  }
  QString target = outputDir + "/" + file.fileName();
  if (QFile::exists(target)) {
    QMessageBox box(QMessageBox::Question, file.fileName(),
                    tr("A file with the same name has already been downloaded. "
                       "What would you like to do?"));
    box.addButton(tr("Overwrite"), QMessageBox::ActionRole);
    box.addButton(tr("Rename new file"), QMessageBox::YesRole);
    box.addButton(tr("Ignore file"), QMessageBox::RejectRole);

    box.exec();
    switch (box.buttonRole(box.clickedButton())) {
    case QMessageBox::RejectRole:
      return;
    case QMessageBox::ActionRole:
      break;
    default:
    case QMessageBox::YesRole:
      target = m_OrganizerCore.downloadManager()->getDownloadFileName(file.fileName());
      break;
    }
  }

  bool success = false;
  if (move) {
    success = shellMove(file.absoluteFilePath(), target, true, this);
  } else {
    success = shellCopy(file.absoluteFilePath(), target, true, this);
  }
  if (!success) {
    const auto e = GetLastError();
    log::error("file operation failed: {}", formatSystemMessage(e));
  }
}

void MainWindow::dropEvent(QDropEvent* event)
{
  Qt::DropAction action = event->proposedAction();
  QString outputDir     = m_OrganizerCore.downloadManager()->getOutputDirectory();
  if (action == Qt::MoveAction) {
    // Tell windows I'm taking control and will delete the source of a move.
    event->setDropAction(Qt::TargetMoveAction);
  }
  for (const QUrl& url : event->mimeData()->urls()) {
    if (url.isLocalFile()) {
      dropLocalFile(url, outputDir, action == Qt::MoveAction);
    } else {
      m_OrganizerCore.downloadManager()->startDownloadURLs(QStringList() << url.url());
    }
  }
  event->accept();
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
  // if the ui is locked, ignore the ALT key event
  // alt-tabbing out of a game triggers this
  auto& uilocker = UILocker::instance();
  auto& settings = Settings::instance();
  if (!uilocker.locked()) {
    // if the menubar is hidden and showMenuBarOnAlt is true,
    // pressing Alt will make it visible
    if (event->key() == Qt::Key_Alt) {
      bool showMenubarOnAlt = settings.interface().showMenubarOnAlt();
      if (showMenubarOnAlt && !ui->menuBar->isVisible()) {
        ui->menuBar->show();
      }
    }
  }

  QMainWindow::keyReleaseEvent(event);
}
