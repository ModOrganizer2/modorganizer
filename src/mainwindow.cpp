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

#include "directoryentry.h"
#include "directoryrefresher.h"
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
#include "lockeddialog.h"
#include "waitingonclosedialog.h"
#include "logbuffer.h"
#include "downloadlistsortproxy.h"
#include "motddialog.h"
#include "filedialogmemory.h"
#include "tutorialmanager.h"
#include "modflagicondelegate.h"
#include "genericicondelegate.h"
#include "selectiondialog.h"
#include "csvbuilder.h"
#include "savetextasdialog.h"
#include "problemsdialog.h"
#include "previewdialog.h"
#include "browserdialog.h"
#include "aboutdialog.h"
#include <safewritefile.h>
#include "nxmaccessmanager.h"
#include "appconfig.h"
#include "eventfilter.h"
#include <utility.h>
#include <dataarchives.h>
#include <bsainvalidation.h>
#include <taskprogressmanager.h>
#include <scopeguard.h>
#include <usvfs.h>
#include "localsavegames.h"
#include "listdialog.h"

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
#include <QDesktopWidget>
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
#include <QSettings>
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
#include <boost/bind.hpp>
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

#ifdef TEST_MODELS
#include "modeltest.h"
#endif // TEST_MODELS

#pragma warning( disable : 4428 )

using namespace MOBase;
using namespace MOShared;

const QSize SmallToolbarSize(24, 24);
const QSize LargeToolbarSize(42, 36);


MainWindow::MainWindow(QSettings &initSettings
                       , OrganizerCore &organizerCore
                       , PluginContainer &pluginContainer
                       , QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
  , m_WasVisible(false)
  , m_Tutorial(this, "MainWindow")
  , m_OldProfileIndex(-1)
  , m_ModListGroupingProxy(nullptr)
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
{
  QWebEngineProfile::defaultProfile()->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
  QWebEngineProfile::defaultProfile()->setHttpCacheMaximumSize(52428800);
  QWebEngineProfile::defaultProfile()->setCachePath(m_OrganizerCore.settings().getCacheDirectory());
  QWebEngineProfile::defaultProfile()->setPersistentStoragePath(m_OrganizerCore.settings().getCacheDirectory());
  ui->setupUi(this);
  updateWindowTitle(QString(), 0, false);

  languageChange(m_OrganizerCore.settings().language());

  m_CategoryFactory.loadCategories();

  ui->logList->setModel(LogBuffer::instance());
  ui->logList->setColumnWidth(0, 100);
  ui->logList->setAutoScroll(true);
  ui->logList->scrollToBottom();
  ui->logList->addAction(ui->actionCopy_Log_to_Clipboard);
  int splitterSize = this->size().height(); // actually total window size, but the splitter doesn't seem to return the true value
  ui->topLevelSplitter->setSizes(QList<int>() << splitterSize - 100 << 100);
  connect(ui->logList->model(), SIGNAL(rowsInserted(const QModelIndex &, int, int)),
          ui->logList, SLOT(scrollToBottom()));
  connect(ui->logList->model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)),
          ui->logList, SLOT(scrollToBottom()));

  m_RefreshProgress = new QProgressBar(statusBar());
  m_RefreshProgress->setTextVisible(true);
  m_RefreshProgress->setRange(0, 100);
  m_RefreshProgress->setValue(0);
  m_RefreshProgress->setVisible(false);
  statusBar()->addWidget(m_RefreshProgress, 1000);
  statusBar()->clearMessage();

  updateProblemsButton();

  // Setup toolbar

  setupActionMenu(ui->actionTool);
  setupActionMenu(ui->actionHelp);
  setupActionMenu(ui->actionEndorseMO);

  createHelpMenu();
  createEndorseMenu();

  toggleMO2EndorseState();

  TaskProgressManager::instance().tryCreateTaskbar();

  // set up mod list
  m_ModListSortProxy = m_OrganizerCore.createModListProxyModel();

  ui->modList->setModel(m_ModListSortProxy);

  GenericIconDelegate *contentDelegate = new GenericIconDelegate(ui->modList, Qt::UserRole + 3, ModList::COL_CONTENT, 150);
  connect(ui->modList->header(), SIGNAL(sectionResized(int,int,int)), contentDelegate, SLOT(columnResized(int,int,int)));
  ui->modList->sortByColumn(ModList::COL_PRIORITY, Qt::AscendingOrder);
  ModFlagIconDelegate *flagDelegate = new ModFlagIconDelegate(ui->modList, ModList::COL_FLAGS, 120);
  connect(ui->modList->header(), SIGNAL(sectionResized(int,int,int)), flagDelegate, SLOT(columnResized(int,int,int)));
  ui->modList->setItemDelegateForColumn(ModList::COL_FLAGS, flagDelegate);
  ui->modList->setItemDelegateForColumn(ModList::COL_CONTENT, contentDelegate);
  ui->modList->header()->installEventFilter(m_OrganizerCore.modList());
  connect(ui->modList->header(), SIGNAL(sectionResized(int, int, int)), this, SLOT(modListSectionResized(int, int, int)));

  bool modListAdjusted = registerWidgetState(ui->modList->objectName(), ui->modList->header(), "mod_list_state");

  if (modListAdjusted) {
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
  }

  ui->modList->header()->setSectionHidden(ModList::COL_NAME, false); // prevent the name-column from being hidden
  ui->modList->installEventFilter(m_OrganizerCore.modList());

  // set up plugin list
  m_PluginListSortProxy = m_OrganizerCore.createPluginListProxyModel();

  ui->espList->setModel(m_PluginListSortProxy);
  ui->espList->sortByColumn(PluginList::COL_PRIORITY, Qt::AscendingOrder);
  ui->espList->setItemDelegateForColumn(PluginList::COL_FLAGS, new GenericIconDelegate(ui->espList));
  ui->espList->installEventFilter(m_OrganizerCore.pluginList());

  ui->bsaList->setLocalMoveOnly(true);

  initDownloadView();
  bool pluginListAdjusted = registerWidgetState(ui->espList->objectName(), ui->espList->header(), "plugin_list_state");
  registerWidgetState(ui->dataTree->objectName(), ui->dataTree->header());
  registerWidgetState(ui->downloadView->objectName(),
                      ui->downloadView->header());

  ui->splitter->setStretchFactor(0, 3);
  ui->splitter->setStretchFactor(1, 2);

  resizeLists(modListAdjusted, pluginListAdjusted);

  QMenu *linkMenu = new QMenu(this);
  linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Toolbar"), this, SLOT(linkToolbar()));
  linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Desktop"), this, SLOT(linkDesktop()));
  linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Start Menu"), this, SLOT(linkMenu()));
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

  ui->apiRequests->setAutoFillBackground(true);
  QPalette palette = ui->apiRequests->palette();
  palette.setColor(ui->apiRequests->backgroundRole(), Qt::darkGreen);
  palette.setColor(ui->apiRequests->foregroundRole(), Qt::white);
  ui->apiRequests->setPalette(palette);
  ui->apiRequests->setVisible(!m_OrganizerCore.settings().hideAPICounter());

  connect(&m_PluginContainer, SIGNAL(diagnosisUpdate()), this, SLOT(updateProblemsButton()));

  connect(ui->savegameList, SIGNAL(itemEntered(QListWidgetItem*)), this, SLOT(saveSelectionChanged(QListWidgetItem*)));

  connect(ui->modList, SIGNAL(dropModeUpdate(bool)), m_OrganizerCore.modList(), SLOT(dropModeUpdate(bool)));

  connect(m_ModListSortProxy, SIGNAL(filterActive(bool)), this, SLOT(modFilterActive(bool)));
  connect(m_ModListSortProxy, SIGNAL(layoutChanged()), this, SLOT(updateModCount()));
  connect(ui->modFilterEdit, SIGNAL(textChanged(QString)), m_ModListSortProxy, SLOT(updateFilter(QString)));

  connect(ui->espFilterEdit, SIGNAL(textChanged(QString)), m_PluginListSortProxy, SLOT(updateFilter(QString)));
  connect(ui->espFilterEdit, SIGNAL(textChanged(QString)), this, SLOT(espFilterChanged(QString)));

  connect(ui->dataTree, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(expandDataTreeItem(QTreeWidgetItem*)));

  connect(m_OrganizerCore.directoryRefresher(), SIGNAL(refreshed()), this, SLOT(directory_refreshed()));
  connect(m_OrganizerCore.directoryRefresher(), SIGNAL(progress(int)), this, SLOT(refresher_progress(int)));
  connect(m_OrganizerCore.directoryRefresher(), SIGNAL(error(QString)), this, SLOT(showError(QString)));

  connect(&m_SavesWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(refreshSavesIfOpen()));

  connect(&m_OrganizerCore.settings(), SIGNAL(languageChanged(QString)), this, SLOT(languageChange(QString)));
  connect(&m_OrganizerCore.settings(), SIGNAL(styleChanged(QString)), this, SIGNAL(styleChanged(QString)));

  connect(m_OrganizerCore.updater(), SIGNAL(restart()), this, SLOT(close()));
  connect(m_OrganizerCore.updater(), SIGNAL(updateAvailable()), this, SLOT(updateAvailable()));
  connect(m_OrganizerCore.updater(), SIGNAL(motdAvailable(QString)), this, SLOT(motdReceived(QString)));

  connect(NexusInterface::instance(&pluginContainer), SIGNAL(requestNXMDownload(QString)), &m_OrganizerCore, SLOT(downloadRequestedNXM(QString)));
  connect(NexusInterface::instance(&pluginContainer), SIGNAL(nxmDownloadURLsAvailable(QString,int,int,QVariant,QVariant,int)), this, SLOT(nxmDownloadURLs(QString,int,int,QVariant,QVariant,int)));
  connect(NexusInterface::instance(&pluginContainer), SIGNAL(needLogin()), &m_OrganizerCore, SLOT(nexusApi()));
  connect(NexusInterface::instance(&pluginContainer)->getAccessManager(), SIGNAL(validateFailed(QString)), this, SLOT(validationFailed(QString)));
  connect(NexusInterface::instance(&pluginContainer)->getAccessManager(), SIGNAL(credentialsReceived(const QString&, int, bool, std::tuple<int, int, int, int>)),
          this, SLOT(updateWindowTitle(const QString&, int, bool)));
  connect(NexusInterface::instance(&pluginContainer)->getAccessManager(), SIGNAL(credentialsReceived(const QString&, int, bool, std::tuple<int, int, int, int>)),
    NexusInterface::instance(&m_PluginContainer), SLOT(setRateMax(const QString&, int, bool, std::tuple<int, int, int, int>)));
  connect(NexusInterface::instance(&pluginContainer), SIGNAL(requestsChanged(int, std::tuple<int, int, int, int>)), this, SLOT(updateAPICounter(int, std::tuple<int, int, int, int>)));

  connect(&TutorialManager::instance(), SIGNAL(windowTutorialFinished(QString)), this, SLOT(windowTutorialFinished(QString)));
  connect(ui->tabWidget, SIGNAL(currentChanged(int)), &TutorialManager::instance(), SIGNAL(tabChanged(int)));
  connect(ui->modList->header(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)), this, SLOT(modListSortIndicatorChanged(int,Qt::SortOrder)));
  connect(ui->linksToolBar, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(linksToolBar_customContextMenuRequested(QPoint)));

  connect(&m_OrganizerCore, &OrganizerCore::modInstalled, this, &MainWindow::modInstalled);
  connect(&m_OrganizerCore, &OrganizerCore::close, this, &QMainWindow::close);

  connect(&m_IntegratedBrowser, SIGNAL(requestDownload(QUrl,QNetworkReply*)), &m_OrganizerCore, SLOT(requestDownload(QUrl,QNetworkReply*)));

  connect(this, SIGNAL(styleChanged(QString)), this, SLOT(updateStyle(QString)));

  m_CheckBSATimer.setSingleShot(true);
  connect(&m_CheckBSATimer, SIGNAL(timeout()), this, SLOT(checkBSAList()));

  connect(ui->espList->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(esplistSelectionsChanged(QItemSelection)));
  connect(ui->modList->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(modlistSelectionsChanged(QItemSelection)));

  new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Enter), this, SLOT(openExplorer_activated()));
  new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return), this, SLOT(openExplorer_activated()));

  new QShortcut(QKeySequence::Refresh, this, SLOT(refreshProfile_activated()));

  new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_F), this, SLOT(search_activated()));
  new QShortcut(QKeySequence(Qt::Key_Escape), this, SLOT(searchClear_activated()));

  m_UpdateProblemsTimer.setSingleShot(true);
  connect(&m_UpdateProblemsTimer, SIGNAL(timeout()), this, SLOT(updateProblemsButton()));

  m_SaveMetaTimer.setSingleShot(false);
  connect(&m_SaveMetaTimer, SIGNAL(timeout()), this, SLOT(saveModMetas()));
  m_SaveMetaTimer.start(5000);

  setCategoryListVisible(initSettings.value("categorylist_visible", true).toBool());
  FileDialogMemory::restore(initSettings);

  fixCategories();

  m_StartTime = QTime::currentTime();

  m_Tutorial.expose("modList", m_OrganizerCore.modList());
  m_Tutorial.expose("espList", m_OrganizerCore.pluginList());

  m_OrganizerCore.setUserInterface(this, this);
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
    ui->showArchiveDataCheckBox->setCheckState(Qt::Checked);
    ui->showArchiveDataCheckBox->setEnabled(true);
    m_showArchiveData = true;
  }
  else
  {
    ui->showArchiveDataCheckBox->setCheckState(Qt::Unchecked);
    ui->showArchiveDataCheckBox->setEnabled(false);
    m_showArchiveData = false;
  }

  refreshExecutablesList();
  updateToolBar();

  for (QAction *action : ui->toolBar->actions()) {
    // set the name of the widget to the name of the action to allow styling
    QWidget *actionWidget = ui->toolBar->widgetForAction(action);
    actionWidget->setObjectName(action->objectName());
    actionWidget->style()->unpolish(actionWidget);
    actionWidget->style()->polish(actionWidget);
  }

  updatePluginCount();
  updateModCount();
}


MainWindow::~MainWindow()
{
  try {
    cleanup();

    m_PluginContainer.setUserInterface(nullptr, nullptr);
    m_OrganizerCore.setUserInterface(nullptr, nullptr);
    m_IntegratedBrowser.close();
    delete ui;
  } catch (std::exception &e) {
    QMessageBox::critical(nullptr, tr("Crash on exit"),
      tr("MO crashed while exiting.  Some settings may not be saved.\n\nError: %1").arg(e.what()),
      QMessageBox::Ok);
  }
}


void MainWindow::updateWindowTitle(const QString &accountName, int, bool premium)
{
  QString title = QString("%1 Mod Organizer v%2").arg(
        m_OrganizerCore.managedGame()->gameName(),
        m_OrganizerCore.getVersion().displayString(3));

  if (!accountName.isEmpty()) {
    title.append(QString(" (%1%2)").arg(accountName, premium ? "*" : ""));
  }

  this->setWindowTitle(title);
}


void MainWindow::disconnectPlugins()
{
  if (ui->actionTool->menu() != nullptr) {
    ui->actionTool->menu()->clear();
  }
}


