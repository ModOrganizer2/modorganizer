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

#include "executableinfo.h"
#include "executableslist.h"
#include "guessedvalue.h"
#include "imodinterface.h"
#include "iplugingame.h"
#include "iplugindiagnose.h"
#include "isavegame.h"
#include "isavegameinfowidget.h"
#include "nexusinterface.h"
#include "organizercore.h"
#include "pluginlistsortproxy.h"
#include "previewgenerator.h"
#include "serverinfo.h"
#include "savegameinfo.h"
#include "spawn.h"
#include "versioninfo.h"
#include "instancemanager.h"
#include "report.h"
#include "modlist.h"
#include "modlistsortproxy.h"
#include "qtgroupingproxy.h"
#include "profile.h"
#include "pluginlist.h"
#include "profilesdialog.h"
#include "editexecutablesdialog.h"
#include "categories.h"
#include "categoriesdialog.h"
#include "modinfodialog.h"
#include "overwriteinfodialog.h"
#include "activatemodsdialog.h"
#include "downloadlist.h"
#include "downloadlistwidget.h"
#include "messagedialog.h"
#include "installationmanager.h"
#include "downloadlistsortproxy.h"
#include "motddialog.h"
#include "filedialogmemory.h"
#include "tutorialmanager.h"
#include "modflagicondelegate.h"
#include "modconflicticondelegate.h"
#include "genericicondelegate.h"
#include "selectiondialog.h"
#include "csvbuilder.h"
#include "savetextasdialog.h"
#include "problemsdialog.h"
#include "previewdialog.h"
#include "browserdialog.h"
#include "aboutdialog.h"
#include "settingsdialog.h"
#include <safewritefile.h>
#include "nxmaccessmanager.h"
#include "shared/appconfig.h"
#include "eventfilter.h"
#include "statusbar.h"
#include "filterlist.h"
#include "datatab.h"
#include "instancemanagerdialog.h"
#include <utility.h>
#include <dataarchives.h>
#include <bsainvalidation.h>
#include <taskprogressmanager.h>
#include <scopeguard.h>
#include <usvfs.h>
#include "localsavegames.h"
#include "listdialog.h"
#include "envshortcut.h"
#include "browserdialog.h"

#include "directoryrefresher.h"
#include "shared/directoryentry.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"

#include <QAbstractItemDelegate>
#include <QAbstractProxyModel>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFIleIconProvider>
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
#include <QDialogButtonBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRect>
#include <QRegExp>
#include <QResizeEvent>
#include <QScopedPointer>
#include <QSizePolicy>
#include <QSize>
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
#include <QWhatsThis>
#include <QWidgetAction>
#include <QWebEngineProfile>
#include <QShortcut>
#include <QColorDialog>
#include <QColor>

#include <QDebug>
#include <QtGlobal>

#ifndef Q_MOC_RUN
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include <boost/assign.hpp>
#include <boost/range/adaptor/reversed.hpp>
#endif

#include <shlobj.h>

#include <limits.h>
#include <exception>
#include <functional>
#include <map>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <utility>

#include "gameplugins.h"

#ifdef TEST_MODELS
#include "modeltest.h"
#endif // TEST_MODELS

#pragma warning( disable : 4428 )

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


MainWindow::MainWindow(Settings &settings
                       , OrganizerCore &organizerCore
                       , PluginContainer &pluginContainer
                       , QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
  , m_WasVisible(false)
  , m_FirstPaint(true)
  , m_linksSeparator(nullptr)
  , m_Tutorial(this, "MainWindow")
  , m_OldProfileIndex(-1)
  , m_ModListSortProxy(nullptr)
  , m_OldExecutableIndex(-1)
  , m_CategoryFactory(CategoryFactory::instance())
  , m_ContextItem(nullptr)
  , m_ContextAction(nullptr)
  , m_ContextRow(-1)
  , m_browseModPage(nullptr)
  , m_CurrentSaveView(nullptr)
  , m_OrganizerCore(organizerCore)
  , m_PluginContainer(pluginContainer)
  , m_DidUpdateMasterList(false)
  , m_ArchiveListWriter(std::bind(&MainWindow::saveArchiveList, this))
  , m_LinkToolbar(nullptr)
  , m_LinkDesktop(nullptr)
  , m_LinkStartMenu(nullptr)
  , m_NumberOfProblems(0)
  , m_ProblemsCheckRequired(false)
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

  QWebEngineProfile::defaultProfile()->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
  QWebEngineProfile::defaultProfile()->setHttpCacheMaximumSize(52428800);
  QWebEngineProfile::defaultProfile()->setCachePath(settings.paths().cache());
  QWebEngineProfile::defaultProfile()->setPersistentStoragePath(settings.paths().cache());

  ui->setupUi(this);
  languageChange(settings.interface().language());
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

  m_Filters.reset(new FilterList(ui, &m_OrganizerCore, m_CategoryFactory));

  connect(
    m_Filters.get(), &FilterList::criteriaChanged,
    [&](auto&& v) { onFiltersCriteria(v); });

  connect(
    m_Filters.get(), &FilterList::optionsChanged,
    [&](auto&& mode, auto&& sep) { onFiltersOptions(mode, sep); });

  ui->logList->setCore(m_OrganizerCore);


  setupToolbar();
  toggleMO2EndorseState();
  toggleUpdateAction();

  TaskProgressManager::instance().tryCreateTaskbar();

  setupModList();

  // set up plugin list
  m_PluginListSortProxy = m_OrganizerCore.createPluginListProxyModel();

  ui->espList->setModel(m_PluginListSortProxy);
  ui->espList->sortByColumn(PluginList::COL_PRIORITY, Qt::AscendingOrder);
  ui->espList->setItemDelegateForColumn(PluginList::COL_FLAGS, new GenericIconDelegate(ui->espList));
  ui->espList->installEventFilter(m_OrganizerCore.pluginList());

  ui->bsaList->setLocalMoveOnly(true);
  ui->bsaList->setHeaderHidden(true);

  initDownloadView();

  const bool pluginListAdjusted =
    settings.geometry().restoreState(ui->espList->header());

  m_DataTab.reset(new DataTab(m_OrganizerCore, m_PluginContainer, this, ui));
  m_DataTab->restoreState(settings);

  connect(m_DataTab.get(), &DataTab::executablesChanged, [&]{ refreshExecutablesList(); });

  connect(
    m_DataTab.get(), &DataTab::originModified,
    [&](int id){ originModified(id); });

  connect(
    m_DataTab.get(), &DataTab::displayModInformation,
    [&](auto&& m, auto&& i, auto&& tab){ displayModInformation(m, i, tab); });

  // Hide stuff we do not need:
  IPluginGame const* game = m_OrganizerCore.managedGame();
  if (!game->feature<GamePlugins>()) {
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->espTab));
  }
  if (!game->feature<DataArchives>()) {
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->bsaTab));
  }

  settings.geometry().restoreState(ui->downloadView->header());

  ui->splitter->setStretchFactor(0, 3);
  ui->splitter->setStretchFactor(1, 2);

  resizeLists(pluginListAdjusted);

  QMenu *linkMenu = new QMenu(this);
  m_LinkToolbar = linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Toolbar and Menu"), this, SLOT(linkToolbar()));
  m_LinkDesktop = linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Desktop"), this, SLOT(linkDesktop()));
  m_LinkStartMenu = linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Start Menu"), this, SLOT(linkMenu()));
  ui->linkButton->setMenu(linkMenu);

  QMenu *listOptionsMenu = new QMenu(ui->listOptionsBtn);
  initModListContextMenu(listOptionsMenu);
  ui->listOptionsBtn->setMenu(listOptionsMenu);
  connect(ui->listOptionsBtn, SIGNAL(pressed()), this, SLOT(on_listOptionsBtn_pressed()));

  ui->openFolderMenu->setMenu(openFolderMenu());

  ui->savegameList->installEventFilter(this);
  ui->savegameList->setMouseTracking(true);

  // don't allow mouse wheel to switch grouping, too many people accidentally
  // turn on grouping and then don't understand what happened
  EventFilter *noWheel
      = new EventFilter(this, [](QObject *, QEvent *event) -> bool {
          return event->type() == QEvent::Wheel;
        });

  ui->groupCombo->installEventFilter(noWheel);
  ui->profileBox->installEventFilter(noWheel);

  if (organizerCore.managedGame()->sortMechanism() == MOBase::IPluginGame::SortMechanism::NONE) {
    ui->bossButton->setDisabled(true);
    ui->bossButton->setToolTip(tr("There is no supported sort mechanism for this game. You will probably have to use a third-party tool."));
  }

  connect(&m_PluginContainer, SIGNAL(diagnosisUpdate()), this, SLOT(scheduleCheckForProblems()));

  connect(ui->savegameList, SIGNAL(itemEntered(QListWidgetItem*)), this, SLOT(saveSelectionChanged(QListWidgetItem*)));

  connect(m_ModListSortProxy, SIGNAL(filterActive(bool)), this, SLOT(modFilterActive(bool)));
  connect(m_ModListSortProxy, SIGNAL(layoutChanged()), this, SLOT(updateModCount()));
  connect(ui->modFilterEdit, SIGNAL(textChanged(QString)), m_ModListSortProxy, SLOT(updateFilter(QString)));

  connect(ui->espFilterEdit, SIGNAL(textChanged(QString)), m_PluginListSortProxy, SLOT(updateFilter(QString)));
  connect(ui->espFilterEdit, SIGNAL(textChanged(QString)), this, SLOT(espFilterChanged(QString)));

  connect(m_OrganizerCore.directoryRefresher(), SIGNAL(refreshed()), this, SLOT(directory_refreshed()));
  connect(
    m_OrganizerCore.directoryRefresher(),
    &DirectoryRefresher::progress,
    this, &MainWindow::refresherProgress);
  connect(m_OrganizerCore.directoryRefresher(), SIGNAL(error(QString)), this, SLOT(showError(QString)));

  connect(&m_SavesWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(refreshSavesIfOpen()));

  connect(&m_OrganizerCore.settings(), SIGNAL(languageChanged(QString)), this, SLOT(languageChange(QString)));
  connect(&m_OrganizerCore.settings(), SIGNAL(styleChanged(QString)), this, SIGNAL(styleChanged(QString)));

  connect(m_OrganizerCore.updater(), SIGNAL(restart()), this, SLOT(close()));
  connect(m_OrganizerCore.updater(), SIGNAL(updateAvailable()), this, SLOT(updateAvailable()));
  connect(m_OrganizerCore.updater(), SIGNAL(motdAvailable(QString)), this, SLOT(motdReceived(QString)));

  connect(&NexusInterface::instance(), SIGNAL(requestNXMDownload(QString)), &m_OrganizerCore, SLOT(downloadRequestedNXM(QString)));
  connect(&NexusInterface::instance(), SIGNAL(nxmDownloadURLsAvailable(QString,int,int,QVariant,QVariant,int)), this, SLOT(nxmDownloadURLs(QString,int,int,QVariant,QVariant,int)));
  connect(&NexusInterface::instance(), SIGNAL(needLogin()), &m_OrganizerCore, SLOT(nexusApi()));

  connect(
    NexusInterface::instance().getAccessManager(),
    SIGNAL(credentialsReceived(const APIUserAccount&)),
    this,
    SLOT(updateWindowTitle(const APIUserAccount&)));

  connect(
    NexusInterface::instance().getAccessManager(),
    SIGNAL(credentialsReceived(const APIUserAccount&)),
    &NexusInterface::instance(),
    SLOT(setUserAccount(const APIUserAccount&)));

  connect(
    &NexusInterface::instance(),
    SIGNAL(requestsChanged(const APIStats&, const APIUserAccount&)),
    this,
    SLOT(onRequestsChanged(const APIStats&, const APIUserAccount&)));

  connect(&TutorialManager::instance(), SIGNAL(windowTutorialFinished(QString)), this, SLOT(windowTutorialFinished(QString)));
  connect(ui->tabWidget, SIGNAL(currentChanged(int)), &TutorialManager::instance(), SIGNAL(tabChanged(int)));
  connect(ui->toolBar, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(toolBar_customContextMenuRequested(QPoint)));
  connect(ui->menuToolbars, &QMenu::aboutToShow, [&]{ updateToolbarMenu(); });
  connect(ui->menuView, &QMenu::aboutToShow, [&]{ updateViewMenu(); });

  connect(&m_OrganizerCore, &OrganizerCore::modInstalled, this, &MainWindow::modInstalled);
  connect(&m_OrganizerCore, &OrganizerCore::close, this, &QMainWindow::close);

  m_CheckBSATimer.setSingleShot(true);
  connect(&m_CheckBSATimer, SIGNAL(timeout()), this, SLOT(checkBSAList()));

  connect(ui->espList->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(esplistSelectionsChanged(QItemSelection)));

  new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Enter), this, SLOT(openExplorer_activated()));
  new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return), this, SLOT(openExplorer_activated()));

  new QShortcut(QKeySequence::Refresh, this, SLOT(refreshProfile_activated()));

  setFilterShortcuts(ui->modList, ui->modFilterEdit);
  setFilterShortcuts(ui->espList, ui->espFilterEdit);
  setFilterShortcuts(ui->downloadView, ui->downloadFilterEdit);

  m_UpdateProblemsTimer.setSingleShot(true);
  connect(&m_UpdateProblemsTimer, &QTimer::timeout, this, &MainWindow::checkForProblemsAsync);
  connect(this, &MainWindow::checkForProblemsDone, this, &MainWindow::updateProblemsButton, Qt::ConnectionType::QueuedConnection);

  m_SaveMetaTimer.setSingleShot(false);
  connect(&m_SaveMetaTimer, SIGNAL(timeout()), this, SLOT(saveModMetas()));
  m_SaveMetaTimer.start(5000);

  FileDialogMemory::restore(settings);

  fixCategories();

  m_StartTime = QTime::currentTime();

  m_Tutorial.expose("modList", m_OrganizerCore.modList());
  m_Tutorial.expose("espList", m_OrganizerCore.pluginList());

  m_OrganizerCore.setUserInterface(this);
  connect(this, &MainWindow::userInterfaceInitialized, &m_OrganizerCore, &OrganizerCore::userInterfaceInitialized);
  for (const QString &fileName : m_PluginContainer.pluginFileNames()) {
    installTranslator(QFileInfo(fileName).baseName());
  }

  registerPluginTools(m_PluginContainer.plugins<IPluginTool>());

  for (IPluginModPage *modPagePlugin : m_PluginContainer.plugins<IPluginModPage>()) {
    registerModPage(modPagePlugin);
  }

  // refresh profiles so the current profile can be activated
  refreshProfiles(false);

  ui->profileBox->setCurrentText(m_OrganizerCore.currentProfile()->name());

  if (m_OrganizerCore.getArchiveParsing())
  {
    ui->dataTabShowFromArchives->setCheckState(Qt::Checked);
    ui->dataTabShowFromArchives->setEnabled(true);
  }
  else
  {
    ui->dataTabShowFromArchives->setCheckState(Qt::Unchecked);
    ui->dataTabShowFromArchives->setEnabled(false);
  }

  QApplication::instance()->installEventFilter(this);

  scheduleCheckForProblems();
  refreshExecutablesList();
  updatePinnedExecutables();
  resetActionIcons();
  updatePluginCount();
  updateModCount();
  processUpdates();

  ui->statusBar->updateNormalMessage(m_OrganizerCore);
}

void MainWindow::setupModList()
{
  m_ModListSortProxy = m_OrganizerCore.createModListProxyModel();
  ui->modList->setModel(m_ModListSortProxy);
  ui->modList->sortByColumn(ModList::COL_PRIORITY, Qt::AscendingOrder);


  connect(
    ui->modList, SIGNAL(dropModeUpdate(bool)),
    m_OrganizerCore.modList(), SLOT(dropModeUpdate(bool)));

  connect(
    ui->modList->header(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
    this, SLOT(modListSortIndicatorChanged(int,Qt::SortOrder)));

  connect(
    ui->modList->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
    this, SLOT(modlistSelectionsChanged(QItemSelection)));

  connect(
    ui->modList->header(), SIGNAL(sectionResized(int, int, int)),
    this, SLOT(modListSectionResized(int, int, int)));


  GenericIconDelegate *contentDelegate = new GenericIconDelegate(
    ui->modList, Qt::UserRole + 3, ModList::COL_CONTENT, 150);

  connect(
    ui->modList->header(), SIGNAL(sectionResized(int,int,int)),
    contentDelegate, SLOT(columnResized(int,int,int)));


  ModFlagIconDelegate *flagDelegate = new ModFlagIconDelegate(
    ui->modList, ModList::COL_FLAGS, 120);

  connect(
    ui->modList->header(), SIGNAL(sectionResized(int,int,int)),
    flagDelegate, SLOT(columnResized(int,int,int)));


  ModConflictIconDelegate* conflictFlagDelegate = new ModConflictIconDelegate(
    ui->modList, ModList::COL_CONFLICTFLAGS, 80);

  connect(
    ui->modList->header(), SIGNAL(sectionResized(int, int, int)),
    conflictFlagDelegate, SLOT(columnResized(int, int, int)));


  ui->modList->setItemDelegateForColumn(ModList::COL_FLAGS, flagDelegate);
  ui->modList->setItemDelegateForColumn(ModList::COL_CONFLICTFLAGS, conflictFlagDelegate);
  ui->modList->setItemDelegateForColumn(ModList::COL_CONTENT, contentDelegate);
  ui->modList->header()->installEventFilter(m_OrganizerCore.modList());


  if (m_OrganizerCore.settings().geometry().restoreState(ui->modList->header())) {
    // hack: force the resize-signal to be triggered because restoreState doesn't seem to do that
    for (int column = 0; column <= ModList::COL_LASTCOLUMN; ++column) {
      int sectionSize = ui->modList->header()->sectionSize(column);
      ui->modList->header()->resizeSection(column, sectionSize + 1);
      ui->modList->header()->resizeSection(column, sectionSize);
    }
  } else {
    // hide these columns by default
    ui->modList->header()->setSectionHidden(ModList::COL_CONTENT, true);
    ui->modList->header()->setSectionHidden(ModList::COL_MODID, true);
    ui->modList->header()->setSectionHidden(ModList::COL_GAME, true);
    ui->modList->header()->setSectionHidden(ModList::COL_INSTALLTIME, true);
    ui->modList->header()->setSectionHidden(ModList::COL_NOTES, true);

    // resize mod list to fit content
    for (int i = 0; i < ui->modList->header()->count(); ++i) {
      ui->modList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }

    ui->modList->header()->setSectionResizeMode(ModList::COL_NAME, QHeaderView::Stretch);
  }

  // prevent the name-column from being hidden
  ui->modList->header()->setSectionHidden(ModList::COL_NAME, false);

  ui->modList->installEventFilter(m_OrganizerCore.modList());
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

    if (auto* actionButton=dynamic_cast<QAbstractButton*>(actionWidget)) {
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

    m_PluginContainer.setUserInterface(nullptr, nullptr);
    m_OrganizerCore.setUserInterface(nullptr);

    if (m_IntegratedBrowser) {
      m_IntegratedBrowser->close();
      m_IntegratedBrowser.reset();
    }

    delete ui;
  } catch (std::exception &e) {
    QMessageBox::critical(nullptr, tr("Crash on exit"),
      tr("MO crashed while exiting.  Some settings may not be saved.\n\nError: %1").arg(e.what()),
      QMessageBox::Ok);
  }
}


void MainWindow::updateWindowTitle(const APIUserAccount& user)
{
  //"\xe2\x80\x93" is an "em dash", a longer "-"
  QString title = QString("%1 \xe2\x80\x93 Mod Organizer v%2").arg(
        m_OrganizerCore.managedGame()->gameName(),
        m_OrganizerCore.getVersion().displayString(3));

  if (!user.name().isEmpty()) {
    const QString premium = (user.type() == APIUserAccountTypes::Premium ? "*" : "");
    title.append(QString(" (%1%2)").arg(user.name(), premium));
  }

  this->setWindowTitle(title);
}


void MainWindow::onRequestsChanged(const APIStats& stats, const APIUserAccount& user)
{
  ui->statusBar->setAPI(stats, user);
}


void MainWindow::disconnectPlugins()
{
  if (ui->actionTool->menu() != nullptr) {
    ui->actionTool->menu()->clear();
  }
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
  m_Tutorial.resize(event->size());
  QMainWindow::resizeEvent(event);
}


static QModelIndex mapToModel(const QAbstractItemModel *targetModel, QModelIndex idx)
{
  QModelIndex result = idx;
  const QAbstractItemModel *model = idx.model();
  while (model != targetModel) {
    if (model == nullptr) {
      return QModelIndex();
    }
    const QAbstractProxyModel *proxyModel = qobject_cast<const QAbstractProxyModel*>(model);
    if (proxyModel == nullptr) {
      return QModelIndex();
    }
    result = proxyModel->mapToSource(result);
    model = proxyModel->sourceModel();
  }
  return result;
}

void MainWindow::setupToolbar()
{
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

  if (!InstanceManager::instance().allowedToChangeInstance()) {
    ui->actionChange_Game->setVisible(false);
  }
}

void MainWindow::setupActionMenu(QAction* a)
{
  a->setMenu(new QMenu(this));

  auto* w = ui->toolBar->widgetForAction(a);
  if (auto* tb=dynamic_cast<QToolButton*>(w))
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

      QAction *exeAction = new QAction(
        iconForExecutable(exe.binaryInfo().filePath()), exe.title());

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
  ui->actionToolBarMediumIcons->setChecked(ui->toolBar->iconSize() == MediumToolbarSize);
  ui->actionToolBarLargeIcons->setChecked(ui->toolBar->iconSize() == LargeToolbarSize);

  ui->actionToolBarIconsOnly->setChecked(ui->toolBar->toolButtonStyle() == Qt::ToolButtonIconOnly);
  ui->actionToolBarTextOnly->setChecked(ui->toolBar->toolButtonStyle() == Qt::ToolButtonTextOnly);
  ui->actionToolBarIconsAndText->setChecked(ui->toolBar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon);
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

void MainWindow::on_centralWidget_customContextMenuRequested(const QPoint &pos)
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
  const QIcon original = m_originalNotificationIcon.isNull() ?
    QIcon(DefaultIconName) : m_originalNotificationIcon;

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
        (numProblems < 10 ? std::to_string(static_cast<long long>(numProblems)) : "more");

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
  if (auto* actionWidget=ui->toolBar->widgetForAction(ui->actionNotifications)) {
    if (auto* button=dynamic_cast<QAbstractButton*>(actionWidget)) {
      button->setIcon(final);
    }
  }

  // updating the status bar, may be null very early when MO is starting
  if (ui->statusBar) {
    ui->statusBar->setNotifications(numProblems > 0);
  }
}


bool MainWindow::errorReported(QString &logFile)
{
  QDir dir(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::logPath()));
  QFileInfoList files = dir.entryInfoList(QStringList("ModOrganizer_??_??_??_??_??.log"),
                                          QDir::Files, QDir::Name | QDir::Reversed);

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

QFuture<void> MainWindow::checkForProblemsAsync() {
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
    for (QObject *pluginObj : m_PluginContainer.plugins<QObject>()) {
      IPlugin *plugin = qobject_cast<IPlugin*>(pluginObj);
      if (plugin == nullptr || plugin->isActive()) {
        IPluginDiagnose *diagnose = qobject_cast<IPluginDiagnose*>(pluginObj);
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
  AboutDialog dialog(m_OrganizerCore.getVersion().displayString(3), this);
  connect(&dialog, SIGNAL(linkClicked(QString)), this, SLOT(linkClicked(QString)));
  dialog.exec();
}


void MainWindow::createEndorseMenu()
{
  auto* menu = ui->actionEndorseMO->menu();
  if (!menu) {
    // shouldn't happen
    return;
  }

  menu->clear();

  QAction *endorseAction = new QAction(tr("Endorse"), menu);
  connect(endorseAction, SIGNAL(triggered()), this, SLOT(actionEndorseMO()));
  menu->addAction(endorseAction);

  QAction *wontEndorseAction = new QAction(tr("Won't Endorse"), menu);
  connect(wontEndorseAction, SIGNAL(triggered()), this, SLOT(actionWontEndorseMO()));
  menu->addAction(wontEndorseAction);
}


void MainWindow::createHelpMenu()
{
  auto* menu = ui->actionHelp->menu();
  if (!menu) {
    // this happens on startup because languageChanged() (which calls this) is
    // called before the menus are actually created
    return;
  }

  menu->clear();

  QAction *helpAction = new QAction(tr("Help on UI"), menu);
  connect(helpAction, SIGNAL(triggered()), this, SLOT(helpTriggered()));
  menu->addAction(helpAction);

  QAction *wikiAction = new QAction(tr("Documentation"), menu);
  connect(wikiAction, SIGNAL(triggered()), this, SLOT(wikiTriggered()));
  menu->addAction(wikiAction);

  QAction *discordAction = new QAction(tr("Chat on Discord"), menu);
  connect(discordAction, SIGNAL(triggered()), this, SLOT(discordTriggered()));
  menu->addAction(discordAction);

  QAction *issueAction = new QAction(tr("Report Issue"), menu);
  connect(issueAction, SIGNAL(triggered()), this, SLOT(issueTriggered()));
  menu->addAction(issueAction);

  QMenu *tutorialMenu = new QMenu(tr("Tutorials"), menu);

  typedef std::vector<std::pair<int, QAction*> > ActionList;

  ActionList tutorials;

  QDirIterator dirIter(QApplication::applicationDirPath() + "/tutorials", QStringList("*.js"), QDir::Files);
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
        log::error("invalid header line for tutorial {}, expected 2 parameters", fileName);
        continue;
      }
      QAction *tutAction = new QAction(params.at(0), tutorialMenu);
      tutAction->setData(fileName);
      tutorials.push_back(std::make_pair(params.at(1).toInt(), tutAction));
    }
  }

  std::sort(tutorials.begin(), tutorials.end(),
            [] (const ActionList::value_type &LHS, const ActionList::value_type &RHS) {
              return LHS.first < RHS.first; } );

  for (auto iter = tutorials.begin(); iter != tutorials.end(); ++iter) {
    connect(iter->second, SIGNAL(triggered()), this, SLOT(tutorialTriggered()));
    tutorialMenu->addAction(iter->second);
  }

  menu->addMenu(tutorialMenu);
  menu->addAction(tr("About"), this, SLOT(about()));
  menu->addAction(tr("About Qt"), qApp, SLOT(aboutQt()));
}