void MainWindow::resizeLists(bool modListCustom, bool pluginListCustom)
{
  if (!modListCustom) {
    // resize mod list to fit content
    for (int i = 0; i < ui->modList->header()->count(); ++i) {
      ui->modList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    ui->modList->header()->setSectionResizeMode(ModList::COL_NAME, QHeaderView::Stretch);
  }

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
  //ui->modList->header()->setSectionResizeMode(ui->modList->header()->count() - 1, QHeaderView::Stretch);
  ui->modList->header()->setStretchLastSection(true);


  // allow resize on plugin list
  for (int i = 0; i < ui->espList->header()->count(); ++i) {
    ui->espList->header()->setSectionResizeMode(i, QHeaderView::Interactive);
  }
  //ui->espList->header()->setSectionResizeMode(ui->espList->header()->count() - 1, QHeaderView::Stretch);
  ui->espList->header()->setStretchLastSection(true);
}

void MainWindow::updateStyle(const QString&)
{
  // no effect?
  ensurePolished();
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

void MainWindow::setupActionMenu(QAction* a)
{
  a->setMenu(new QMenu(this));

  auto* w = ui->toolBar->widgetForAction(a);
  if (auto* tb=dynamic_cast<QToolButton*>(w))
    tb->setPopupMode(QToolButton::InstantPopup);
}

void MainWindow::updateToolBar()
{
  for (auto* a : ui->linksToolBar->actions()) {
    ui->linksToolBar->removeAction(a);
    a->deleteLater();
  }

  bool hasLinks = false;

  std::vector<Executable>::iterator begin, end;
  m_OrganizerCore.executablesList()->getExecutables(begin, end);

  for (auto iter = begin; iter != end; ++iter) {
    if (iter->isShownOnToolbar()) {
      hasLinks = true;

      QAction *exeAction = new QAction(iconForExecutable(iter->m_BinaryInfo.filePath()),
                                        iter->m_Title,
                                        ui->toolBar);

      exeAction->setObjectName(QString("custom__") + iter->m_Title);
      if (!connect(exeAction, SIGNAL(triggered()), this, SLOT(startExeAction()))) {
        qDebug("failed to connect trigger?");
      }

      ui->linksToolBar->addAction(exeAction);
    }
  }

  // don't show the toolbar if there are no links
  ui->linksToolBar->setVisible(hasLinks);
}

QMenu* MainWindow::createPopupMenu()
{
  auto* m = QMainWindow::createPopupMenu();

  m->addSeparator();

  auto* a = new QAction(tr("Small Icons"), m);
  connect(a, &QAction::triggered, [&]{ setToolbarSize(SmallToolbarSize); });
  a->setCheckable(true);
  a->setChecked(ui->toolBar->iconSize() == SmallToolbarSize);
  m->addAction(a);

  a = new QAction(tr("Large Icons"), m);
  connect(a, &QAction::triggered, [&]{ setToolbarSize(LargeToolbarSize); });
  a->setCheckable(true);
  a->setChecked(ui->toolBar->iconSize() == LargeToolbarSize);
  m->addAction(a);

  m->addSeparator();

  a = new QAction(tr("Icons only"), m);
  connect(a, &QAction::triggered, [&]{ setToolbarButtonStyle(Qt::ToolButtonIconOnly); });
  a->setCheckable(true);
  a->setChecked(ui->toolBar->toolButtonStyle() == Qt::ToolButtonIconOnly);
  m->addAction(a);

  a = new QAction(tr("Text only"), m);
  connect(a, &QAction::triggered, [&]{ setToolbarButtonStyle(Qt::ToolButtonTextOnly); });
  a->setCheckable(true);
  a->setChecked(ui->toolBar->toolButtonStyle() == Qt::ToolButtonTextOnly);
  m->addAction(a);

  a = new QAction(tr("Text and Icons"), m);
  connect(a, &QAction::triggered, [&]{ setToolbarButtonStyle(Qt::ToolButtonTextUnderIcon); });
  a->setCheckable(true);
  a->setChecked(ui->toolBar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon);
  m->addAction(a);

  return m;
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

void MainWindow::scheduleUpdateButton()
{
  if (!m_UpdateProblemsTimer.isActive()) {
    m_UpdateProblemsTimer.start(1000);
  }
}

void MainWindow::updateProblemsButton()
{
  size_t numProblems = checkForProblems();
  if (numProblems > 0) {
    ui->actionNotifications->setToolTip(tr("There are notifications to read"));

    QPixmap mergedIcon = QPixmap(":/MO/gui/warning").scaled(64, 64);
    {
      QPainter painter(&mergedIcon);
      std::string badgeName = std::string(":/MO/gui/badge_") + (numProblems < 10 ? std::to_string(static_cast<long long>(numProblems)) : "more");
      painter.drawPixmap(32, 32, 32, 32, QPixmap(badgeName.c_str()));
    }
    ui->actionNotifications->setIcon(QIcon(mergedIcon));
  } else {
    ui->actionNotifications->setToolTip(tr("There are no notifications"));
    ui->actionNotifications->setIcon(QIcon(":/MO/gui/warning"));
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


size_t MainWindow::checkForProblems()
{
  size_t numProblems = 0;
  for (QObject *pluginObj : m_PluginContainer.plugins<QObject>()) {
    IPlugin *plugin = qobject_cast<IPlugin*>(pluginObj);
    if (plugin == nullptr || plugin->isActive()) {
      IPluginDiagnose *diagnose = qobject_cast<IPluginDiagnose*>(pluginObj);
      if (diagnose != nullptr)
        numProblems += diagnose->activeProblems().size();
    }
  }
  return numProblems;
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
      qCritical() << "Failed to open " << fileName;
      continue;
    }
    QString firstLine = QString::fromUtf8(file.readLine());
    if (firstLine.startsWith("//TL")) {
      QStringList params = firstLine.mid(4).trimmed().split('#');
      if (params.size() != 2) {
        qCritical() << "invalid header line for tutorial " << fileName << " expected 2 parameters";
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
#pragma message("why is this so complicated? mapping the index doesn't work, probably a bug in QtGroupingProxy?")
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
      qCritical() << "Failed to open " << fileName;
      continue;
    }
    QString firstLine = QString::fromUtf8(file.readLine());
    if (firstLine.startsWith("//WIN")) {
      QString windowName = firstLine.mid(6).trimmed();
      if (!m_OrganizerCore.settings().directInterface().value("CompletedWindowTutorials/" + windowName, false).toBool()) {
        TutorialManager::instance().activateTutorial(windowName, fileName);
      }
    }
  }
}

void MainWindow::showEvent(QShowEvent *event)
{
  refreshFilters();

  QMainWindow::showEvent(event);

  if (!m_WasVisible) {
    // only the first time the window becomes visible
    m_Tutorial.registerControl();

    hookUpWindowTutorials();

    if (m_OrganizerCore.settings().directInterface().value("first_start", true).toBool()) {
      QString firstStepsTutorial = ToQString(AppConfig::firstStepsTutorial());
      if (TutorialManager::instance().hasTutorial(firstStepsTutorial)) {
        if (QMessageBox::question(this, tr("Show tutorial?"),
                                  tr("You are starting Mod Organizer for the first time. "
                                     "Do you want to show a tutorial of its basic features? If you choose "
                                     "no you can always start the tutorial from the \"Help\"-menu."),
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
          TutorialManager::instance().activateTutorial("MainWindow", firstStepsTutorial);
        }
      } else {
        qCritical() << firstStepsTutorial << " missing";
        QPoint pos = ui->toolBar->mapToGlobal(QPoint());
        pos.rx() += ui->toolBar->width() / 2;
        pos.ry() += ui->toolBar->height();
        QWhatsThis::showText(pos,
            QObject::tr("Please use \"Help\" from the toolbar to get usage instructions to all elements"));
      }

    m_OrganizerCore.settings().directInterface().setValue("first_start", false);
    }

    // this has no visible impact when called before the ui is visible
    int grouping = m_OrganizerCore.settings().directInterface().value("group_state").toInt();
    ui->groupCombo->setCurrentIndex(grouping);

    allowListResize();

    m_OrganizerCore.settings().registerAsNXMHandler(false);
    m_WasVisible = true;
	updateProblemsButton();
  }
}


void MainWindow::closeEvent(QCloseEvent* event)
{
  if (!exit()) {
    event->ignore();
  }
}

bool MainWindow::exit()
{
  m_closing = true;

  if (m_OrganizerCore.downloadManager()->downloadsInProgressNoPause()) {
    if (QMessageBox::question(this, tr("Downloads in progress"),
                          tr("There are still downloads in progress, do you really want to quit?"),
                          QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Cancel) {
      return false;
    } else {
      m_OrganizerCore.downloadManager()->pauseAll();
    }
  }

  std::vector<QString> hiddenList;
  hiddenList.push_back(QFileInfo(QCoreApplication::applicationFilePath()).fileName());
  HANDLE injected_process_still_running = m_OrganizerCore.findAndOpenAUSVFSProcess(hiddenList, GetCurrentProcessId());
  if (injected_process_still_running != INVALID_HANDLE_VALUE)
  {
    m_OrganizerCore.waitForApplication(injected_process_still_running);
    if (!m_closing) { // if operation cancelled
      return false;
    }
  }

  setCursor(Qt::WaitCursor);
  return true;
}

void MainWindow::cleanup()
{
  if (ui->logList->model() != nullptr) {
    disconnect(ui->logList->model(), nullptr, nullptr, nullptr);
    ui->logList->setModel(nullptr);
  }

  QWebEngineProfile::defaultProfile()->clearAllVisitedLinks();
  m_IntegratedBrowser.close();
  m_SaveMetaTimer.stop();
  m_MetaSave.waitForFinished();
}


void MainWindow::setBrowserGeometry(const QByteArray &geometry)
{
  m_IntegratedBrowser.restoreGeometry(geometry);
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

  QRect screenRect = QApplication::desktop()->availableGeometry(m_CurrentSaveView);

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
  m_CurrentSaveView->setProperty("displayItem", qVariantFromValue(static_cast<void *>(newItem)));
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
      m_IntegratedBrowser.setWindowTitle(plugin->displayName());
      m_IntegratedBrowser.openUrl(plugin->pageURL());
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
  action->setData(qVariantFromValue((QObject*)tool));
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
  action->setData(qVariantFromValue(reinterpret_cast<QObject*>(modPage)));

  connect(action, SIGNAL(triggered()), this, SLOT(modPagePluginInvoke()), Qt::QueuedConnection);

  m_browseModPage->menu()->addAction(action);
}


void MainWindow::startExeAction()
{
  QAction *action = qobject_cast<QAction*>(sender());
  if (action != nullptr) {
    const Executable &selectedExecutable(m_OrganizerCore.executablesList()->find(action->text()));
    QString customOverwrite = m_OrganizerCore.currentProfile()->setting("custom_overwrites", selectedExecutable.m_Title).toString();
    auto forcedLibraries = m_OrganizerCore.currentProfile()->determineForcedLibraries(selectedExecutable.m_Title);
    if (!m_OrganizerCore.currentProfile()->forcedLibrariesEnabled(selectedExecutable.m_Title)) {
      forcedLibraries.clear();
    }
    m_OrganizerCore.spawnBinary(
        selectedExecutable.m_BinaryInfo, selectedExecutable.m_Arguments,
        selectedExecutable.m_WorkingDirectory.length() != 0
            ? selectedExecutable.m_WorkingDirectory
            : selectedExecutable.m_BinaryInfo.absolutePath(),
        selectedExecutable.m_SteamAppID,
        customOverwrite,
        forcedLibraries);
  } else {
    qCritical("not an action?");
  }
}


void MainWindow::setExecutableIndex(int index)
{
  QComboBox *executableBox = findChild<QComboBox*>("executablesListBox");

  if ((index != 0) && (executableBox->count() > index)) {
    executableBox->setCurrentIndex(index);
  } else {
    executableBox->setCurrentIndex(1);
  }
}

void MainWindow::activateSelectedProfile()
{
  m_OrganizerCore.setCurrentProfile(ui->profileBox->currentText());

  m_ModListSortProxy->setProfile(m_OrganizerCore.currentProfile());

  refreshSaveList();
  m_OrganizerCore.refreshModList();
  updateModCount();
  updatePluginCount();
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

    // ensure the new index is valid
    if (index < 0 || index >= ui->profileBox->count()) {
      qDebug("invalid profile index, using last profile");
      ui->profileBox->setCurrentIndex(ui->profileBox->count() - 1);
    }

    if (ui->profileBox->currentIndex() == 0) {
      ui->profileBox->setCurrentIndex(previousIndex);
      ProfilesDialog(ui->profileBox->currentText(), m_OrganizerCore.managedGame(), this).exec();
      while (!refreshProfiles()) {
        ProfilesDialog(ui->profileBox->currentText(), m_OrganizerCore.managedGame(), this).exec();
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

void MainWindow::updateTo(QTreeWidgetItem *subTree, const std::wstring &directorySoFar, const DirectoryEntry &directoryEntry, bool conflictsOnly, QIcon *fileIcon, QIcon *folderIcon)
{
  bool isDirectory = true;
  //QIcon folderIcon = (new QFileIconProvider())->icon(QFileIconProvider::Folder);
  //QIcon fileIcon = (new QFileIconProvider())->icon(QFileIconProvider::File);

  std::wostringstream temp;
  temp << directorySoFar << "\\" << directoryEntry.getName();
  {
    std::vector<DirectoryEntry*>::const_iterator current, end;
    directoryEntry.getSubDirectories(current, end);
    for (; current != end; ++current) {
      QString pathName = ToQString((*current)->getName());
      QStringList columns(pathName);
      columns.append("");
      if (!(*current)->isEmpty()) {
        QTreeWidgetItem *directoryChild = new QTreeWidgetItem(columns);
        directoryChild->setData(0, Qt::DecorationRole, *folderIcon);
        directoryChild->setData(0, Qt::UserRole + 3, isDirectory);

        if (conflictsOnly || !m_showArchiveData) {
          updateTo(directoryChild, temp.str(), **current, conflictsOnly, fileIcon, folderIcon);
          if (directoryChild->childCount() != 0) {
            subTree->addChild(directoryChild);
          }
          else {
            delete directoryChild;
          }
        }
        else {
          QTreeWidgetItem *onDemandLoad = new QTreeWidgetItem(QStringList());
          onDemandLoad->setData(0, Qt::UserRole + 0, "__loaded_on_demand__");
          onDemandLoad->setData(0, Qt::UserRole + 1, ToQString(temp.str()));
          onDemandLoad->setData(0, Qt::UserRole + 2, conflictsOnly);
          directoryChild->addChild(onDemandLoad);
          subTree->addChild(directoryChild);
        }
      }
      else {
        QTreeWidgetItem *directoryChild = new QTreeWidgetItem(columns);
        directoryChild->setData(0, Qt::DecorationRole, *folderIcon);
        directoryChild->setData(0, Qt::UserRole + 3, isDirectory);
        subTree->addChild(directoryChild);
      }
    }
  }


  isDirectory = false;
  {
    for (const FileEntry::Ptr current : directoryEntry.getFiles()) {
      if (conflictsOnly && (current->getAlternatives().size() == 0)) {
        continue;
      }

      bool isArchive = false;
      int originID = current->getOrigin(isArchive);
      if (!m_showArchiveData && isArchive) {
        continue;
      }

      QString fileName = ToQString(current->getName());
      QStringList columns(fileName);
      FilesOrigin origin = m_OrganizerCore.directoryStructure()->getOriginByID(originID);
      QString source("data");
      unsigned int modIndex = ModInfo::getIndex(ToQString(origin.getName()));
      if (modIndex != UINT_MAX) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
        source = modInfo->name();
      }

      std::pair<std::wstring, int> archive = current->getArchive();
      if (archive.first.length() != 0) {
        source.append(" (").append(ToQString(archive.first)).append(")");
      }
      columns.append(source);
      QTreeWidgetItem *fileChild = new QTreeWidgetItem(columns);
      if (isArchive) {
        QFont font = fileChild->font(0);
        font.setItalic(true);
        fileChild->setFont(0, font);
        fileChild->setFont(1, font);
      } else if (fileName.endsWith(ModInfo::s_HiddenExt)) {
        QFont font = fileChild->font(0);
        font.setStrikeOut(true);
        fileChild->setFont(0, font);
        fileChild->setFont(1, font);
      }
      fileChild->setData(0, Qt::UserRole, ToQString(current->getFullPath()));
      fileChild->setData(0, Qt::DecorationRole, *fileIcon);
      fileChild->setData(0, Qt::UserRole + 3, isDirectory);
      fileChild->setData(0, Qt::UserRole + 1, isArchive);
      fileChild->setData(1, Qt::UserRole, source);
      fileChild->setData(1, Qt::UserRole + 1, originID);

      std::vector<std::pair<int, std::pair<std::wstring, int>>> alternatives = current->getAlternatives();

      if (!alternatives.empty()) {
        std::wostringstream altString;
        altString << ToWString(tr("Also in: <br>"));
        for (std::vector<std::pair<int, std::pair<std::wstring, int>>>::iterator altIter = alternatives.begin();
             altIter != alternatives.end(); ++altIter) {
          if (altIter != alternatives.begin()) {
            altString << " , ";
          }
          altString << "<span style=\"white-space: nowrap;\"><i>" << m_OrganizerCore.directoryStructure()->getOriginByID(altIter->first).getName() << "</font></span>";
        }
        fileChild->setToolTip(1, QString("%1").arg(ToQString(altString.str())));
        fileChild->setForeground(1, QBrush(Qt::red));
      } else {
        fileChild->setToolTip(1, tr("No conflict"));
      }
      subTree->addChild(fileChild);
    }
  }


  //subTree->sortChildren(0, Qt::AscendingOrder);
}

void MainWindow::delayedRemove()
{
  for (QTreeWidgetItem *item : m_RemoveWidget) {
    item->removeChild(item->child(0));
  }
  m_RemoveWidget.clear();
}

void MainWindow::expandDataTreeItem(QTreeWidgetItem *item)
{
  if ((item->childCount() == 1) && (item->child(0)->data(0, Qt::UserRole).toString() == "__loaded_on_demand__")) {
    // read the data we need from the sub-item, then dispose of it
    QTreeWidgetItem *onDemandDataItem = item->child(0);
    std::wstring path = ToWString(onDemandDataItem->data(0, Qt::UserRole + 1).toString());
    bool conflictsOnly = onDemandDataItem->data(0, Qt::UserRole + 2).toBool();

    std::wstring virtualPath = (path + L"\\").substr(6) + ToWString(item->text(0));
    DirectoryEntry *dir = m_OrganizerCore.directoryStructure()->findSubDirectoryRecursive(virtualPath);
    if (dir != nullptr) {
      QIcon folderIcon = (new QFileIconProvider())->icon(QFileIconProvider::Folder);
      QIcon fileIcon = (new QFileIconProvider())->icon(QFileIconProvider::File);
      updateTo(item, path, *dir, conflictsOnly, &fileIcon, &folderIcon);
    } else {
      qWarning("failed to update view of %ls", path.c_str());
    }
    m_RemoveWidget.push_back(item);
    QTimer::singleShot(5, this, SLOT(delayedRemove()));
  }
}


bool MainWindow::refreshProfiles(bool selectProfile)
{
  QComboBox* profileBox = findChild<QComboBox*>("profileBox");

  QString currentProfileName = profileBox->currentText();

  profileBox->blockSignals(true);
  profileBox->clear();
  profileBox->addItem(QObject::tr("<Manage...>"));

  QDir profilesDir(Settings::instance().getProfileDirectory());
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
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");
  executablesList->setEnabled(false);
  executablesList->clear();
  executablesList->addItem(tr("<Edit...>"));

  QAbstractItemModel *model = executablesList->model();

  std::vector<Executable>::const_iterator current, end;
  m_OrganizerCore.executablesList()->getExecutables(current, end);
  for(int i = 0; current != end; ++current, ++i) {
    QIcon icon = iconForExecutable(current->m_BinaryInfo.filePath());
    executablesList->addItem(icon, current->m_Title);
    model->setData(model->index(i, 0), QSize(0, executablesList->iconSize().height() + 4), Qt::SizeHintRole);
  }

  setExecutableIndex(1);
  executablesList->setEnabled(true);
}


void MainWindow::refreshDataTree()
{
  QCheckBox *conflictsBox = findChild<QCheckBox*>("conflictsCheckBox");
  QTreeWidget *tree = findChild<QTreeWidget*>("dataTree");
  QIcon folderIcon = (new QFileIconProvider())->icon(QFileIconProvider::Folder);
  QIcon fileIcon = (new QFileIconProvider())->icon(QFileIconProvider::File);
  tree->clear();
  QStringList columns("data");
  columns.append("");
  QTreeWidgetItem *subTree = new QTreeWidgetItem(columns);
  subTree->setData(0, Qt::DecorationRole, (new QFileIconProvider())->icon(QFileIconProvider::Folder));
  updateTo(subTree, L"", *m_OrganizerCore.directoryStructure(), conflictsBox->isChecked(), &fileIcon, &folderIcon);
  tree->insertTopLevelItem(0, subTree);
  subTree->setExpanded(true);
}

void MainWindow::refreshDataTreeKeepExpandedNodes()
{
	QCheckBox *conflictsBox = findChild<QCheckBox*>("conflictsCheckBox");
	QTreeWidget *tree = findChild<QTreeWidget*>("dataTree");
  QIcon folderIcon = (new QFileIconProvider())->icon(QFileIconProvider::Folder);
  QIcon fileIcon = (new QFileIconProvider())->icon(QFileIconProvider::File);
	QStringList expandedNodes;
	QTreeWidgetItemIterator it1(tree, QTreeWidgetItemIterator::NotHidden | QTreeWidgetItemIterator::HasChildren);
	while (*it1) {
		QTreeWidgetItem *current = (*it1);
		if (current->isExpanded() && !(current->text(0)=="data")) {
			expandedNodes.append(current->text(0)+"/"+current->parent()->text(0));
		}
		++it1;
	}

	tree->clear();
	QStringList columns("data");
	columns.append("");
	QTreeWidgetItem *subTree = new QTreeWidgetItem(columns);
  subTree->setData(0, Qt::DecorationRole, (new QFileIconProvider())->icon(QFileIconProvider::Folder));
	updateTo(subTree, L"", *m_OrganizerCore.directoryStructure(), conflictsBox->isChecked(), &fileIcon, &folderIcon);
	tree->insertTopLevelItem(0, subTree);
	subTree->setExpanded(true);
	QTreeWidgetItemIterator it2(tree, QTreeWidgetItemIterator::HasChildren);
	while (*it2) {
		QTreeWidgetItem *current = (*it2);
		if (!(current->text(0)=="data") && expandedNodes.contains(current->text(0)+"/"+current->parent()->text(0))) {
			current->setExpanded(true);
		}
		++it2;
	}
}


void MainWindow::refreshSavesIfOpen()
{
  if (ui->tabWidget->currentIndex() == 3) {
    refreshSaveList();
  }
}

QDir MainWindow::currentSavesDir() const
{
  QDir savesDir;
  if (m_OrganizerCore.currentProfile()->localSavesEnabled()) {
    savesDir.setPath(m_OrganizerCore.currentProfile()->savePath());
  } else {
    QString iniPath = m_OrganizerCore.currentProfile()->localSettingsEnabled()
                    ? m_OrganizerCore.currentProfile()->absolutePath()
                    : m_OrganizerCore.managedGame()->documentsDirectory().absolutePath();
    iniPath += "/" + m_OrganizerCore.managedGame()->iniFiles()[0];

    wchar_t path[MAX_PATH];
    ::GetPrivateProfileStringW(
          L"General", L"SLocalSavePath", L"Saves",
          path, MAX_PATH,
          iniPath.toStdWString().c_str()
          );
    savesDir.setPath(m_OrganizerCore.managedGame()->documentsDirectory().absoluteFilePath(QString::fromWCharArray(path)));
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
  qDebug("reading save games from %s", qUtf8Printable(savesDir.absolutePath()));

  QFileInfoList files = savesDir.entryInfoList(QDir::Files, QDir::Time);
  for (const QFileInfo &file : files) {
    QListWidgetItem *item = new QListWidgetItem(file.fileName());
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
  std::vector<FileEntry::Ptr> files = m_OrganizerCore.directoryStructure()->getFiles();

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

  for (FileEntry::Ptr current : files) {
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
      if (m_OrganizerCore.settings().forceEnableCoreFiles()
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

    FilesOrigin origin = m_OrganizerCore.directoryStructure()->getOriginByID(originID);
    QString modName("data");
    unsigned int modIndex = ModInfo::getIndex(ToQString(origin.getName()));
    if (modIndex != UINT_MAX) {
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
/*  QNetworkProxyQuery query(QUrl("http://www.google.com"), QNetworkProxyQuery::UrlRequest);
  query.setProtocolTag("http");
  QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(query);
  if ((proxies.size() > 0) && (proxies.at(0).type() != QNetworkProxy::NoProxy)) {
    qDebug("Using proxy: %s", qUtf8Printable(proxies.at(0).hostName()));
    QNetworkProxy::setApplicationProxy(proxies[0]);
  } else {
    qDebug("Not using proxy");
  }*/
}


void MainWindow::activateProxy(bool activate)
{
  QProgressDialog busyDialog(tr("Activating Network Proxy"), QString(), 0, 0, parentWidget());
  busyDialog.setWindowFlags(busyDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
  busyDialog.setWindowModality(Qt::WindowModal);
  busyDialog.show();
  QFuture<void> future = QtConcurrent::run(MainWindow::setupNetworkProxy, activate);
  while (!future.isFinished()) {
    QCoreApplication::processEvents();
    ::Sleep(100);
  }
  busyDialog.hide();
}

void MainWindow::readSettings()
{
  QSettings settings(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::iniFileName()), QSettings::IniFormat);

  if (settings.contains("window_geometry")) {
    restoreGeometry(settings.value("window_geometry").toByteArray());
  }

  if (settings.contains("window_state")) {
    restoreState(settings.value("window_state").toByteArray());
  }

  if (settings.contains("toolbar_size")) {
    setToolbarSize(settings.value("toolbar_size").toSize());
  }

  if (settings.contains("toolbar_button_style")) {
    setToolbarButtonStyle(static_cast<Qt::ToolButtonStyle>(
      settings.value("toolbar_button_style").toInt()));
  }

  if (settings.contains("window_split")) {
    ui->splitter->restoreState(settings.value("window_split").toByteArray());
  }

  if (settings.contains("log_split")) {
    ui->topLevelSplitter->restoreState(settings.value("log_split").toByteArray());
  }

  bool filtersVisible = settings.value("filters_visible", false).toBool();
  setCategoryListVisible(filtersVisible);
  ui->displayCategoriesBtn->setChecked(filtersVisible);

  int selectedExecutable = settings.value("selected_executable").toInt();
  setExecutableIndex(selectedExecutable);

  if (settings.value("Settings/use_proxy", false).toBool()) {
    activateProxy(true);
  }
}

void MainWindow::processUpdates() {
  QSettings settings(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::iniFileName()), QSettings::IniFormat);
  QVersionNumber lastVersion = QVersionNumber::fromString(settings.value("version", "2.1.2").toString()).normalized();
  QVersionNumber currentVersion = QVersionNumber::fromString(m_OrganizerCore.getVersion().displayString()).normalized();
  if (!m_OrganizerCore.settings().directInterface().value("first_start", true).toBool()) {
    if (lastVersion < QVersionNumber(2, 1, 3)) {
      bool lastHidden = true;
      for (int i = ModList::COL_GAME; i < ui->modList->model()->columnCount(); ++i) {
        bool hidden = ui->modList->header()->isSectionHidden(i);
        ui->modList->header()->setSectionHidden(i, lastHidden);
        lastHidden = hidden;
      }
    }
    if (lastVersion < QVersionNumber(2, 1, 6)) {
      ui->modList->header()->setSectionHidden(ModList::COL_NOTES, true);
    }
    if (lastVersion < QVersionNumber(2, 2, 0)) {
      QSettings &instance = Settings::instance().directInterface();
      instance.beginGroup("Settings");
      instance.remove("steam_password");
      instance.remove("nexus_username");
      instance.remove("nexus_password");
      instance.remove("nexus_login");
      instance.remove("nexus_api_key");
      instance.remove("ask_for_nexuspw");
      instance.remove("nmm_version");
      instance.endGroup();
      instance.beginGroup("Servers");
      instance.remove("");
      instance.endGroup();
    }
    if (lastVersion < QVersionNumber(2, 2, 1)) {
      // hide new columns by default
      for (int i=DownloadList::COL_MODNAME; i<DownloadList::COL_COUNT; ++i) {
        ui->downloadView->header()->hideSection(i);
      }
    }
  }

  if (currentVersion > lastVersion) {
    //NOP
  } else if (currentVersion < lastVersion)
    qWarning() << tr("Notice: Your current MO version (%1) is lower than the previously used one (%2). "
                     "The GUI may not downgrade gracefully, so you may experience oddities. "
                     "However, there should be no serious issues.").arg(currentVersion.toString()).arg(lastVersion.toString()).toStdWString();
  //save version in all case
  settings.setValue("version", currentVersion.toString());
}

void MainWindow::storeSettings(QSettings &settings) {
  settings.setValue("group_state", ui->groupCombo->currentIndex());
  settings.setValue("selected_executable",
                    ui->executablesListBox->currentIndex());

  if (settings.value("reset_geometry", false).toBool()) {
    settings.remove("window_geometry");
    settings.remove("window_state");
    settings.remove("toolbar_size");
    settings.remove("toolbar_button_style");
    settings.remove("window_split");
    settings.remove("log_split");
    settings.remove("filters_visible");
    settings.remove("browser_geometry");
    settings.remove("geometry");
    settings.remove("reset_geometry");
  } else {
    settings.setValue("window_geometry", saveGeometry());
    settings.setValue("window_state", saveState());
    settings.setValue("toolbar_size", ui->toolBar->iconSize());
    settings.setValue("toolbar_button_style", static_cast<int>(ui->toolBar->toolButtonStyle()));
    settings.setValue("window_split", ui->splitter->saveState());
    settings.setValue("log_split", ui->topLevelSplitter->saveState());
    settings.setValue("browser_geometry", m_IntegratedBrowser.saveGeometry());
    settings.setValue("filters_visible", ui->displayCategoriesBtn->isChecked());
    for (const std::pair<QString, QHeaderView*> kv : m_PersistedGeometry) {
      QString key = QString("geometry/") + kv.first;
      settings.setValue(key, kv.second->saveState());
    }
  }
}

ILockedWaitingForProcess* MainWindow::lock()
{
  if (m_LockDialog != nullptr) {
    ++m_LockCount;
    return m_LockDialog;
  }
  if (m_closing)
    m_LockDialog = new WaitingOnCloseDialog(this);
  else
    m_LockDialog = new LockedDialog(this, true);
  m_LockDialog->setModal(true);
  m_LockDialog->show();
  setEnabled(false);
  m_LockDialog->setEnabled(true); //What's the point otherwise?
  ++m_LockCount;
  return m_LockDialog;
}

void MainWindow::unlock()
{
  //If you come through here with a null lock pointer, it's a bug!
  if (m_LockDialog == nullptr) {
    qDebug("Unlocking main window when already unlocked");
    return;
  }
  --m_LockCount;
  if (m_LockCount == 0) {
    if (m_closing && m_LockDialog->canceled())
      m_closing = false;
    m_LockDialog->hide();
    m_LockDialog->deleteLater();
    m_LockDialog = nullptr;
    setEnabled(true);
  }
}

void MainWindow::on_btnRefreshData_clicked()
{
  m_OrganizerCore.refreshDirectoryStructure();
}

void MainWindow::on_btnRefreshDownloads_clicked()
{
  m_OrganizerCore.downloadManager()->refreshList();
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
  if (index == 0) {
    m_OrganizerCore.refreshESPList();
  } else if (index == 1) {
    m_OrganizerCore.refreshBSAList();
  } else if (index == 2) {
    refreshDataTreeKeepExpandedNodes();
  } else if (index == 3) {
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
      m_OrganizerCore.installMod(fileName, QString());
    }
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}

void MainWindow::on_startButton_clicked() {
  ui->startButton->setEnabled(false);
  try {
    const Executable &selectedExecutable(getSelectedExecutable());
    QString customOverwrite = m_OrganizerCore.currentProfile()->setting("custom_overwrites", selectedExecutable.m_Title).toString();
    auto forcedLibraries = m_OrganizerCore.currentProfile()->determineForcedLibraries(selectedExecutable.m_Title);
    if (!m_OrganizerCore.currentProfile()->forcedLibrariesEnabled(selectedExecutable.m_Title)) {
      forcedLibraries.clear();
    }
    m_OrganizerCore.spawnBinary(
        selectedExecutable.m_BinaryInfo, selectedExecutable.m_Arguments,
        selectedExecutable.m_WorkingDirectory.length() != 0
            ? selectedExecutable.m_WorkingDirectory
            : selectedExecutable.m_BinaryInfo.absolutePath(),
        selectedExecutable.m_SteamAppID,
        customOverwrite,
        forcedLibraries);
  } catch (...) {
    ui->startButton->setEnabled(true);
    throw;
  }
  ui->startButton->setEnabled(true);
}

static HRESULT CreateShortcut(LPCWSTR targetFileName, LPCWSTR arguments,
                              LPCSTR linkFileName, LPCWSTR description,
                              LPCTSTR iconFileName, int iconNumber,
                              LPCWSTR currentDirectory)
{
  HRESULT result = E_INVALIDARG;
  if ((targetFileName != nullptr) && (wcslen(targetFileName) > 0) &&
       (arguments != nullptr) &&
       (linkFileName != nullptr) && (strlen(linkFileName) > 0) &&
       (description != nullptr) &&
       (currentDirectory != nullptr)) {

    IShellLink* shellLink;
    result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IShellLink, (LPVOID*)&shellLink);

    if (!SUCCEEDED(result)) {
      qCritical("failed to create IShellLink instance");
      return result;
    }

    result = shellLink->SetPath(targetFileName);
    if (!SUCCEEDED(result)) {
      qCritical("failed to set target path %ls", targetFileName);
      shellLink->Release();
      return result;
    }

    result = shellLink->SetArguments(arguments);
    if (!SUCCEEDED(result)) {
      qCritical("failed to set arguments: %ls", arguments);
      shellLink->Release();
      return result;
    }

    if (wcslen(description) > 0) {
      result = shellLink->SetDescription(description);
      if (!SUCCEEDED(result)) {
        qCritical("failed to set description: %ls", description);
        shellLink->Release();
        return result;
      }
    }

    if (wcslen(currentDirectory) > 0) {
      result = shellLink->SetWorkingDirectory(currentDirectory);
      if (!SUCCEEDED(result)) {
        qCritical("failed to set working directory: %ls", currentDirectory);
        shellLink->Release();
        return result;
      }
    }

    if (iconFileName != nullptr) {
      result = shellLink->SetIconLocation(iconFileName, iconNumber);
      if (!SUCCEEDED(result)) {
        qCritical("failed to load program icon: %ls %d", iconFileName, iconNumber);
        shellLink->Release();
        return result;
      }
    }

    IPersistFile *persistFile;
    result = shellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&persistFile);
    if (SUCCEEDED(result)) {
      wchar_t linkFileNameW[MAX_PATH];
      if (MultiByteToWideChar(CP_ACP, 0, linkFileName, -1, linkFileNameW, MAX_PATH) > 0) {
        result = persistFile->Save(linkFileNameW, TRUE);
      } else {
        qCritical("failed to create link: %s", linkFileName);
      }
      persistFile->Release();
    } else {
      qCritical("failed to create IPersistFile instance");
    }

    shellLink->Release();
  }
  return result;
}


bool MainWindow::modifyExecutablesDialog()
{
  bool result = false;
  try {
    EditExecutablesDialog dialog(*m_OrganizerCore.executablesList(),
                                 *m_OrganizerCore.modList(),
                                 m_OrganizerCore.currentProfile(),
                                 m_OrganizerCore.managedGame());
    QSettings &settings = m_OrganizerCore.settings().directInterface();
    QString key = QString("geometry/%1").arg(dialog.objectName());
    if (settings.contains(key)) {
      dialog.restoreGeometry(settings.value(key).toByteArray());
    }
    if (dialog.exec() == QDialog::Accepted) {
      m_OrganizerCore.setExecutablesList(dialog.getExecutablesList());
      result = true;
    }
    settings.setValue(key, dialog.saveGeometry());
    refreshExecutablesList();
  } catch (const std::exception &e) {
    reportError(e.what());
  }
  return result;
}

void MainWindow::on_executablesListBox_currentIndexChanged(int index)
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");

  int previousIndex = m_OldExecutableIndex;
  m_OldExecutableIndex = index;

  if (executablesList->isEnabled()) {
    //I think the 2nd test is impossible
    if ((index == 0) || (index > static_cast<int>(m_OrganizerCore.executablesList()->size()))) {
      if (modifyExecutablesDialog()) {
        setExecutableIndex(previousIndex);
      }
    } else {
      setExecutableIndex(index);
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
                                  m_OrganizerCore.managedGame(),
                                  this);
    QSettings &settings = m_OrganizerCore.settings().directInterface();
    QString key = QString("geometry/%1").arg(profilesDialog.objectName());
    if (settings.contains(key)) {
      profilesDialog.restoreGeometry(settings.value(key).toByteArray());
    }
    // workaround: need to disable monitoring of the saves directory, otherwise the active
    // profile directory is locked
    stopMonitorSaves();
    profilesDialog.exec();
    settings.setValue(key, profilesDialog.saveGeometry());
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
  if (modifyExecutablesDialog()) {
    setExecutableIndex(m_OldExecutableIndex);
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

void MainWindow::refresher_progress(int percent)
{
  if (percent == 100) {
    m_RefreshProgress->setVisible(false);
    this->setEnabled(true);
  } else if (!m_RefreshProgress->isVisible()) {
    this->setEnabled(false);
    m_RefreshProgress->setVisible(true);
    m_RefreshProgress->setRange(0, 100);
    m_RefreshProgress->setValue(percent);
  }
}

void MainWindow::directory_refreshed()
{
  // some problem-reports may rely on the virtual directory tree so they need to be updated
  // now
  refreshDataTreeKeepExpandedNodes();
  updateProblemsButton();
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
  IDs.insert(std::make_pair<QString, int>(info->getGameName(), info->getNexusID()));
  modUpdateCheck(IDs);
}

void MainWindow::procError(QProcess::ProcessError error)
{
  reportError(tr("failed to spawn notepad.exe: %1").arg(error));
  this->sender()->deleteLater();
}

void MainWindow::procFinished(int, QProcess::ExitStatus)
{
  this->sender()->deleteLater();
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
  const FileEntry::Ptr filePtr = m_OrganizerCore.directoryStructure()->findFile(ToWString(filePath));
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

QTreeWidgetItem *MainWindow::addFilterItem(QTreeWidgetItem *root, const QString &name, int categoryID, ModListSortProxy::FilterType type)
{
  QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(name));
  item->setData(0, Qt::ToolTipRole, name);
  item->setData(0, Qt::UserRole, categoryID);
  item->setData(0, Qt::UserRole + 1, type);
  if (root != nullptr) {
    root->addChild(item);
  } else {
    ui->categoriesList->addTopLevelItem(item);
  }
  return item;
}

void MainWindow::addContentFilters()
{
  for (unsigned i = 0; i < ModInfo::NUM_CONTENT_TYPES; ++i) {
    addFilterItem(nullptr, tr("<Contains %1>").arg(ModInfo::getContentTypeName(i)), i, ModListSortProxy::TYPE_CONTENT);
  }
}

void MainWindow::addCategoryFilters(QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID)
{
  for (unsigned int i = 1;
       i < static_cast<unsigned int>(m_CategoryFactory.numCategories()); ++i) {
    if ((m_CategoryFactory.getParentID(i) == targetID)) {
      int categoryID = m_CategoryFactory.getCategoryID(i);
      if (categoriesUsed.find(categoryID) != categoriesUsed.end()) {
        QTreeWidgetItem *item =
            addFilterItem(root, m_CategoryFactory.getCategoryName(i),
                          categoryID, ModListSortProxy::TYPE_CATEGORY);
        if (m_CategoryFactory.hasChildren(i)) {
          addCategoryFilters(item, categoriesUsed, categoryID);
        }
      }
    }
  }
}

void MainWindow::refreshFilters()
{
  QItemSelection currentSelection = ui->modList->selectionModel()->selection();

  QVariant currentIndexName = ui->modList->currentIndex().data();
  ui->modList->setCurrentIndex(QModelIndex());

  QStringList selectedItems;
  for (QTreeWidgetItem *item : ui->categoriesList->selectedItems()) {
    selectedItems.append(item->text(0));
  }

  ui->categoriesList->clear();
  addFilterItem(nullptr, tr("<Checked>"), CategoryFactory::CATEGORY_SPECIAL_CHECKED, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Unchecked>"), CategoryFactory::CATEGORY_SPECIAL_UNCHECKED, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Update>"), CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Mod Backup>"), CategoryFactory::CATEGORY_SPECIAL_BACKUP, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Managed by MO>"), CategoryFactory::CATEGORY_SPECIAL_MANAGED, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Managed outside MO>"), CategoryFactory::CATEGORY_SPECIAL_UNMANAGED, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<No category>"), CategoryFactory::CATEGORY_SPECIAL_NOCATEGORY, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Conflicted>"), CategoryFactory::CATEGORY_SPECIAL_CONFLICT, ModListSortProxy::TYPE_SPECIAL);
  addFilterItem(nullptr, tr("<Not Endorsed>"), CategoryFactory::CATEGORY_SPECIAL_NOTENDORSED, ModListSortProxy::TYPE_SPECIAL);

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
          qWarning("cycle in categories: %s", qUtf8Printable(SetJoin(cycleTest, ", ")));
          break;
        }
        currentID = m_CategoryFactory.getParentID(m_CategoryFactory.getCategoryIndex(currentID));
      }
    }
  }

  addCategoryFilters(nullptr, categoriesUsed, 0);

  for (const QString &item : selectedItems) {
    QList<QTreeWidgetItem*> matches = ui->categoriesList->findItems(item, Qt::MatchFixedString | Qt::MatchRecursive);
    if (matches.size() > 0) {
      matches.at(0)->setSelected(true);
    }
  }
  ui->modList->selectionModel()->select(currentSelection, QItemSelectionModel::Select);
  QModelIndexList matchList;
  if (currentIndexName.isValid()) {
    matchList = ui->modList->model()->match(ui->modList->model()->index(0, 0), Qt::DisplayRole, currentIndexName);
  }

  if (matchList.size() > 0) {
    ui->modList->setCurrentIndex(matchList.at(0));
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
    QDir modDir(QDir::fromNativeSeparators(m_OrganizerCore.settings().getModDirectory()));
    if (!modDir.exists(regName) ||
        (QMessageBox::question(this, tr("Overwrite?"),
          tr("This will replace the existing mod \"%1\". Continue?").arg(regName),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
      if (modDir.exists(regName) && !shellDelete(QStringList(modDir.absoluteFilePath(regName)))) {
        reportError(tr("failed to remove mod \"%1\"").arg(regName));
      } else {
        QString destinationPath = QDir::fromNativeSeparators(m_OrganizerCore.settings().getModDirectory()) + "/" + regName;
        if (!modDir.rename(modInfo->absolutePath(), destinationPath)) {
          reportError(tr("failed to rename \"%1\" to \"%2\"").arg(modInfo->absolutePath()).arg(destinationPath));
        }
        m_OrganizerCore.refreshModList();
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
  QString installationFile = modInfo->getInstallationFile();
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
      m_OrganizerCore.installMod(fullInstallationFile, modInfo->name());
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
  m_OrganizerCore.refreshModList();
}

void MainWindow::resumeDownload(int downloadIndex)
{
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
    m_OrganizerCore.downloadManager()->resumeDownload(downloadIndex);
  } else {
    QString apiKey;
    if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
      m_OrganizerCore.doAfterLogin([this, downloadIndex] () {
        this->resumeDownload(downloadIndex);
      });
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to resume a download"), this);
    }
  }
}


void MainWindow::endorseMod(ModInfo::Ptr mod)
{
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
    mod->endorse(true);
  } else {
    QString apiKey;
    if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
      m_OrganizerCore.doAfterLogin([&]() { this->endorseMod(mod); });
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to endorse"), this);
    }
  }
}


void MainWindow::endorse_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
      MessageDialog::showMessage(tr("Endorsing multiple mods will take a while. Please wait..."), this);
      for (QModelIndex idx : selection->selectedRows()) {
        ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->endorse(true);
      }
    }
    else {
      QString apiKey;
      if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
        MessageDialog::showMessage(tr("Endorsing multiple mods will take a while. Please wait..."), this);
        for (QModelIndex idx : selection->selectedRows()) {
          ModInfo::Ptr modInfo = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
          m_OrganizerCore.doAfterLogin([&]() { this->endorseMod(modInfo); });
        }
        NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
      } else {
        MessageDialog::showMessage(tr("You need to be logged in with Nexus to endorse"), this);
        return;
      }
    }
  }
  else {
    endorseMod(ModInfo::getByIndex(m_ContextRow));
  }
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
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
    ModInfo::getByIndex(m_ContextRow)->endorse(false);
  } else {
    QString apiKey;
    if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
      m_OrganizerCore.doAfterLogin([&]() { this->unendorseMod(mod); });
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to endorse"), this);
    }
  }
}


void MainWindow::unendorse_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
      MessageDialog::showMessage(tr("Unendorsing multiple mods will take a while. Please wait..."), this);
      for (QModelIndex idx : selection->selectedRows()) {
        ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->endorse(false);
      }
    } else {
      QString apiKey;
      if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
        MessageDialog::showMessage(tr("Unendorsing multiple mods will take a while. Please wait..."), this);
        for (QModelIndex idx : selection->selectedRows()) {
          ModInfo::Ptr modInfo = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
          m_OrganizerCore.doAfterLogin([&]() { this->unendorseMod(modInfo); });
        }
        NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
      } else {
        MessageDialog::showMessage(tr("You need to be logged in with Nexus to endorse"), this);
        return;
      }
    }
  }
  else {
    unendorseMod(ModInfo::getByIndex(m_ContextRow));
  }
}


void MainWindow::trackMod(ModInfo::Ptr mod, bool doTrack)
{
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
    ModInfo::getByIndex(m_ContextRow)->track(doTrack);
  } else {
    QString apiKey;
    if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
      m_OrganizerCore.doAfterLogin([&]() { this->trackMod(mod, doTrack); });
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to track"), this);
    }
  }
}


void MainWindow::track_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
      for (auto idx : selection->selectedRows()) {
        ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->track(true);
      }
    } else {
      QString apiKey;
      if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
        for (auto idx : selection->selectedRows()) {
          auto modInfo = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
          m_OrganizerCore.doAfterLogin([&]() { this->trackMod(modInfo, true); });
        }
        NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
      } else {
        MessageDialog::showMessage(tr("You need to be logged in with Nexus to track"), this);
      }
    }
  } else {
    trackMod(ModInfo::getByIndex(m_ContextRow), true);
  }
}

void MainWindow::untrack_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
      for (auto idx : selection->selectedRows()) {
        ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->track(false);
      }
    } else {
      QString apiKey;
      if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
        for (auto idx : selection->selectedRows()) {
          auto modInfo = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
          m_OrganizerCore.doAfterLogin([&]() { this->trackMod(modInfo, false); });
        }
        NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
      } else {
        MessageDialog::showMessage(tr("You need to be logged in with Nexus to track"), this);
      }
    }
  } else {
    trackMod(ModInfo::getByIndex(m_ContextRow), false);
  }
}