void MainWindow::modFilterActive(bool filterActive)
{
  ui->clearFiltersButton->setVisible(filterActive);
  if (filterActive) {
//    m_OrganizerCore.modList()->setOverwriteMarkers(std::set<unsigned int>(), std::set<unsigned int>());
    ui->modList->setStyleSheet("QTreeView { border: 2px ridge #f00; }");
    ui->activeModsCounter->setStyleSheet("QLCDNumber { border: 2px ridge #f00; }");
  } else if (ui->groupCombo->currentIndex() != 0) {
    ui->modList->setStyleSheet("QTreeView { border: 2px ridge #337733; }");
    ui->activeModsCounter->setStyleSheet("");
  } else {
    ui->modList->setStyleSheet("");
    ui->activeModsCounter->setStyleSheet("");
  }
}

void MainWindow::espFilterChanged(const QString &filter)
{
  if (!filter.isEmpty()) {
    ui->espList->setStyleSheet("QTreeView { border: 2px ridge #f00; }");
    ui->activePluginsCounter->setStyleSheet("QLCDNumber { border: 2px ridge #f00; }");
  } else {
    ui->espList->setStyleSheet("");
    ui->activePluginsCounter->setStyleSheet("");
  }
  updatePluginCount();
}

void MainWindow::downloadFilterChanged(const QString &filter)
{
  if (!filter.isEmpty()) {
    ui->downloadView->setStyleSheet("QTreeView { border: 2px ridge #f00; }");
  } else {
    ui->downloadView->setStyleSheet("");
  }
}

void MainWindow::expandModList(const QModelIndex &index)
{
  QAbstractItemModel *model = ui->modList->model();

  for (int i = 0; i < model->rowCount(); ++i) {
    QModelIndex targetIdx = model->index(i, 0);
    if (model->data(targetIdx).toString() == index.data().toString()) {
      ui->modList->expand(targetIdx);
      break;
    }
  }
}


bool MainWindow::addProfile()
{
  QComboBox *profileBox = findChild<QComboBox*>("profileBox");
  bool okClicked = false;

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
  QDirIterator dirIter(QApplication::applicationDirPath() + "/tutorials", QStringList("*.js"), QDir::Files);
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
      "no you can always start the tutorial from the \"Help\"-menu."),
    QMessageBox::Yes | QMessageBox::No);

  dlg.setCheckBox(new QCheckBox(tr("Never ask to show tutorials")));

  const auto r = dlg.exec();

  if (dlg.checkBox()->isChecked()) {
    GlobalSettings::setHideTutorialQuestion(true);
  }

  return (r == QMessageBox::Yes);
}