void MainWindow::validationFailed(const QString &error)
{
  qDebug("Nexus API validation failed: %s", qUtf8Printable(error));
}

void MainWindow::windowTutorialFinished(const QString &windowName)
{
  m_OrganizerCore.settings().directInterface().setValue(QString("CompletedWindowTutorials/") + windowName, true);
}

void MainWindow::overwriteClosed(int)
{
  OverwriteInfoDialog *dialog = this->findChild<OverwriteInfoDialog*>("__overwriteDialog");
  if (dialog != nullptr) {
    m_OrganizerCore.modList()->modInfoChanged(dialog->modInfo());
    QSettings &settings = m_OrganizerCore.settings().directInterface();
    QString key = QString("geometry/%1").arg(dialog->objectName());
    settings.setValue(key, dialog->saveGeometry());
    dialog->deleteLater();
  }
  m_OrganizerCore.refreshDirectoryStructure();
}


void MainWindow::displayModInformation(ModInfo::Ptr modInfo, unsigned int index, int tab)
{
  if (!m_OrganizerCore.modList()->modInfoAboutToChange(modInfo)) {
    qDebug("A different mod information dialog is open. If this is incorrect, please restart MO");
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
      QSettings &settings = m_OrganizerCore.settings().directInterface();
      QString key = QString("geometry/%1").arg(dialog->objectName());
      if (settings.contains(key)) {
        dialog->restoreGeometry(settings.value(key).toByteArray());
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
    ModInfoDialog dialog(modInfo, m_OrganizerCore.directoryStructure(), modInfo->hasFlag(ModInfo::FLAG_FOREIGN), &m_OrganizerCore, &m_PluginContainer, this);
    connect(&dialog, SIGNAL(linkActivated(QString)), this, SLOT(linkClicked(QString)));
    connect(&dialog, SIGNAL(downloadRequest(QString)), &m_OrganizerCore, SLOT(downloadRequestedNXM(QString)));
    connect(&dialog, SIGNAL(modOpen(QString, int)), this, SLOT(displayModInformation(QString, int)), Qt::QueuedConnection);
    connect(&dialog, SIGNAL(modOpenNext(int)), this, SLOT(modOpenNext(int)), Qt::QueuedConnection);
    connect(&dialog, SIGNAL(modOpenPrev(int)), this, SLOT(modOpenPrev(int)), Qt::QueuedConnection);
    connect(&dialog, SIGNAL(originModified(int)), this, SLOT(originModified(int)));
    connect(&dialog, SIGNAL(endorseMod(ModInfo::Ptr)), this, SLOT(endorseMod(ModInfo::Ptr)));

	//Open the tab first if we want to use the standard indexes of the tabs.
	if (tab != -1) {
		dialog.openTab(tab);
	}

  dialog.restoreState(m_OrganizerCore.settings());
  QSettings &settings = m_OrganizerCore.settings().directInterface();
  QString key = QString("geometry/%1").arg(dialog.objectName());
  if (settings.contains(key)) {
    dialog.restoreGeometry(settings.value(key).toByteArray());
  }

	//If no tab was specified use the first tab from the left based on the user order.
	if (tab == -1) {
		for (int i = 0; i < dialog.findChild<QTabWidget*>("tabWidget")->count(); ++i) {
			if (dialog.findChild<QTabWidget*>("tabWidget")->isTabEnabled(i)) {
				dialog.findChild<QTabWidget*>("tabWidget")->setCurrentIndex(i);
				break;
			}
		}
	}

    dialog.exec();
    dialog.saveState(m_OrganizerCore.settings());
    settings.setValue(key, dialog.saveGeometry());

    modInfo->saveMeta();
    emit modInfoDisplayed();
    m_OrganizerCore.modList()->modInfoChanged(modInfo);
  }

  if (m_OrganizerCore.currentProfile()->modEnabled(index)
      && !modInfo->hasFlag(ModInfo::FLAG_FOREIGN)) {
    FilesOrigin& origin = m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(modInfo->name()));
    origin.enable(false);

    if (m_OrganizerCore.directoryStructure()->originExists(ToWString(modInfo->name()))) {
      FilesOrigin& origin = m_OrganizerCore.directoryStructure()->getOriginByName(ToWString(modInfo->name()));
      origin.enable(false);

      m_OrganizerCore.directoryRefresher()->addModToStructure(m_OrganizerCore.directoryStructure()
                                             , modInfo->name()
                                             , m_OrganizerCore.currentProfile()->getModPriority(index)
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


void MainWindow::modOpenNext(int tab)
{
  QModelIndex index = m_ModListSortProxy->mapFromSource(m_OrganizerCore.modList()->index(m_ContextRow, 0));
  index = m_ModListSortProxy->index((index.row() + 1) % m_ModListSortProxy->rowCount(), 0);

  m_ContextRow = m_ModListSortProxy->mapToSource(index).row();
  ModInfo::Ptr mod = ModInfo::getByIndex(m_ContextRow);
  std::vector<ModInfo::EFlag> flags = mod->getFlags();
  if ((std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) ||
      (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end()) ||
      (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end())) {
    // skip overwrite and backups and separators
    modOpenNext(tab);
  } else {
    displayModInformation(m_ContextRow,tab);
  }
}

void MainWindow::modOpenPrev(int tab)
{
  QModelIndex index = m_ModListSortProxy->mapFromSource(m_OrganizerCore.modList()->index(m_ContextRow, 0));
  int row = index.row() - 1;
  if (row == -1) {
    row = m_ModListSortProxy->rowCount() - 1;
  }

  m_ContextRow = m_ModListSortProxy->mapToSource(m_ModListSortProxy->index(row, 0)).row();
  ModInfo::Ptr mod = ModInfo::getByIndex(m_ContextRow);
  std::vector<ModInfo::EFlag> flags = mod->getFlags();
  if ((std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) ||
      (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end()) ||
      (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end())) {
    // skip overwrite and backups and separators
    modOpenPrev(tab);
  } else {
    displayModInformation(m_ContextRow,tab);
  }
}

void MainWindow::displayModInformation(const QString &modName, int tab)
{
  unsigned int index = ModInfo::getIndex(modName);
  if (index == UINT_MAX) {
    qCritical("failed to resolve mod name %s", qUtf8Printable(modName));
    return;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  displayModInformation(modInfo, index, tab);
}


void MainWindow::displayModInformation(int row, int tab)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);
  displayModInformation(modInfo, row, tab);
}


void MainWindow::ignoreMissingData_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      int row_idx = idx.data(Qt::UserRole + 1).toInt();
      ModInfo::Ptr info = ModInfo::getByIndex(row_idx);
      //QDir(info->absolutePath()).mkdir("textures");
      info->testValid();
      info->markValidated(true);
      connect(this, SIGNAL(modListDataChanged(QModelIndex, QModelIndex)), m_OrganizerCore.modList(), SIGNAL(dataChanged(QModelIndex, QModelIndex)));

      emit modListDataChanged(m_OrganizerCore.modList()->index(row_idx, 0), m_OrganizerCore.modList()->index(row_idx, m_OrganizerCore.modList()->columnCount() - 1));
    }
  } else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
    //QDir(info->absolutePath()).mkdir("textures");
    info->testValid();
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
    QString webUrl;
    for (QModelIndex idx : selection->selectedRows()) {
      row_idx = idx.data(Qt::UserRole + 1).toInt();
      info = ModInfo::getByIndex(row_idx);
      int modID = info->getNexusID();
      webUrl = info->getURL();
      gameName = info->getGameName();
      if (modID > 0)  {
        linkClicked(NexusInterface::instance(&m_PluginContainer)->getModURL(modID, gameName));
      }
      else if (webUrl != "") {
        linkClicked(webUrl);
      }
    }
  }
  else {
    int modID = m_OrganizerCore.modList()->data(m_OrganizerCore.modList()->index(m_ContextRow, 0), Qt::UserRole).toInt();
    QString gameName = m_OrganizerCore.modList()->data(m_OrganizerCore.modList()->index(m_ContextRow, 0), Qt::UserRole + 4).toString();
    if (modID > 0)  {
      linkClicked(NexusInterface::instance(&m_PluginContainer)->getModURL(modID, gameName));
    } else {
      MessageDialog::showMessage(tr("Nexus ID for this Mod is unknown"), this);
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
    QString webUrl;
    for (QModelIndex idx : selection->selectedRows()) {
      row_idx = idx.data(Qt::UserRole + 1).toInt();
      info = ModInfo::getByIndex(row_idx);
      int modID = info->getNexusID();
      webUrl = info->getURL();
      gameName = info->getGameName();
      if (modID > 0) {
        linkClicked(NexusInterface::instance(&m_PluginContainer)->getModURL(modID, gameName));
      }
      else if (webUrl != "") {
        linkClicked(webUrl);
      }
    }
  }
  else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
    if (info->getURL() != "") {
      linkClicked(info->getURL());
    }
    else {
      MessageDialog::showMessage(tr("Web page for this mod is unknown"), this);
    }
  }
}

void MainWindow::openExplorer_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::Ptr info = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
      shell::ExploreFile(info->absolutePath());
    }
  }
  else {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
    shell::ExploreFile(modInfo->absolutePath());
  }
}

void MainWindow::openOriginExplorer_clicked()
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
      shell::ExploreFile(modInfo->absolutePath());
    }
  }
  else {
    QModelIndex idx = selection->currentIndex();
    QString fileName = idx.data().toString();
    ModInfo::Ptr modInfo = ModInfo::getByIndex(ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)));
    shell::ExploreFile(modInfo->absolutePath());
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
        shell::ExploreFile(modInfo->absolutePath());
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
          shell::ExploreFile(modInfo->absolutePath());
        }
      }
		}
	}
}

void MainWindow::refreshProfile_activated()
{
	m_OrganizerCore.profileRefresh();
}

void MainWindow::search_activated()
{
  if (ui->modList->hasFocus() || ui->modFilterEdit->hasFocus()) {
    ui->modFilterEdit->setFocus();
    ui->modFilterEdit->setSelection(0, INT_MAX);
  }

  else if (ui->espList->hasFocus() || ui->espFilterEdit->hasFocus()) {
    ui->espFilterEdit->setFocus();
    ui->espFilterEdit->setSelection(0, INT_MAX);
  }

  else if (ui->downloadView->hasFocus() || ui->downloadFilterEdit->hasFocus()) {
    ui->downloadFilterEdit->setFocus();
    ui->downloadFilterEdit->setSelection(0, INT_MAX);
  }
}

void MainWindow::searchClear_activated()
{
  if (ui->modList->hasFocus() || ui->modFilterEdit->hasFocus()) {
    ui->modFilterEdit->clear();
    ui->modList->setFocus();
  }

  else if (ui->espList->hasFocus() || ui->espFilterEdit->hasFocus()) {
    ui->espFilterEdit->clear();
    ui->espList->setFocus();
  }

  else if (ui->downloadView->hasFocus() || ui->downloadFilterEdit->hasFocus()) {
    ui->downloadFilterEdit->clear();
    ui->downloadView->setFocus();
  }
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

  if (m_OrganizerCore.getMod(name) != nullptr) {
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

  m_OrganizerCore.refreshModList();

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
  if (m_OrganizerCore.getMod(name) != nullptr)
  {
    reportError(tr("A separator with this name already exists"));
    return;
  }
  name->append("_separator");
  if (m_OrganizerCore.getMod(name) != nullptr)
  {
    return;
  }

  int newPriority = -1;
  if (m_ContextRow >= 0 && m_ModListSortProxy->sortColumn() == ModList::COL_PRIORITY)
  {
    newPriority = m_OrganizerCore.currentProfile()->getModPriority(m_ContextRow);
  }

  if (m_OrganizerCore.createMod(name) == nullptr) { return; }
  m_OrganizerCore.refreshModList();

  if (newPriority >= 0)
  {
    m_OrganizerCore.modList()->changeModPriority(ModInfo::getIndex(name), newPriority);
  }
  QSettings &settings = m_OrganizerCore.settings().directInterface();
  QColor previousColor = settings.value("previousSeparatorColor", QColor()).value<QColor>();
  if (previousColor.isValid()) {
    ModInfo::getByIndex(ModInfo::getIndex(name))->setColor(previousColor);
  }

}

void MainWindow::setColor_clicked()
{
  QSettings &settings = m_OrganizerCore.settings().directInterface();
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  QColorDialog dialog(this);
  dialog.setOption(QColorDialog::ShowAlphaChannel);
  QColor currentColor = modInfo->getColor();
  QColor previousColor = settings.value("previousSeparatorColor", QColor()).value<QColor>();
  if (currentColor.isValid())
    dialog.setCurrentColor(currentColor);
  else
    dialog.setCurrentColor(previousColor);
  if (!dialog.exec())
    return;
  currentColor = dialog.currentColor();
  if (!currentColor.isValid())
    return;
  settings.setValue("previousSeparatorColor", currentColor);
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::Ptr info = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
      auto flags = info->getFlags();
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end())
      {
        info->setColor(currentColor);
      }
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
      auto flags = info->getFlags();
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end())
      {
        info->setColor(color);
      }
    }
  }
  else {
    modInfo->setColor(color);
  }
  Settings::instance().directInterface().remove("previousSeparatorColor");
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

  if (m_OrganizerCore.getMod(name) != nullptr) {
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
  QSettings &settings = m_OrganizerCore.settings().directInterface();
  QString key = QString("geometry/%1").arg(dialog.objectName());

  dialog.setWindowTitle("Select a mod...");
  dialog.setChoices(mods);

  if (settings.contains(key)) {
    dialog.restoreGeometry(settings.value(key).toByteArray());
  }
  if (dialog.exec() == QDialog::Accepted) {

    QString result = dialog.getChoice();
    if (!result.isEmpty()) {

      QString modAbsolutePath;

      for (const auto& mod : m_OrganizerCore.modsSortedByProfilePriority()) {
        if (result.compare(mod) == 0) {
          ModInfo::Ptr modInfo = ModInfo::getByIndex(ModInfo::getIndex(mod));
          modAbsolutePath = modInfo->absolutePath();
          break;
        }
      }

      if (modAbsolutePath.isNull()) {
        qWarning("Mod %s has not been found, for some reason", qUtf8Printable(result));
        return;
      }

      doMoveOverwriteContentToMod(modAbsolutePath);
    }
  }
  settings.setValue(key, dialog.saveGeometry());
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
    qCritical("Move operation failed: %s", qUtf8Printable(windowsErrorString(::GetLastError())));
  }

  m_OrganizerCore.refreshModList();
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
        updateProblemsButton();
        m_OrganizerCore.refreshModList();
      } else {
        qCritical("Delete operation failed: %s", qUtf8Printable(windowsErrorString(::GetLastError())));
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
      openExplorer_clicked();
      // workaround to cancel the editor that might have opened because of
      // selection-click
      ui->modList->closePersistentEditor(index);
    }
    catch (const std::exception &e) {
      reportError(e.what());
    }
  }
  else {
    try {
      m_ContextRow = m_ModListSortProxy->mapToSource(index).row();
      sourceIdx.column();
      int tab = -1;
      switch (sourceIdx.column()) {
        case ModList::COL_NOTES: tab = ModInfoDialog::TAB_NOTES; break;
        case ModList::COL_VERSION: tab = ModInfoDialog::TAB_NEXUS; break;
        case ModList::COL_MODID: tab = ModInfoDialog::TAB_NEXUS; break;
        case ModList::COL_GAME: tab = ModInfoDialog::TAB_NEXUS; break;
        case ModList::COL_CATEGORY: tab = ModInfoDialog::TAB_CATEGORIES; break;
        case ModList::COL_FLAGS: tab = ModInfoDialog::TAB_CONFLICTS; break;
        default: tab = -1;
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
    qCritical("not a menu?");
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
      qDebug("change categories on: %s", qUtf8Printable(idx.data().toString()));
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
    qCritical("not a menu?");
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
    if (archiveFile.commitIfDifferent(m_ArchiveListHash)) {
      qDebug("%s saved", qUtf8Printable(QDir::toNativeSeparators(m_OrganizerCore.currentProfile()->getArchivesFileName())));
    }
  } else {
    qWarning("archive list not initialised");
  }
}

void MainWindow::checkModsForUpdates()
{
  bool checkingModsForUpdate = false;
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
    checkingModsForUpdate = ModInfo::checkAllForUpdate(&m_PluginContainer, this);
    NexusInterface::instance(&m_PluginContainer)->requestEndorsementInfo(this, QVariant(), QString());
    NexusInterface::instance(&m_PluginContainer)->requestTrackingInfo(this, QVariant(), QString());
  } else {
    QString apiKey;
    if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
      m_OrganizerCore.doAfterLogin([this] () { this->checkModsForUpdates(); });
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
    } else {
      qWarning("You are not currently authenticated with Nexus. Please do so under Settings -> Nexus.");
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
    m_ModListSortProxy->setCategoryFilter(boost::assign::list_of(CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE));
    for (int i = 0; i < ui->categoriesList->topLevelItemCount(); ++i) {
      if (ui->categoriesList->topLevelItem(i)->data(0, Qt::UserRole) == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
        ui->categoriesList->setCurrentItem(ui->categoriesList->topLevelItem(i));
        break;
      }
    }
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
      VersionInfo verOld(info->getVersion().canonicalString(), schemes[i]);
      VersionInfo verNew(info->getNewestVersion().canonicalString(), schemes[i]);
      if (verOld < verNew) {
        info->setVersion(verOld);
        info->setNewestVersion(verNew);
        success = true;
      }
    }
    if (!success) {
      QMessageBox::information(this, tr("Sorry"),
          tr("I don't know a versioning scheme where %1 is newer than %2.").arg(info->getNewestVersion().canonicalString()).arg(info->getVersion().canonicalString()),
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
      IDs.insert(std::make_pair<QString, int>(info->getGameName(), info->getNexusID()));
    }
  } else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
    IDs.insert(std::make_pair<QString, int>(info->getGameName(), info->getNexusID()));
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
      categoryBox->setChecked(categoryID == info->getPrimaryCategory());
      action->setDefaultWidget(categoryBox);
    } catch (const std::exception &e) {
      qCritical("failed to create category checkbox: %s", e.what());
    }

    action->setData(categoryID);
    primaryCategoryMenu->addAction(action);
  }
}

void MainWindow::addPrimaryCategoryCandidates()
{
  QMenu *menu = qobject_cast<QMenu*>(sender());
  if (menu == nullptr) {
    qCritical("not a menu?");
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
  shell::ExploreFile(dataPath);
}

void MainWindow::openLogsFolder()
{
	QString logsPath = qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::logPath());
  shell::ExploreFile(logsPath);
}

void MainWindow::openInstallFolder()
{
  shell::ExploreFile(qApp->applicationDirPath());
}

void MainWindow::openPluginsFolder()
{
	QString pluginsPath = QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::pluginPath());
  shell::ExploreFile(pluginsPath);
}


void MainWindow::openProfileFolder()
{
  shell::ExploreFile(m_OrganizerCore.currentProfile()->absolutePath());
}

void MainWindow::openIniFolder()
{
  if (m_OrganizerCore.currentProfile()->localSettingsEnabled())
  {
    shell::ExploreFile(m_OrganizerCore.currentProfile()->absolutePath());
  }
  else {
    shell::ExploreFile(m_OrganizerCore.managedGame()->documentsDirectory());
  }
}

void MainWindow::openDownloadsFolder()
{
  shell::ExploreFile(m_OrganizerCore.settings().getDownloadDirectory());
}

void MainWindow::openModsFolder()
{
  shell::ExploreFile(m_OrganizerCore.settings().getModDirectory());
}

void MainWindow::openGameFolder()
{
  shell::ExploreFile(m_OrganizerCore.managedGame()->gameDirectory());
}