void MainWindow::showEvent(QShowEvent *event)
{
  QMainWindow::showEvent(event);

  if (!m_WasVisible) {
    readSettings();
    refreshFilters();

    // this needs to be connected here instead of in the constructor because the
    // actual changing of the stylesheet is done by MOApplication, which
    // connects its signal in runApplication() (in main.cpp), and that happens
    // _after_ the MainWindow is constructed, but _before_ it is shown
    //
    // by connecting the event here, changing the style setting will first be
    // handled by MOApplication, and then in updateStyle(), at which point the
    // stylesheet has already been set correctly
    connect(this, SIGNAL(styleChanged(QString)), this, SLOT(updateStyle(QString)));

    // only the first time the window becomes visible
    m_Tutorial.registerControl();

    hookUpWindowTutorials();

    if (m_OrganizerCore.settings().firstStart()) {
      QString firstStepsTutorial = ToQString(AppConfig::firstStepsTutorial());
      if (TutorialManager::instance().hasTutorial(firstStepsTutorial)) {
        if (shouldStartTutorial()) {
          TutorialManager::instance().activateTutorial("MainWindow", firstStepsTutorial);
        }
      } else {
        log::error("{} missing", firstStepsTutorial);
        QPoint pos = ui->toolBar->mapToGlobal(QPoint());
        pos.rx() += ui->toolBar->width() / 2;
        pos.ry() += ui->toolBar->height();
        QWhatsThis::showText(pos,
            QObject::tr("Please use \"Help\" from the toolbar to get usage instructions to all elements"));
      }

      m_OrganizerCore.settings().setFirstStart(false);
    }

    m_OrganizerCore.settings().widgets().restoreIndex(ui->groupCombo);

    m_OrganizerCore.settings().nexus().registerAsNXMHandler(false);
    m_WasVisible = true;
    updateProblemsButton();

    // Notify plugin that the UI is initialized:
    emit userInterfaceInitialized();
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
    if (QMessageBox::question(this, tr("Downloads in progress"),
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

void MainWindow::displaySaveGameInfo(QListWidgetItem *newItem)
{
  // don't display the widget if the main window doesn't have focus
  //
  // this goes against the standard behaviour for tooltips, which are displayed
  // on hover regardless of focus, but this widget is so large and busy that
  // it's probably better this way
  if (!isActiveWindow()){
    return;
  }

  QString const &save = newItem->data(Qt::UserRole).toString();
  if (m_CurrentSaveView == nullptr) {
    IPluginGame const *game = m_OrganizerCore.managedGame();
    SaveGameInfo const *info = game->feature<SaveGameInfo>();
    if (info != nullptr) {
      m_CurrentSaveView = info->getSaveGameWidget(this);
    }
    if (m_CurrentSaveView == nullptr) {
      return;
    }
  }
  m_CurrentSaveView->setSave(save);

  QWindow *window = m_CurrentSaveView->window()->windowHandle();
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
  m_CurrentSaveView->setProperty("displayItem", QVariant::fromValue(static_cast<void *>(newItem)));
}


void MainWindow::saveSelectionChanged(QListWidgetItem *newItem)
{
  if (newItem == nullptr) {
    hideSaveGameInfo();
  } else if (m_CurrentSaveView == nullptr || newItem != m_CurrentSaveView->property("displayItem").value<void*>()) {
    displaySaveGameInfo(newItem);
  }
}


void MainWindow::hideSaveGameInfo()
{
  if (m_CurrentSaveView != nullptr) {
    m_CurrentSaveView->deleteLater();
    m_CurrentSaveView = nullptr;
  }
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
  if ((object == ui->savegameList) &&
      ((event->type() == QEvent::Leave) || (event->type() == QEvent::WindowDeactivate))) {
    hideSaveGameInfo();
  } else if (event->type() == QEvent::StatusTip && object != this) {
    QMainWindow::event(event);
    return true;
  }

  return false;
}


void MainWindow::toolPluginInvoke()
{
  QAction *triggeredAction = qobject_cast<QAction*>(sender());
  IPluginTool *plugin = qobject_cast<IPluginTool*>(triggeredAction->data().value<QObject*>());
  if (plugin != nullptr) {
    try {
      plugin->display();
    } catch (const std::exception &e) {
      reportError(tr("Plugin \"%1\" failed: %2").arg(plugin->name()).arg(e.what()));
    } catch (...) {
      reportError(tr("Plugin \"%1\" failed").arg(plugin->name()));
    }
  }
}

void MainWindow::modPagePluginInvoke()
{
  QAction *triggeredAction = qobject_cast<QAction*>(sender());
  IPluginModPage *plugin = qobject_cast<IPluginModPage*>(triggeredAction->data().value<QObject*>());
  if (plugin != nullptr) {
    if (plugin->useIntegratedBrowser()) {

      if (!m_IntegratedBrowser) {
        m_IntegratedBrowser.reset(new BrowserDialog);

        connect(
          m_IntegratedBrowser.get(), SIGNAL(requestDownload(QUrl,QNetworkReply*)),
          &m_OrganizerCore, SLOT(requestDownload(QUrl,QNetworkReply*)));
      }

      m_IntegratedBrowser->setWindowTitle(plugin->displayName());
      m_IntegratedBrowser->openUrl(plugin->pageURL());
    } else {
      QDesktopServices::openUrl(QUrl(plugin->pageURL()));
    }
  }
}

void MainWindow::registerPluginTool(IPluginTool *tool, QString name, QMenu *menu)
{
  if (!menu) {
    menu = ui->actionTool->menu();
  }

  if (name.isEmpty())
    name = tool->displayName();

  QAction *action = new QAction(tool->icon(), name, menu);
  action->setToolTip(tool->tooltip());
  tool->setParentWidget(this);
  action->setData(QVariant::fromValue((QObject*)tool));
  connect(action, SIGNAL(triggered()), this, SLOT(toolPluginInvoke()), Qt::QueuedConnection);

  menu->addAction(action);
}

void MainWindow::registerPluginTools(std::vector<IPluginTool *> toolPlugins)
{
  // Sort the plugins by display name
  std::sort(toolPlugins.begin(), toolPlugins.end(),
    [](IPluginTool *left, IPluginTool *right) {
      return left->displayName().toLower() < right->displayName().toLower();
    }
  );

  // Remove inactive plugins
  toolPlugins.erase(
    std::remove_if(toolPlugins.begin(), toolPlugins.end(), [](IPluginTool *plugin) -> bool { return !plugin->isActive(); }),
    toolPlugins.end()
    );

  // Group the plugins into submenus
  QMap<QString, QList<QPair<QString, IPluginTool *>>> submenuMap;
  for (auto toolPlugin : toolPlugins) {
    QStringList toolName = toolPlugin->displayName().split("/");
    QString submenu = toolName[0];
    toolName.pop_front();
    submenuMap[submenu].append(QPair<QString, IPluginTool *>(toolName.join("/"), toolPlugin));
  }

  // Start registering plugins
  for (auto submenuKey : submenuMap.keys()) {
    if (submenuMap[submenuKey].length() > 1) {
      QMenu *submenu = new QMenu(submenuKey, this);
      for (auto info : submenuMap[submenuKey]) {
        registerPluginTool(info.second, info.first, submenu);
      }
      ui->actionTool->menu()->addMenu(submenu);
    }
    else {
      registerPluginTool(submenuMap[submenuKey].front().second);
    }
  }
}

void MainWindow::registerModPage(IPluginModPage *modPage)
{
  // turn the browser action into a drop-down menu if necessary
  if (!m_browseModPage) {
    m_browseModPage = new QAction(ui->actionNexus->icon(), tr("Browse Mod Page"), this);
    setupActionMenu(m_browseModPage);

    m_browseModPage->menu()->addAction(ui->actionNexus);

    ui->toolBar->insertAction(ui->actionNexus, m_browseModPage);
    ui->toolBar->removeAction(ui->actionNexus);
  }

  QAction *action = new QAction(modPage->icon(), modPage->displayName(), this);
  modPage->setParentWidget(this);
  action->setData(QVariant::fromValue(reinterpret_cast<QObject*>(modPage)));

  connect(action, SIGNAL(triggered()), this, SLOT(modPagePluginInvoke()), Qt::QueuedConnection);

  m_browseModPage->menu()->addAction(action);
}


void MainWindow::startExeAction()
{
  QAction *action = qobject_cast<QAction*>(sender());

  if (action == nullptr) {
    log::error("not an action?");
    return;
  }

  const auto& list = *m_OrganizerCore.executablesList();

  const auto title = action->text();
  auto itor = list.find(title);

  if (itor == list.end()) {
    log::warn("startExeAction(): executable '{}' not found", title);
    return;
  }

  action->setEnabled(false);
  Guard g([&]{ action->setEnabled(true); });

  m_OrganizerCore.processRunner()
    .setFromExecutable(*itor)
    .setWaitForCompletion(ProcessRunner::Refresh)
    .run();
}

void MainWindow::activateSelectedProfile()
{
  m_OrganizerCore.setCurrentProfile(ui->profileBox->currentText());

  m_ModListSortProxy->setProfile(m_OrganizerCore.currentProfile());

  refreshSaveList();
  m_OrganizerCore.refresh();
  updateModCount();
  updatePluginCount();
  ui->statusBar->updateNormalMessage(m_OrganizerCore);
}

void MainWindow::on_profileBox_currentIndexChanged(int index)
{
  if (ui->profileBox->isEnabled()) {
    int previousIndex = m_OldProfileIndex;
    m_OldProfileIndex = index;

    if ((previousIndex != -1) &&
        (m_OrganizerCore.currentProfile() != nullptr) &&
        m_OrganizerCore.currentProfile()->exists()) {
      m_OrganizerCore.saveCurrentLists();
    }

    // Avoid doing any refresh if currentProfile is already set but previous index was -1
    // as it means that this is happening during initialization so everything has already been set.
    if (previousIndex == -1
        && m_OrganizerCore.currentProfile() != nullptr
        && m_OrganizerCore.currentProfile()->exists()
        && ui->profileBox->currentText() == m_OrganizerCore.currentProfile()->name()){
      return;
    }

    // ensure the new index is valid
    if (index < 0 || index >= ui->profileBox->count()) {
      log::debug("invalid profile index, using last profile");
      ui->profileBox->setCurrentIndex(ui->profileBox->count() - 1);
    }

    if (ui->profileBox->currentIndex() == 0) {
      ui->profileBox->setCurrentIndex(previousIndex);
      ProfilesDialog(ui->profileBox->currentText(), m_OrganizerCore, this).exec();
      while (!refreshProfiles()) {
        ProfilesDialog(ui->profileBox->currentText(), m_OrganizerCore, this).exec();
      }
    } else {
      activateSelectedProfile();
    }

    LocalSavegames *saveGames = m_OrganizerCore.managedGame()->feature<LocalSavegames>();
    if (saveGames != nullptr) {
      if (saveGames->prepareProfile(m_OrganizerCore.currentProfile()))
        refreshSaveList();
    }

    BSAInvalidation *invalidation = m_OrganizerCore.managedGame()->feature<BSAInvalidation>();
    if (invalidation != nullptr) {
      if (invalidation->prepareProfile(m_OrganizerCore.currentProfile()))
        QTimer::singleShot(5, &m_OrganizerCore, SLOT(profileRefresh()));
    }
  }
}

bool MainWindow::refreshProfiles(bool selectProfile)
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
      reportError(QObject::tr("failed to parse profile %1: %2").arg(profileIter.fileName()).arg(error.what()));
    }
  }

  // now select one of the profiles, preferably the one that was selected before
  profileBox->blockSignals(false);

  if (selectProfile) {
    if (profileBox->count() > 1) {
      profileBox->setCurrentText(currentProfileName);
      if (profileBox->currentIndex() == 0) {
        profileBox->setCurrentIndex(1);
      }
    }
  }
  return profileBox->count() > 1;
}


void MainWindow::refreshExecutablesList()
{
  QAbstractItemModel *model = ui->executablesListBox->model();

  auto add = [&](const QString& title, const QFileInfo& binary) {
    QIcon icon;
    if (!binary.fileName().isEmpty()) {
      icon = iconForExecutable(binary.filePath());
    }

    ui->executablesListBox->addItem(icon, title);

    const auto i = ui->executablesListBox->count() - 1;

    model->setData(
      model->index(i, 0),
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

void MainWindow::refreshSavesIfOpen()
{
  if (ui->tabWidget->currentWidget() == ui->savesTab) {
    refreshSaveList();
  }
}

QDir MainWindow::currentSavesDir() const
{
  QDir savesDir;
  if (m_OrganizerCore.currentProfile()->localSavesEnabled()) {
    savesDir.setPath(m_OrganizerCore.currentProfile()->savePath());
  } else {
    auto iniFiles = m_OrganizerCore.managedGame()->iniFiles();

    if (iniFiles.isEmpty()) {
      return m_OrganizerCore.managedGame()->savesDirectory();
    }

    QString iniPath = m_OrganizerCore.currentProfile()->absoluteIniFilePath(iniFiles[0]);

    wchar_t path[MAX_PATH];
    if (::GetPrivateProfileStringW(
          L"General", L"SLocalSavePath", L"",
          path, MAX_PATH,
          iniPath.toStdWString().c_str()
    )) {
      savesDir.setPath(m_OrganizerCore.managedGame()->documentsDirectory().absoluteFilePath(QString::fromWCharArray(path)));
    }
    else {
      savesDir = m_OrganizerCore.managedGame()->savesDirectory();
    }
  }

  return savesDir;
}

void MainWindow::startMonitorSaves()
{
  stopMonitorSaves();

  QDir savesDir = currentSavesDir();

  m_SavesWatcher.addPath(savesDir.absolutePath());
}

void MainWindow::stopMonitorSaves()
{
  if (m_SavesWatcher.directories().length() > 0) {
    m_SavesWatcher.removePaths(m_SavesWatcher.directories());
  }
}

void MainWindow::refreshSaveList()
{
  ui->savegameList->clear();

  startMonitorSaves(); // re-starts monitoring

  QStringList filters;
  filters << QString("*.") + m_OrganizerCore.managedGame()->savegameExtension();

  QDir savesDir = currentSavesDir();
  savesDir.setNameFilters(filters);
  savesDir.setFilter(QDir::Files);
  QDirIterator it(savesDir, QDirIterator::Subdirectories);
  log::debug("reading save games from {}", savesDir.absolutePath());

  QFileInfoList files;
  while (it.hasNext()) {
    it.next();
    files.append(it.fileInfo());
  }
  std::sort(files.begin(), files.end(), [](auto const& lhs, auto const& rhs) {
    return lhs.fileTime(QFileDevice::FileModificationTime) < rhs.fileTime(QFileDevice::FileModificationTime);
  });

  for (const QFileInfo &file : files) {
    QListWidgetItem *item = new QListWidgetItem(savesDir.relativeFilePath(file.absoluteFilePath()));
    item->setData(Qt::UserRole, file.absoluteFilePath());
    ui->savegameList->addItem(item);
  }
}


static bool BySortValue(const std::pair<UINT32, QTreeWidgetItem*> &LHS, const std::pair<UINT32, QTreeWidgetItem*> &RHS)
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

void MainWindow::updateBSAList(const QStringList &defaultArchives, const QStringList &activeArchives)
{
  m_DefaultArchives = defaultArchives;
  ui->bsaList->clear();
  ui->bsaList->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  std::vector<std::pair<UINT32, QTreeWidgetItem*>> items;

  BSAInvalidation * invalidation = m_OrganizerCore.managedGame()->feature<BSAInvalidation>();
  std::vector<FileEntryPtr> files = m_OrganizerCore.directoryStructure()->getFiles();

  QStringList plugins = m_OrganizerCore.findFiles("", [](const QString &fileName) -> bool {
    return fileName.endsWith(".esp", Qt::CaseInsensitive)
      || fileName.endsWith(".esm", Qt::CaseInsensitive)
      || fileName.endsWith(".esl", Qt::CaseInsensitive);
  });

  auto hasAssociatedPlugin = [&](const QString &bsaName) -> bool {
    for (const QString &pluginName : plugins) {
      QFileInfo pluginInfo(pluginName);
      if (bsaName.startsWith(QFileInfo(pluginName).baseName(), Qt::CaseInsensitive)
        && (m_OrganizerCore.pluginList()->state(pluginInfo.fileName()) == IPluginList::STATE_ACTIVE)) {
        return true;
      }
    }
    return false;
  };

  for (FileEntryPtr current : files) {
    QFileInfo fileInfo(ToQString(current->getName().c_str()));

    if (fileInfo.suffix().toLower() == "bsa" || fileInfo.suffix().toLower() == "ba2") {
      int index = activeArchives.indexOf(fileInfo.fileName());
      if (index == -1) {
        index = 0xFFFF;
      }
      else {
        index += 2;
      }

      if ((invalidation != nullptr) && invalidation->isInvalidationBSA(fileInfo.fileName())) {
        index = 1;
      }

      int originId = current->getOrigin();
      FilesOrigin & origin = m_OrganizerCore.directoryStructure()->getOriginByID(originId);

      QTreeWidgetItem * newItem = new QTreeWidgetItem(QStringList()
        << fileInfo.fileName()
        << ToQString(origin.getName()));
      newItem->setData(0, Qt::UserRole, index);
      newItem->setData(1, Qt::UserRole, originId);
      newItem->setFlags(newItem->flags() & ~(Qt::ItemIsDropEnabled | Qt::ItemIsUserCheckable));
      newItem->setCheckState(0, (index != -1) ? Qt::Checked : Qt::Unchecked);
      newItem->setData(0, Qt::UserRole, false);
      if (m_OrganizerCore.settings().game().forceEnableCoreFiles()
        && defaultArchives.contains(fileInfo.fileName())) {
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
      if (index < 0) index = 0;

      UINT32 sortValue = ((origin.getPriority() & 0xFFFF) << 16) | (index & 0xFFFF);
      items.push_back(std::make_pair(sortValue, newItem));
    }
  }
  std::sort(items.begin(), items.end(), BySortValue);

  for (auto iter = items.begin(); iter != items.end(); ++iter) {
    int originID = iter->second->data(1, Qt::UserRole).toInt();

    const FilesOrigin& origin = m_OrganizerCore.directoryStructure()->getOriginByID(originID);

    QString modName;
    const unsigned int modIndex = ModInfo::getIndex(ToQString(origin.getName()));

    if (modIndex == UINT_MAX) {
      modName = UnmanagedModName();
    } else {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
      modName = modInfo->name();
    }

    QList<QTreeWidgetItem*> items = ui->bsaList->findItems(modName, Qt::MatchFixedString);
    QTreeWidgetItem * subItem = nullptr;
    if (items.length() > 0) {
      subItem = items.at(0);
    }
    else {
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
  DataArchives * archives = m_OrganizerCore.managedGame()->feature<DataArchives>();

  if (archives != nullptr) {
    ui->bsaList->blockSignals(true);
    ON_BLOCK_EXIT([&]() { ui->bsaList->blockSignals(false); });

    QStringList defaultArchives = archives->archives(m_OrganizerCore.currentProfile());

    bool warning = false;

    for (int i = 0; i < ui->bsaList->topLevelItemCount(); ++i) {
      bool modWarning = false;
      QTreeWidgetItem * tlItem = ui->bsaList->topLevelItem(i);
      for (int j = 0; j < tlItem->childCount(); ++j) {
        QTreeWidgetItem * item = tlItem->child(j);
        QString filename = item->text(0);
        item->setIcon(0, QIcon());
        item->setToolTip(0, QString());

        if (item->checkState(0) == Qt::Unchecked) {
          if (defaultArchives.contains(filename)) {
            item->setIcon(0, QIcon(":/MO/gui/warning"));
            item->setToolTip(0, tr("This bsa is enabled in the ini file so it may be required!"));
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
    ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
    std::set<int> categories = modInfo->getCategories();
    for (std::set<int>::iterator iter = categories.begin();
         iter != categories.end(); ++iter) {
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
  QProgressDialog busyDialog(tr("Activating Network Proxy"), QString(), 0, 0, parentWidget());
  busyDialog.setWindowFlags(busyDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
  busyDialog.setWindowModality(Qt::WindowModal);
  busyDialog.show();

  QFutureWatcher<void> futureWatcher;
  QEventLoop loop;
  connect(&futureWatcher, &QFutureWatcher<void>::finished,
          &loop, &QEventLoop::quit,
          Qt::QueuedConnection);

  futureWatcher.setFuture(
    QtConcurrent::run(MainWindow::setupNetworkProxy, activate)
  );

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

  s.widgets().restoreIndex(ui->groupCombo);
  m_Filters->restoreState(s);

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

void MainWindow::processUpdates() {
  auto& settings = m_OrganizerCore.settings();
  const auto earliest = QVersionNumber::fromString("2.1.2").normalized();

  const auto lastVersion = settings.version().value_or(earliest);
  const auto currentVersion = m_OrganizerCore.getVersion().asQVersionNumber();

  settings.processUpdates(currentVersion, lastVersion);

  if (!settings.firstStart()) {
    if (lastVersion < QVersionNumber(2, 1, 6)) {
      ui->modList->header()->setSectionHidden(ModList::COL_NOTES, true);
    }

    if (lastVersion < QVersionNumber(2, 2, 1)) {
      // hide new columns by default
      for (int i=DownloadList::COL_MODNAME; i<DownloadList::COL_COUNT; ++i) {
        ui->downloadView->header()->hideSection(i);
      }
    }

    if (lastVersion < QVersionNumber(2, 3)) {
      for (int i=1; i<ui->dataTree->header()->count(); ++i)
        ui->dataTree->setColumnWidth(i, 150);
    }
  }

  if (currentVersion < lastVersion) {
    const auto text = tr(
      "Notice: Your current MO version (%1) is lower than the previously used one (%2). "
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
  s.geometry().saveState(ui->modList->header());

  s.widgets().saveIndex(ui->groupCombo);
  s.widgets().saveIndex(ui->executablesListBox);

  m_Filters->saveState(s);
  m_DataTab->saveState(s);

  s.interface().setFilterOptions(FilterWidget::options());
}

QMainWindow* MainWindow::mainWindow()
{
  return this;
}

void MainWindow::on_btnRefreshDownloads_clicked()
{
  m_OrganizerCore.downloadManager()->refreshList();
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
    refreshSaveList();
  }
}


void MainWindow::installMod(QString fileName)
{
  try {
    if (fileName.isEmpty()) {
      QStringList extensions = m_OrganizerCore.installationManager()->getSupportedExtensions();
      for (auto iter = extensions.begin(); iter != extensions.end(); ++iter) {
        *iter = "*." + *iter;
      }

      fileName = FileDialogMemory::getOpenFileName("installMod", this, tr("Choose Mod"), QString(),
                                                   tr("Mod Archive").append(QString(" (%1)").arg(extensions.join(" "))));
    }

    if (fileName.isEmpty()) {
      return;
    } else {
      m_OrganizerCore.installMod(fileName, false, nullptr, QString());
    }
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}

void MainWindow::on_startButton_clicked()
{
  const Executable* selectedExecutable = getSelectedExecutable();
  if (!selectedExecutable) {
    return;
  }

  ui->startButton->setEnabled(false);
  Guard g([&]{ ui->startButton->setEnabled(true); });

  m_OrganizerCore.processRunner()
    .setFromExecutable(*selectedExecutable)
    .setWaitForCompletion(ProcessRunner::Refresh)
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
  } catch (const std::exception &e) {
    reportError(e.what());
  }

  return result;
}

void MainWindow::on_executablesListBox_currentIndexChanged(int index)
{
  if (!ui->executablesListBox->isEnabled()) {
    return;
  }

  const int previousIndex =
    (m_OldExecutableIndex > 0 ? m_OldExecutableIndex : 1);

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
  QDesktopServices::openUrl(QUrl("https://modorganizer2.github.io/"));
}

void MainWindow::discordTriggered()
{
  QDesktopServices::openUrl(QUrl("https://discord.gg/cYwdcxj"));
}

void MainWindow::issueTriggered()
{
  QDesktopServices::openUrl(QUrl("https://github.com/Modorganizer2/modorganizer/issues"));
}

void MainWindow::tutorialTriggered()
{
  QAction *tutorialAction = qobject_cast<QAction*>(sender());
  if (tutorialAction != nullptr) {
    if (QMessageBox::question(this, tr("Start Tutorial?"),
          tr("You're about to start a tutorial. For technical reasons it's not possible to end "
             "the tutorial early. Continue?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      TutorialManager::instance().activateTutorial("MainWindow", tutorialAction->data().toString());
    }
  }
}


void MainWindow::on_actionInstallMod_triggered()
{
  installMod();
}

void MainWindow::on_actionAdd_Profile_triggered()
{
  for (;;) {
    ProfilesDialog profilesDialog(m_OrganizerCore.currentProfile()->name(),
                                  m_OrganizerCore,
                                  this);

    // workaround: need to disable monitoring of the saves directory, otherwise the active
    // profile directory is locked
    stopMonitorSaves();
    profilesDialog.exec();
    refreshSaveList(); // since the save list may now be outdated we have to refresh it completely

    if (refreshProfiles() && !profilesDialog.failed()) {
      break;
    }
  }

  LocalSavegames *saveGames = m_OrganizerCore.managedGame()->feature<LocalSavegames>();
  if (saveGames != nullptr) {
    if (saveGames->prepareProfile(m_OrganizerCore.currentProfile()))
      refreshSaveList();
  }

  BSAInvalidation *invalidation = m_OrganizerCore.managedGame()->feature<BSAInvalidation>();
  if (invalidation != nullptr) {
    if (invalidation->prepareProfile(m_OrganizerCore.currentProfile()))
      QTimer::singleShot(5, &m_OrganizerCore, SLOT(profileRefresh()));
  }
}

void MainWindow::on_actionModify_Executables_triggered()
{
  const auto sel = (m_OldExecutableIndex > 0 ?  m_OldExecutableIndex - 1 : 0);

  if (modifyExecutablesDialog(sel)) {
    const auto newCount = ui->executablesListBox->count();
    if (m_OldExecutableIndex >= 0 && m_OldExecutableIndex < newCount) {
      ui->executablesListBox->setCurrentIndex(m_OldExecutableIndex);
    } else {
      ui->executablesListBox->setCurrentIndex(newCount - 1);
    }
  }
}


void MainWindow::setModListSorting(int index)
{
  Qt::SortOrder order = ((index & 0x01) != 0) ? Qt::DescendingOrder : Qt::AscendingOrder;
  int column = index >> 1;
  ui->modList->header()->setSortIndicator(column, order);
}


void MainWindow::setESPListSorting(int index)
{
  switch (index) {
    case 0: {
      ui->espList->header()->setSortIndicator(1, Qt::AscendingOrder);
    } break;
    case 1: {
      ui->espList->header()->setSortIndicator(1, Qt::DescendingOrder);
    } break;
    case 2: {
      ui->espList->header()->setSortIndicator(0, Qt::AscendingOrder);
    } break;
    case 3: {
      ui->espList->header()->setSortIndicator(0, Qt::DescendingOrder);
    } break;
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

void MainWindow::directory_refreshed()
{
  // some problem-reports may rely on the virtual directory tree so they need to be updated
  // now
  scheduleCheckForProblems();

  if (ui->tabWidget->currentWidget() == ui->dataTab) {
    m_DataTab->updateTree();
  }
}

void MainWindow::esplist_changed()
{
  updatePluginCount();
}

void MainWindow::modorder_changed()
{
  for (unsigned int i = 0; i < m_OrganizerCore.currentProfile()->numMods(); ++i) {
    int priority = m_OrganizerCore.currentProfile()->getModPriority(i);
    if (m_OrganizerCore.currentProfile()->modEnabled(i)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      // priorities in the directory structure are one higher because data is 0
      m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(modInfo->internalName())).setPriority(priority + 1);
    }
  }
  m_OrganizerCore.refreshBSAList();
  m_OrganizerCore.currentProfile()->writeModlist();
  m_ArchiveListWriter.write();
  m_OrganizerCore.directoryStructure()->getFileRegister()->sortOrigins();

  { // refresh selection
    QModelIndex current = ui->modList->currentIndex();
    if (current.isValid()) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(current.data(Qt::UserRole + 1).toInt());
      // clear caches on all mods conflicting with the moved mod
      for (int i :  modInfo->getModOverwrite()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i :  modInfo->getModOverwritten()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i : modInfo->getModArchiveOverwrite()) {
          ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i : modInfo->getModArchiveOverwritten()) {
          ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i : modInfo->getModArchiveLooseOverwrite()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i : modInfo->getModArchiveLooseOverwritten()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      // update conflict check on the moved mod
      modInfo->doConflictCheck();
      m_OrganizerCore.modList()->setOverwriteMarkers(modInfo->getModOverwrite(), modInfo->getModOverwritten());
      m_OrganizerCore.modList()->setArchiveOverwriteMarkers(modInfo->getModArchiveOverwrite(), modInfo->getModArchiveOverwritten());
      m_OrganizerCore.modList()->setArchiveLooseOverwriteMarkers(modInfo->getModArchiveLooseOverwrite(), modInfo->getModArchiveLooseOverwritten());
      if (m_ModListSortProxy != nullptr)
        m_ModListSortProxy->invalidate();
      ui->modList->verticalScrollBar()->repaint();
    }
  }
}

void MainWindow::modInstalled(const QString &modName)
{
  QModelIndexList posList =
      m_OrganizerCore.modList()->match(m_OrganizerCore.modList()->index(0, 0), Qt::DisplayRole, modName);
  if (posList.count() == 1) {
    ui->modList->scrollTo(posList.at(0));
  }

  // force an update to happen
  std::multimap<QString, int> IDs;
  ModInfo::Ptr info = ModInfo::getByIndex(ModInfo::getIndex(modName));
  IDs.insert(std::make_pair<QString, int>(info->gameName(), info->nexusId()));
  modUpdateCheck(IDs);
}

void MainWindow::showMessage(const QString &message)
{
  MessageDialog::showMessage(message, this);
}

void MainWindow::showError(const QString &message)
{
  reportError(message);
}

void MainWindow::installMod_clicked()
{
  installMod();
}

void MainWindow::modRenamed(const QString &oldName, const QString &newName)
{
  Profile::renameModInAllProfiles(oldName, newName);

  // immediately refresh the active profile because the data in memory is invalid
  m_OrganizerCore.currentProfile()->refreshModStatus();

  // also fix the directory structure
  try {
    if (m_OrganizerCore.directoryStructure()->originExists(ToWString(oldName))) {
      FilesOrigin &origin = m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(oldName));
      origin.setName(ToWString(newName));
    } else {

    }
  } catch (const std::exception &e) {
    reportError(tr("failed to change origin name: %1").arg(e.what()));
  }
}

void MainWindow::fileMoved(const QString &filePath, const QString &oldOriginName, const QString &newOriginName)
{
  const FileEntryPtr filePtr = m_OrganizerCore.directoryStructure()->findFile(ToWString(filePath));
  if (filePtr.get() != nullptr) {
    try {
      if (m_OrganizerCore.directoryStructure()->originExists(ToWString(newOriginName))) {
        FilesOrigin &newOrigin = m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(newOriginName));

        QString fullNewPath = ToQString(newOrigin.getPath()) + "\\" + filePath;
        WIN32_FIND_DATAW findData;
        HANDLE hFind;
        hFind = ::FindFirstFileW(ToWString(fullNewPath).c_str(), &findData);
        filePtr->addOrigin(newOrigin.getID(), findData.ftCreationTime, L"", -1);
        FindClose(hFind);
      }
      if (m_OrganizerCore.directoryStructure()->originExists(ToWString(oldOriginName))) {
        FilesOrigin &oldOrigin = m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(oldOriginName));
        filePtr->removeOrigin(oldOrigin.getID());
      }
    } catch (const std::exception &e) {
      reportError(tr("failed to move \"%1\" from mod \"%2\" to \"%3\": %4").arg(filePath).arg(oldOriginName).arg(newOriginName).arg(e.what()));
    }
  } else {
    // this is probably not an error, the specified path is likely a directory
  }
}


void MainWindow::renameMod_clicked()
{
  try {
    ui->modList->edit(ui->modList->currentIndex());
  } catch (const std::exception &e) {
    reportError(tr("failed to rename mod: %1").arg(e.what()));
  }
}


void MainWindow::restoreBackup_clicked()
{
  QRegExp backupRegEx("(.*)_backup[0-9]*$");
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  if (backupRegEx.indexIn(modInfo->name()) != -1) {
    QString regName = backupRegEx.cap(1);
    QDir modDir(QDir::fromNativeSeparators(m_OrganizerCore.settings().paths().mods()));
    if (!modDir.exists(regName) ||
        (QMessageBox::question(this, tr("Overwrite?"),
          tr("This will replace the existing mod \"%1\". Continue?").arg(regName),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
      if (modDir.exists(regName) && !shellDelete(QStringList(modDir.absoluteFilePath(regName)))) {
        reportError(tr("failed to remove mod \"%1\"").arg(regName));
      } else {
        QString destinationPath = QDir::fromNativeSeparators(m_OrganizerCore.settings().paths().mods()) + "/" + regName;
        if (!modDir.rename(modInfo->absolutePath(), destinationPath)) {
          reportError(tr("failed to rename \"%1\" to \"%2\"").arg(modInfo->absolutePath()).arg(destinationPath));
        }
        m_OrganizerCore.refresh();
      }
    }
  }
}

void MainWindow::modlistChanged(const QModelIndex&, int)
{
  m_OrganizerCore.currentProfile()->writeModlist();
  updateModCount();
}

void MainWindow::modlistChanged(const QModelIndexList&, int)
{
  m_OrganizerCore.currentProfile()->writeModlist();
  updateModCount();
}

void MainWindow::modlistSelectionsChanged(const QItemSelection &selected)
{
  if (selected.count()) {
    auto selection = selected.last();
    auto index = selection.indexes().last();
    ModInfo::Ptr selectedMod = ModInfo::getByIndex(index.data(Qt::UserRole + 1).toInt());
    m_OrganizerCore.modList()->setOverwriteMarkers(selectedMod->getModOverwrite(), selectedMod->getModOverwritten());
    m_OrganizerCore.modList()->setArchiveOverwriteMarkers(selectedMod->getModArchiveOverwrite(), selectedMod->getModArchiveOverwritten());
    m_OrganizerCore.modList()->setArchiveLooseOverwriteMarkers(selectedMod->getModArchiveLooseOverwrite(), selectedMod->getModArchiveLooseOverwritten());
  } else {
    m_OrganizerCore.modList()->setOverwriteMarkers(std::set<unsigned int>(), std::set<unsigned int>());
    m_OrganizerCore.modList()->setArchiveOverwriteMarkers(std::set<unsigned int>(), std::set<unsigned int>());
    m_OrganizerCore.modList()->setArchiveLooseOverwriteMarkers(std::set<unsigned int>(), std::set<unsigned int>());
  }
  ui->modList->verticalScrollBar()->repaint();

  m_OrganizerCore.pluginList()->highlightPlugins(ui->modList->selectionModel(), *m_OrganizerCore.directoryStructure(), *m_OrganizerCore.currentProfile());
  ui->espList->verticalScrollBar()->repaint();
}

void MainWindow::esplistSelectionsChanged(const QItemSelection &selected)
{
  m_OrganizerCore.modList()->highlightMods(ui->espList->selectionModel(), *m_OrganizerCore.directoryStructure());
  ui->modList->verticalScrollBar()->repaint();
}

void MainWindow::modListSortIndicatorChanged(int, Qt::SortOrder)
{
  ui->modList->verticalScrollBar()->repaint();
}

void MainWindow::modListSectionResized(int logicalIndex, int oldSize, int newSize)
{
  bool enabled = (newSize != 0);
  qobject_cast<ModListSortProxy *>(ui->modList->model())->setColumnVisible(logicalIndex, enabled);
}

void MainWindow::removeMod_clicked()
{
  const int max_items = 20;

  try {
    QItemSelectionModel *selection = ui->modList->selectionModel();
    if (selection->hasSelection() && selection->selectedRows().count() > 1) {
      QString mods;
      QStringList modNames;

      int i = 0;
      for (QModelIndex idx : selection->selectedRows()) {
        QString name = idx.data().toString();
        if (!ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->isRegular()) {
          continue;
        }

        // adds an item for the mod name until `i` reaches `max_items`, which
        // adds one "..." item; subsequent mods are not shown on the list but
        // are still added to `modNames` below so they can be removed correctly

        if (i < max_items) {
          mods += "<li>" + name + "</li>";
        }
        else if (i == max_items) {
          mods += "<li>...</li>";
        }

        modNames.append(ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->name());
        ++i;
      }
      if (QMessageBox::question(this, tr("Confirm"),
                                tr("Remove the following mods?<br><ul>%1</ul>").arg(mods),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        // use mod names instead of indexes because those become invalid during the removal
        DownloadManager::startDisableDirWatcher();
        for (QString name : modNames) {
          m_OrganizerCore.modList()->removeRowForce(ModInfo::getIndex(name), QModelIndex());
        }
        DownloadManager::endDisableDirWatcher();
      }
    } else {
      m_OrganizerCore.modList()->removeRow(m_ContextRow, QModelIndex());
    }
    updateModCount();
    updatePluginCount();
  } catch (const std::exception &e) {
    reportError(tr("failed to remove mod: %1").arg(e.what()));
  }
}


void MainWindow::modRemoved(const QString &fileName)
{
  if (!fileName.isEmpty() && !QFileInfo(fileName).isAbsolute()) {
    m_OrganizerCore.downloadManager()->markUninstalled(fileName);
  }
}


void MainWindow::reinstallMod_clicked()
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  QString installationFile = modInfo->installationFile();
  if (installationFile.length() != 0) {
    QString fullInstallationFile;
    QFileInfo fileInfo(installationFile);
    if (fileInfo.isAbsolute()) {
      if (fileInfo.exists()) {
        fullInstallationFile = installationFile;
      } else {
        fullInstallationFile = m_OrganizerCore.downloadManager()->getOutputDirectory() + "/" + fileInfo.fileName();
      }
    } else {
      fullInstallationFile = m_OrganizerCore.downloadManager()->getOutputDirectory() + "/" + installationFile;
    }
    if (QFile::exists(fullInstallationFile)) {
      m_OrganizerCore.installMod(fullInstallationFile, true, modInfo, modInfo->name());
    } else {
      QMessageBox::information(this, tr("Failed"), tr("Installation file no longer exists"));
    }
  } else {
    QMessageBox::information(this, tr("Failed"),
                             tr("Mods installed with old versions of MO can't be reinstalled in this way."));
  }
}

void MainWindow::backupMod_clicked()
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  QString backupDirectory = m_OrganizerCore.installationManager()->generateBackupName(modInfo->absolutePath());
  if (!copyDir(modInfo->absolutePath(), backupDirectory, false)) {
    QMessageBox::information(this, tr("Failed"),
      tr("Failed to create backup."));
  }
  m_OrganizerCore.refresh();
}

void MainWindow::resumeDownload(int downloadIndex)
{
  m_OrganizerCore.loggedInAction(this, [this, downloadIndex] {
    m_OrganizerCore.downloadManager()->resumeDownload(downloadIndex);
  });
}


void MainWindow::endorseMod(ModInfo::Ptr mod)
{
  m_OrganizerCore.loggedInAction(this, [this, mod] {
    mod->endorse(true);
  });
}


void MainWindow::endorse_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();

  m_OrganizerCore.loggedInAction(this, [this] {
    QItemSelectionModel *selection = ui->modList->selectionModel();
    if (selection->hasSelection() && selection->selectedRows().count() > 1) {
      MessageDialog::showMessage(tr("Endorsing multiple mods will take a while. Please wait..."), this);
    }

    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->endorse(true);
    }
  });
}

void MainWindow::dontendorse_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->setNeverEndorse();
    }
  }
  else {
    ModInfo::getByIndex(m_ContextRow)->setNeverEndorse();
  }
}


void MainWindow::unendorseMod(ModInfo::Ptr mod)
{
  m_OrganizerCore.loggedInAction(this, [mod] {
    mod->endorse(false);
  });
}


void MainWindow::unendorse_clicked()
{
  m_OrganizerCore.loggedInAction(this, [this] {
    QItemSelectionModel *selection = ui->modList->selectionModel();
    if (selection->hasSelection() && selection->selectedRows().count() > 1) {
      MessageDialog::showMessage(tr("Unendorsing multiple mods will take a while. Please wait..."), this);
    }

    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->endorse(false);
    }
  });
}


void MainWindow::trackMod(ModInfo::Ptr mod, bool doTrack)
{
  m_OrganizerCore.loggedInAction(this, [mod, doTrack] {
    mod->track(doTrack);
  });
}


void MainWindow::track_clicked()
{
  m_OrganizerCore.loggedInAction(this, [this] {
    QItemSelectionModel *selection = ui->modList->selectionModel();
    for (auto idx : selection->selectedRows()) {
      ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->track(true);
    }
  });
}

void MainWindow::untrack_clicked()
{
  m_OrganizerCore.loggedInAction(this, [this] {
    QItemSelectionModel *selection = ui->modList->selectionModel();
    for (auto idx : selection->selectedRows()) {
      ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->track(false);
    }
  });
}

void MainWindow::windowTutorialFinished(const QString &windowName)
{
  m_OrganizerCore.settings().interface().setTutorialCompleted(windowName);
}

void MainWindow::overwriteClosed(int)
{
  OverwriteInfoDialog *dialog = this->findChild<OverwriteInfoDialog*>("__overwriteDialog");
  if (dialog != nullptr) {
    m_OrganizerCore.modList()->modInfoChanged(dialog->modInfo());
    dialog->deleteLater();
  }
  m_OrganizerCore.refreshDirectoryStructure();
}


void MainWindow::displayModInformation(
  ModInfo::Ptr modInfo, unsigned int modIndex, ModInfoTabIDs tabID)
{
  if (!m_OrganizerCore.modList()->modInfoAboutToChange(modInfo)) {
    log::debug("A different mod information dialog is open. If this is incorrect, please restart MO");
    return;
  }
  std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
    QDialog *dialog = this->findChild<QDialog*>("__overwriteDialog");
    try {
      if (dialog == nullptr) {
        dialog = new OverwriteInfoDialog(modInfo, this);
        dialog->setObjectName("__overwriteDialog");
      } else {
        qobject_cast<OverwriteInfoDialog*>(dialog)->setModInfo(modInfo);
      }

      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      connect(dialog, SIGNAL(finished(int)), this, SLOT(overwriteClosed(int)));
    } catch (const std::exception &e) {
      reportError(tr("Failed to display overwrite dialog: %1").arg(e.what()));
    }
  } else {
    modInfo->saveMeta();

    ModInfoDialog dialog(this, &m_OrganizerCore, &m_PluginContainer, modInfo);
    connect(&dialog, SIGNAL(originModified(int)), this, SLOT(originModified(int)));

	  //Open the tab first if we want to use the standard indexes of the tabs.
	  if (tabID != ModInfoTabIDs::None) {
		  dialog.selectTab(tabID);
	  }

    dialog.exec();

    modInfo->saveMeta();
    emit modInfoDisplayed();
    m_OrganizerCore.modList()->modInfoChanged(modInfo);
  }

  if (m_OrganizerCore.currentProfile()->modEnabled(modIndex)
      && !modInfo->hasFlag(ModInfo::FLAG_FOREIGN)) {
    FilesOrigin& origin = m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(modInfo->name()));
    origin.enable(false);

    if (m_OrganizerCore.directoryStructure()->originExists(ToWString(modInfo->name()))) {
      FilesOrigin& origin = m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(modInfo->name()));
      origin.enable(false);

      m_OrganizerCore.directoryRefresher()->addModToStructure(m_OrganizerCore.directoryStructure()
                                             , modInfo->name()
                                             , m_OrganizerCore.currentProfile()->getModPriority(modIndex)
                                             , modInfo->absolutePath()
                                             , modInfo->stealFiles()
                                             , modInfo->archives());
      DirectoryRefresher::cleanStructure(m_OrganizerCore.directoryStructure());
      m_OrganizerCore.directoryStructure()->getFileRegister()->sortOrigins();
      m_OrganizerCore.refreshLists();
    }
  }
}

bool MainWindow::closeWindow()
{
  return close();
}

void MainWindow::setWindowEnabled(bool enabled)
{
  setEnabled(enabled);
}


ModInfo::Ptr MainWindow::nextModInList()
{
  const QModelIndex start = m_ModListSortProxy->mapFromSource(
    m_OrganizerCore.modList()->index(m_ContextRow, 0));

  auto index = start;

  for (;;) {
    index = m_ModListSortProxy->index((index.row() + 1) % m_ModListSortProxy->rowCount(), 0);
    m_ContextRow = m_ModListSortProxy->mapToSource(index).row();

    if (index == start || !index.isValid()) {
      // wrapped around, give up
      break;
    }

    ModInfo::Ptr mod = ModInfo::getByIndex(m_ContextRow);

    // skip overwrite and backups and separators
    if (mod->hasFlag(ModInfo::FLAG_OVERWRITE) ||
        mod->hasFlag(ModInfo::FLAG_BACKUP) ||
        mod->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      continue;
    }

    return mod;
  }

  return {};
}

ModInfo::Ptr MainWindow::previousModInList()
{
  const QModelIndex start = m_ModListSortProxy->mapFromSource(
    m_OrganizerCore.modList()->index(m_ContextRow, 0));

  auto index = start;

  for (;;) {
    int row = index.row() - 1;
    if (row == -1) {
      row = m_ModListSortProxy->rowCount() - 1;
    }

    index = m_ModListSortProxy->index(row, 0);
    m_ContextRow = m_ModListSortProxy->mapToSource(index).row();

    if (index == start || !index.isValid()) {
      // wrapped around, give up
      break;
    }

    // skip overwrite and backups and separators
    ModInfo::Ptr mod = ModInfo::getByIndex(m_ContextRow);

    if (mod->hasFlag(ModInfo::FLAG_OVERWRITE) ||
        mod->hasFlag(ModInfo::FLAG_BACKUP) ||
        mod->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      continue;
    }

    return mod;
  }

  return {};
}

void MainWindow::displayModInformation(const QString &modName, ModInfoTabIDs tabID)
{
  unsigned int index = ModInfo::getIndex(modName);
  if (index == UINT_MAX) {
    log::error("failed to resolve mod name {}", modName);
    return;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  displayModInformation(modInfo, index, tabID);
}


void MainWindow::displayModInformation(int row, ModInfoTabIDs tabID)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);
  displayModInformation(modInfo, row, tabID);
}


void MainWindow::ignoreMissingData_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      int row_idx = idx.data(Qt::UserRole + 1).toInt();
      ModInfo::Ptr info = ModInfo::getByIndex(row_idx);
      info->markValidated(true);
      connect(this, SIGNAL(modListDataChanged(QModelIndex, QModelIndex)), m_OrganizerCore.modList(), SIGNAL(dataChanged(QModelIndex, QModelIndex)));

      emit modListDataChanged(m_OrganizerCore.modList()->index(row_idx, 0), m_OrganizerCore.modList()->index(row_idx, m_OrganizerCore.modList()->columnCount() - 1));
    }
  } else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
    info->markValidated(true);
    connect(this, SIGNAL(modListDataChanged(QModelIndex, QModelIndex)), m_OrganizerCore.modList(), SIGNAL(dataChanged(QModelIndex, QModelIndex)));

    emit modListDataChanged(m_OrganizerCore.modList()->index(m_ContextRow, 0), m_OrganizerCore.modList()->index(m_ContextRow, m_OrganizerCore.modList()->columnCount() - 1));
  }
}

void MainWindow::markConverted_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      int row_idx = idx.data(Qt::UserRole + 1).toInt();
      ModInfo::Ptr info = ModInfo::getByIndex(row_idx);
      info->markConverted(true);
      connect(this, SIGNAL(modListDataChanged(QModelIndex, QModelIndex)), m_OrganizerCore.modList(), SIGNAL(dataChanged(QModelIndex, QModelIndex)));
      emit modListDataChanged(m_OrganizerCore.modList()->index(row_idx, 0), m_OrganizerCore.modList()->index(row_idx, m_OrganizerCore.modList()->columnCount() - 1));
    }
  } else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
    info->markConverted(true);
    connect(this, SIGNAL(modListDataChanged(QModelIndex, QModelIndex)), m_OrganizerCore.modList(), SIGNAL(dataChanged(QModelIndex, QModelIndex)));
    emit modListDataChanged(m_OrganizerCore.modList()->index(m_ContextRow, 0), m_OrganizerCore.modList()->index(m_ContextRow, m_OrganizerCore.modList()->columnCount() - 1));
  }
}


void MainWindow::restoreHiddenFiles_clicked()
{
  const int max_items = 20;
  QItemSelectionModel* selection = ui->modList->selectionModel();

  QFlags<FileRenamer::RenameFlags> flags = FileRenamer::UNHIDE;
  flags |= FileRenamer::MULTIPLE;

  FileRenamer renamer(this, flags);

  FileRenamer::RenameResults result = FileRenamer::RESULT_OK;

  // multi selection
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    QString mods;
    QStringList modNames;
    int i = 0;

    for (QModelIndex idx : selection->selectedRows()) {

      QString name = idx.data().toString();
      int row_idx = idx.data(Qt::UserRole + 1).toInt();
      ModInfo::Ptr modInfo = ModInfo::getByIndex(row_idx);
      const auto flags = modInfo->getFlags();

      if (!modInfo->isRegular() ||
          std::find(flags.begin(), flags.end(), ModInfo::FLAG_HIDDEN_FILES) == flags.end()) {
        continue;
      }

      // adds an item for the mod name until `i` reaches `max_items`, which
      // adds one "..." item; subsequent mods are not shown on the list but
      // are still added to `modNames` below so they can be removed correctly
      if (i < max_items) {
        mods += "<li>" + name + "</li>";
      }
      else if (i == max_items) {
        mods += "<li>...</li>";
      }

      modNames.append(ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->name());
      ++i;
    }
    if (QMessageBox::question(this, tr("Confirm"),
        tr("Restore all hidden files in the following mods?<br><ul>%1</ul>").arg(mods),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

      for (QModelIndex idx : selection->selectedRows()) {

        int row_idx = idx.data(Qt::UserRole + 1).toInt();
        ModInfo::Ptr modInfo = ModInfo::getByIndex(row_idx);

        const auto flags = modInfo->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_HIDDEN_FILES) != flags.end()) {
          const QString modDir = modInfo->absolutePath();

          auto partialResult = restoreHiddenFilesRecursive(renamer, modDir);

          if (partialResult == FileRenamer::RESULT_CANCEL) {
            result = FileRenamer::RESULT_CANCEL;
            break;
          }
          originModified((m_OrganizerCore.directoryStructure()->getOriginByName(
            ToWString(modInfo->internalName()))).getID());
        }
      }
    }
  }
  else {
    //single selection
    ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
    const QString modDir = modInfo->absolutePath();

    if (QMessageBox::question(this, tr("Are you sure?"),
        tr("About to restore all hidden files in:\n") + modInfo->name(),
        QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok) {

      result = restoreHiddenFilesRecursive(renamer, modDir);

      originModified((m_OrganizerCore.directoryStructure()->getOriginByName(
        ToWString(modInfo->internalName()))).getID());
    }
  }

  if (result == FileRenamer::RESULT_CANCEL){
    log::debug("Restoring hidden files operation cancelled");
  }
  else {
    log::debug("Finished restoring hidden files");
  }
}


void MainWindow::visitOnNexus_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    int count = selection->selectedRows().count();
    if (count > 10) {
      if (QMessageBox::question(this, tr("Opening Nexus Links"),
            tr("You are trying to open %1 links to Nexus Mods.  Are you sure you want to do this?").arg(count),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
      }
    }
    int row_idx;
    ModInfo::Ptr info;
    QString gameName;

    for (QModelIndex idx : selection->selectedRows()) {
      row_idx = idx.data(Qt::UserRole + 1).toInt();
      info = ModInfo::getByIndex(row_idx);
      int modID = info->nexusId();
      gameName = info->gameName();
      if (modID > 0)  {
        linkClicked(NexusInterface::instance().getModURL(modID, gameName));
      } else {
        log::error("mod '{}' has no nexus id", info->name());
      }
    }
  }
  else {
    int modID = m_OrganizerCore.modList()->data(m_OrganizerCore.modList()->index(m_ContextRow, 0), Qt::UserRole).toInt();
    QString gameName = m_OrganizerCore.modList()->data(m_OrganizerCore.modList()->index(m_ContextRow, 0), Qt::UserRole + 4).toString();
    if (modID > 0)  {
      linkClicked(NexusInterface::instance().getModURL(modID, gameName));
    } else {
      MessageDialog::showMessage(tr("Nexus ID for this mod is unknown"), this);
    }
  }
}

void MainWindow::visitWebPage_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    int count = selection->selectedRows().count();
    if (count > 10) {
      if (QMessageBox::question(this, tr("Opening Web Pages"),
        tr("You are trying to open %1 Web Pages.  Are you sure you want to do this?").arg(count),
        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
      }
    }
    int row_idx;
    ModInfo::Ptr info;
    QString gameName;
    for (QModelIndex idx : selection->selectedRows()) {
      row_idx = idx.data(Qt::UserRole + 1).toInt();
      info = ModInfo::getByIndex(row_idx);

      const auto url = info->parseCustomURL();
      if (url.isValid()) {
        linkClicked(url.toString());
      }
    }
  }
  else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);

    const auto url = info->parseCustomURL();
    if (url.isValid()) {
      linkClicked(url.toString());
    }
  }
}

void MainWindow::visitNexusOrWebPage(const QModelIndex& idx)
{
  int row_idx = idx.data(Qt::UserRole + 1).toInt();

  ModInfo::Ptr info = ModInfo::getByIndex(row_idx);
  if (!info) {
    log::error("mod {} not found", row_idx);
    return;
  }

  int modID = info->nexusId();
  QString gameName = info->gameName();
  const auto url = info->parseCustomURL();

  if (modID > 0) {
    linkClicked(NexusInterface::instance().getModURL(modID, gameName));
  } else if (url.isValid()) {
    linkClicked(url.toString());
  } else {
    log::error("mod '{}' has no valid link", info->name());
  }
}

void MainWindow::visitNexusOrWebPage_clicked() {
  QItemSelectionModel* selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    int count = selection->selectedRows().count();
    if (count > 10) {
      if (QMessageBox::question(this, tr("Opening Web Pages"),
        tr("You are trying to open %1 Web Pages.  Are you sure you want to do this?").arg(count),
        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
      }
    }

    for (QModelIndex idx : selection->selectedRows()) {
      visitNexusOrWebPage(idx);
    }
  }
  else {
    QModelIndex idx = m_OrganizerCore.modList()->index(m_ContextRow, 0);
    visitNexusOrWebPage(idx);
  }
}

void MainWindow::openExplorer_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::Ptr info = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
      shell::Explore(info->absolutePath());
    }
  }
  else {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
    shell::Explore(modInfo->absolutePath());
  }
}

void MainWindow::openPluginOriginExplorer_clicked()
{
  QItemSelectionModel *selection = ui->espList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 0) {
    for (QModelIndex idx : selection->selectedRows()) {
      QString fileName = idx.data().toString();
      unsigned int modIndex = ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName));
      if (modIndex == UINT_MAX) {
        continue;
      }
      ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
      shell::Explore(modInfo->absolutePath());
    }
  }
  else {
    QModelIndex idx = selection->currentIndex();
    QString fileName = idx.data().toString();
    ModInfo::Ptr modInfo = ModInfo::getByIndex(ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)));
    shell::Explore(modInfo->absolutePath());
  }
}

void MainWindow::openExplorer_activated()
{
	if (ui->modList->hasFocus()) {
		QItemSelectionModel *selection = ui->modList->selectionModel();
		if (selection->hasSelection() && selection->selectedRows().count() == 1 ) {

			QModelIndex idx = selection->currentIndex();
			ModInfo::Ptr modInfo = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
			std::vector<ModInfo::EFlag> flags = modInfo->getFlags();

			if (modInfo->isRegular() || (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end())) {
        shell::Explore(modInfo->absolutePath());
			}

		}
	}

	if (ui->espList->hasFocus()) {
		QItemSelectionModel *selection = ui->espList->selectionModel();

		if (selection->hasSelection() && selection->selectedRows().count() == 1) {

			QModelIndex idx = selection->currentIndex();
			QString fileName = idx.data().toString();


      unsigned int modInfoIndex = ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName));
      if (modInfoIndex != UINT_MAX) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(modInfoIndex);
        std::vector<ModInfo::EFlag> flags = modInfo->getFlags();

        if (modInfo->isRegular() || (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end())) {
          shell::Explore(modInfo->absolutePath());
        }
      }
		}
	}
}

void MainWindow::refreshProfile_activated()
{
	m_OrganizerCore.profileRefresh();
}

void MainWindow::updateModCount()
{
  int activeCount = 0;
  int visActiveCount = 0;
  int backupCount = 0;
  int visBackupCount = 0;
  int foreignCount = 0;
  int visForeignCount = 0;
  int separatorCount = 0;
  int visSeparatorCount = 0;
  int regularCount = 0;
  int visRegularCount = 0;

  QStringList allMods = m_OrganizerCore.modList()->allMods();

  auto hasFlag = [](std::vector<ModInfo::EFlag> flags, ModInfo::EFlag filter) {
    return std::find(flags.begin(), flags.end(), filter) != flags.end();
  };

  bool isEnabled;
  bool isVisible;
  for (QString mod : allMods) {
    int modIndex = ModInfo::getIndex(mod);
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
    std::vector<ModInfo::EFlag> modFlags = modInfo->getFlags();
    isEnabled = m_OrganizerCore.currentProfile()->modEnabled(modIndex);
    isVisible = m_ModListSortProxy->filterMatchesMod(modInfo, isEnabled);

    for (auto flag : modFlags) {
      switch (flag) {
      case ModInfo::FLAG_BACKUP: backupCount++;
        if (isVisible)
          visBackupCount++;
        break;
      case ModInfo::FLAG_FOREIGN: foreignCount++;
        if (isVisible)
          visForeignCount++;
        break;
      case ModInfo::FLAG_SEPARATOR: separatorCount++;
        if (isVisible)
          visSeparatorCount++;
        break;
      }
    }

    if (!hasFlag(modFlags, ModInfo::FLAG_BACKUP) &&
        !hasFlag(modFlags, ModInfo::FLAG_FOREIGN) &&
        !hasFlag(modFlags, ModInfo::FLAG_SEPARATOR) &&
        !hasFlag(modFlags, ModInfo::FLAG_OVERWRITE)) {
      if (isEnabled) {
        activeCount++;
        if (isVisible)
          visActiveCount++;
      }
      if (isVisible)
        visRegularCount++;
      regularCount++;
    }
  }

  ui->activeModsCounter->display(visActiveCount);
  ui->activeModsCounter->setToolTip(tr("<table cellspacing=\"5\">"
    "<tr><th>Type</th><th>All</th><th>Visible</th>"
    "<tr><td>Enabled mods:&emsp;</td><td align=right>%1 / %2</td><td align=right>%3 / %4</td></tr>"
    "<tr><td>Unmanaged/DLCs:&emsp;</td><td align=right>%5</td><td align=right>%6</td></tr>"
    "<tr><td>Mod backups:&emsp;</td><td align=right>%7</td><td align=right>%8</td></tr>"
    "<tr><td>Separators:&emsp;</td><td align=right>%9</td><td align=right>%10</td></tr>"
    "</table>")
    .arg(activeCount)
    .arg(regularCount)
    .arg(visActiveCount)
    .arg(visRegularCount)
    .arg(foreignCount)
    .arg(visForeignCount)
    .arg(backupCount)
    .arg(visBackupCount)
    .arg(separatorCount)
    .arg(visSeparatorCount)
  );
}

void MainWindow::updatePluginCount()
{
  int activeMasterCount = 0;
  int activeLightMasterCount = 0;
  int activeRegularCount = 0;
  int masterCount = 0;
  int lightMasterCount = 0;
  int regularCount = 0;
  int activeVisibleCount = 0;

  PluginList *list = m_OrganizerCore.pluginList();
  QString filter = ui->espFilterEdit->text();

  for (QString plugin : list->pluginNames()) {
    bool active = list->isEnabled(plugin);
    bool visible = m_PluginListSortProxy->filterMatchesPlugin(plugin);
    if (list->isLight(plugin) || list->isLightFlagged(plugin)) {
      lightMasterCount++;
      activeLightMasterCount += active;
      activeVisibleCount += visible && active;
    } else if (list->isMaster(plugin)) {
      masterCount++;
      activeMasterCount += active;
      activeVisibleCount += visible && active;
    } else {
      regularCount++;
      activeRegularCount += active;
      activeVisibleCount += visible && active;
    }
  }

  int activeCount = activeMasterCount + activeLightMasterCount + activeRegularCount;
  int totalCount = masterCount + lightMasterCount + regularCount;

  ui->activePluginsCounter->display(activeVisibleCount);
  ui->activePluginsCounter->setToolTip(tr("<table cellspacing=\"6\">"
    "<tr><th>Type</th><th>Active      </th><th>Total</th></tr>"
    "<tr><td>All plugins:</td><td align=right>%1    </td><td align=right>%2</td></tr>"
    "<tr><td>ESMs:</td><td align=right>%3    </td><td align=right>%4</td></tr>"
    "<tr><td>ESPs:</td><td align=right>%7    </td><td align=right>%8</td></tr>"
    "<tr><td>ESMs+ESPs:</td><td align=right>%9    </td><td align=right>%10</td></tr>"
    "<tr><td>ESLs:</td><td align=right>%5    </td><td align=right>%6</td></tr>"
    "</table>")
    .arg(activeCount).arg(totalCount)
    .arg(activeMasterCount).arg(masterCount)
    .arg(activeLightMasterCount).arg(lightMasterCount)
    .arg(activeRegularCount).arg(regularCount)
    .arg(activeMasterCount+activeRegularCount).arg(masterCount+regularCount)
  );
}

void MainWindow::information_clicked()
{
  try {
    displayModInformation(m_ContextRow);
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}

void MainWindow::createEmptyMod_clicked()
{
  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);

  while (name->isEmpty()) {
    bool ok;
    name.update(QInputDialog::getText(this, tr("Create Mod..."),
                                      tr("This will create an empty mod.\n"
                                         "Please enter a name:"), QLineEdit::Normal, "", &ok),
                GUESS_USER);
    if (!ok) {
      return;
    }
  }

  if (m_OrganizerCore.modList()->getMod(name) != nullptr) {
    reportError(tr("A mod with this name already exists"));
    return;
  }

  int newPriority = -1;
  if (m_ContextRow >= 0 && m_ModListSortProxy->sortColumn() == ModList::COL_PRIORITY) {
    newPriority = m_OrganizerCore.currentProfile()->getModPriority(m_ContextRow);
  }

  IModInterface *newMod = m_OrganizerCore.createMod(name);
  if (newMod == nullptr) {
    return;
  }

  m_OrganizerCore.refresh();

  if (newPriority >= 0) {
    m_OrganizerCore.modList()->changeModPriority(ModInfo::getIndex(name), newPriority);
  }
}

void MainWindow::createSeparator_clicked()
{
  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);
  while (name->isEmpty())
  {
    bool ok;
    name.update(QInputDialog::getText(this, tr("Create Separator..."),
      tr("This will create a new separator.\n"
        "Please enter a name:"), QLineEdit::Normal, "", &ok),
      GUESS_USER);
    if (!ok) { return; }
  }
  if (m_OrganizerCore.modList()->getMod(name) != nullptr)
  {
    reportError(tr("A separator with this name already exists"));
    return;
  }
  name->append("_separator");
  if (m_OrganizerCore.modList()->getMod(name) != nullptr)
  {
    return;
  }

  int newPriority = -1;
  if (m_ContextRow >= 0 && m_ModListSortProxy->sortColumn() == ModList::COL_PRIORITY)
  {
    newPriority = m_OrganizerCore.currentProfile()->getModPriority(m_ContextRow);
  }

  if (m_OrganizerCore.createMod(name) == nullptr) { return; }
  m_OrganizerCore.refresh();

  if (newPriority >= 0)
  {
    m_OrganizerCore.modList()->changeModPriority(ModInfo::getIndex(name), newPriority);
  }

  if (auto c=m_OrganizerCore.settings().colors().previousSeparatorColor()) {
    ModInfo::getByIndex(ModInfo::getIndex(name))->setColor(*c);
  }
}