void MainWindow::openMyGamesFolder()
{
  shell::ExploreFile(m_OrganizerCore.managedGame()->documentsDirectory());
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
			if (mod_Name->isChecked())
				fields.push_back(std::make_pair(QString("#Mod_Name"), CSVBuilder::TYPE_STRING));
			if (mod_Status->isChecked())
				fields.push_back(std::make_pair(QString("#Mod_Status"), CSVBuilder::TYPE_STRING));
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

			for (unsigned int i = 0; i < numMods; ++i) {
				ModInfo::Ptr info = ModInfo::getByIndex(i);
				bool enabled = m_OrganizerCore.currentProfile()->modEnabled(i);
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
						builder.setRowField("#Mod_Priority", QString("%1").arg(m_OrganizerCore.currentProfile()->getModPriority(i), 4, 10, QChar('0')));
					if (mod_Name->isChecked())
						builder.setRowField("#Mod_Name", info->name());
					if (mod_Status->isChecked())
						builder.setRowField("#Mod_Status", (enabled)? "Enabled" : "Disabled");
          if (mod_Note->isChecked())
            builder.setRowField("#Note", QString("%1").arg(info->comments().remove(',')));
					if (primary_Category->isChecked())
						builder.setRowField("#Primary_Category", (m_CategoryFactory.categoryExists(info->getPrimaryCategory())) ? m_CategoryFactory.getCategoryName(info->getPrimaryCategory()) : "");
					if (nexus_ID->isChecked())
						builder.setRowField("#Nexus_ID", info->getNexusID());
					if (mod_Nexus_URL->isChecked())
						builder.setRowField("#Mod_Nexus_URL",(info->getNexusID()>0)? NexusInterface::instance(&m_PluginContainer)->getModURL(info->getNexusID(), info->getGameName()) : "");
					if (mod_Version->isChecked())
						builder.setRowField("#Mod_Version", info->getVersion().canonicalString());
					if (install_Date->isChecked())
						builder.setRowField("#Install_Date", info->creationTime().toString("yyyy/MM/dd HH:mm:ss"));
					if (download_File_Name->isChecked())
						builder.setRowField("#Download_File_Name", info->getInstallationFile());

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

	FolderMenu->addAction(tr("Open Game folder"), this, SLOT(openGameFolder()));

	FolderMenu->addAction(tr("Open MyGames folder"), this, SLOT(openMyGamesFolder()));

  FolderMenu->addAction(tr("Open INIs folder"), this, SLOT(openIniFolder()));

	FolderMenu->addSeparator();

	FolderMenu->addAction(tr("Open Instance folder"), this, SLOT(openInstanceFolder()));

  FolderMenu->addAction(tr("Open Mods folder"), this, SLOT(openModsFolder()));

	FolderMenu->addAction(tr("Open Profile folder"), this, SLOT(openProfileFolder()));

	FolderMenu->addAction(tr("Open Downloads folder"), this, SLOT(openDownloadsFolder()));

	FolderMenu->addSeparator();

	FolderMenu->addAction(tr("Open MO2 Install folder"), this, SLOT(openInstallFolder()));

	FolderMenu->addAction(tr("Open MO2 Plugins folder"), this, SLOT(openPluginsFolder()));

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

    if (m_ContextRow == -1) {
      // no selection
      QMenu menu(this);
      initModListContextMenu(&menu);
      menu.exec(modList->mapToGlobal(pos));
    } else {
      QMenu menu(this);

      QMenu *allMods = new QMenu(&menu);
      initModListContextMenu(allMods);
      allMods->setTitle(tr("All Mods"));
      menu.addMenu(allMods);
      menu.addSeparator();

      ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
      std::vector<ModInfo::EFlag> flags = info->getFlags();
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
        if (QDir(info->absolutePath()).count() > 2) {
          menu.addAction(tr("Sync to Mods..."), &m_OrganizerCore, SLOT(syncOverwrite()));
          menu.addAction(tr("Create Mod..."), this, SLOT(createModFromOverwrite()));
          menu.addAction(tr("Move content to Mod..."), this, SLOT(moveOverwriteContentToExistingMod()));
          menu.addAction(tr("Clear Overwrite..."), this, SLOT(clearOverwrite()));
        }
        menu.addAction(tr("Open in Explorer"), this, SLOT(openExplorer_clicked()));
      } else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end()) {
        menu.addAction(tr("Restore Backup"), this, SLOT(restoreBackup_clicked()));
        menu.addAction(tr("Remove Backup..."), this, SLOT(removeMod_clicked()));
      } else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_SEPARATOR) != flags.end()){
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
        if(info->getColor().isValid())
          menu.addAction(tr("Reset Color"), this, SLOT(resetColor_clicked()));
        menu.addSeparator();
      } else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) != flags.end()) {
        addModSendToContextMenu(&menu);
      } else {
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

        if (info->getNexusID() > 0)
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

        menu.addSeparator();

        if (info->getNexusID() > 0 && Settings::instance().endorsementIntegration()) {
          switch (info->endorsedState()) {
            case ModInfo::ENDORSED_TRUE: {
              menu.addAction(tr("Un-Endorse"), this, SLOT(unendorse_clicked()));
            } break;
            case ModInfo::ENDORSED_FALSE: {
              menu.addAction(tr("Endorse"), this, SLOT(endorse_clicked()));
              menu.addAction(tr("Won't endorse"), this, SLOT(dontendorse_clicked()));
            } break;
            case ModInfo::ENDORSED_NEVER: {
              menu.addAction(tr("Endorse"), this, SLOT(endorse_clicked()));
            } break;
            default: {
              QAction *action = new QAction(tr("Endorsement state unknown"), &menu);
              action->setEnabled(false);
              menu.addAction(action);
            } break;
          }
        }

        if (info->getNexusID() > 0) {
          switch (info->trackedState()) {
            case ModInfo::TRACKED_FALSE: {
              menu.addAction(tr("Start tracking"), this, SLOT(track_clicked()));
            } break;
            case ModInfo::TRACKED_TRUE: {
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

        if (info->getNexusID() > 0)  {
          menu.addAction(tr("Visit on Nexus"), this, SLOT(visitOnNexus_clicked()));
        } else if ((info->getURL() != "")) {
          menu.addAction(tr("Visit web page"), this, SLOT(visitWebPage_clicked()));
        }

        menu.addAction(tr("Open in Explorer"), this, SLOT(openExplorer_clicked()));
      }

      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) == flags.end()) {
        QAction *infoAction = menu.addAction(tr("Information..."), this, SLOT(information_clicked()));
        menu.setDefaultAction(infoAction);
      }

      menu.exec(modList->mapToGlobal(pos));
    }
  } catch (const std::exception &e) {
    reportError(tr("Exception: ").arg(e.what()));
  } catch (...) {
    reportError(tr("Unknown exception"));
  }
}


void MainWindow::on_categoriesList_itemSelectionChanged()
{
  QModelIndexList indices = ui->categoriesList->selectionModel()->selectedRows();
  std::vector<int> categories;
  std::vector<int> content;
  for (const QModelIndex &index : indices) {
    int filterType = index.data(Qt::UserRole + 1).toInt();
    if ((filterType == ModListSortProxy::TYPE_CATEGORY)
        || (filterType == ModListSortProxy::TYPE_SPECIAL)) {
      int categoryId = index.data(Qt::UserRole).toInt();
      if (categoryId != CategoryFactory::CATEGORY_NONE) {
        categories.push_back(categoryId);
      }
    } else if (filterType == ModListSortProxy::TYPE_CONTENT) {
      int contentId = index.data(Qt::UserRole).toInt();
      content.push_back(contentId);
    }
  }

  m_ModListSortProxy->setCategoryFilter(categories);
  m_ModListSortProxy->setContentFilter(content);
  ui->clickBlankButton->setEnabled(categories.size() > 0 || content.size() >0);

  if (indices.count() == 0) {
    ui->currentCategoryLabel->setText(QString("(%1)").arg(tr("<All>")));
  } else if (indices.count() > 1) {
    ui->currentCategoryLabel->setText(QString("(%1)").arg(tr("<Multiple>")));
  } else {
    ui->currentCategoryLabel->setText(QString("(%1)").arg(indices.first().data().toString()));
  }
  ui->modList->reset();
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


void MainWindow::on_savegameList_customContextMenuRequested(const QPoint &pos)
{
  QItemSelectionModel *selection = ui->savegameList->selectionModel();

  if (!selection->hasSelection()) {
    return;
  }

  QMenu menu;
  QAction *action = menu.addAction(tr("Enable Mods..."));
  action->setEnabled(false);

  if (selection->selectedIndexes().count() == 1) {
    SaveGameInfo const *info = this->m_OrganizerCore.managedGame()->feature<SaveGameInfo>();
    if (info != nullptr) {
      QString save = ui->savegameList->currentItem()->data(Qt::UserRole).toString();
      SaveGameInfo::MissingAssets missing = info->getMissingAssets(save);
      if (missing.size() != 0) {
        connect(action, &QAction::triggered, this, [this, missing]{ fixMods_clicked(missing); });
        action->setEnabled(true);
      }
    }
  }

  QString deleteMenuLabel = tr("Delete %n save(s)", "", selection->selectedIndexes().count());

  menu.addAction(deleteMenuLabel, this, SLOT(deleteSavegame_clicked()));

  menu.exec(ui->savegameList->mapToGlobal(pos));
}

void MainWindow::linkToolbar()
{
  Executable &exe(getSelectedExecutable());
  exe.showOnToolbar(!exe.isShownOnToolbar());
  ui->linkButton->menu()->actions().at(static_cast<int>(ShortcutType::Toolbar))->setIcon(exe.isShownOnToolbar() ? QIcon(":/MO/gui/remove") : QIcon(":/MO/gui/link"));
  updateToolBar();
}

namespace {
QString getLinkfile(const QString &dir, const Executable &exec)
{
  return QDir::fromNativeSeparators(dir) + "/" + exec.m_Title + ".lnk";
}

QString getDesktopLinkfile(const Executable &exec)
{
  return getLinkfile(getDesktopDirectory(), exec);
}

QString getStartMenuLinkfile(const Executable &exec)
{
  return getLinkfile(getStartMenuDirectory(), exec);
}
}

void MainWindow::addWindowsLink(const ShortcutType mapping)
{
  const Executable &selectedExecutable(getSelectedExecutable());
  QString const linkName = getLinkfile(mapping == ShortcutType::Desktop ? getDesktopDirectory() : getStartMenuDirectory(),
                                       selectedExecutable);

  if (QFile::exists(linkName)) {
    if (QFile::remove(linkName)) {
      ui->linkButton->menu()->actions().at(static_cast<int>(mapping))->setIcon(QIcon(":/MO/gui/link"));
    } else {
      reportError(tr("failed to remove %1").arg(linkName));
    }
  } else {
    QFileInfo const exeInfo(qApp->applicationFilePath());
    // create link
    QString executable = QDir::toNativeSeparators(selectedExecutable.m_BinaryInfo.absoluteFilePath());

    std::wstring targetFile       = ToWString(exeInfo.absoluteFilePath());
    std::wstring parameter        = ToWString(
      QString("\"moshortcut://%1:%2\"").arg(InstanceManager::instance().currentInstance(),selectedExecutable.m_Title));
    std::wstring description      = ToWString(QString("Run %1 with ModOrganizer").arg(selectedExecutable.m_Title));
    std::wstring iconFile         = ToWString(executable);
    std::wstring currentDirectory = ToWString(QDir::toNativeSeparators(qApp->applicationDirPath()));

    if (CreateShortcut(targetFile.c_str()
                       , parameter.c_str()
                       , QDir::toNativeSeparators(linkName).toUtf8().constData()
                       , description.c_str()
                       , (selectedExecutable.usesOwnIcon() ? iconFile.c_str() : nullptr), 0
                       , currentDirectory.c_str()) == 0) {
      ui->linkButton->menu()->actions().at(static_cast<int>(mapping))->setIcon(QIcon(":/MO/gui/remove"));
    } else {
      reportError(tr("failed to create %1").arg(linkName));
    }
  }
}

void MainWindow::linkDesktop()
{
  addWindowsLink(ShortcutType::Desktop);
}

void MainWindow::linkMenu()
{
  addWindowsLink(ShortcutType::StartMenu);
}

void MainWindow::on_actionSettings_triggered()
{
  Settings &settings = m_OrganizerCore.settings();

  QString oldModDirectory(settings.getModDirectory());
  QString oldCacheDirectory(settings.getCacheDirectory());
  QString oldProfilesDirectory(settings.getProfileDirectory());
  QString oldManagedGameDirectory(settings.getManagedGameDirectory());
  bool oldDisplayForeign(settings.displayForeign());
  bool proxy = settings.useProxy();
  DownloadManager *dlManager = m_OrganizerCore.downloadManager();

  settings.query(&m_PluginContainer, this);

  if (oldManagedGameDirectory != settings.getManagedGameDirectory()) {
    QMessageBox::about(this, tr("Restarting MO"),
      tr("Changing the managed game directory requires restarting MO.\n"
         "Any pending downloads will be paused.\n\n"
         "Click OK to restart MO now."));
    dlManager->pauseAll();
    qApp->exit(INT_MAX);
  }

  InstallationManager *instManager = m_OrganizerCore.installationManager();
  instManager->setModsDirectory(settings.getModDirectory());
  instManager->setDownloadDirectory(settings.getDownloadDirectory());

  fixCategories();
  refreshFilters();

  if (settings.getProfileDirectory() != oldProfilesDirectory) {
    refreshProfiles();
  }

  if (dlManager->getOutputDirectory() != settings.getDownloadDirectory()) {
    if (dlManager->downloadsInProgress()) {
      MessageDialog::showMessage(tr("Can't change download directory while "
                                    "downloads are in progress!"),
                                 this);
    } else {
      dlManager->setOutputDirectory(settings.getDownloadDirectory());
    }
  }
  dlManager->setPreferredServers(settings.getPreferredServers());

  if ((settings.getModDirectory() != oldModDirectory)
      || (settings.displayForeign() != oldDisplayForeign)) {
    m_OrganizerCore.profileRefresh();
  }

  const auto state = settings.archiveParsing();
  if (state != m_OrganizerCore.getArchiveParsing())
  {
    m_OrganizerCore.setArchiveParsing(state);
    if (!state)
    {
      ui->showArchiveDataCheckBox->setCheckState(Qt::Unchecked);
      ui->showArchiveDataCheckBox->setEnabled(false);
      m_showArchiveData = false;
    }
    else
    {
      ui->showArchiveDataCheckBox->setCheckState(Qt::Checked);
      ui->showArchiveDataCheckBox->setEnabled(true);
      m_showArchiveData = true;
    }
    m_OrganizerCore.refreshModList();
    m_OrganizerCore.refreshDirectoryStructure();
    m_OrganizerCore.refreshLists();
  }

  if (settings.getCacheDirectory() != oldCacheDirectory) {
    NexusInterface::instance(&m_PluginContainer)->setCacheDirectory(settings.getCacheDirectory());
  }

  if (proxy != settings.useProxy()) {
    activateProxy(settings.useProxy());
  }

  ui->apiRequests->setVisible(!settings.hideAPICounter());

  updateDownloadView();

  m_OrganizerCore.updateVFSParams(settings.logLevel(), settings.crashDumpsType(), settings.executablesBlacklist());
  m_OrganizerCore.cycleDiagnostics();

  toggleMO2EndorseState();
}


void MainWindow::on_actionNexus_triggered()
{
  const IPluginGame *game = m_OrganizerCore.managedGame();
  QString gameName = game->gameShortName();
  if (game->gameNexusName().isEmpty() && game->primarySources().count())
    gameName = game->primarySources()[0];
  QDesktopServices::openUrl(QUrl(NexusInterface::instance(&m_PluginContainer)->getGameURL(gameName)));
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
      qDebug("localization file %s not found", qUtf8Printable(fileName));
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
  qDebug("loaded language %s", qUtf8Printable(newLanguage));

  ui->profileBox->setItemText(0, QObject::tr("<Manage...>"));

  createHelpMenu();

  updateDownloadView();
  updateProblemsButton();

  QMenu *listOptionsMenu = new QMenu(ui->listOptionsBtn);
  initModListContextMenu(listOptionsMenu);
  ui->listOptionsBtn->setMenu(listOptionsMenu);

  ui->openFolderMenu->setMenu(openFolderMenu());
}

void MainWindow::writeDataToFile(QFile &file, const QString &directory, const DirectoryEntry &directoryEntry)
{
  for (FileEntry::Ptr current : directoryEntry.getFiles()) {
    bool isArchive = false;
    int origin = current->getOrigin(isArchive);
    if (isArchive) {
      // TODO: don't list files from archives. maybe make this an option?
      continue;
    }
    QString fullName = directory + "\\" + ToQString(current->getName());
    file.write(fullName.toUtf8());

    file.write("\t(");
    file.write(ToQString(m_OrganizerCore.directoryStructure()->getOriginByID(origin).getName()).toUtf8());
    file.write(")\r\n");
  }

  // recurse into subdirectories
  std::vector<DirectoryEntry*>::const_iterator current, end;
  directoryEntry.getSubDirectories(current, end);
  for (; current != end; ++current) {
    writeDataToFile(file, directory + "\\" + ToQString((*current)->getName()), **current);
  }
}

void MainWindow::writeDataToFile()
{
  QString fileName = QFileDialog::getSaveFileName(this);
  if (!fileName.isEmpty()) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
      reportError(tr("failed to write to file %1").arg(fileName));
    }

    writeDataToFile(file, "data", *m_OrganizerCore.directoryStructure());
    file.close();

    MessageDialog::showMessage(tr("%1 written").arg(QDir::toNativeSeparators(fileName)), this);
  }
}

void MainWindow::addAsExecutable()
{
  if (m_ContextItem == nullptr) {
    return;
  }

  using FileExecutionTypes = OrganizerCore::FileExecutionTypes;

  QFileInfo targetInfo(m_ContextItem->data(0, Qt::UserRole).toString());
  QFileInfo binaryInfo;
  QString arguments;
  FileExecutionTypes type;

  if (!OrganizerCore::getFileExecutionContext(this, targetInfo, binaryInfo, arguments, type)) {
    return;
  }

  switch (type)
  {
    case FileExecutionTypes::Executable: {
        QString name = QInputDialog::getText(this, tr("Enter Name"),
              tr("Please enter a name for the executable"), QLineEdit::Normal,
              targetInfo.baseName());

        if (!name.isEmpty()) {
          //Note: If this already exists, you'll lose custom settings
          m_OrganizerCore.executablesList()->addExecutable(name,
                                                           binaryInfo.absoluteFilePath(),
                                                           arguments,
                                                           targetInfo.absolutePath(),
                                                           QString(),
                                                           Executable::CustomExecutable);
          refreshExecutablesList();
        }

        break;
      }

    case FileExecutionTypes::Other:  // fall-through
    default: {
        QMessageBox::information(this, tr("Not an executable"), tr("This is not a recognized executable."));
        break;
      }
  }
}


void MainWindow::originModified(int originID)
{
  FilesOrigin &origin = m_OrganizerCore.directoryStructure()->getOriginByID(originID);
  origin.enable(false);
  m_OrganizerCore.directoryStructure()->addFromOrigin(origin.getName(), origin.getPath(), origin.getPriority());
  DirectoryRefresher::cleanStructure(m_OrganizerCore.directoryStructure());
}