void MainWindow::setColor_clicked()
{
  auto& settings = m_OrganizerCore.settings();
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);

  QColorDialog dialog(this);
  dialog.setOption(QColorDialog::ShowAlphaChannel);

  QColor currentColor = modInfo->color();
  if (currentColor.isValid()) {
    dialog.setCurrentColor(currentColor);
  }
  else if (auto c=settings.colors().previousSeparatorColor()) {
    dialog.setCurrentColor(*c);
  }

  if (!dialog.exec())
    return;

  currentColor = dialog.currentColor();
  if (!currentColor.isValid())
    return;

  settings.colors().setPreviousSeparatorColor(currentColor);

  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::Ptr info = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
       info->setColor(currentColor);
    }
  }
  else {
    modInfo->setColor(currentColor);
  }
}

void MainWindow::resetColor_clicked()
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  QColor color = QColor();
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::Ptr info = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
       info->setColor(color);
    }
  }
  else {
    modInfo->setColor(color);
  }

  m_OrganizerCore.settings().colors().removePreviousSeparatorColor();
}

void MainWindow::createModFromOverwrite()
{
  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);

  while (name->isEmpty()) {
    bool ok;
    name.update(QInputDialog::getText(this, tr("Create Mod..."),
                                      tr("This will move all files from overwrite into a new, regular mod.\n"
                                         "Please enter a name:"), QLineEdit::Normal, "", &ok),
                GUESS_USER);
    if (!ok) {
      return;
    }
  }

  if (m_OrganizerCore.modList()->getMod(name) != nullptr) {
    reportError(tr("A mod with this name already exists"));
    return;
  }

  const IModInterface *newMod = m_OrganizerCore.createMod(name);
  if (newMod == nullptr) {
    return;
  }

  doMoveOverwriteContentToMod(newMod->absolutePath());
}

void MainWindow::moveOverwriteContentToExistingMod()
{
  QStringList mods;
  auto indexesByPriority = m_OrganizerCore.currentProfile()->getAllIndexesByPriority();
  for (auto & iter : indexesByPriority) {
    if ((iter.second != UINT_MAX)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(iter.second);
      if (!modInfo->hasFlag(ModInfo::FLAG_SEPARATOR) && !modInfo->hasFlag(ModInfo::FLAG_FOREIGN) && !modInfo->hasFlag(ModInfo::FLAG_OVERWRITE)) {
        mods << modInfo->name();
      }
    }
  }

  ListDialog dialog(this);
  dialog.setWindowTitle("Select a mod...");
  dialog.setChoices(mods);

  if (dialog.exec() == QDialog::Accepted) {
    QString result = dialog.getChoice();
    if (!result.isEmpty()) {

      QString modAbsolutePath;

      for (const auto& mod : m_OrganizerCore.modsSortedByProfilePriority(m_OrganizerCore.currentProfile())) {
        if (result.compare(mod) == 0) {
          ModInfo::Ptr modInfo = ModInfo::getByIndex(ModInfo::getIndex(mod));
          modAbsolutePath = modInfo->absolutePath();
          break;
        }
      }

      if (modAbsolutePath.isNull()) {
        log::warn("Mod {} has not been found, for some reason", result);
        return;
      }

      doMoveOverwriteContentToMod(modAbsolutePath);
    }
  }
}

void MainWindow::doMoveOverwriteContentToMod(const QString &modAbsolutePath)
{
  unsigned int overwriteIndex = ModInfo::findMod([](ModInfo::Ptr mod) -> bool {
    std::vector<ModInfo::EFlag> flags = mod->getFlags();
    return std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end(); });

  ModInfo::Ptr overwriteInfo = ModInfo::getByIndex(overwriteIndex);
  bool successful = shellMove((QDir::toNativeSeparators(overwriteInfo->absolutePath()) + "\\*"),
    (QDir::toNativeSeparators(modAbsolutePath)), false, this);

  if (successful) {
    MessageDialog::showMessage(tr("Move successful."), this);
  }
  else {
    const auto e = GetLastError();
    log::error("Move operation failed: {}", formatSystemMessage(e));
  }

  m_OrganizerCore.refresh();
}

void MainWindow::clearOverwrite()
{
  unsigned int overwriteIndex = ModInfo::findMod([](ModInfo::Ptr mod) -> bool {
    std::vector<ModInfo::EFlag> flags = mod->getFlags();
    return std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE)
      != flags.end();
  });

  ModInfo::Ptr modInfo = ModInfo::getByIndex(overwriteIndex);
  if (modInfo)
  {
    QDir overwriteDir(modInfo->absolutePath());
    if (QMessageBox::question(this, tr("Are you sure?"),
      tr("About to recursively delete:\n") + overwriteDir.absolutePath(),
      QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok)
    {
      QStringList delList;
      for (auto f : overwriteDir.entryList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot))
        delList.push_back(overwriteDir.absoluteFilePath(f));
      if (shellDelete(delList, true)) {
        scheduleCheckForProblems();
        m_OrganizerCore.refresh();
      } else {
        const auto e = GetLastError();
        log::error("Delete operation failed: {}", formatSystemMessage(e));
      }
    }
  }
}

void MainWindow::cancelModListEditor()
{
  ui->modList->setEnabled(false);
  ui->modList->setEnabled(true);
}

void MainWindow::on_modList_doubleClicked(const QModelIndex &index)
{
  if (!index.isValid()) {
    return;
  }

  if (m_OrganizerCore.modList()->timeElapsedSinceLastChecked() <= QApplication::doubleClickInterval()) {
    // don't interpret double click if we only just checked a mod
    return;
  }

  QModelIndex sourceIdx = mapToModel(m_OrganizerCore.modList(), index);
  if (!sourceIdx.isValid()) {
    return;
  }

  Qt::KeyboardModifiers modifiers = QApplication::queryKeyboardModifiers();
  if (modifiers.testFlag(Qt::ControlModifier)) {
    try {
      m_ContextRow = m_ModListSortProxy->mapToSource(index).row();

      ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
      shell::Explore(modInfo->absolutePath());

      // workaround to cancel the editor that might have opened because of
      // selection-click
      ui->modList->closePersistentEditor(index);
    }
    catch (const std::exception &e) {
      reportError(e.what());
    }
  }
  else if (modifiers.testFlag(Qt::ShiftModifier)) {
    try {
      m_ContextRow = m_ModListSortProxy->mapToSource(index).row();
      QModelIndex idx = m_OrganizerCore.modList()->index(m_ContextRow, 0);
      visitNexusOrWebPage(idx);
      ui->modList->closePersistentEditor(index);
    }
    catch (const std::exception & e) {
      reportError(e.what());
    }
  }
  else{
    try {
      m_ContextRow = m_ModListSortProxy->mapToSource(index).row();
      sourceIdx.column();

      auto tab = ModInfoTabIDs::None;

      switch (sourceIdx.column()) {
        case ModList::COL_NOTES: tab = ModInfoTabIDs::Notes; break;
        case ModList::COL_VERSION: tab = ModInfoTabIDs::Nexus; break;
        case ModList::COL_MODID: tab = ModInfoTabIDs::Nexus; break;
        case ModList::COL_GAME: tab = ModInfoTabIDs::Nexus; break;
        case ModList::COL_CATEGORY: tab = ModInfoTabIDs::Categories; break;
        case ModList::COL_CONFLICTFLAGS: tab = ModInfoTabIDs::Conflicts; break;
      }

      displayModInformation(sourceIdx.row(), tab);
      // workaround to cancel the editor that might have opened because of
      // selection-click
      ui->modList->closePersistentEditor(index);
    }
    catch (const std::exception &e) {
      reportError(e.what());
    }
  }
}

void MainWindow::on_listOptionsBtn_pressed()
{
  m_ContextRow = -1;
}

void MainWindow::openOriginInformation_clicked()
{
  try {
    QItemSelectionModel *selection = ui->espList->selectionModel();
    //we don't want to open multiple modinfodialogs.
    /*if (selection->hasSelection() && selection->selectedRows().count() > 0) {

      for (QModelIndex idx : selection->selectedRows()) {
        QString fileName = idx.data().toString();
        ModInfo::Ptr modInfo = ModInfo::getByIndex(ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)));
        std::vector<ModInfo::EFlag> flags = modInfo->getFlags();

        if (modInfo->isRegular() || (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end())) {
          displayModInformation(ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)));
        }
      }
    }
    else {}*/
    QModelIndex idx = selection->currentIndex();
    QString fileName = idx.data().toString();

    ModInfo::Ptr modInfo = ModInfo::getByIndex(ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)));
    std::vector<ModInfo::EFlag> flags = modInfo->getFlags();

    if (modInfo->isRegular() || (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end())) {
      displayModInformation(ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)));
    }
  }
  catch (const std::exception &e) {
    reportError(e.what());
  }
}

void MainWindow::on_espList_doubleClicked(const QModelIndex &index)
{
  if (!index.isValid()) {
    return;
  }

  if (m_OrganizerCore.pluginList()->timeElapsedSinceLastChecked() <= QApplication::doubleClickInterval()) {
    // don't interpret double click if we only just checked a plugin
    return;
  }

  QModelIndex sourceIdx = mapToModel(m_OrganizerCore.pluginList(), index);
  if (!sourceIdx.isValid()) {
    return;
  }
  try {

    QItemSelectionModel *selection = ui->espList->selectionModel();

    if (selection->hasSelection() && selection->selectedRows().count() == 1) {

      QModelIndex idx = selection->currentIndex();
      QString fileName = idx.data().toString();

      if (ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)) == UINT_MAX)
        return;

      ModInfo::Ptr modInfo = ModInfo::getByIndex(ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)));
      std::vector<ModInfo::EFlag> flags = modInfo->getFlags();

      if (modInfo->isRegular() || (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end())) {

        Qt::KeyboardModifiers modifiers = QApplication::queryKeyboardModifiers();
        if (modifiers.testFlag(Qt::ControlModifier)) {
          openExplorer_activated();
          // workaround to cancel the editor that might have opened because of
          // selection-click
          ui->espList->closePersistentEditor(index);
        }
        else {

          displayModInformation(ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)));
          // workaround to cancel the editor that might have opened because of
          // selection-click
          ui->espList->closePersistentEditor(index);
        }
      }
    }
  }
  catch (const std::exception &e) {
    reportError(e.what());
  }
}

bool MainWindow::populateMenuCategories(QMenu *menu, int targetID)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  const std::set<int> &categories = modInfo->getCategories();

  bool childEnabled = false;

  for (unsigned int i = 1; i < m_CategoryFactory.numCategories(); ++i) {
    if (m_CategoryFactory.getParentID(i) == targetID) {
      QMenu *targetMenu = menu;
      if (m_CategoryFactory.hasChildren(i)) {
        targetMenu = menu->addMenu(m_CategoryFactory.getCategoryName(i).replace('&', "&&"));
      }

      int id = m_CategoryFactory.getCategoryID(i);
      QScopedPointer<QCheckBox> checkBox(new QCheckBox(targetMenu));
      bool enabled = categories.find(id) != categories.end();
      checkBox->setText(m_CategoryFactory.getCategoryName(i).replace('&', "&&"));
      if (enabled) {
        childEnabled = true;
      }
      checkBox->setChecked(enabled ? Qt::Checked : Qt::Unchecked);

      QScopedPointer<QWidgetAction> checkableAction(new QWidgetAction(targetMenu));
      checkableAction->setDefaultWidget(checkBox.take());
      checkableAction->setData(id);
      targetMenu->addAction(checkableAction.take());

      if (m_CategoryFactory.hasChildren(i)) {
        if (populateMenuCategories(targetMenu, m_CategoryFactory.getCategoryID(i)) || enabled) {
          targetMenu->setIcon(QIcon(":/MO/gui/resources/check.png"));
        }
      }
    }
  }
  return childEnabled;
}

void MainWindow::replaceCategoriesFromMenu(QMenu *menu, int modRow)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(modRow);
  for (QAction* action : menu->actions()) {
    if (action->menu() != nullptr) {
      replaceCategoriesFromMenu(action->menu(), modRow);
    } else {
      QWidgetAction *widgetAction = qobject_cast<QWidgetAction*>(action);
      if (widgetAction != nullptr) {
        QCheckBox *checkbox = qobject_cast<QCheckBox*>(widgetAction->defaultWidget());
        modInfo->setCategory(widgetAction->data().toInt(), checkbox->isChecked());
      }
    }
  }
}

void MainWindow::addRemoveCategoriesFromMenu(QMenu *menu, int modRow, int referenceRow)
{
  if (referenceRow != -1 && referenceRow != modRow) {
    ModInfo::Ptr editedModInfo = ModInfo::getByIndex(referenceRow);
    for (QAction* action : menu->actions()) {
      if (action->menu() != nullptr) {
        addRemoveCategoriesFromMenu(action->menu(), modRow, referenceRow);
      } else {
        QWidgetAction *widgetAction = qobject_cast<QWidgetAction*>(action);
        if (widgetAction != nullptr) {
          QCheckBox *checkbox = qobject_cast<QCheckBox*>(widgetAction->defaultWidget());
          int categoryId = widgetAction->data().toInt();
          bool checkedBefore = editedModInfo->categorySet(categoryId);
          bool checkedAfter = checkbox->isChecked();

          if (checkedBefore != checkedAfter) { // only update if the category was changed on the edited mod
            ModInfo::Ptr currentModInfo = ModInfo::getByIndex(modRow);
            currentModInfo->setCategory(categoryId, checkedAfter);
          }
        }
      }
    }
  } else {
    replaceCategoriesFromMenu(menu, modRow);
  }
}

void MainWindow::addRemoveCategories_MenuHandler() {
  QMenu *menu = qobject_cast<QMenu*>(sender());
  if (menu == nullptr) {
    log::error("not a menu?");
    return;
  }

  QList<QPersistentModelIndex> selected;
  for (const QModelIndex &idx : ui->modList->selectionModel()->selectedRows()) {
    selected.append(QPersistentModelIndex(idx));
  }

  if (selected.size() > 0) {
    int minRow = INT_MAX;
    int maxRow = -1;

    for (const QPersistentModelIndex &idx : selected) {
      log::debug("change categories on: {}", idx.data().toString());
      QModelIndex modIdx = mapToModel(m_OrganizerCore.modList(), idx);
      if (modIdx.row() != m_ContextIdx.row()) {
        addRemoveCategoriesFromMenu(menu, modIdx.row(), m_ContextIdx.row());
      }
      if (idx.row() < minRow) minRow = idx.row();
      if (idx.row() > maxRow) maxRow = idx.row();
    }
    replaceCategoriesFromMenu(menu, m_ContextIdx.row());

    m_OrganizerCore.modList()->notifyChange(minRow, maxRow + 1);

    for (const QPersistentModelIndex &idx : selected) {
      ui->modList->selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
  } else {
    //For single mod selections, just do a replace
    replaceCategoriesFromMenu(menu, m_ContextRow);
    m_OrganizerCore.modList()->notifyChange(m_ContextRow);
  }

  refreshFilters();
}

void MainWindow::replaceCategories_MenuHandler() {
  QMenu *menu = qobject_cast<QMenu*>(sender());
  if (menu == nullptr) {
    log::error("not a menu?");
    return;
  }

  QList<QPersistentModelIndex> selected;
  for (const QModelIndex &idx : ui->modList->selectionModel()->selectedRows()) {
    selected.append(QPersistentModelIndex(idx));
  }

  if (selected.size() > 0) {
    QStringList selectedMods;
    int minRow = INT_MAX;
    int maxRow = -1;
    for (int i = 0; i < selected.size(); ++i) {
      QModelIndex temp = mapToModel(m_OrganizerCore.modList(), selected.at(i));
      selectedMods.append(temp.data().toString());
      replaceCategoriesFromMenu(menu, mapToModel(m_OrganizerCore.modList(), selected.at(i)).row());
      if (temp.row() < minRow) minRow = temp.row();
      if (temp.row() > maxRow) maxRow = temp.row();
    }

    m_OrganizerCore.modList()->notifyChange(minRow, maxRow + 1);

    // find mods by their name because indices are invalidated
    QAbstractItemModel *model = ui->modList->model();
    for (const QString &mod : selectedMods) {
      QModelIndexList matches = model->match(model->index(0, 0), Qt::DisplayRole, mod, 1,
                                             Qt::MatchFixedString | Qt::MatchCaseSensitive | Qt::MatchRecursive);
      if (matches.size() > 0) {
        ui->modList->selectionModel()->select(matches.at(0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
      }
    }
  } else {
    //For single mod selections, just do a replace
    replaceCategoriesFromMenu(menu, m_ContextRow);
    m_OrganizerCore.modList()->notifyChange(m_ContextRow);
  }

  refreshFilters();
}

void MainWindow::saveArchiveList()
{
  if (m_OrganizerCore.isArchivesInit()) {
    SafeWriteFile archiveFile(m_OrganizerCore.currentProfile()->getArchivesFileName());
    for (int i = 0; i < ui->bsaList->topLevelItemCount(); ++i) {
      QTreeWidgetItem * tlItem = ui->bsaList->topLevelItem(i);
      for (int j = 0; j < tlItem->childCount(); ++j) {
        QTreeWidgetItem * item = tlItem->child(j);
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

void MainWindow::checkModsForUpdates()
{
  bool checkingModsForUpdate = false;
  if (NexusInterface::instance().getAccessManager()->validated()) {
    checkingModsForUpdate = ModInfo::checkAllForUpdate(&m_PluginContainer, this);
    NexusInterface::instance().requestEndorsementInfo(this, QVariant(), QString());
    NexusInterface::instance().requestTrackingInfo(this, QVariant(), QString());
  } else {
    QString apiKey;
    if (GlobalSettings::nexusApiKey(apiKey)) {
      m_OrganizerCore.doAfterLogin([this] () { this->checkModsForUpdates(); });
      NexusInterface::instance().getAccessManager()->apiCheck(apiKey);
    } else {
      log::warn("{}", tr("You are not currently authenticated with Nexus. Please do so under Settings -> Nexus."));
    }
  }

  bool updatesAvailable = false;
  for (auto mod : m_OrganizerCore.modList()->allMods()) {
    ModInfo::Ptr modInfo = ModInfo::getByName(mod);
    if (modInfo->updateAvailable()) {
      updatesAvailable = true;
      break;
    }
  }

  if (updatesAvailable || checkingModsForUpdate) {
    m_ModListSortProxy->setCriteria({{
        ModListSortProxy::TypeSpecial,
        CategoryFactory::UpdateAvailable,
        false}
    });

    m_Filters->setSelection({{
      ModListSortProxy::TypeSpecial,
      CategoryFactory::UpdateAvailable,
      false
    }});
  }
}

void MainWindow::changeVersioningScheme() {
  if (QMessageBox::question(this, tr("Continue?"),
        tr("The versioning scheme decides which version is considered newer than another.\n"
           "This function will guess the versioning scheme under the assumption that the installed version is outdated."),
        QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Yes) {

    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);

    bool success = false;

    static VersionInfo::VersionScheme schemes[] = { VersionInfo::SCHEME_REGULAR, VersionInfo::SCHEME_DECIMALMARK, VersionInfo::SCHEME_NUMBERSANDLETTERS };

    for (int i = 0; i < sizeof(schemes) / sizeof(VersionInfo::VersionScheme) && !success; ++i) {
      VersionInfo verOld(info->version().canonicalString(), schemes[i]);
      VersionInfo verNew(info->newestVersion().canonicalString(), schemes[i]);
      if (verOld < verNew) {
        info->setVersion(verOld);
        info->setNewestVersion(verNew);
        success = true;
      }
    }
    if (!success) {
      QMessageBox::information(this, tr("Sorry"),
          tr("I don't know a versioning scheme where %1 is newer than %2.").arg(info->newestVersion().canonicalString()).arg(info->version().canonicalString()),
          QMessageBox::Ok);
    }
  }
}

void MainWindow::ignoreUpdate() {
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::Ptr info = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
      info->ignoreUpdate(true);
    }
  }
  else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
    info->ignoreUpdate(true);
  }
  if (m_ModListSortProxy != nullptr)
    m_ModListSortProxy->invalidate();
}

void MainWindow::checkModUpdates_clicked()
{
  std::multimap<QString, int> IDs;
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::Ptr info = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
      IDs.insert(std::make_pair<QString, int>(info->gameName(), info->nexusId()));
    }
  } else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
    IDs.insert(std::make_pair<QString, int>(info->gameName(), info->nexusId()));
  }
  modUpdateCheck(IDs);
}

void MainWindow::unignoreUpdate()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::Ptr info = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
      info->ignoreUpdate(false);
    }
  }
  else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
    info->ignoreUpdate(false);
  }
  if (m_ModListSortProxy != nullptr)
    m_ModListSortProxy->invalidate();
}

void MainWindow::addPrimaryCategoryCandidates(QMenu *primaryCategoryMenu,
                                              ModInfo::Ptr info) {
  const std::set<int> &categories = info->getCategories();
  for (int categoryID : categories) {
    int catIdx = m_CategoryFactory.getCategoryIndex(categoryID);
    QWidgetAction *action = new QWidgetAction(primaryCategoryMenu);
    try {
      QRadioButton *categoryBox = new QRadioButton(
          m_CategoryFactory.getCategoryName(catIdx).replace('&', "&&"),
          primaryCategoryMenu);
      connect(categoryBox, &QRadioButton::toggled, [info, categoryID](bool enable) {
        if (enable) {
          info->setPrimaryCategory(categoryID);
        }
      });
      categoryBox->setChecked(categoryID == info->primaryCategory());
      action->setDefaultWidget(categoryBox);
    } catch (const std::exception &e) {
      log::error("failed to create category checkbox: {}", e.what());
    }

    action->setData(categoryID);
    primaryCategoryMenu->addAction(action);
  }
}

void MainWindow::addPrimaryCategoryCandidates()
{
  QMenu *menu = qobject_cast<QMenu*>(sender());
  if (menu == nullptr) {
    log::error("not a menu?");
    return;
  }
  menu->clear();
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);

  addPrimaryCategoryCandidates(menu, modInfo);
}

void MainWindow::enableVisibleMods()
{
  if (QMessageBox::question(nullptr, tr("Confirm"), tr("Really enable all visible mods?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    m_ModListSortProxy->enableAllVisible();
  }
}

void MainWindow::disableVisibleMods()
{
  if (QMessageBox::question(nullptr, tr("Confirm"), tr("Really disable all visible mods?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    m_ModListSortProxy->disableAllVisible();
  }
}

void MainWindow::openInstanceFolder()
{
  QString dataPath = qApp->property("dataPath").toString();
  shell::Explore(dataPath);
}

void MainWindow::openLogsFolder()
{
	QString logsPath = qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::logPath());
  shell::Explore(logsPath);
}

void MainWindow::openInstallFolder()
{
  shell::Explore(qApp->applicationDirPath());
}

void MainWindow::openPluginsFolder()
{
	QString pluginsPath = QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::pluginPath());
  shell::Explore(pluginsPath);
}

void MainWindow::openStylesheetsFolder()
{
  QString ssPath = QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::stylesheetsPath());
  shell::Explore(ssPath);
}

void MainWindow::openProfileFolder()
{
  shell::Explore(m_OrganizerCore.currentProfile()->absolutePath());
}

void MainWindow::openIniFolder()
{
  if (m_OrganizerCore.currentProfile()->localSettingsEnabled())
  {
    shell::Explore(m_OrganizerCore.currentProfile()->absolutePath());
  }
  else {
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


void MainWindow::exportModListCSV()
{
	//SelectionDialog selection(tr("Choose what to export"));

	//selection.addChoice(tr("Everything"), tr("All installed mods are included in the list"), 0);
	//selection.addChoice(tr("Active Mods"), tr("Only active (checked) mods from your current profile are included"), 1);
	//selection.addChoice(tr("Visible"), tr("All mods visible in the mod list are included"), 2);

	QDialog selection(this);
	QGridLayout *grid = new QGridLayout;
	selection.setWindowTitle(tr("Export to csv"));

	QLabel *csvDescription = new QLabel();
	csvDescription->setText(tr("CSV (Comma Separated Values) is a format that can be imported in programs like Excel to create a spreadsheet.\nYou can also use online editors and converters instead."));
	grid->addWidget(csvDescription);

	QGroupBox *groupBoxRows = new QGroupBox(tr("Select what mods you want export:"));
	QRadioButton *all = new QRadioButton(tr("All installed mods"));
	QRadioButton *active = new QRadioButton(tr("Only active (checked) mods from your current profile"));
	QRadioButton *visible = new QRadioButton(tr("All currently visible mods in the mod list"));

	QVBoxLayout *vbox = new QVBoxLayout;
	vbox->addWidget(all);
	vbox->addWidget(active);
	vbox->addWidget(visible);
	vbox->addStretch(1);
	groupBoxRows->setLayout(vbox);



	grid->addWidget(groupBoxRows);

	QButtonGroup *buttonGroupRows = new QButtonGroup();
	buttonGroupRows->addButton(all, 0);
	buttonGroupRows->addButton(active, 1);
	buttonGroupRows->addButton(visible, 2);
	buttonGroupRows->button(0)->setChecked(true);



	QGroupBox *groupBoxColumns = new QGroupBox(tr("Choose what Columns to export:"));
	groupBoxColumns->setFlat(true);

	QCheckBox *mod_Priority = new QCheckBox(tr("Mod_Priority"));
	mod_Priority->setChecked(true);
	QCheckBox *mod_Name = new QCheckBox(tr("Mod_Name"));
	mod_Name->setChecked(true);
  QCheckBox *mod_Note = new QCheckBox(tr("Notes_column"));
	QCheckBox *mod_Status = new QCheckBox(tr("Mod_Status"));
  mod_Status->setChecked(true);
	QCheckBox *primary_Category = new QCheckBox(tr("Primary_Category"));
	QCheckBox *nexus_ID = new QCheckBox(tr("Nexus_ID"));
	QCheckBox *mod_Nexus_URL = new QCheckBox(tr("Mod_Nexus_URL"));
	QCheckBox *mod_Version = new QCheckBox(tr("Mod_Version"));
	QCheckBox *install_Date = new QCheckBox(tr("Install_Date"));
	QCheckBox *download_File_Name = new QCheckBox(tr("Download_File_Name"));

	QVBoxLayout *vbox1 = new QVBoxLayout;
	vbox1->addWidget(mod_Priority);
	vbox1->addWidget(mod_Name);
	vbox1->addWidget(mod_Status);
  vbox1->addWidget(mod_Note);
	vbox1->addWidget(primary_Category);
	vbox1->addWidget(nexus_ID);
	vbox1->addWidget(mod_Nexus_URL);
	vbox1->addWidget(mod_Version);
	vbox1->addWidget(install_Date);
	vbox1->addWidget(download_File_Name);
	groupBoxColumns->setLayout(vbox1);

	grid->addWidget(groupBoxColumns);

	QPushButton *ok = new QPushButton("Ok");
	QPushButton *cancel = new QPushButton("Cancel");
	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	connect(buttons, SIGNAL(accepted()), &selection, SLOT(accept()));
	connect(buttons, SIGNAL(rejected()), &selection, SLOT(reject()));

	grid->addWidget(buttons);

	selection.setLayout(grid);


	if (selection.exec() == QDialog::Accepted) {

		unsigned int numMods = ModInfo::getNumMods();
		int selectedRowID = buttonGroupRows->checkedId();

		try {
			QBuffer buffer;
			buffer.open(QIODevice::ReadWrite);
			CSVBuilder builder(&buffer);
			builder.setEscapeMode(CSVBuilder::TYPE_STRING, CSVBuilder::QUOTE_ALWAYS);
			std::vector<std::pair<QString, CSVBuilder::EFieldType> > fields;
			if (mod_Priority->isChecked())
				fields.push_back(std::make_pair(QString("#Mod_Priority"), CSVBuilder::TYPE_STRING));
      if (mod_Status->isChecked())
        fields.push_back(std::make_pair(QString("#Mod_Status"), CSVBuilder::TYPE_STRING));
			if (mod_Name->isChecked())
				fields.push_back(std::make_pair(QString("#Mod_Name"), CSVBuilder::TYPE_STRING));
      if (mod_Note->isChecked())
        fields.push_back(std::make_pair(QString("#Note"), CSVBuilder::TYPE_STRING));
			if (primary_Category->isChecked())
				fields.push_back(std::make_pair(QString("#Primary_Category"), CSVBuilder::TYPE_STRING));
			if (nexus_ID->isChecked())
				fields.push_back(std::make_pair(QString("#Nexus_ID"), CSVBuilder::TYPE_INTEGER));
			if (mod_Nexus_URL->isChecked())
				fields.push_back(std::make_pair(QString("#Mod_Nexus_URL"), CSVBuilder::TYPE_STRING));
			if (mod_Version->isChecked())
				fields.push_back(std::make_pair(QString("#Mod_Version"), CSVBuilder::TYPE_STRING));
			if (install_Date->isChecked())
				fields.push_back(std::make_pair(QString("#Install_Date"), CSVBuilder::TYPE_STRING));
			if (download_File_Name->isChecked())
				fields.push_back(std::make_pair(QString("#Download_File_Name"), CSVBuilder::TYPE_STRING));

			builder.setFields(fields);

			builder.writeHeader();

      auto indexesByPriority = m_OrganizerCore.currentProfile()->getAllIndexesByPriority();
      for (auto& iter : indexesByPriority) {
				ModInfo::Ptr info = ModInfo::getByIndex(iter.second);
				bool enabled = m_OrganizerCore.currentProfile()->modEnabled(iter.second);
				if ((selectedRowID == 1) && !enabled) {
					continue;
				}
				else if ((selectedRowID == 2) && !m_ModListSortProxy->filterMatchesMod(info, enabled)) {
					continue;
				}
				std::vector<ModInfo::EFlag> flags = info->getFlags();
				if ((std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) == flags.end()) &&
					(std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) == flags.end())) {
					if (mod_Priority->isChecked())
						builder.setRowField("#Mod_Priority", QString("%1").arg(iter.first, 4, 10, QChar('0')));
          if (mod_Status->isChecked())
            builder.setRowField("#Mod_Status", (enabled) ? "+" : "-");
					if (mod_Name->isChecked())
						builder.setRowField("#Mod_Name", info->name());
          if (mod_Note->isChecked())
            builder.setRowField("#Note", QString("%1").arg(info->comments().remove(',')));
					if (primary_Category->isChecked())
						builder.setRowField("#Primary_Category", (m_CategoryFactory.categoryExists(info->primaryCategory())) ? m_CategoryFactory.getCategoryNameByID(info->primaryCategory()) : "");
					if (nexus_ID->isChecked())
						builder.setRowField("#Nexus_ID", info->nexusId());
					if (mod_Nexus_URL->isChecked())
						builder.setRowField("#Mod_Nexus_URL",(info->nexusId()>0)? NexusInterface::instance().getModURL(info->nexusId(), info->gameName()) : "");
					if (mod_Version->isChecked())
						builder.setRowField("#Mod_Version", info->version().canonicalString());
					if (install_Date->isChecked())
						builder.setRowField("#Install_Date", info->creationTime().toString("yyyy/MM/dd HH:mm:ss"));
					if (download_File_Name->isChecked())
						builder.setRowField("#Download_File_Name", info->installationFile());

					builder.writeRow();
				}
			}

			SaveTextAsDialog saveDialog(this);
			saveDialog.setText(buffer.data());
			saveDialog.exec();
		}
		catch (const std::exception &e) {
			reportError(tr("export failed: %1").arg(e.what()));
		}
	}
}

static void addMenuAsPushButton(QMenu *menu, QMenu *subMenu)
{
  QPushButton *pushBtn = new QPushButton(subMenu->title());
  pushBtn->setMenu(subMenu);
  QWidgetAction *action = new QWidgetAction(menu);
  action->setDefaultWidget(pushBtn);
  menu->addAction(action);
}

QMenu *MainWindow::openFolderMenu()
{
	QMenu *FolderMenu = new QMenu(this);

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
  FolderMenu->addAction(tr("Open MO2 Stylesheets folder"), this, SLOT(openStylesheetsFolder()));
  FolderMenu->addAction(tr("Open MO2 Logs folder"), this, SLOT(openLogsFolder()));

	return FolderMenu;
}

void MainWindow::initModListContextMenu(QMenu *menu)
{
  menu->addAction(tr("Install Mod..."), this, SLOT(installMod_clicked()));
  menu->addAction(tr("Create empty mod"), this, SLOT(createEmptyMod_clicked()));
  menu->addAction(tr("Create Separator"), this, SLOT(createSeparator_clicked()));

  menu->addSeparator();

  menu->addAction(tr("Enable all visible"), this, SLOT(enableVisibleMods()));
  menu->addAction(tr("Disable all visible"), this, SLOT(disableVisibleMods()));
  menu->addAction(tr("Check for updates"), this, SLOT(checkModsForUpdates()));
  menu->addAction(tr("Refresh"), &m_OrganizerCore, SLOT(profileRefresh()));
  menu->addAction(tr("Export to csv..."), this, SLOT(exportModListCSV()));
}

void MainWindow::addModSendToContextMenu(QMenu *menu)
{
  if (m_ModListSortProxy->sortColumn() != ModList::COL_PRIORITY)
    return;

  QMenu *sub_menu = new QMenu(menu);
  sub_menu->setTitle(tr("Send to"));
  sub_menu->addAction(tr("Top"), this, SLOT(sendSelectedModsToTop_clicked()));
  sub_menu->addAction(tr("Bottom"), this, SLOT(sendSelectedModsToBottom_clicked()));
  sub_menu->addAction(tr("Priority..."), this, SLOT(sendSelectedModsToPriority_clicked()));
  sub_menu->addAction(tr("Separator..."), this, SLOT(sendSelectedModsToSeparator_clicked()));

  menu->addMenu(sub_menu);
  menu->addSeparator();
}

void MainWindow::addPluginSendToContextMenu(QMenu *menu)
{
  if (m_PluginListSortProxy->sortColumn() != PluginList::COL_PRIORITY)
    return;

  QMenu *sub_menu = new QMenu(this);
  sub_menu->setTitle(tr("Send to"));
  sub_menu->addAction(tr("Top"), this, SLOT(sendSelectedPluginsToTop_clicked()));
  sub_menu->addAction(tr("Bottom"), this, SLOT(sendSelectedPluginsToBottom_clicked()));
  sub_menu->addAction(tr("Priority..."), this, SLOT(sendSelectedPluginsToPriority_clicked()));

  menu->addMenu(sub_menu);
  menu->addSeparator();
}

void MainWindow::on_modList_customContextMenuRequested(const QPoint &pos)
{
  try {
    QTreeView *modList = findChild<QTreeView*>("modList");

    m_ContextIdx = mapToModel(m_OrganizerCore.modList(), modList->indexAt(pos));
    m_ContextRow = m_ContextIdx.row();
    int contextColumn = m_ContextIdx.column();

    if (m_ContextRow == -1) {
      // no selection
      QMenu menu(this);
      initModListContextMenu(&menu);
      menu.exec(modList->viewport()->mapToGlobal(pos));
    }
    else {
      QMenu menu(this);

      QMenu *allMods = new QMenu(&menu);
      initModListContextMenu(allMods);
      allMods->setTitle(tr("All Mods"));
      menu.addMenu(allMods);
      menu.addSeparator();

      ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
      std::vector<ModInfo::EFlag> flags = info->getFlags();
      // Context menu for overwrites
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
        if (QDir(info->absolutePath()).count() > 2) {
          menu.addAction(tr("Sync to Mods..."), &m_OrganizerCore, SLOT(syncOverwrite()));
          menu.addAction(tr("Create Mod..."), this, SLOT(createModFromOverwrite()));
          menu.addAction(tr("Move content to Mod..."), this, SLOT(moveOverwriteContentToExistingMod()));
          menu.addAction(tr("Clear Overwrite..."), this, SLOT(clearOverwrite()));
        }
        menu.addAction(tr("Open in Explorer"), this, SLOT(openExplorer_clicked()));
      }
      // Context menu for mod backups
      else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end()) {
          menu.addAction(tr("Restore Backup"), this, SLOT(restoreBackup_clicked()));
          menu.addAction(tr("Remove Backup..."), this, SLOT(removeMod_clicked()));
          menu.addSeparator();
          if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
            menu.addAction(tr("Ignore missing data"), this, SLOT(ignoreMissingData_clicked()));
          }
          if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_ALTERNATE_GAME) != flags.end()) {
            menu.addAction(tr("Mark as converted/working"), this, SLOT(markConverted_clicked()));
          }
          menu.addSeparator();
          if (info->nexusId() > 0) {
            menu.addAction(tr("Visit on Nexus"), this, SLOT(visitOnNexus_clicked()));
          }

          const auto url = info->parseCustomURL();
          if (url.isValid()) {
            menu.addAction(
              tr("Visit on %1").arg(url.host()),
              this, SLOT(visitWebPage_clicked()));
          }

          menu.addAction(tr("Open in Explorer"), this, SLOT(openExplorer_clicked()));
      }
      else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end()){
        menu.addSeparator();
        QMenu *addRemoveCategoriesMenu = new QMenu(tr("Change Categories"), &menu);
        populateMenuCategories(addRemoveCategoriesMenu, 0);
        connect(addRemoveCategoriesMenu, SIGNAL(aboutToHide()), this, SLOT(addRemoveCategories_MenuHandler()));
        addMenuAsPushButton(&menu, addRemoveCategoriesMenu);
        QMenu *primaryCategoryMenu = new QMenu(tr("Primary Category"), &menu);
        connect(primaryCategoryMenu, SIGNAL(aboutToShow()), this, SLOT(addPrimaryCategoryCandidates()));
        addMenuAsPushButton(&menu, primaryCategoryMenu);
        menu.addSeparator();
        menu.addAction(tr("Rename Separator..."), this, SLOT(renameMod_clicked()));
        menu.addAction(tr("Remove Separator..."), this, SLOT(removeMod_clicked()));
        menu.addSeparator();
        addModSendToContextMenu(&menu);
        menu.addAction(tr("Select Color..."), this, SLOT(setColor_clicked()));

        if(info->color().isValid())
          menu.addAction(tr("Reset Color"), this, SLOT(resetColor_clicked()));

        menu.addSeparator();
      }
      else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) != flags.end()) {
        addModSendToContextMenu(&menu);
      }
      else {
        QMenu *addRemoveCategoriesMenu = new QMenu(tr("Change Categories"), &menu);
        populateMenuCategories(addRemoveCategoriesMenu, 0);
        connect(addRemoveCategoriesMenu, SIGNAL(aboutToHide()), this, SLOT(addRemoveCategories_MenuHandler()));
        addMenuAsPushButton(&menu, addRemoveCategoriesMenu);

        QMenu *primaryCategoryMenu = new QMenu(tr("Primary Category"), &menu);
        connect(primaryCategoryMenu, SIGNAL(aboutToShow()), this, SLOT(addPrimaryCategoryCandidates()));
        addMenuAsPushButton(&menu, primaryCategoryMenu);

        menu.addSeparator();

        if (info->downgradeAvailable()) {
          menu.addAction(tr("Change versioning scheme"), this, SLOT(changeVersioningScheme()));
        }

        if (info->nexusId() > 0)
          menu.addAction(tr("Force-check updates"), this, SLOT(checkModUpdates_clicked()));
        if (info->updateIgnored()) {
          menu.addAction(tr("Un-ignore update"), this, SLOT(unignoreUpdate()));
        } else {
          if (info->updateAvailable() || info->downgradeAvailable()) {
              menu.addAction(tr("Ignore update"), this, SLOT(ignoreUpdate()));
          }
        }
        menu.addSeparator();

        menu.addAction(tr("Enable selected"), this, SLOT(enableSelectedMods_clicked()));
        menu.addAction(tr("Disable selected"), this, SLOT(disableSelectedMods_clicked()));

        menu.addSeparator();

        addModSendToContextMenu(&menu);

        menu.addAction(tr("Rename Mod..."), this, SLOT(renameMod_clicked()));
        menu.addAction(tr("Reinstall Mod"), this, SLOT(reinstallMod_clicked()));
        menu.addAction(tr("Remove Mod..."), this, SLOT(removeMod_clicked()));
        menu.addAction(tr("Create Backup"), this, SLOT(backupMod_clicked()));

        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_HIDDEN_FILES) != flags.end()) {
          menu.addAction(tr("Restore hidden files"), this, SLOT(restoreHiddenFiles_clicked()));
        }

        menu.addSeparator();

        if (contextColumn == ModList::COL_NOTES) {
          menu.addAction(tr("Select Color..."), this, SLOT(setColor_clicked()));

          if (info->color().isValid())
            menu.addAction(tr("Reset Color"), this, SLOT(resetColor_clicked()));

          menu.addSeparator();
        }

        if (info->nexusId() > 0 && Settings::instance().nexus().endorsementIntegration()) {
          switch (info->endorsedState()) {
            case EndorsedState::ENDORSED_TRUE: {
              menu.addAction(tr("Un-Endorse"), this, SLOT(unendorse_clicked()));
            } break;
            case EndorsedState::ENDORSED_FALSE: {
              menu.addAction(tr("Endorse"), this, SLOT(endorse_clicked()));
              menu.addAction(tr("Won't endorse"), this, SLOT(dontendorse_clicked()));
            } break;
            case EndorsedState::ENDORSED_NEVER: {
              menu.addAction(tr("Endorse"), this, SLOT(endorse_clicked()));
            } break;
            default: {
              QAction *action = new QAction(tr("Endorsement state unknown"), &menu);
              action->setEnabled(false);
              menu.addAction(action);
            } break;
          }
        }

        if (info->nexusId() > 0) {
          switch (info->trackedState()) {
            case TrackedState::TRACKED_FALSE: {
              menu.addAction(tr("Start tracking"), this, SLOT(track_clicked()));
            } break;
            case TrackedState::TRACKED_TRUE: {
              menu.addAction(tr("Stop tracking"), this, SLOT(untrack_clicked()));
            } break;
            default: {
              QAction *action = new QAction(tr("Tracked state unknown"), &menu);
              action->setEnabled(false);
              menu.addAction(action);
            } break;
          }
        }

        menu.addSeparator();

        std::vector<ModInfo::EFlag> flags = info->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
          menu.addAction(tr("Ignore missing data"), this, SLOT(ignoreMissingData_clicked()));
        }

        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_ALTERNATE_GAME) != flags.end()) {
          menu.addAction(tr("Mark as converted/working"), this, SLOT(markConverted_clicked()));
        }

        menu.addSeparator();

        if (info->nexusId() > 0)  {
          menu.addAction(tr("Visit on Nexus"), this, SLOT(visitOnNexus_clicked()));
        }

        const auto url = info->parseCustomURL();
        if (url.isValid()) {
          menu.addAction(
            tr("Visit on %1").arg(url.host()),
            this, SLOT(visitWebPage_clicked()));
        }

        menu.addAction(tr("Open in Explorer"), this, SLOT(openExplorer_clicked()));
      }

      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) == flags.end()) {
        QAction *infoAction = menu.addAction(tr("Information..."), this, SLOT(information_clicked()));
        menu.setDefaultAction(infoAction);
      }

      menu.exec(modList->viewport()->mapToGlobal(pos));
    }
  } catch (const std::exception &e) {
    reportError(tr("Exception: ").arg(e.what()));
  } catch (...) {
    reportError(tr("Unknown exception"));
  }
}