void MainWindow::hideFile()
{
  QString oldName = m_ContextItem->data(0, Qt::UserRole).toString();
  QString newName = oldName + ModInfo::s_HiddenExt;

  if (QFileInfo(newName).exists()) {
    if (QMessageBox::question(this, tr("Replace file?"), tr("There already is a hidden version of this file. Replace it?"),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      if (!QFile(newName).remove()) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Failed to remove \"%1\". Maybe you lack the required file permissions?").arg(newName));
        return;
      }
    } else {
      return;
    }
  }

  if (QFile::rename(oldName, newName)) {
    originModified(m_ContextItem->data(1, Qt::UserRole + 1).toInt());
	refreshDataTreeKeepExpandedNodes();
  } else {
    reportError(tr("failed to rename \"%1\" to \"%2\"").arg(oldName).arg(QDir::toNativeSeparators(newName)));
  }
}


void MainWindow::unhideFile()
{
  QString oldName = m_ContextItem->data(0, Qt::UserRole).toString();
  QString newName = oldName.left(oldName.length() - ModInfo::s_HiddenExt.length());
  if (QFileInfo(newName).exists()) {
    if (QMessageBox::question(this, tr("Replace file?"), tr("There already is a visible version of this file. Replace it?"),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      if (!QFile(newName).remove()) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Failed to remove \"%1\". Maybe you lack the required file permissions?").arg(newName));
        return;
      }
    } else {
      return;
    }
  }
  if (QFile::rename(oldName, newName)) {
    originModified(m_ContextItem->data(1, Qt::UserRole + 1).toInt());
	refreshDataTreeKeepExpandedNodes();
  } else {
    reportError(tr("failed to rename \"%1\" to \"%2\"").arg(QDir::toNativeSeparators(oldName)).arg(QDir::toNativeSeparators(newName)));
  }
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


void MainWindow::previewDataFile()
{
  QString fileName = QDir::fromNativeSeparators(m_ContextItem->data(0, Qt::UserRole).toString());
  m_OrganizerCore.previewFileWithAlternatives(this, fileName);
}

void MainWindow::openDataFile()
{
  if (m_ContextItem == nullptr) {
    return;
  }

  QFileInfo targetInfo(m_ContextItem->data(0, Qt::UserRole).toString());
  m_OrganizerCore.executeFileVirtualized(this, targetInfo);
}


void MainWindow::updateAvailable()
{
  for (QAction *action : ui->toolBar->actions()) {
    if (action->text() == tr("Update")) {
      action->setEnabled(true);
      action->setToolTip(tr("Update available"));
      break;
    }
  }
}


void MainWindow::motdReceived(const QString &motd)
{
  // don't show motd after 5 seconds, may be annoying. Hopefully the user's
  // internet connection is faster next time
  if (m_StartTime.secsTo(QTime::currentTime()) < 5) {
    uint hash = qHash(motd);
    if (hash != m_OrganizerCore.settings().getMotDHash()) {
      MotDDialog dialog(motd);
      dialog.exec();
      m_OrganizerCore.settings().setMotDHash(hash);
    }
  }
}

void MainWindow::on_dataTree_customContextMenuRequested(const QPoint &pos)
{
  QTreeWidget *dataTree = findChild<QTreeWidget*>("dataTree");
  m_ContextItem = dataTree->itemAt(pos.x(), pos.y());

  QMenu menu;
  if ((m_ContextItem != nullptr) && (m_ContextItem->childCount() == 0)
      && (m_ContextItem->data(0, Qt::UserRole + 3).toBool() != true)) {
    menu.addAction(tr("Open/Execute"), this, SLOT(openDataFile()));
    menu.addAction(tr("Add as Executable"), this, SLOT(addAsExecutable()));

    QString fileName = m_ContextItem->text(0);
    if (m_PluginContainer.previewGenerator().previewSupported(QFileInfo(fileName).suffix())) {
      menu.addAction(tr("Preview"), this, SLOT(previewDataFile()));
    }

    // offer to hide/unhide file, but not for files from archives
    if (!m_ContextItem->data(0, Qt::UserRole + 1).toBool()) {
      if (m_ContextItem->text(0).endsWith(ModInfo::s_HiddenExt)) {
        menu.addAction(tr("Un-Hide"), this, SLOT(unhideFile()));
      } else {
        menu.addAction(tr("Hide"), this, SLOT(hideFile()));
      }
    }

    menu.addSeparator();
  }
  menu.addAction(tr("Write To File..."), this, SLOT(writeDataToFile()));
  menu.addAction(tr("Refresh"), this, SLOT(on_btnRefreshData_clicked()));

  menu.exec(dataTree->mapToGlobal(pos));
}

void MainWindow::on_conflictsCheckBox_toggled(bool)
{
  refreshDataTreeKeepExpandedNodes();
}

void MainWindow::on_actionUpdate_triggered()
{
  m_OrganizerCore.startMOUpdate();
}

void MainWindow::on_actionExit_triggered()
{
  exit();
}