void MainWindow::deleteSavegame_clicked()
{
  SaveGameInfo const *info = m_OrganizerCore.managedGame()->feature<SaveGameInfo>();

  QString savesMsgLabel;
  QStringList deleteFiles;

  int count = 0;

  for (const QModelIndex &idx : ui->savegameList->selectionModel()->selectedIndexes()) {
    QString name = idx.data(Qt::UserRole).toString();

    if (count < 10) {
      savesMsgLabel += "<li>" + QFileInfo(name).completeBaseName() + "</li>";
    }
    ++count;

    if (info == nullptr) {
      deleteFiles.push_back(name);
    } else {
      ISaveGame const *save = info->getSaveGameInfo(name);
      deleteFiles += save->allFiles();
      delete save;
    }
  }

  if (count > 10) {
    savesMsgLabel += "<li><i>... " + tr("%1 more").arg(count - 10) + "</i></li>";
  }

  if (QMessageBox::question(this, tr("Confirm"),
                            tr("Are you sure you want to remove the following %n save(s)?<br>"
                               "<ul>%1</ul><br>"
                               "Removed saves will be sent to the Recycle Bin.", "", count)
                              .arg(savesMsgLabel),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    shellDelete(deleteFiles, true); // recycle bin delete.
    refreshSaveList();
  }
}


void MainWindow::fixMods_clicked(SaveGameInfo::MissingAssets const &missingAssets)
{
  ActivateModsDialog dialog(missingAssets, this);
  if (dialog.exec() == QDialog::Accepted) {
    // activate the required mods, then enable all esps
    std::set<QString> modsToActivate = dialog.getModsToActivate();
    for (std::set<QString>::iterator iter = modsToActivate.begin(); iter != modsToActivate.end(); ++iter) {
      if ((*iter != "<data>") && (*iter != "<overwrite>")) {
        unsigned int modIndex = ModInfo::getIndex(*iter);
        m_OrganizerCore.currentProfile()->setModEnabled(modIndex, true);
      }
    }

    m_OrganizerCore.currentProfile()->writeModlist();
    m_OrganizerCore.refreshLists();

    std::set<QString> espsToActivate = dialog.getESPsToActivate();
    for (std::set<QString>::iterator iter = espsToActivate.begin(); iter != espsToActivate.end(); ++iter) {
      m_OrganizerCore.pluginList()->enableESP(*iter);
    }
    m_OrganizerCore.saveCurrentLists();
  }
}


void MainWindow::on_savegameList_customContextMenuRequested(const QPoint& pos)
{
  QItemSelectionModel* selection = ui->savegameList->selectionModel();

  if (!selection->hasSelection()) {
    return;
  }

  QMenu menu;

  SaveGameInfo const* info = this->m_OrganizerCore.managedGame()->feature<SaveGameInfo>();
  if (info != nullptr) {
    QAction* action = menu.addAction(tr("Enable Mods..."));
    action->setEnabled(false);
    if (selection->selectedIndexes().count() == 1) {
      QString save = ui->savegameList->currentItem()->data(Qt::UserRole).toString();
      SaveGameInfo::MissingAssets missing = info->getMissingAssets(save);
      if (missing.size() != 0) {
        connect(action, &QAction::triggered, this, [this, missing] { fixMods_clicked(missing); });
        action->setEnabled(true);
      }
    }
  }

  QString deleteMenuLabel = tr("Delete %n save(s)", "", selection->selectedIndexes().count());

  menu.addAction(deleteMenuLabel, this, SLOT(deleteSavegame_clicked()));

  menu.exec(ui->savegameList->viewport()->mapToGlobal(pos));
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
  if (auto* exe=getSelectedExecutable()) {
    env::Shortcut(*exe).toggle(env::Shortcut::Desktop);
  }
}

void MainWindow::linkMenu()
{
  if (auto* exe=getSelectedExecutable()) {
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

  m_LinkToolbar->setIcon(
    exe->isShownOnToolbar() ? removeIcon : addIcon);

  m_LinkDesktop->setIcon(
    shortcut.exists(env::Shortcut::Desktop) ? removeIcon : addIcon);

  m_LinkStartMenu->setIcon(
    shortcut.exists(env::Shortcut::StartMenu) ? removeIcon : addIcon);
}

void MainWindow::on_actionSettings_triggered()
{
  Settings &settings = m_OrganizerCore.settings();

  QString oldModDirectory(settings.paths().mods());
  QString oldCacheDirectory(settings.paths().cache());
  QString oldProfilesDirectory(settings.paths().profiles());
  QString oldManagedGameDirectory(settings.game().directory().value_or(""));
  bool oldDisplayForeign(settings.interface().displayForeign());
  bool proxy = settings.network().useProxy();
  DownloadManager *dlManager = m_OrganizerCore.downloadManager();
  const bool oldCheckForUpdates = settings.checkForUpdates();
  const int oldMaxDumps = settings.diagnostics().crashDumpsMax();


  SettingsDialog dialog(&m_PluginContainer, settings, this);
  dialog.exec();

  auto e = dialog.exitNeeded();

  if (oldManagedGameDirectory != settings.game().directory()) {
    e |= Exit::Restart;
  }

  if (e.testFlag(Exit::Restart)) {
    const auto r = MOBase::TaskDialog(this)
      .title(tr("Restart Mod Organizer"))
      .main("Restart Mod Organizer")
      .content(tr("Mod Organizer must restart to finish configuration changes"))
      .icon(QMessageBox::Question)
      .button({tr("Restart"), QMessageBox::Yes})
      .button({tr("Continue"), tr("Some things might be weird."), QMessageBox::No})
      .exec();

    if (r == QMessageBox::Yes) {
      ExitModOrganizer(e);
    }
  }

  InstallationManager *instManager = m_OrganizerCore.installationManager();
  instManager->setModsDirectory(settings.paths().mods());
  instManager->setDownloadDirectory(settings.paths().downloads());

  // Schedule a problem check since diagnose plugins may have been enabled / disabled.
  scheduleCheckForProblems();

  fixCategories();
  refreshFilters();

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

  if ((settings.paths().mods() != oldModDirectory)
      || (settings.interface().displayForeign() != oldDisplayForeign)) {
    m_OrganizerCore.profileRefresh();
  }

  const auto state = settings.archiveParsing();
  if (state != m_OrganizerCore.getArchiveParsing())
  {
    m_OrganizerCore.setArchiveParsing(state);
    if (!state)
    {
      ui->dataTabShowFromArchives->setCheckState(Qt::Unchecked);
      ui->dataTabShowFromArchives->setEnabled(false);
    }
    else
    {
      ui->dataTabShowFromArchives->setCheckState(Qt::Checked);
      ui->dataTabShowFromArchives->setEnabled(true);
    }
    m_OrganizerCore.refresh();
  }

  if (settings.paths().cache() != oldCacheDirectory) {
    NexusInterface::instance().setCacheDirectory(
      settings.paths().cache());
  }

  if (proxy != settings.network().useProxy()) {
    activateProxy(settings.network().useProxy());
  }

  ui->statusBar->checkSettings(m_OrganizerCore.settings());
  updateDownloadView();

  m_OrganizerCore.setLogLevel(settings.diagnostics().logLevel());

  if (settings.diagnostics().crashDumpsMax() != oldMaxDumps) {
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

void MainWindow::on_actionNexus_triggered()
{
  const IPluginGame *game = m_OrganizerCore.managedGame();
  QString gameName = game->gameShortName();
  if (game->gameNexusName().isEmpty() && game->primarySources().count())
    gameName = game->primarySources()[0];
  QDesktopServices::openUrl(QUrl(NexusInterface::instance().getGameURL(gameName)));
}


void MainWindow::linkClicked(const QString &url)
{
  QDesktopServices::openUrl(QUrl(url));
}


void MainWindow::installTranslator(const QString &name)
{
  QTranslator *translator = new QTranslator(this);
  QString fileName = name + "_" + m_CurrentLanguage;
  if (!translator->load(fileName, qApp->applicationDirPath() + "/translations")) {
    if (m_CurrentLanguage.contains(QRegularExpression("^.*_(EN|en)(-.*)?$"))) {
      log::debug("localization file %s not found", fileName);
    } // we don't actually expect localization files for English (en, en-us, en-uk, and any variation thereof)
  }

  qApp->installTranslator(translator);
  m_Translators.push_back(translator);
}


void MainWindow::languageChange(const QString &newLanguage)
{
  for (QTranslator *trans : m_Translators) {
    qApp->removeTranslator(trans);
  }
  m_Translators.clear();

  m_CurrentLanguage = newLanguage;

  installTranslator("qt");
  installTranslator("qtbase");
  installTranslator(ToQString(AppConfig::translationPrefix()));
  for (const QString &fileName : m_PluginContainer.pluginFileNames()) {
    installTranslator(QFileInfo(fileName).baseName());
  }
  ui->retranslateUi(this);
  log::debug("loaded language {}", newLanguage);

  ui->profileBox->setItemText(0, QObject::tr("<Manage...>"));

  createHelpMenu();

  updateDownloadView();

  QMenu *listOptionsMenu = new QMenu(ui->listOptionsBtn);
  initModListContextMenu(listOptionsMenu);
  ui->listOptionsBtn->setMenu(listOptionsMenu);

  ui->openFolderMenu->setMenu(openFolderMenu());
}

void MainWindow::originModified(int originID)
{
  FilesOrigin &origin = m_OrganizerCore.directoryStructure()->getOriginByID(originID);
  origin.enable(false);

  DirectoryStats dummy;
  m_OrganizerCore.directoryStructure()->addFromOrigin(
    origin.getName(), origin.getPath(), origin.getPriority(), dummy);

  DirectoryRefresher::cleanStructure(m_OrganizerCore.directoryStructure());
}


void MainWindow::enableSelectedPlugins_clicked()
{
  m_OrganizerCore.pluginList()->enableSelected(ui->espList->selectionModel());
}


void MainWindow::disableSelectedPlugins_clicked()
{
  m_OrganizerCore.pluginList()->disableSelected(ui->espList->selectionModel());
}

void MainWindow::sendSelectedPluginsToTop_clicked()
{
  m_OrganizerCore.pluginList()->sendToPriority(ui->espList->selectionModel(), 0);
}

void MainWindow::sendSelectedPluginsToBottom_clicked()
{
  m_OrganizerCore.pluginList()->sendToPriority(ui->espList->selectionModel(), INT_MAX);
}

void MainWindow::sendSelectedPluginsToPriority_clicked()
{
  bool ok;
  int newPriority = QInputDialog::getInt(this,
    tr("Set Priority"), tr("Set the priority of the selected plugins"),
    0, 0, INT_MAX, 1, &ok);
  if (!ok) return;

  m_OrganizerCore.pluginList()->sendToPriority(ui->espList->selectionModel(), newPriority);
}


void MainWindow::enableSelectedMods_clicked()
{
  m_OrganizerCore.modList()->enableSelected(ui->modList->selectionModel());
  if (m_ModListSortProxy != nullptr) {
    m_ModListSortProxy->invalidate();
  }
}


void MainWindow::disableSelectedMods_clicked()
{
  m_OrganizerCore.modList()->disableSelected(ui->modList->selectionModel());
  if (m_ModListSortProxy != nullptr) {
    m_ModListSortProxy->invalidate();
  }
}

void MainWindow::updateAvailable()
{
  ui->actionUpdate->setEnabled(true);
  ui->actionUpdate->setToolTip(tr("Update available"));
  ui->statusBar->setUpdateAvailable(true);
}


void MainWindow::motdReceived(const QString &motd)
{
  // don't show motd after 5 seconds, may be annoying. Hopefully the user's
  // internet connection is faster next time
  if (m_StartTime.secsTo(QTime::currentTime()) < 5) {
    uint hash = qHash(motd);
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
  // Normally this would be the managed game but MO2 is only uploaded to the Skyrim SE site right now
  IPluginGame * game = m_OrganizerCore.getGame("skyrimse");
  if (!game) return;

  if (QMessageBox::question(this, tr("Endorse Mod Organizer"),
                            tr("Do you want to endorse Mod Organizer on %1 now?").arg(
                                      NexusInterface::instance().getGameURL(game->gameShortName())),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    NexusInterface::instance().requestToggleEndorsement(
      game->gameShortName(), game->nexusModOrganizerID(), m_OrganizerCore.getVersion().canonicalString(), true, this, QVariant(), QString());
  }
}

void MainWindow::actionWontEndorseMO()
{
  // Normally this would be the managed game but MO2 is only uploaded to the Skyrim SE site right now
  IPluginGame * game = m_OrganizerCore.getGame("skyrimse");
  if (!game) return;

  if (QMessageBox::question(this, tr("Abstain from Endorsing Mod Organizer"),
    tr("Are you sure you want to abstain from endorsing Mod Organizer 2?\n"
      "You will have to visit the mod page on the %1 Nexus site to change your mind.").arg(
      NexusInterface::instance().getGameURL(game->gameShortName())),
    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    NexusInterface::instance().requestToggleEndorsement(
      game->gameShortName(), game->nexusModOrganizerID(), m_OrganizerCore.getVersion().canonicalString(), false, this, QVariant(), QString());
  }
}

void MainWindow::initDownloadView()
{
  DownloadList *sourceModel = new DownloadList(m_OrganizerCore.downloadManager(), ui->downloadView);
  DownloadListSortProxy *sortProxy = new DownloadListSortProxy(m_OrganizerCore.downloadManager(), ui->downloadView);
  sortProxy->setSourceModel(sourceModel);
  connect(ui->downloadFilterEdit, SIGNAL(textChanged(QString)), sortProxy, SLOT(updateFilter(QString)));
  connect(ui->downloadFilterEdit, SIGNAL(textChanged(QString)), this, SLOT(downloadFilterChanged(QString)));

  ui->downloadView->setSourceModel(sourceModel);
  ui->downloadView->setModel(sortProxy);
  ui->downloadView->setManager(m_OrganizerCore.downloadManager());
  ui->downloadView->setItemDelegate(new DownloadProgressDelegate(m_OrganizerCore.downloadManager(), sortProxy, ui->downloadView));
  updateDownloadView();

  connect(ui->downloadView, SIGNAL(installDownload(int)), &m_OrganizerCore, SLOT(installDownload(int)));
  connect(ui->downloadView, SIGNAL(queryInfo(int)), m_OrganizerCore.downloadManager(), SLOT(queryInfo(int)));
  connect(ui->downloadView, SIGNAL(queryInfoMd5(int)), m_OrganizerCore.downloadManager(), SLOT(queryInfoMd5(int)));
  connect(ui->downloadView, SIGNAL(visitOnNexus(int)), m_OrganizerCore.downloadManager(), SLOT(visitOnNexus(int)));
  connect(ui->downloadView, SIGNAL(openFile(int)), m_OrganizerCore.downloadManager(), SLOT(openFile(int)));
  connect(ui->downloadView, SIGNAL(openMetaFile(int)), m_OrganizerCore.downloadManager(), SLOT(openMetaFile(int)));
  connect(ui->downloadView, SIGNAL(openInDownloadsFolder(int)), m_OrganizerCore.downloadManager(), SLOT(openInDownloadsFolder(int)));
  connect(ui->downloadView, SIGNAL(removeDownload(int, bool)), m_OrganizerCore.downloadManager(), SLOT(removeDownload(int, bool)));
  connect(ui->downloadView, SIGNAL(restoreDownload(int)), m_OrganizerCore.downloadManager(), SLOT(restoreDownload(int)));
  connect(ui->downloadView, SIGNAL(cancelDownload(int)), m_OrganizerCore.downloadManager(), SLOT(cancelDownload(int)));
  connect(ui->downloadView, SIGNAL(pauseDownload(int)), m_OrganizerCore.downloadManager(), SLOT(pauseDownload(int)));
  connect(ui->downloadView, SIGNAL(resumeDownload(int)), this, SLOT(resumeDownload(int)));
}

void MainWindow::updateDownloadView()
{
  // this means downlaodTab initialization hasnt happened yet
  if (ui->downloadView->model() == nullptr) {
    return;
  }
  // set the view attribute and default row sizes
  if (m_OrganizerCore.settings().interface().compactDownloads()) {
    ui->downloadView->setProperty("downloadView", "compact");
    setStyleSheet("DownloadListWidget::item { padding: 4px 2px; }");
  } else {
    ui->downloadView->setProperty("downloadView", "standard");
    setStyleSheet("DownloadListWidget::item { padding: 16px 4px; }");
  }
  //setStyleSheet("DownloadListWidget::item:hover { padding: 0px; }");
  //setStyleSheet("DownloadListWidget::item:selected { padding: 0px; }");

  // reapply global stylesheet on the widget level (!) to override the defaults
  //ui->downloadView->setStyleSheet(styleSheet());

  ui->downloadView->setMetaDisplay(m_OrganizerCore.settings().interface().metaDownloads());
  ui->downloadView->style()->unpolish(ui->downloadView);
  ui->downloadView->style()->polish(ui->downloadView);
  qobject_cast<DownloadListHeader*>(ui->downloadView->header())->customResizeSections();
  m_OrganizerCore.downloadManager()->refreshList();
}

void MainWindow::modUpdateCheck(std::multimap<QString, int> IDs)
{
  if (NexusInterface::instance().getAccessManager()->validated()) {
    ModInfo::manualUpdateCheck(this, IDs);
  } else {
    QString apiKey;
    if (GlobalSettings::nexusApiKey(apiKey)) {
      m_OrganizerCore.doAfterLogin([=]() { this->modUpdateCheck(IDs); });
      NexusInterface::instance().getAccessManager()->apiCheck(apiKey);
    } else
      log::warn("{}", tr("You are not currently authenticated with Nexus. Please do so under Settings -> Nexus."));
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

  switch (s.nexus().endorsementState())
  {
    case EndorsementState::Accepted:
    {
      text = tr("Thank you for endorsing MO2! :)");
      break;
    }

    case EndorsementState::Refused:
    {
      text = tr("Please reconsider endorsing MO2 on Nexus!");
      break;
    }

    case EndorsementState::NoDecision:
    {
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

void MainWindow::nxmEndorsementsAvailable(QVariant userData, QVariant resultData, int)
{
  QVariantList data = resultData.toList();
  std::multimap<QString, std::pair<int, QString>> sorted;
  QStringList games = m_OrganizerCore.managedGame()->validShortNames();
  games += m_OrganizerCore.managedGame()->gameShortName();
  bool searchedMO2NexusGame = false;
  for (auto endorsementData : data) {
    QVariantMap endorsement = endorsementData.toMap();
    std::pair<int, QString> data = std::make_pair<int, QString>(endorsement["mod_id"].toInt(), endorsement["status"].toString());
    sorted.insert(std::pair<QString, std::pair<int, QString>>(endorsement["domain_name"].toString(), data));
  }
  for (auto game : games) {
    IPluginGame *gamePlugin = m_OrganizerCore.getGame(game);
    if (gamePlugin != nullptr && gamePlugin->gameShortName().compare("SkyrimSE", Qt::CaseInsensitive) == 0)
      searchedMO2NexusGame = true;
    auto iter = sorted.equal_range(gamePlugin->gameNexusName());
    for (auto result = iter.first; result != iter.second; ++result) {
      std::vector<ModInfo::Ptr> modsList = ModInfo::getByModID(result->first, result->second.first);

      for (auto mod : modsList) {
        if (result->second.second == "Endorsed")
          mod->setIsEndorsed(true);
        else if (result->second.second == "Abstained")
          mod->setNeverEndorse();
        else
          mod->setIsEndorsed(false);
      }

      if (Settings::instance().nexus().endorsementIntegration()) {
        if (result->first == "skyrimspecialedition" && result->second.first == gamePlugin->nexusModOrganizerID()) {
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

void MainWindow::nxmUpdateInfoAvailable(QString gameName, QVariant userData, QVariant resultData, int)
{
  QString gameNameReal;
  for (IPluginGame *game : m_PluginContainer.plugins<IPluginGame>()) {
    if (game->gameNexusName() == gameName) {
      gameNameReal = game->gameShortName();
      break;
    }
  }
  QVariantList resultList = resultData.toList();

  QFutureWatcher<std::pair<QString, std::set<QSharedPointer<ModInfo>>>> *watcher = new QFutureWatcher<std::pair<QString, std::set<QSharedPointer<ModInfo>>>>();
  QObject::connect(watcher, &QFutureWatcher<std::set<QSharedPointer<ModInfo>>>::finished, this, &MainWindow::finishUpdateInfo);
  QFuture<std::pair<QString, std::set<QSharedPointer<ModInfo>>>> future = QtConcurrent::run([=]() -> std::pair<QString, std::set<QSharedPointer<ModInfo>>> {
    return std::make_pair(gameNameReal, ModInfo::filteredMods(gameNameReal, resultList, userData.toBool(), true));
  });
  watcher->setFuture(future);
  if (m_ModListSortProxy != nullptr)
    m_ModListSortProxy->invalidate();
}

void MainWindow::finishUpdateInfo()
{
  QFutureWatcher<std::pair<QString, std::set<QSharedPointer<ModInfo>>>> *watcher = static_cast<QFutureWatcher<std::pair<QString, std::set<QSharedPointer<ModInfo>>>> *>(sender());

  QString game = watcher->result().first;
  auto finalMods = watcher->result().second;

  if (finalMods.empty()) {
    log::info("{}", tr("None of your %1 mods appear to have had recent file updates.").arg(game));
  }

  std::set<std::pair<QString, int>> organizedGames;
  for (auto mod : finalMods) {
    if (mod->canBeUpdated()) {
      organizedGames.insert(std::make_pair<QString, int>(mod->gameName().toLower(), mod->nexusId()));
    }
  }

  if (!finalMods.empty() && organizedGames.empty())
    log::warn("{}", tr("All of your mods have been checked recently. We restrict update checks to help preserve your available API requests."));

  for (auto game : organizedGames)
    NexusInterface::instance().requestUpdates(game.second, this, QVariant(), game.first, QString());

  disconnect(sender());
  delete sender();
}

void MainWindow::nxmUpdatesAvailable(QString gameName, int modID, QVariant userData, QVariant resultData, int requestID)
{
  QVariantMap resultInfo = resultData.toMap();
  QList files = resultInfo["files"].toList();
  QList fileUpdates = resultInfo["file_updates"].toList();
  QString gameNameReal;
  for (IPluginGame *game : m_PluginContainer.plugins<IPluginGame>()) {
    if (game->gameNexusName() == gameName) {
      gameNameReal = game->gameShortName();
      break;
    }
  }
  std::vector<ModInfo::Ptr> modsList = ModInfo::getByModID(gameNameReal, modID);

  bool requiresInfo = false;
  for (auto mod : modsList) {
    bool foundUpdate = false;
    bool oldFile = false;
    QString installedFile = mod->installationFile();
    if (!installedFile.isEmpty()) {
      QVariantMap foundFile;
      for (auto file : files) {
        QVariantMap fileData = file.toMap();
        if (fileData["file_name"].toString().compare(installedFile, Qt::CaseInsensitive) == 0) {
          foundFile = fileData;
          mod->setNexusFileStatus(foundFile["category_id"].toInt());
          if (mod->getNexusFileStatus() == 4 || mod->getNexusFileStatus() == 6)
            oldFile = true;
        }
      }
      for (auto update : fileUpdates) {
        QVariantMap updateData = update.toMap();
        // Locate the current install file as an update
        if (installedFile == updateData["old_file_name"].toString()) {
          int currentUpdate = updateData["new_file_id"].toInt();
          bool finalUpdate = false;
          // Crawl the updates to make sure we have the latest file version
          while (!finalUpdate) {
            finalUpdate = true;
            for (auto updateScan : fileUpdates) {
              QVariantMap updateScanData = updateScan.toMap();
              if (currentUpdate == updateScanData["old_file_id"].toInt()) {
                currentUpdate = updateScanData["new_file_id"].toInt();
                finalUpdate = false;
                // Apply the version data from the latest file
                for (auto file : files) {
                  QVariantMap fileData = file.toMap();
                  if (fileData["file_id"].toInt() == currentUpdate) {
                    if (fileData["category_id"].toInt() != 6) {
                      mod->setNewestVersion(fileData["version"].toString());
                      foundUpdate = true;
                    }
                  }
                }
                break;
              }
            }
          }
          break;
        } else if (installedFile == updateData["new_file_name"].toString()) {
          // This is a safety mechanism if this is the latest update file so we don't use the mod version
          if (!foundUpdate && !oldFile) {
            foundUpdate = true;
            mod->setNewestVersion(foundFile["version"].toString());
          }
        }
      }
    }

    if (foundUpdate) {
      // Just get the standard data updates for endorsements and descriptions
      mod->setLastNexusUpdate(QDateTime::currentDateTimeUtc());
      if (m_ModListSortProxy != nullptr)
        m_ModListSortProxy->invalidate();
    } else {
      // Scrape mod data here so we can use the mod version if no file update was located
      requiresInfo = true;
    }
  }

  if (requiresInfo)
    NexusInterface::instance().requestModInfo(gameNameReal, modID, this, QVariant(), QString());
}

void MainWindow::nxmModInfoAvailable(QString gameName, int modID, QVariant userData, QVariant resultData, int requestID)
{
  QVariantMap result = resultData.toMap();
  bool foundUpdate = false;
  QString gameNameReal;
  for (IPluginGame *game : m_PluginContainer.plugins<IPluginGame>()) {
    if (game->gameNexusName() == gameName) {
      gameNameReal = game->gameShortName();
      break;
    }
  }
  std::vector<ModInfo::Ptr> modsList = ModInfo::getByModID(gameNameReal, modID);
  for (auto mod : modsList) {
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime updateTarget = mod->getExpires();
    if (now >= updateTarget) {
      // if file is still listed as optional or miscellaneous don't update the version as often optional files are left
      // with an older version than the main mod version.
      if (mod->getNexusFileStatus() != 3 && mod->getNexusFileStatus() != 5) {
        mod->setNewestVersion(result["version"].toString());
        foundUpdate = true;
      }
      // update the LastNexusUpdate time in any case since we did perform the check.
      mod->setLastNexusUpdate(QDateTime::currentDateTimeUtc());
    }
    mod->setNexusDescription(result["description"].toString());
    if ((mod->endorsedState() != EndorsedState::ENDORSED_NEVER) && (result.contains("endorsement"))) {
      QVariantMap endorsement = result["endorsement"].toMap();
      QString endorsementStatus = endorsement["endorse_status"].toString();
      if (endorsementStatus.compare("Endorsed") == 00)
        mod->setIsEndorsed(true);
      else if (endorsementStatus.compare("Abstained") == 00)
        mod->setNeverEndorse();
      else
        mod->setIsEndorsed(false);
    }
    mod->setLastNexusQuery(QDateTime::currentDateTimeUtc());
    mod->setNexusLastModified(QDateTime::fromSecsSinceEpoch(result["updated_timestamp"].toInt(), Qt::UTC));
    mod->saveMeta();
  }
  if (foundUpdate && m_ModListSortProxy != nullptr)
    m_ModListSortProxy->invalidate();
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

  switch (s)
  {
    case EndorsementState::Accepted:
    {
      QMessageBox::information(this, tr("Thank you!"), tr("Thank you for your endorsement!"));
      break;
    }

    case EndorsementState::Refused:
    {
      // don't spam message boxes if the user doesn't want to endorse
      log::info("Mod Organizer will not be endorsed and will no longer ask you to endorse.");
      break;
    }

    case EndorsementState::NoDecision:
    {
      log::error("bad status '{}' in endorsement response", itor->toString());
      return;
    }
  }

  m_OrganizerCore.settings().nexus().setEndorsementState(s);
  toggleMO2EndorseState();

  if (!disconnect(sender(), SIGNAL(nxmEndorsementToggled(QString, int, QVariant, QVariant, int)),
    this, SLOT(nxmEndorsementToggled(QString, int, QVariant, QVariant, int)))) {
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

    bool found = false;
    auto resultsList = resultData.toList();
    for (auto item : resultsList) {
      auto results = item.toMap();
      if ((gameNames[results["domain_name"].toString()].compare(modInfo->gameName(), Qt::CaseInsensitive) == 0) &&
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

  for (const QVariant &var : resultData.toList()) {
    const QVariantMap map = var.toMap();

    const auto name = map["short_name"].toString();
    const auto isPremium = map["name"].toString().contains("Premium", Qt::CaseInsensitive);
    const auto isCDN = map["short_name"].toString().contains("CDN", Qt::CaseInsensitive);

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


void MainWindow::nxmRequestFailed(QString gameName, int modID, int, QVariant, int, QNetworkReply::NetworkError error, const QString &errorString)
{
  if (error == QNetworkReply::ContentAccessDenied || error == QNetworkReply::ContentNotFoundError) {
    log::debug("{}", tr("Mod ID %1 no longer seems to be available on Nexus.").arg(modID));

    // update last checked timestamp on orphaned mods as well to avoid repeating requests
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
    MessageDialog::showMessage(tr("Request to Nexus failed: %1").arg(errorString), this);
  }
}


BSA::EErrorCode MainWindow::extractBSA(BSA::Archive &archive, BSA::Folder::Ptr folder, const QString &destination,
                           QProgressDialog &progress)
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
    if (QMessageBox::critical(this, tr("Error"), tr("failed to extract %1 (errorcode %2)").arg(errorFile).arg(result),
                              QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
      return result;
    }
  }

  for (unsigned int i = 0; i < folder->getNumSubFolders(); ++i) {
    BSA::Folder::Ptr subFolder = folder->getSubFolder(i);
    BSA::EErrorCode res = extractBSA(archive, subFolder,
                                     destination.mid(0).append("/").append(subFolder->getName().c_str()), progress);
    if (res != BSA::ERROR_NONE) {
      return res;
    }
  }
  return BSA::ERROR_NONE;
}


bool MainWindow::extractProgress(QProgressDialog &progress, int percentage, std::string fileName)
{
  progress.setLabelText(fileName.c_str());
  progress.setValue(percentage);
  QCoreApplication::processEvents();
  return !progress.wasCanceled();
}


void MainWindow::extractBSATriggered()
{
  using namespace boost::placeholders;

  QTreeWidgetItem *item = m_ContextItem;
  QString origin;

  QString targetFolder = FileDialogMemory::getExistingDirectory("extractBSA", this, tr("Extract BSA"));
  QStringList archives = {};
  if (!targetFolder.isEmpty()) {
    if (!item->parent()) {
      for (int i = 0; i < item->childCount(); ++i) {
        archives.append(item->child(i)->text(0));
      }
      origin = QDir::fromNativeSeparators(ToQString(m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(item->text(0))).getPath()));
    } else {
      origin = QDir::fromNativeSeparators(ToQString(m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(item->text(1))).getPath()));
      archives = QStringList({ item->text(0) });
    }

    for (auto archiveName : archives) {
      BSA::Archive archive;
      QString archivePath = QString("%1\\%2").arg(origin).arg(archiveName);
      BSA::EErrorCode result = archive.read(archivePath.toLocal8Bit().constData(), true);
      if ((result != BSA::ERROR_NONE) && (result != BSA::ERROR_INVALIDHASHES)) {
        reportError(tr("failed to read %1: %2").arg(archivePath).arg(result));
        return;
      }

      QProgressDialog progress(this);
      progress.setMaximum(100);
      progress.setValue(0);
      progress.show();
      archive.extractAll(QDir::toNativeSeparators(targetFolder).toLocal8Bit().constData(),
        boost::bind(&MainWindow::extractProgress, this, boost::ref(progress), _1, _2));
      if (result == BSA::ERROR_INVALIDHASHES) {
        reportError(tr("This archive contains invalid hashes. Some files may be broken."));
      }
      archive.close();
    }
  }
}

void MainWindow::on_bsaList_customContextMenuRequested(const QPoint &pos)
{
  m_ContextItem = ui->bsaList->itemAt(pos);

//  m_ContextRow = ui->bsaList->indexOfTopLevelItem(ui->bsaList->itemAt(pos));

  QMenu menu;
  menu.addAction(tr("Extract..."), this, SLOT(extractBSATriggered()));

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

  ProblemsDialog problems(m_PluginContainer.plugins<QObject>(), this);
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
  if (visible) {
    ui->categoriesGroup->show();
    ui->displayCategoriesBtn->setText(ToQString(L"\u00ab"));
  } else {
    ui->categoriesGroup->hide();
    ui->displayCategoriesBtn->setText(ToQString(L"\u00bb"));
  }
}

void MainWindow::on_displayCategoriesBtn_toggled(bool checked)
{
  setCategoryListVisible(checked);
}

void MainWindow::deselectFilters()
{
  m_Filters->clearSelection();
}

void MainWindow::refreshFilters()
{
  QItemSelection currentSelection = ui->modList->selectionModel()->selection();

  int idxRow = ui->modList->currentIndex().row();
  QVariant currentIndexName = ui->modList->model()->index(idxRow, 0).data();
  ui->modList->setCurrentIndex(QModelIndex());

  m_Filters->refresh();

  ui->modList->selectionModel()->select(currentSelection, QItemSelectionModel::Select);

  QModelIndexList matchList;
  if (currentIndexName.isValid()) {
    matchList = ui->modList->model()->match(
      ui->modList->model()->index(0, 0),
      Qt::DisplayRole,
      currentIndexName);
  }

  if (matchList.size() > 0) {
    ui->modList->setCurrentIndex(matchList.at(0));
  }
}

void MainWindow::onFiltersCriteria(const std::vector<ModListSortProxy::Criteria>& criteria)
{
  m_ModListSortProxy->setCriteria(criteria);

  QString label = "?";

  if (criteria.empty()) {
    label = "";
  } else if (criteria.size() == 1) {
    const auto& c = criteria[0];

    if (c.type == ModListSortProxy::TypeContent) {
      const auto *content = m_OrganizerCore.modDataContents().findById(c.id);
      label = content ? content->name() : QString();
    } else {
      label = m_CategoryFactory.getCategoryNameByID(c.id);
    }

    if (label.isEmpty()) {
      log::error("category {}:{} not found", c.type, c.id);
    }
  } else {
    label = tr("<Multiple>");
  }

  ui->currentCategoryLabel->setText(label);
  ui->modList->reset();
}

void MainWindow::onFiltersOptions(
  ModListSortProxy::FilterMode mode, ModListSortProxy::SeparatorsMode sep)
{
  m_ModListSortProxy->setOptions(mode, sep);
}

void MainWindow::updateESPLock(bool locked)
{
  QItemSelection currentSelection = ui->espList->selectionModel()->selection();
  if (currentSelection.count() == 0) {
    // this path is probably useless
    m_OrganizerCore.pluginList()->lockESPIndex(m_ContextRow, locked);
  } else {
    Q_FOREACH (const QModelIndex &idx, currentSelection.indexes()) {
      if (m_OrganizerCore.pluginList()->isEnabled(mapToModel(m_OrganizerCore.pluginList(), idx).row())) {
        m_OrganizerCore.pluginList()->lockESPIndex(mapToModel(m_OrganizerCore.pluginList(), idx).row(), locked);
      }
    }
  }
}


void MainWindow::lockESPIndex()
{
  updateESPLock(true);
}

void MainWindow::unlockESPIndex()
{
  updateESPLock(false);
}


void MainWindow::removeFromToolbar()
{
  const auto& title = m_ContextAction->text();
  auto& list = *m_OrganizerCore.executablesList();

  auto itor = list.find(title);
  if (itor == list.end()) {
    log::warn("removeFromToolbar(): executable '{}' not found", title);
    return;
  }

  itor->setShownOnToolbar(false);
  updatePinnedExecutables();
}


void MainWindow::toolBar_customContextMenuRequested(const QPoint &point)
{
  QAction *action = ui->toolBar->actionAt(point);

  if (action != nullptr) {
    if (action->objectName().startsWith("custom_")) {
      m_ContextAction = action;
      QMenu menu;
      menu.addAction(tr("Remove '%1' from the toolbar").arg(action->text()), this, SLOT(removeFromToolbar()));
      menu.exec(ui->toolBar->mapToGlobal(point));
      return;
    }
  }

  // did not click a link button, show the default context menu
  auto* m = createPopupMenu();
  m->exec(ui->toolBar->mapToGlobal(point));
}

void MainWindow::on_espList_customContextMenuRequested(const QPoint &pos)
{
  m_ContextRow = m_PluginListSortProxy->mapToSource(ui->espList->indexAt(pos)).row();

  QMenu menu;
  menu.addAction(tr("Enable selected"), this, SLOT(enableSelectedPlugins_clicked()));
  menu.addAction(tr("Disable selected"), this, SLOT(disableSelectedPlugins_clicked()));

  menu.addSeparator();

  menu.addAction(tr("Enable all"), m_OrganizerCore.pluginList(), SLOT(enableAll()));
  menu.addAction(tr("Disable all"), m_OrganizerCore.pluginList(), SLOT(disableAll()));

  menu.addSeparator();

  addPluginSendToContextMenu(&menu);

  QItemSelection currentSelection = ui->espList->selectionModel()->selection();
  bool hasLocked = false;
  bool hasUnlocked = false;
  for (const QModelIndex &idx : currentSelection.indexes()) {
    int row = m_PluginListSortProxy->mapToSource(idx).row();
    if (m_OrganizerCore.pluginList()->isEnabled(row)) {
      if (m_OrganizerCore.pluginList()->isESPLocked(row)) {
        hasLocked = true;
      } else {
        hasUnlocked = true;
      }
    }
  }

  if (hasLocked) {
    menu.addAction(tr("Unlock load order"), this, SLOT(unlockESPIndex()));
  }
  if (hasUnlocked) {
    menu.addAction(tr("Lock load order"), this, SLOT(lockESPIndex()));
  }

  menu.addSeparator();


  QModelIndex idx = ui->espList->selectionModel()->currentIndex();
  unsigned int modInfoIndex = ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(idx.data().toString()));
  //this is to avoid showing the option on game files like skyrim.esm
  if (modInfoIndex != UINT_MAX) {
    menu.addAction(tr("Open Origin in Explorer"), this, SLOT(openPluginOriginExplorer_clicked()));
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modInfoIndex);
    std::vector<ModInfo::EFlag> flags = modInfo->getFlags();

    if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) == flags.end()) {
      QAction *infoAction = menu.addAction(tr("Open Origin Info..."), this, SLOT(openOriginInformation_clicked()));
      menu.setDefaultAction(infoAction);
    }
  }

  try {
    menu.exec(ui->espList->viewport()->mapToGlobal(pos));
  } catch (const std::exception &e) {
    reportError(tr("Exception: ").arg(e.what()));
  } catch (...) {
    reportError(tr("Unknown exception"));
  }
}

void MainWindow::on_groupCombo_currentIndexChanged(int index)
{
  if (m_ModListSortProxy == nullptr) {
    return;
  }
  QAbstractProxyModel *newModel = nullptr;
  switch (index) {
    case 1: {
        newModel = new QtGroupingProxy(m_OrganizerCore.modList(), QModelIndex(), ModList::COL_CATEGORY, Qt::UserRole,
                                       0, Qt::UserRole + 2);
      } break;
    case 2: {
        newModel = new QtGroupingProxy(m_OrganizerCore.modList(), QModelIndex(), ModList::COL_MODID, Qt::DisplayRole,
                                       QtGroupingProxy::FLAG_NOGROUPNAME | QtGroupingProxy::FLAG_NOSINGLE,
                                       Qt::UserRole + 2);
      } break;
    default: {
        newModel = nullptr;
      } break;
  }

  if (newModel != nullptr) {
#ifdef TEST_MODELS
    new ModelTest(newModel, this);
#endif // TEST_MODELS
    m_ModListSortProxy->setSourceModel(newModel);
    connect(ui->modList, SIGNAL(expanded(QModelIndex)),newModel, SLOT(expanded(QModelIndex)));
    connect(ui->modList, SIGNAL(collapsed(QModelIndex)), newModel, SLOT(collapsed(QModelIndex)));
    connect(newModel, SIGNAL(expandItem(QModelIndex)), this, SLOT(expandModList(QModelIndex)));
  } else {
    m_ModListSortProxy->setSourceModel(m_OrganizerCore.modList());
  }
  modFilterActive(m_ModListSortProxy->isFilterActive());
}

Executable* MainWindow::getSelectedExecutable()
{
  const QString name = ui->executablesListBox->itemText(
    ui->executablesListBox->currentIndex());

  try
  {
    return &m_OrganizerCore.executablesList()->get(name);
  }
  catch(std::runtime_error&)
  {
    return nullptr;
  }
}

void MainWindow::on_showHiddenBox_toggled(bool checked)
{
  m_OrganizerCore.downloadManager()->setShowHidden(checked);
}

void MainWindow::on_bossButton_clicked()
{
  const auto r = QMessageBox::question(
    this, tr("Sorting plugins"),
    tr("Are you sure you want to sort your plugins list?"),
    QMessageBox::Yes | QMessageBox::No);

  if (r != QMessageBox::Yes) {
    return;
  }


  m_OrganizerCore.savePluginList();

  setEnabled(false);
  ON_BLOCK_EXIT([&] () { setEnabled(true); });

  if (runLoot(this, m_OrganizerCore, m_DidUpdateMasterList)) {
    m_DidUpdateMasterList = true;
    m_OrganizerCore.refreshESPList(false);
    m_OrganizerCore.savePluginList();
  }
}


const char *MainWindow::PATTERN_BACKUP_GLOB = ".????_??_??_??_??_??";
const char *MainWindow::PATTERN_BACKUP_REGEX = "\\.(\\d\\d\\d\\d_\\d\\d_\\d\\d_\\d\\d_\\d\\d_\\d\\d)";
const char *MainWindow::PATTERN_BACKUP_DATE = "yyyy_MM_dd_hh_mm_ss";


bool MainWindow::createBackup(const QString &filePath, const QDateTime &time)
{
  QString outPath = filePath + "." + time.toString(PATTERN_BACKUP_DATE);
  if (shellCopy(QStringList(filePath), QStringList(outPath), this)) {
    QFileInfo fileInfo(filePath);
    removeOldFiles(fileInfo.absolutePath(), fileInfo.fileName() + PATTERN_BACKUP_GLOB, 10, QDir::Name);
    return true;
  } else {
    return false;
  }
}

void MainWindow::on_saveButton_clicked()
{
  m_OrganizerCore.savePluginList();
  QDateTime now = QDateTime::currentDateTime();
  if (createBackup(m_OrganizerCore.currentProfile()->getPluginsFileName(), now)
      && createBackup(m_OrganizerCore.currentProfile()->getLoadOrderFileName(), now)
      && createBackup(m_OrganizerCore.currentProfile()->getLockedOrderFileName(), now)) {
    MessageDialog::showMessage(tr("Backup of load order created"), this);
  }
}

QString MainWindow::queryRestore(const QString &filePath)
{
  QFileInfo pluginFileInfo(filePath);
  QString pattern = pluginFileInfo.fileName() + ".*";
  QFileInfoList files = pluginFileInfo.absoluteDir().entryInfoList(QStringList(pattern), QDir::Files, QDir::Name);

  SelectionDialog dialog(tr("Choose backup to restore"), this);
  QRegExp exp(pluginFileInfo.fileName() + PATTERN_BACKUP_REGEX);
  QRegExp exp2(pluginFileInfo.fileName() + "\\.(.*)");
  for(const QFileInfo &info : boost::adaptors::reverse(files)) {
    if (exp.exactMatch(info.fileName())) {
      QDateTime time = QDateTime::fromString(exp.cap(1), PATTERN_BACKUP_DATE);
      dialog.addChoice(time.toString(), "", exp.cap(1));
    } else if (exp2.exactMatch(info.fileName())) {
      dialog.addChoice(exp2.cap(1), "", exp2.cap(1));
    }
  }

  if (dialog.numChoices() == 0) {
    QMessageBox::information(this, tr("No Backups"), tr("There are no backups to restore"));
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
  QString choice = queryRestore(pluginName);
  if (!choice.isEmpty()) {
    QString loadOrderName = m_OrganizerCore.currentProfile()->getLoadOrderFileName();
    QString lockedName = m_OrganizerCore.currentProfile()->getLockedOrderFileName();
    if (!shellCopy(pluginName    + "." + choice, pluginName, true, this) ||
        !shellCopy(loadOrderName + "." + choice, loadOrderName, true, this) ||
        !shellCopy(lockedName    + "." + choice, lockedName, true, this)) {

      const auto e = GetLastError();

      QMessageBox::critical(
        this, tr("Restore failed"),
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
  QString choice = queryRestore(modlistName);
  if (!choice.isEmpty()) {
    if (!shellCopy(modlistName + "." + choice, modlistName, true, this)) {
      const auto e = GetLastError();
      QMessageBox::critical(
        this, tr("Restore failed"),
        tr("Failed to restore the backup. Errorcode: %1")
          .arg(formatSystemMessage(e)));
    }
    m_OrganizerCore.refresh(false);
  }
}

void MainWindow::on_managedArchiveLabel_linkHovered(const QString&)
{
  QToolTip::showText(QCursor::pos(),
  ui->managedArchiveLabel->toolTip());
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  //Accept copy or move drags to the download window. Link drags are not
  //meaningful (Well, they are - we could drop a link in the download folder,
  //but you need privileges to do that).
  if (ui->downloadTab->isVisible() &&
      (event->proposedAction() == Qt::CopyAction ||
       event->proposedAction() == Qt::MoveAction) &&
      event->answerRect().intersects(ui->downloadTab->rect())) {

    //If I read the documentation right, this won't work under a motif windows
    //manager and the check needs to be done at the drop. However, that means
    //the user might be allowed to drop things which we can't sanely process
    QMimeData const *data = event->mimeData();

    if (data->hasUrls()) {
      QStringList extensions =
                m_OrganizerCore.installationManager()->getSupportedExtensions();

      //This is probably OK - scan to see if these are moderately sane archive
      //types
      QList<QUrl> urls = data->urls();
      bool ok = true;
      for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
          QString local = url.toLocalFile();
          bool fok = false;
          for (auto ext : extensions) {
            if (local.endsWith(ext, Qt::CaseInsensitive)) {
              fok = true;
              break;
            }
          }
          if (! fok) {
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

void MainWindow::dropLocalFile(const QUrl &url, const QString &outputDir, bool move)
{
  QFileInfo file(url.toLocalFile());
  if (!file.exists()) {
    log::warn("invalid source file: {}", file.absoluteFilePath());
    return;
  }
  QString target = outputDir + "/" + file.fileName();
  if (QFile::exists(target)) {
    QMessageBox box(QMessageBox::Question,
                    file.fileName(),
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

void MainWindow::dropEvent(QDropEvent *event)
{
  Qt::DropAction action = event->proposedAction();
  QString outputDir = m_OrganizerCore.downloadManager()->getOutputDirectory();
  if (action == Qt::MoveAction) {
    //Tell windows I'm taking control and will delete the source of a move.
    event->setDropAction(Qt::TargetMoveAction);
  }
  for (const QUrl &url : event->mimeData()->urls()) {
    if (url.isLocalFile()) {
      dropLocalFile(url, outputDir, action == Qt::MoveAction);
    } else {
      m_OrganizerCore.downloadManager()->startDownloadURLs(QStringList() << url.url());
    }
  }
  event->accept();
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
  // if the menubar is hidden, pressing Alt will make it visible
  if (event->key() == Qt::Key_Alt) {
    if (!ui->menuBar->isVisible()) {
      ui->menuBar->show();
    }
  }

  QMainWindow::keyReleaseEvent(event);
}

void MainWindow::on_clearFiltersButton_clicked()
{
  ui->modFilterEdit->clear();
	deselectFilters();
}

void MainWindow::sendSelectedModsToPriority(int newPriority)
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    std::vector<int> modsToMove;
    for (auto idx : selection->selectedRows(ModList::COL_PRIORITY)) {
      modsToMove.push_back(m_OrganizerCore.currentProfile()->modIndexByPriority(idx.data().toInt()));
    }
    m_OrganizerCore.modList()->changeModPriority(modsToMove, newPriority);
  } else {
    m_OrganizerCore.modList()->changeModPriority(m_ContextRow, newPriority);
  }
}

void MainWindow::sendSelectedModsToTop_clicked()
{
  sendSelectedModsToPriority(0);
}

void MainWindow::sendSelectedModsToBottom_clicked()
{
  sendSelectedModsToPriority(INT_MAX);
}

void MainWindow::sendSelectedModsToPriority_clicked()
{
  bool ok;
  int newPriority = QInputDialog::getInt(this,
    tr("Set Priority"), tr("Set the priority of the selected mods"),
    0, 0, INT_MAX, 1, &ok);
  if (!ok) return;

 sendSelectedModsToPriority(newPriority);
}

void MainWindow::sendSelectedModsToSeparator_clicked()
{
  QStringList separators;
  auto indexesByPriority = m_OrganizerCore.currentProfile()->getAllIndexesByPriority();
  for (auto iter = indexesByPriority.begin(); iter != indexesByPriority.end(); iter++) {
    if ((iter->second != UINT_MAX)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(iter->second);
      if (modInfo->hasFlag(ModInfo::FLAG_SEPARATOR)) {
        separators << modInfo->name().chopped(10);  // Chops the "_separator" away from the name
      }
    }
  }

  ListDialog dialog(this);
  dialog.setWindowTitle("Select a separator...");
  dialog.setChoices(separators);

  if (dialog.exec() == QDialog::Accepted) {
    QString result = dialog.getChoice();
    if (!result.isEmpty()) {
      result += "_separator";

      int newPriority = INT_MAX;
      bool foundSection = false;
      for (auto mod : m_OrganizerCore.modsSortedByProfilePriority(m_OrganizerCore.currentProfile())) {
        unsigned int modIndex = ModInfo::getIndex(mod);
        ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
        if (!foundSection && result.compare(mod) == 0) {
          foundSection = true;
        } else if (foundSection && modInfo->hasFlag(ModInfo::FLAG_SEPARATOR)) {
          newPriority = m_OrganizerCore.currentProfile()->getModPriority(modIndex);
          break;
        }
      }

      QItemSelectionModel *selection = ui->modList->selectionModel();
      if (selection->hasSelection() && selection->selectedRows().count() > 1) {
        std::vector<int> modsToMove;
        for (QModelIndex idx : selection->selectedRows(ModList::COL_PRIORITY)) {
          modsToMove.push_back(m_OrganizerCore.currentProfile()->modIndexByPriority(idx.data().toInt()));
        }
        m_OrganizerCore.modList()->changeModPriority(modsToMove, newPriority);
      } else {
        int oldPriority = m_OrganizerCore.currentProfile()->getModPriority(m_ContextRow);
        if (oldPriority < newPriority)
          --newPriority;
        m_OrganizerCore.modList()->changeModPriority(m_ContextRow, newPriority);
      }
    }
  }
}