void MainWindow::actionEndorseMO()
{
  // Normally this would be the managed game but MO2 is only uploaded to the Skyrim SE site right now
  IPluginGame * game = m_OrganizerCore.getGame("skyrimse");
  if (!game) return;

  if (QMessageBox::question(this, tr("Endorse Mod Organizer"),
                            tr("Do you want to endorse Mod Organizer on %1 now?").arg(
                                      NexusInterface::instance(&m_PluginContainer)->getGameURL(game->gameShortName())),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    NexusInterface::instance(&m_PluginContainer)->requestToggleEndorsement(
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
      NexusInterface::instance(&m_PluginContainer)->getGameURL(game->gameShortName())),
    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    NexusInterface::instance(&m_PluginContainer)->requestToggleEndorsement(
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
  connect(ui->downloadView, SIGNAL(openInDownloadsFolder(int)), m_OrganizerCore.downloadManager(), SLOT(openInDownloadsFolder(int)));
  connect(ui->downloadView, SIGNAL(removeDownload(int, bool)), m_OrganizerCore.downloadManager(), SLOT(removeDownload(int, bool)));
  connect(ui->downloadView, SIGNAL(restoreDownload(int)), m_OrganizerCore.downloadManager(), SLOT(restoreDownload(int)));
  connect(ui->downloadView, SIGNAL(cancelDownload(int)), m_OrganizerCore.downloadManager(), SLOT(cancelDownload(int)));
  connect(ui->downloadView, SIGNAL(pauseDownload(int)), m_OrganizerCore.downloadManager(), SLOT(pauseDownload(int)));
  connect(ui->downloadView, SIGNAL(resumeDownload(int)), this, SLOT(resumeDownload(int)));
}

void MainWindow::updateDownloadView()
{
  // set the view attribute and default row sizes
  if (m_OrganizerCore.settings().compactDownloads()) {
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

  ui->downloadView->setMetaDisplay(m_OrganizerCore.settings().metaDownloads());
  ui->downloadView->style()->unpolish(ui->downloadView);
  ui->downloadView->style()->polish(ui->downloadView);
  qobject_cast<DownloadListHeader*>(ui->downloadView->header())->customResizeSections();
  m_OrganizerCore.downloadManager()->refreshList();
}

void MainWindow::modUpdateCheck(std::multimap<QString, int> IDs)
{
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->validated()) {
    ModInfo::manualUpdateCheck(&m_PluginContainer, this, IDs);
  } else {
    QString apiKey;
    if (m_OrganizerCore.settings().getNexusApiKey(apiKey)) {
      m_OrganizerCore.doAfterLogin([=]() { this->modUpdateCheck(IDs); });
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
    } else
      qWarning("You are not currently authenticated with Nexus. Please do so under Settings -> Nexus.");
  }
}

void MainWindow::toggleMO2EndorseState()
{
  if (Settings::instance().endorsementIntegration()) {
    ui->actionEndorseMO->setVisible(true);
    if (Settings::instance().directInterface().contains("endorse_state")) {
      ui->actionEndorseMO->menu()->setEnabled(false);
      if (Settings::instance().directInterface().value("endorse_state").toString() == "Endorsed") {
        ui->actionEndorseMO->setToolTip(tr("Thank you for endorsing MO2! :)"));
        ui->actionEndorseMO->setStatusTip(tr("Thank you for endorsing MO2! :)"));
      } else if (Settings::instance().directInterface().value("endorse_state").toString() == "Abstained") {
        ui->actionEndorseMO->setToolTip(tr("Please reconsider endorsing MO2 on Nexus!"));
        ui->actionEndorseMO->setStatusTip(tr("Please reconsider endorsing MO2 on Nexus!"));
      }
    } else {
      ui->actionEndorseMO->menu()->setEnabled(true);
    }
  } else
    ui->actionEndorseMO->setVisible(false);
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

      if (Settings::instance().endorsementIntegration()) {
        if (result->first == "skyrimspecialedition" && result->second.first == gamePlugin->nexusModOrganizerID()) {
          Settings::instance().directInterface().setValue("endorse_state", result->second.second);
          toggleMO2EndorseState();
        }
      }
    }
  }

  if (!searchedMO2NexusGame && Settings::instance().endorsementIntegration()) {
    auto gamePlugin = m_OrganizerCore.getGame("SkyrimSE");
    if (gamePlugin) {
      auto iter = sorted.equal_range(gamePlugin->gameNexusName());
      for (auto result = iter.first; result != iter.second; ++result) {
        if (result->second.first == gamePlugin->nexusModOrganizerID()) {
          Settings::instance().directInterface().setValue("endorse_state", result->second.second);
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

  QFutureWatcher<std::set<QSharedPointer<ModInfo>>> *watcher = new QFutureWatcher<std::set<QSharedPointer<ModInfo>>>();
  QObject::connect(watcher, &QFutureWatcher<std::set<QSharedPointer<ModInfo>>>::finished, this, &MainWindow::finishUpdateInfo);
  QFuture<std::set<QSharedPointer<ModInfo>>> future = QtConcurrent::run([=]() -> std::set<QSharedPointer<ModInfo>> {
    return ModInfo::filteredMods(gameNameReal, resultList, userData.toBool(), true);
  });
  watcher->setFuture(future);
  if (m_ModListSortProxy != nullptr)
    m_ModListSortProxy->invalidate();
}

void MainWindow::finishUpdateInfo()
{
  QFutureWatcher<std::set<QSharedPointer<ModInfo>>> *watcher = static_cast<QFutureWatcher<std::set<QSharedPointer<ModInfo>>> *>(sender());

  auto finalMods = watcher->result();

  if (finalMods.empty()) {
    qInfo("None of your mods appear to have had recent file updates.");
  }

  std::set<std::pair<QString, int>> organizedGames;
  for (auto mod : finalMods) {
    if (mod->canBeUpdated()) {
      organizedGames.insert(std::make_pair<QString, int>(mod->getGameName().toLower(), mod->getNexusID()));
    }
  }

  if (!finalMods.empty() && organizedGames.empty())
    qWarning("All of your mods have been checked recently. We restrict update checks to help preserve your available API requests.");

  for (auto game : organizedGames)
    NexusInterface::instance(&m_PluginContainer)->requestUpdates(game.second, this, QVariant(), game.first, QString());

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
    QString installedFile = mod->getInstallationFile();
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
    NexusInterface::instance(&m_PluginContainer)->requestModInfo(gameNameReal, modID, this, QVariant(), QString());
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
      mod->setNewestVersion(result["version"].toString());
      mod->setLastNexusUpdate(QDateTime::currentDateTimeUtc());
      foundUpdate = true;
    }
    mod->setNexusDescription(result["description"].toString());
    if ((mod->endorsedState() != ModInfo::ENDORSED_NEVER) && (result.contains("endorsement"))) {
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
  QMap results = resultData.toMap();
  if (results["status"].toString().compare("Endorsed") == 0) {
    QMessageBox::information(this, tr("Thank you!"), tr("Thank you for your endorsement!"));
    Settings::instance().directInterface().setValue("endorse_state", "Endorsed");
  } else if (results["status"].toString().compare("Abstained") == 0) {
    QMessageBox::information(this, tr("Okay."), tr("This mod will not be endorsed and will no longer ask you to endorse."));
    Settings::instance().directInterface().setValue("endorse_state", "Abstained");
  }
  toggleMO2EndorseState();
  if (!disconnect(sender(), SIGNAL(nxmEndorsementToggled(QString, int, QVariant, QVariant, int)),
    this, SLOT(nxmEndorsementToggled(QString, int, QVariant, QVariant, int)))) {
    qCritical("failed to disconnect endorsement slot");
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
    if (modInfo->getNexusID() <= 0)
      continue;

    bool found = false;
    auto resultsList = resultData.toList();
    for (auto item : resultsList) {
      auto results = item.toMap();
      if ((gameNames[results["domain_name"].toString()].compare(modInfo->getGameName(), Qt::CaseInsensitive) == 0) &&
          (results["mod_id"].toInt() == modInfo->getNexusID())) {
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
  QVariantList serverList = resultData.toList();

  QList<ServerInfo> servers;
  for (const QVariant &server : serverList) {
    QVariantMap serverInfo = server.toMap();
    ServerInfo info;
    info.name = serverInfo["short_name"].toString();
    info.premium = serverInfo["name"].toString().contains("Premium", Qt::CaseInsensitive);
    info.lastSeen = QDate::currentDate();
    info.preferred = serverInfo["short_name"].toString().contains("CDN", Qt::CaseInsensitive);
    servers.append(info);
  }
  m_OrganizerCore.settings().updateServers(servers);
}


void MainWindow::nxmRequestFailed(QString gameName, int modID, int, QVariant, int, QNetworkReply::NetworkError error, const QString &errorString)
{
  if (error == QNetworkReply::ContentAccessDenied || error == QNetworkReply::ContentNotFoundError) {
    qDebug(qUtf8Printable(tr("Mod ID %1 no longer seems to be available on Nexus.").arg(modID)));
  } else {
    MessageDialog::showMessage(tr("Request to Nexus failed: %1").arg(errorString), this);
  }
}


void MainWindow::updateAPICounter(int queueCount, std::tuple<int, int, int, int> limits)
{
  ui->apiRequests->setText(QString("API: Q: %1 | D: %2 | H: %3").arg(queueCount).arg(std::get<0>(limits)).arg(std::get<2>(limits)));
  int requestsRemaining = std::get<0>(limits) + std::get<2>(limits);
  if (requestsRemaining > 300) {
    QPalette palette = ui->apiRequests->palette();
    palette.setColor(ui->apiRequests->backgroundRole(), Qt::darkGreen);
    ui->apiRequests->setPalette(palette);
  } else if (requestsRemaining < 150) {
    QPalette palette = ui->apiRequests->palette();
    palette.setColor(ui->apiRequests->backgroundRole(), Qt::darkRed);
    ui->apiRequests->setPalette(palette);
  } else {
    QPalette palette = ui->apiRequests->palette();
    palette.setColor(ui->apiRequests->backgroundRole(), Qt::darkYellow);
    ui->apiRequests->setPalette(palette);
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


void MainWindow::displayColumnSelection(const QPoint &pos)
{
  QMenu menu;

  // display a list of all headers as checkboxes
  QAbstractItemModel *model = ui->modList->header()->model();
  for (int i = 1; i < model->columnCount(); ++i) {
    QString columnName = model->headerData(i, Qt::Horizontal).toString();
    QCheckBox *checkBox = new QCheckBox(&menu);
    checkBox->setText(columnName);
    checkBox->setChecked(!ui->modList->header()->isSectionHidden(i));
    QWidgetAction *checkableAction = new QWidgetAction(&menu);
    checkableAction->setDefaultWidget(checkBox);
    menu.addAction(checkableAction);
  }
  menu.exec(pos);

  // view/hide columns depending on check-state
  int i = 1;
  for (const QAction *action : menu.actions()) {
    const QWidgetAction *widgetAction = qobject_cast<const QWidgetAction*>(action);
    if (widgetAction != nullptr) {
      const QCheckBox *checkBox = qobject_cast<const QCheckBox*>(widgetAction->defaultWidget());
      if (checkBox != nullptr) {
        ui->modList->header()->setSectionHidden(i, !checkBox->isChecked());
      }
    }
    ++i;
  }
}

void MainWindow::on_bsaList_customContextMenuRequested(const QPoint &pos)
{
  m_ContextItem = ui->bsaList->itemAt(pos);

//  m_ContextRow = ui->bsaList->indexOfTopLevelItem(ui->bsaList->itemAt(pos));

  QMenu menu;
  menu.addAction(tr("Extract..."), this, SLOT(extractBSATriggered()));

  menu.exec(ui->bsaList->mapToGlobal(pos));
}

void MainWindow::on_bsaList_itemChanged(QTreeWidgetItem*, int)
{
  m_ArchiveListWriter.write();
  m_CheckBSATimer.start(500);
}

void MainWindow::on_actionNotifications_triggered()
{
  updateProblemsButton();
  ProblemsDialog problems(m_PluginContainer.plugins<QObject>(), this);

  QSettings &settings = m_OrganizerCore.settings().directInterface();
  QString key = QString("geometry/%1").arg(problems.objectName());
  if (settings.contains(key)) {
    problems.restoreGeometry(settings.value(key).toByteArray());
  }
  problems.exec();
  settings.setValue(key, problems.saveGeometry());
  updateProblemsButton();
}

void MainWindow::on_actionChange_Game_triggered()
{
  if (QMessageBox::question(this, tr("Are you sure?"),
                            tr("This will restart MO, continue?"),
                            QMessageBox::Yes | QMessageBox::Cancel)
      == QMessageBox::Yes) {
    InstanceManager::instance().clearCurrentInstance();
    qApp->exit(INT_MAX);
  }
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

void MainWindow::editCategories()
{
  CategoriesDialog dialog(this);
  QSettings &settings = m_OrganizerCore.settings().directInterface();
  QString key = QString("geometry/%1").arg(dialog.objectName());
  if (settings.contains(key)) {
    dialog.restoreGeometry(settings.value(key).toByteArray());
  }
  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
  settings.setValue(key, dialog.saveGeometry());

}

void MainWindow::deselectFilters()
{
  ui->categoriesList->clearSelection();
}

void MainWindow::on_categoriesList_customContextMenuRequested(const QPoint &pos)
{
  QMenu menu;
  menu.addAction(tr("Edit Categories..."), this, SLOT(editCategories()));
  menu.addAction(tr("Deselect filter"), this, SLOT(deselectFilters()));

  menu.exec(ui->categoriesList->mapToGlobal(pos));
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
  try {
    Executable &exe = m_OrganizerCore.executablesList()->find(m_ContextAction->text());
    exe.showOnToolbar(false);
  } catch (const std::runtime_error&) {
    qDebug("executable doesn't exist any more");
  }

  updateToolBar();
}


void MainWindow::linksToolBar_customContextMenuRequested(const QPoint &point)
{
  QAction *action = ui->linksToolBar->actionAt(point);

  if (action != nullptr) {
    if (action->objectName().startsWith("custom_")) {
      m_ContextAction = action;
      QMenu menu;
      menu.addAction(tr("Remove '%1' from the toolbar").arg(action->text()), this, SLOT(removeFromToolbar()));
      menu.exec(ui->linksToolBar->mapToGlobal(point));
      return;
    }
  }

  // did not click a link button, show the default context menu
  auto* m = createPopupMenu();
  m->exec(ui->linksToolBar->mapToGlobal(point));
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
    menu.addAction(tr("Open Origin in Explorer"), this, SLOT(openOriginExplorer_clicked()));
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modInfoIndex);
    std::vector<ModInfo::EFlag> flags = modInfo->getFlags();

    if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) == flags.end()) {
      QAction *infoAction = menu.addAction(tr("Open Origin Info..."), this, SLOT(openOriginInformation_clicked()));
      menu.setDefaultAction(infoAction);
    }
  }

  try {
    menu.exec(ui->espList->mapToGlobal(pos));
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

const Executable &MainWindow::getSelectedExecutable() const
{
  QString name = ui->executablesListBox->itemText(ui->executablesListBox->currentIndex());
  return m_OrganizerCore.executablesList()->find(name);
}

Executable &MainWindow::getSelectedExecutable()
{
  QString name = ui->executablesListBox->itemText(ui->executablesListBox->currentIndex());
  return m_OrganizerCore.executablesList()->find(name);
}

void MainWindow::on_linkButton_pressed()
{
  const Executable &selectedExecutable(getSelectedExecutable());

  const QIcon addIcon(":/MO/gui/link");
  const QIcon removeIcon(":/MO/gui/remove");

  const QFileInfo linkDesktopFile(getDesktopLinkfile(selectedExecutable));
  const QFileInfo linkMenuFile(getStartMenuLinkfile(selectedExecutable));

  ui->linkButton->menu()->actions().at(static_cast<int>(ShortcutType::Toolbar))->setIcon(selectedExecutable.isShownOnToolbar() ? removeIcon : addIcon);
  ui->linkButton->menu()->actions().at(static_cast<int>(ShortcutType::Desktop))->setIcon(linkDesktopFile.exists() ? removeIcon : addIcon);
  ui->linkButton->menu()->actions().at(static_cast<int>(ShortcutType::StartMenu))->setIcon(linkMenuFile.exists() ? removeIcon : addIcon);
}

void MainWindow::on_showHiddenBox_toggled(bool checked)
{
  m_OrganizerCore.downloadManager()->setShowHidden(checked);
}


void MainWindow::createStdoutPipe(HANDLE *stdOutRead, HANDLE *stdOutWrite)
{
  SECURITY_ATTRIBUTES secAttributes;
  secAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  secAttributes.bInheritHandle = TRUE;
  secAttributes.lpSecurityDescriptor = nullptr;

  if (!::CreatePipe(stdOutRead, stdOutWrite, &secAttributes, 0)) {
    qCritical("failed to create stdout reroute");
  }

  if (!::SetHandleInformation(*stdOutRead, HANDLE_FLAG_INHERIT, 0)) {
    qCritical("failed to correctly set up the stdout reroute");
    *stdOutWrite = *stdOutRead = INVALID_HANDLE_VALUE;
  }
}

std::string MainWindow::readFromPipe(HANDLE stdOutRead)
{
  static const int chunkSize = 128;
  std::string result;

  char buffer[chunkSize + 1];
  buffer[chunkSize] = '\0';

  DWORD read = 1;
  while (read > 0) {
    if (!::ReadFile(stdOutRead, buffer, chunkSize, &read, nullptr)) {
      break;
    }
    if (read > 0) {
      result.append(buffer, read);
      if (read < chunkSize) {
        break;
      }
    }
  }
  return result;
}

void MainWindow::processLOOTOut(const std::string &lootOut, std::string &errorMessages, QProgressDialog &dialog)
{
  std::vector<std::string> lines;
  boost::split(lines, lootOut, boost::is_any_of("\r\n"));

  std::regex exRequires("\"([^\"]*)\" requires \"([^\"]*)\", but it is missing\\.");
  std::regex exIncompatible("\"([^\"]*)\" is incompatible with \"([^\"]*)\", but both are present\\.");

  for (const std::string &line : lines) {
    if (line.length() > 0) {
      size_t progidx    = line.find("[progress]");
      size_t erroridx   = line.find("[error]");
      if (progidx != std::string::npos) {
        dialog.setLabelText(line.substr(progidx + 11).c_str());
      } else if (erroridx != std::string::npos) {
        qWarning("%s", line.c_str());
        errorMessages.append(boost::algorithm::trim_copy(line.substr(erroridx + 8)) + "\n");
      } else {
        std::smatch match;
        if (std::regex_match(line, match, exRequires)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          m_OrganizerCore.pluginList()->addInformation(modName.c_str(), tr("depends on missing \"%1\"").arg(dependency.c_str()));
        } else if (std::regex_match(line, match, exIncompatible)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          m_OrganizerCore.pluginList()->addInformation(modName.c_str(), tr("incompatible with \"%1\"").arg(dependency.c_str()));
        } else {
          qDebug("[loot] %s", line.c_str());
        }
      }
    }
  }
}

void MainWindow::on_bossButton_clicked()
{
  std::string reportURL;
  std::string errorMessages;

  //m_OrganizerCore.currentProfile()->writeModlistNow();
  m_OrganizerCore.savePluginList();
  //Create a backup of the load orders w/ LOOT in name
  //to make sure that any sorting is easily undo-able.
  //Need to figure out how I want to do that.

  bool success = false;

  try {
    setEnabled(false);
    ON_BLOCK_EXIT([&] () { setEnabled(true); });
    QProgressDialog dialog(this);
    dialog.setLabelText(tr("Please wait while LOOT is running"));
    dialog.setMaximum(0);
    dialog.show();

    QString outPath = QDir::temp().absoluteFilePath("lootreport.json");

    QStringList parameters;
    parameters << "--game" << m_OrganizerCore.managedGame()->gameShortName()
        << "--gamePath" << QString("\"%1\"").arg(m_OrganizerCore.managedGame()->gameDirectory().absolutePath())
        << "--pluginListPath" << QString("\"%1/loadorder.txt\"").arg(m_OrganizerCore.profilePath())
        << "--out" << QString("\"%1\"").arg(outPath);

    if (m_DidUpdateMasterList) {
      parameters << "--skipUpdateMasterlist";
    }
    HANDLE stdOutWrite = INVALID_HANDLE_VALUE;
    HANDLE stdOutRead = INVALID_HANDLE_VALUE;
    createStdoutPipe(&stdOutRead, &stdOutWrite);
    try {
      m_OrganizerCore.prepareVFS();
    } catch (const UsvfsConnectorException &e) {
      qDebug(e.what());
      return;
    } catch (const std::exception &e) {
      QMessageBox::warning(qApp->activeWindow(), tr("Error"), e.what());
      return;
    }

    HANDLE loot = startBinary(QFileInfo(qApp->applicationDirPath() + "/loot/lootcli.exe"),
                              parameters.join(" "),
                              qApp->applicationDirPath() + "/loot",
                              true,
                              stdOutWrite);

    // we don't use the write end
    ::CloseHandle(stdOutWrite);

    m_OrganizerCore.pluginList()->clearAdditionalInformation();

    DWORD retLen;
    JOBOBJECT_BASIC_PROCESS_ID_LIST info;
    HANDLE processHandle = loot;

    if (loot != INVALID_HANDLE_VALUE) {
      bool isJobHandle = true;
      ULONG lastProcessID;
      DWORD res = ::MsgWaitForMultipleObjects(1, &loot, false, 100, QS_KEY | QS_MOUSE);
      while ((res != WAIT_FAILED) && (res != WAIT_OBJECT_0)) {
        if (isJobHandle) {
          if (::QueryInformationJobObject(loot, JobObjectBasicProcessIdList, &info, sizeof(info), &retLen) > 0) {
            if (info.NumberOfProcessIdsInList == 0) {
              qDebug("no more processes in job");
              break;
            } else {
              if (lastProcessID != info.ProcessIdList[0]) {
                lastProcessID = info.ProcessIdList[0];
                if (processHandle != loot) {
                  ::CloseHandle(processHandle);
                }
                processHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, lastProcessID);
              }
            }
          } else {
            // the info-object I passed only provides space for 1 process id. but since this code only cares about whether there
            // is more than one that's good enough. ERROR_MORE_DATA simply signals there are at least two processes running.
            // any other error probably means the handle is a regular process handle, probably caused by running MO in a job without
            // the right to break out.
            if (::GetLastError() != ERROR_MORE_DATA) {
              isJobHandle = false;
            }
          }
        }

        if (dialog.wasCanceled()) {
          if (isJobHandle) {
            ::TerminateJobObject(loot, 1);
          } else {
            ::TerminateProcess(loot, 1);
          }
        }

        // keep processing events so the app doesn't appear dead
        QCoreApplication::processEvents();
        std::string lootOut = readFromPipe(stdOutRead);
        processLOOTOut(lootOut, errorMessages, dialog);

        res = ::MsgWaitForMultipleObjects(1, &loot, false, 100, QS_KEY | QS_MOUSE);
      }

      std::string remainder = readFromPipe(stdOutRead).c_str();
      if (remainder.length() > 0) {
        processLOOTOut(remainder, errorMessages, dialog);
      }
      DWORD exitCode = 0UL;
      ::GetExitCodeProcess(processHandle, &exitCode);
      ::CloseHandle(processHandle);
      if (exitCode != 0UL) {
        reportError(tr("loot failed. Exit code was: %1").arg(exitCode));
        return;
      } else {
        success = true;
        QFile outFile(outPath);
        outFile.open(QIODevice::ReadOnly);
        QJsonDocument doc = QJsonDocument::fromJson(outFile.readAll());
        QJsonArray array = doc.array();
        for (auto iter = array.begin();  iter != array.end(); ++iter) {
          QJsonObject pluginObj = (*iter).toObject();
          QJsonArray pluginMessages = pluginObj["messages"].toArray();
          for (auto msgIter = pluginMessages.begin(); msgIter != pluginMessages.end(); ++msgIter) {
            QJsonObject msg = (*msgIter).toObject();
            m_OrganizerCore.pluginList()->addInformation(pluginObj["name"].toString(),
                QString("%1: %2").arg(msg["type"].toString(), msg["message"].toString()));
          }
          if (pluginObj["dirty"].toString() == "yes")
            m_OrganizerCore.pluginList()->addInformation(pluginObj["name"].toString(), "dirty");
        }

      }
    } else {
      reportError(tr("failed to start loot"));
    }
  } catch (const std::exception &e) {
    reportError(tr("failed to run loot: %1").arg(e.what()));
  }

  if (errorMessages.length() > 0) {
    QMessageBox *warn = new QMessageBox(QMessageBox::Warning, tr("Errors occured"), errorMessages.c_str(), QMessageBox::Ok, this);
    warn->setModal(false);
    warn->show();
  }

  if (success) {
    m_DidUpdateMasterList = true;
    if (reportURL.length() > 0) {
      m_IntegratedBrowser.setWindowTitle("LOOT Report");
      QString report(reportURL.c_str());
      QStringList temp = report.split("?");
      QUrl url = QUrl::fromLocalFile(temp.at(0));
      if (temp.size() > 1) {
        url.setQuery(temp.at(1).toUtf8());
      }
      m_IntegratedBrowser.openUrl(url);
    }
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
      QMessageBox::critical(this, tr("Restore failed"),
                            tr("Failed to restore the backup. Errorcode: %1").arg(windowsErrorString(::GetLastError())));
    }
    m_OrganizerCore.refreshESPList(true);
  }
}

void MainWindow::on_saveModsButton_clicked()
{
  m_OrganizerCore.currentProfile()->writeModlistNow(true);
  QDateTime now = QDateTime::currentDateTime();
  if (createBackup(m_OrganizerCore.currentProfile()->getModlistFileName(), now)) {
    MessageDialog::showMessage(tr("Backup of modlist created"), this);
  }
}

void MainWindow::on_restoreModsButton_clicked()
{
  QString modlistName = m_OrganizerCore.currentProfile()->getModlistFileName();
  QString choice = queryRestore(modlistName);
  if (!choice.isEmpty()) {
    if (!shellCopy(modlistName + "." + choice, modlistName, true, this)) {
      QMessageBox::critical(this, tr("Restore failed"),
                            tr("Failed to restore the backup. Errorcode: %1").arg(windowsErrorString(::GetLastError())));
    }
    m_OrganizerCore.refreshModList(false);
  }
}

void MainWindow::on_actionCopy_Log_to_Clipboard_triggered()
{
  QStringList lines;
  QAbstractItemModel *model = ui->logList->model();
  for (int i = 0; i < model->rowCount(); ++i) {
    lines.append(QString("%1 [%2] %3").arg(model->index(i, 0).data().toString())
                                      .arg(model->index(i, 1).data(Qt::UserRole).toString())
                                      .arg(model->index(i, 1).data().toString()));
  }
  QApplication::clipboard()->setText(lines.join("\n"));
}

void MainWindow::on_categoriesAndBtn_toggled(bool checked)
{
  if (checked) {
    m_ModListSortProxy->setFilterMode(ModListSortProxy::FILTER_AND);
  }
}

void MainWindow::on_categoriesOrBtn_toggled(bool checked)
{
  if (checked) {
    m_ModListSortProxy->setFilterMode(ModListSortProxy::FILTER_OR);
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
    qWarning("invalid source file: %s", qUtf8Printable(file.absoluteFilePath()));
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
    qCritical("file operation failed: %s", qUtf8Printable(windowsErrorString(::GetLastError())));
  }
}

bool MainWindow::registerWidgetState(const QString &name, QHeaderView *view, const char *oldSettingName) {
  // register the view so it's geometry gets saved at exit
  m_PersistedGeometry.push_back(std::make_pair(name, view));

  // also, restore the geometry if it was saved before
  QSettings &settings = m_OrganizerCore.settings().directInterface();

  QString key = QString("geometry/%1").arg(name);
  QByteArray data;

  if ((oldSettingName != nullptr) && settings.contains(oldSettingName)) {
    data = settings.value(oldSettingName).toByteArray();
    settings.remove(oldSettingName);
  } else if (settings.contains(key)) {
    data = settings.value(key).toByteArray();
  }

  if (!data.isEmpty()) {
    view->restoreState(data);
    return true;
  } else {
    return false;
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


void MainWindow::on_clickBlankButton_clicked()
{
  deselectFilters();
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
  QSettings &settings = m_OrganizerCore.settings().directInterface();
  QString key = QString("geometry/%1").arg(dialog.objectName());

  dialog.setWindowTitle("Select a separator...");
  dialog.setChoices(separators);
  dialog.restoreGeometry(settings.value(key).toByteArray());
  if (dialog.exec() == QDialog::Accepted) {
    QString result = dialog.getChoice();
    if (!result.isEmpty()) {
      result += "_separator";

      int newPriority = INT_MAX;
      bool foundSection = false;
      for (auto mod : m_OrganizerCore.modsSortedByProfilePriority()) {
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
  settings.setValue(key, dialog.saveGeometry());
}

void MainWindow::on_showArchiveDataCheckBox_toggled(const bool checked)
{
  if (m_OrganizerCore.getArchiveParsing() && checked)
  {
    m_showArchiveData = checked;
  }
  else
  {
    m_showArchiveData = false;
  }
  refreshDataTree();
}

