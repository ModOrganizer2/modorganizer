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
#include "downloadlistwidgetcompact.h"
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
  updateWindowTitle(QString(), false);

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
  statusBar()->hide();

  ui->actionEndorseMO->setVisible(false);

  updateProblemsButton();

  // Setup toolbar
  QWidget *spacer = new QWidget(ui->toolBar);
  spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
  QWidget *widget = ui->toolBar->widgetForAction(ui->actionTool);
  QToolButton *toolBtn = qobject_cast<QToolButton*>(widget);

  if (toolBtn->menu() == nullptr) {
    actionToToolButton(ui->actionTool);
  }

  actionToToolButton(ui->actionHelp);
  createHelpWidget();

  for (QAction *action : ui->toolBar->actions()) {
    if (action->isSeparator()) {
      // insert spacers
      ui->toolBar->insertWidget(action, spacer);
      m_Sep = action;
      // m_Sep would only use the last seperator anyway, and we only have the one anyway?
      break;
    }
  }

  TaskProgressManager::instance().tryCreateTaskbar();

  // set up mod list
  m_ModListSortProxy = m_OrganizerCore.createModListProxyModel();

  ui->modList->setModel(m_ModListSortProxy);

  GenericIconDelegate *contentDelegate = new GenericIconDelegate(ui->modList, Qt::UserRole + 3, ModList::COL_CONTENT, 150);
  connect(ui->modList->header(), SIGNAL(sectionResized(int,int,int)), contentDelegate, SLOT(columnResized(int,int,int)));
  ui->modList->sortByColumn(ModList::COL_PRIORITY, Qt::AscendingOrder);
  ui->modList->setItemDelegateForColumn(ModList::COL_FLAGS, new ModFlagIconDelegate(ui->modList));
  ui->modList->setItemDelegateForColumn(ModList::COL_CONTENT, contentDelegate);
  ui->modList->header()->installEventFilter(m_OrganizerCore.modList());

  bool modListAdjusted = registerWidgetState(ui->modList->objectName(), ui->modList->header(), "mod_list_state");

  if (modListAdjusted) {
    // hack: force the resize-signal to be triggered because restoreState doesn't seem to do that
    int sectionSize = ui->modList->header()->sectionSize(ModList::COL_CONTENT);
    ui->modList->header()->resizeSection(ModList::COL_CONTENT, sectionSize + 1);
    ui->modList->header()->resizeSection(ModList::COL_CONTENT, sectionSize);
  } else {
    // hide these columns by default
    ui->modList->header()->setSectionHidden(ModList::COL_CONTENT, true);
    ui->modList->header()->setSectionHidden(ModList::COL_MODID, true);
    ui->modList->header()->setSectionHidden(ModList::COL_GAME, true);
    ui->modList->header()->setSectionHidden(ModList::COL_INSTALLTIME, true);
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

  ui->listOptionsBtn->setMenu(modListContextMenu());

  ui->openFolderMenu->setMenu(openFolderMenu());

  updateDownloadListDelegate();

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

  connect(ui->savegameList, SIGNAL(itemEntered(QListWidgetItem*)), this, SLOT(saveSelectionChanged(QListWidgetItem*)));

  connect(ui->modList, SIGNAL(dropModeUpdate(bool)), m_OrganizerCore.modList(), SLOT(dropModeUpdate(bool)));

  connect(ui->modList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(modlistSelectionChanged(QModelIndex,QModelIndex)));
  connect(m_ModListSortProxy, SIGNAL(filterActive(bool)), this, SLOT(modFilterActive(bool)));
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
  connect(NexusInterface::instance(&pluginContainer), SIGNAL(needLogin()), &m_OrganizerCore, SLOT(nexusLogin()));
  connect(NexusInterface::instance(&pluginContainer)->getAccessManager(), SIGNAL(loginFailed(QString)), this, SLOT(loginFailed(QString)));
  connect(NexusInterface::instance(&pluginContainer)->getAccessManager(), SIGNAL(credentialsReceived(const QString&, bool)),
          this, SLOT(updateWindowTitle(const QString&, bool)));

  connect(&TutorialManager::instance(), SIGNAL(windowTutorialFinished(QString)), this, SLOT(windowTutorialFinished(QString)));
  connect(ui->tabWidget, SIGNAL(currentChanged(int)), &TutorialManager::instance(), SIGNAL(tabChanged(int)));
  connect(ui->modList->header(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)), this, SLOT(modListSortIndicatorChanged(int,Qt::SortOrder)));
  connect(ui->toolBar, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(toolBar_customContextMenuRequested(QPoint)));

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
  for (IPluginTool *toolPlugin : m_PluginContainer.plugins<IPluginTool>()) {
    registerPluginTool(toolPlugin);
  }

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
    ui->toolBar->widgetForAction(action)->setObjectName(action->objectName());
  }
}


MainWindow::~MainWindow()
{
  cleanup();

  m_PluginContainer.setUserInterface(nullptr, nullptr);
  m_OrganizerCore.setUserInterface(nullptr, nullptr);
  m_IntegratedBrowser.close();
  delete ui;
}


void MainWindow::updateWindowTitle(const QString &accountName, bool premium)
{
  QString title = QString("%1 Mod Organizer v%2").arg(
        m_OrganizerCore.managedGame()->gameName(),
        m_OrganizerCore.getVersion().displayString());

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


void MainWindow::actionToToolButton(QAction *&sourceAction)
{
  QToolButton *button = new QToolButton(ui->toolBar);
  button->setObjectName(sourceAction->objectName());
  button->setIcon(sourceAction->icon());
  button->setText(sourceAction->text());
  button->setPopupMode(QToolButton::InstantPopup);
  button->setToolButtonStyle(ui->toolBar->toolButtonStyle());
  button->setToolTip(sourceAction->toolTip());
  button->setShortcut(sourceAction->shortcut());
  QMenu *buttonMenu = new QMenu(sourceAction->text(), button);
  button->setMenu(buttonMenu);
  QAction *newAction = ui->toolBar->insertWidget(sourceAction, button);
  newAction->setObjectName(sourceAction->objectName());
  newAction->setIcon(sourceAction->icon());
  newAction->setText(sourceAction->text());
  newAction->setToolTip(sourceAction->toolTip());
  newAction->setShortcut(sourceAction->shortcut());
  ui->toolBar->removeAction(sourceAction);
  sourceAction->deleteLater();
  sourceAction = newAction;
}

void MainWindow::updateToolBar()
{
  for (QAction *action : ui->toolBar->actions()) {
    if (action->objectName().startsWith("custom__")) {
      ui->toolBar->removeAction(action);
      action->deleteLater();
    }
  }

  std::vector<Executable>::iterator begin, end;
  m_OrganizerCore.executablesList()->getExecutables(begin, end);
  for (auto iter = begin; iter != end; ++iter) {
    if (iter->isShownOnToolbar()) {
      QAction *exeAction = new QAction(iconForExecutable(iter->m_BinaryInfo.filePath()),
                                        iter->m_Title,
                                        ui->toolBar);
      exeAction->setObjectName(QString("custom__") + iter->m_Title);
      if (!connect(exeAction, SIGNAL(triggered()), this, SLOT(startExeAction()))) {
        qDebug("failed to connect trigger?");
      }
      ui->toolBar->insertAction(m_Sep, exeAction);
    }
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
    ui->actionProblems->setEnabled(true);
    ui->actionProblems->setIconText(tr("Problems"));
    ui->actionProblems->setToolTip(tr("There are potential problems with your setup"));

    QPixmap mergedIcon = QPixmap(":/MO/gui/warning").scaled(64, 64);
    {
      QPainter painter(&mergedIcon);
      std::string badgeName = std::string(":/MO/gui/badge_") + (numProblems < 10 ? std::to_string(static_cast<long long>(numProblems)) : "more");
      painter.drawPixmap(32, 32, 32, 32, QPixmap(badgeName.c_str()));
    }
    ui->actionProblems->setIcon(QIcon(mergedIcon));
  } else {
    ui->actionProblems->setEnabled(false);
    ui->actionProblems->setIconText(tr("No Problems"));
    ui->actionProblems->setToolTip(tr("Everything seems to be in order"));
    ui->actionProblems->setIcon(QIcon(":/MO/gui/warning"));
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
  for (IPluginDiagnose *diagnose : m_PluginContainer.plugins<IPluginDiagnose>()) {
    numProblems += diagnose->activeProblems().size();
  }
  return numProblems;
}

void MainWindow::about()
{
  AboutDialog dialog(m_OrganizerCore.getVersion().displayString(), this);
  connect(&dialog, SIGNAL(linkClicked(QString)), this, SLOT(linkClicked(QString)));
  dialog.exec();
}


void MainWindow::createHelpWidget()
{
  QToolButton *toolBtn = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionHelp));
  QMenu *buttonMenu = toolBtn->menu();
  if (buttonMenu == nullptr) {
    return;
  }
  buttonMenu->clear();

  QAction *helpAction = new QAction(tr("Help on UI"), buttonMenu);
  connect(helpAction, SIGNAL(triggered()), this, SLOT(helpTriggered()));
  buttonMenu->addAction(helpAction);

  QAction *wikiAction = new QAction(tr("Documentation Wiki"), buttonMenu);
  connect(wikiAction, SIGNAL(triggered()), this, SLOT(wikiTriggered()));
  buttonMenu->addAction(wikiAction);

  QAction *issueAction = new QAction(tr("Report Issue"), buttonMenu);
  connect(issueAction, SIGNAL(triggered()), this, SLOT(issueTriggered()));
  buttonMenu->addAction(issueAction);

  QMenu *tutorialMenu = new QMenu(tr("Tutorials"), buttonMenu);

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

  buttonMenu->addMenu(tutorialMenu);
  buttonMenu->addAction(tr("About"), this, SLOT(about()));
  buttonMenu->addAction(tr("About Qt"), qApp, SLOT(aboutQt()));
}

void MainWindow::modFilterActive(bool filterActive)
{
  if (filterActive) {
//    m_OrganizerCore.modList()->setOverwriteMarkers(std::set<unsigned int>(), std::set<unsigned int>());
    ui->modList->setStyleSheet("QTreeView { border: 2px ridge #f00; }");
  } else if (ui->groupCombo->currentIndex() != 0) {
    ui->modList->setStyleSheet("QTreeView { border: 2px ridge #337733; }");
  } else {
    ui->modList->setStyleSheet("");
  }
}

void MainWindow::espFilterChanged(const QString &filter)
{
  if (!filter.isEmpty()) {
    ui->espList->setStyleSheet("QTreeView { border: 2px ridge #f00; }");
  } else {
    ui->espList->setStyleSheet("");
  }
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
  m_closing = true;

  if (m_OrganizerCore.downloadManager()->downloadsInProgressNoPause()) {
    if (QMessageBox::question(this, tr("Downloads in progress"),
                          tr("There are still downloads in progress, do you really want to quit?"),
                          QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Cancel) {
      event->ignore();
      return;
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
      event->ignore();
      return;
    }
  }

  setCursor(Qt::WaitCursor);
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

  ui->savegameList->activateWindow();
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

void MainWindow::registerPluginTool(IPluginTool *tool)
{
  QAction *action = new QAction(tool->icon(), tool->displayName(), ui->toolBar);
  action->setToolTip(tool->tooltip());
  tool->setParentWidget(this);
  action->setData(qVariantFromValue((QObject*)tool));
  connect(action, SIGNAL(triggered()), this, SLOT(toolPluginInvoke()), Qt::QueuedConnection);
  QToolButton *toolBtn = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionTool));
  toolBtn->menu()->addAction(action);
}

void MainWindow::registerModPage(IPluginModPage *modPage)
{
  // turn the browser action into a drop-down menu if necessary
  if (ui->actionNexus->menu() == nullptr) {
    QAction *nexusAction = ui->actionNexus;
    // TODO: use a different icon for nexus!
    ui->actionNexus = new QAction(nexusAction->icon(), tr("Browse Mod Page"), ui->toolBar);
    ui->toolBar->insertAction(nexusAction, ui->actionNexus);
    ui->toolBar->removeAction(nexusAction);
    actionToToolButton(ui->actionNexus);

    QToolButton *browserBtn = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionNexus));
    browserBtn->menu()->addAction(nexusAction);
  }

  QAction *action = new QAction(modPage->icon(), modPage->displayName(), ui->toolBar);
  modPage->setParentWidget(this);
  action->setData(qVariantFromValue(reinterpret_cast<QObject*>(modPage)));

  connect(action, SIGNAL(triggered()), this, SLOT(modPagePluginInvoke()), Qt::QueuedConnection);
  QToolButton *toolBtn = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionNexus));
  toolBtn->menu()->addAction(action);
}


void MainWindow::startExeAction()
{
  QAction *action = qobject_cast<QAction*>(sender());
  if (action != nullptr) {
    const Executable &selectedExecutable(
        m_OrganizerCore.executablesList()->find(action->text()));
	QString customOverwrite= m_OrganizerCore.currentProfile()->setting("custom_overwrites", selectedExecutable.m_Title).toString();
    m_OrganizerCore.spawnBinary(
        selectedExecutable.m_BinaryInfo, selectedExecutable.m_Arguments,
        selectedExecutable.m_WorkingDirectory.length() != 0
            ? selectedExecutable.m_WorkingDirectory
            : selectedExecutable.m_BinaryInfo.absolutePath(),
        selectedExecutable.m_SteamAppID, customOverwrite);
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
    if (saveGames != nullptr && saveGames->updateSaveGames(m_OrganizerCore.currentProfile())) {
      refreshSaveList();
    }
  }
}

void MainWindow::updateTo(QTreeWidgetItem* subTree, const std::wstring& directorySoFar,
	const DirectoryEntry& directoryEntry, const bool conflictsOnly)
{
	{
		for (const FileEntry::Ptr current : directoryEntry.getFiles()) {
			if (conflictsOnly && (current->getAlternatives().size() == 0)) {
				continue;
			}
			bool isArchive = false;
			int originID = current->getOrigin(isArchive);
			if (!(isArchive & !m_showArchiveData))
			{
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
				}
				else if (fileName.endsWith(ModInfo::s_HiddenExt)) {
					QFont font = fileChild->font(0);
					font.setStrikeOut(true);
					fileChild->setFont(0, font);
					fileChild->setFont(1, font);
				}
				fileChild->setData(0, Qt::UserRole, ToQString(current->getFullPath()));
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
				}
				else {
					fileChild->setToolTip(1, tr("No conflict"));
				}
				subTree->addChild(fileChild);
			}
		}
	}

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
				if (conflictsOnly || !m_showArchiveData) {
					updateTo(directoryChild, temp.str(), **current, conflictsOnly);
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
		}
	}

	subTree->sortChildren(0, Qt::AscendingOrder);
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
      updateTo(item, path, *dir, conflictsOnly);
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
  tree->clear();
  QStringList columns("data");
  columns.append("");
  QTreeWidgetItem *subTree = new QTreeWidgetItem(columns);
  updateTo(subTree, L"", *m_OrganizerCore.directoryStructure(), conflictsBox->isChecked());
  tree->insertTopLevelItem(0, subTree);
  subTree->setExpanded(true);
}

void MainWindow::refreshDataTreeKeepExpandedNodes()
{
	QCheckBox *conflictsBox = findChild<QCheckBox*>("conflictsCheckBox");
	QTreeWidget *tree = findChild<QTreeWidget*>("dataTree");

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
	updateTo(subTree, L"", *m_OrganizerCore.directoryStructure(), conflictsBox->isChecked());
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
    wchar_t path[MAX_PATH];
    ::GetPrivateProfileStringW(
          L"General", L"SLocalSavePath", L"Saves",
          path, MAX_PATH,
          ToWString(m_OrganizerCore.currentProfile()->absolutePath() + "/" +
                      m_OrganizerCore.managedGame()->iniFiles()[0]).c_str());
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
  qDebug("reading save games from %s", qPrintable(savesDir.absolutePath()));

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
    qDebug("Using proxy: %s", qPrintable(proxies.at(0).hostName()));
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
  }

  if (currentVersion > lastVersion)
    settings.setValue("version", currentVersion.toString());
  else if (currentVersion < lastVersion)
    qWarning() << tr("Notice: Your current MO version (%1) is lower than the previous version (%2).<br>"
                     "The GUI may not downgrade gracefully, so you may experience oddities.<br>"
                     "However, there should be no serious issues.").arg(lastVersion.toString()).arg(currentVersion.toString()).toStdWString();
}

void MainWindow::storeSettings(QSettings &settings) {
  settings.setValue("group_state", ui->groupCombo->currentIndex());

  settings.setValue("window_geometry", saveGeometry());
  settings.setValue("window_split", ui->splitter->saveState());
  settings.setValue("log_split", ui->topLevelSplitter->saveState());

  settings.setValue("browser_geometry", m_IntegratedBrowser.saveGeometry());

  settings.setValue("filters_visible", ui->displayCategoriesBtn->isChecked());

  settings.setValue("selected_executable",
                    ui->executablesListBox->currentIndex());

  for (const std::pair<QString, QHeaderView*> kv : m_PersistedGeometry) {
    QString key = QString("geometry/") + kv.first;
    settings.setValue(key, kv.second->saveState());
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
    refreshDataTree();
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
  const Executable &selectedExecutable(getSelectedExecutable());
  QString customOverwrite = m_OrganizerCore.currentProfile()->setting("custom_overwrites", selectedExecutable.m_Title).toString();
  m_OrganizerCore.spawnBinary(
      selectedExecutable.m_BinaryInfo, selectedExecutable.m_Arguments,
      selectedExecutable.m_WorkingDirectory.length() != 0
          ? selectedExecutable.m_WorkingDirectory
          : selectedExecutable.m_BinaryInfo.absolutePath(),
      selectedExecutable.m_SteamAppID, customOverwrite);
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
                                 m_OrganizerCore.currentProfile());
    if (dialog.exec() == QDialog::Accepted) {
      m_OrganizerCore.setExecutablesList(dialog.getExecutablesList());
      result = true;
    }
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
  QDesktopServices::openUrl(QUrl("http://wiki.step-project.com/Guide:Mod_Organizer"));
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
  if (saveGames != nullptr && saveGames->updateSaveGames(m_OrganizerCore.currentProfile())) {
    refreshSaveList();
  }

//  addProfile();
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
	if (percent == 100)
	{
		m_RefreshProgress->setVisible(false);
		statusBar()->hide();
		this->setEnabled(true);
	}
	else if (!m_RefreshProgress->isVisible())
	{
		this->setEnabled(false);
		statusBar()->show();
		m_RefreshProgress->setVisible(true);
		m_RefreshProgress->setRange(0, 100);
		m_RefreshProgress->setValue(percent);
	}
}

void MainWindow::directory_refreshed()
{
  // some problem-reports may rely on the virtual directory tree so they need to be updated
  // now
  refreshDataTree();
  updateProblemsButton();
  statusBar()->hide();
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
      if (m_ModListSortProxy != nullptr) {
        m_ModListSortProxy->invalidate();
      }
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
          qWarning("cycle in categories: %s", qPrintable(SetJoin(cycleTest, ", ")));
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
}

void MainWindow::modlistSelectionChanged(const QModelIndex &current, const QModelIndex&)
{
  if (current.isValid()) {
    ModInfo::Ptr selectedMod = ModInfo::getByIndex(current.data(Qt::UserRole + 1).toInt());
    m_OrganizerCore.modList()->setOverwriteMarkers(selectedMod->getModOverwrite(), selectedMod->getModOverwritten());
    m_OrganizerCore.modList()->setArchiveOverwriteMarkers(selectedMod->getModArchiveOverwrite(), selectedMod->getModArchiveOverwritten());
    m_OrganizerCore.modList()->setArchiveLooseOverwriteMarkers(selectedMod->getModArchiveLooseOverwrite(), selectedMod->getModArchiveLooseOverwritten());
  } else {
    m_OrganizerCore.modList()->setOverwriteMarkers(std::set<unsigned int>(), std::set<unsigned int>());
    m_OrganizerCore.modList()->setArchiveOverwriteMarkers(std::set<unsigned int>(), std::set<unsigned int>());
    m_OrganizerCore.modList()->setArchiveLooseOverwriteMarkers(std::set<unsigned int>(), std::set<unsigned int>());
  }
/*  if ((m_ModListSortProxy != nullptr)
      && !m_ModListSortProxy->beingInvalidated()) {
    m_ModListSortProxy->invalidate();
  }*/
  ui->modList->verticalScrollBar()->repaint();
}

void MainWindow::modlistSelectionsChanged(const QItemSelection &selected)
{
  m_OrganizerCore.pluginList()->highlightPlugins(selected, *m_OrganizerCore.directoryStructure(), *m_OrganizerCore.currentProfile());
  ui->espList->verticalScrollBar()->repaint();
}

void MainWindow::esplistSelectionsChanged(const QItemSelection &selected)
{
  m_OrganizerCore.modList()->highlightMods(selected, *m_OrganizerCore.directoryStructure());
  ui->modList->verticalScrollBar()->repaint();
}

void MainWindow::modListSortIndicatorChanged(int, Qt::SortOrder)
{
  ui->modList->verticalScrollBar()->repaint();
}

void MainWindow::removeMod_clicked()
{
  try {
    QItemSelectionModel *selection = ui->modList->selectionModel();
    if (selection->hasSelection() && selection->selectedRows().count() > 1) {
      QString mods;
      QStringList modNames;
      for (QModelIndex idx : selection->selectedRows()) {
        QString name = idx.data().toString();
        if (!ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt())->isRegular()) {
          continue;
        }
        mods += "<li>" + name + "</li>";
        modNames.append(name);
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


void MainWindow::resumeDownload(int downloadIndex)
{
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->loggedIn()) {
    m_OrganizerCore.downloadManager()->resumeDownload(downloadIndex);
  } else {
    QString username, password;
    if (m_OrganizerCore.settings().getNexusLogin(username, password)) {
      m_OrganizerCore.doAfterLogin([this, downloadIndex] () {
        this->resumeDownload(downloadIndex);
      });
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->login(username, password);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to resume a download"), this);
    }
  }
}


void MainWindow::endorseMod(ModInfo::Ptr mod)
{
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->loggedIn()) {
    mod->endorse(true);
  } else {
    QString username, password;
    if (m_OrganizerCore.settings().getNexusLogin(username, password)) {
      m_OrganizerCore.doAfterLogin(boost::bind(&MainWindow::endorseMod, this, mod));
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->login(username, password);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to endorse"), this);
    }
  }
}


void MainWindow::endorse_clicked()
{
  endorseMod(ModInfo::getByIndex(m_ContextRow));
}

void MainWindow::dontendorse_clicked()
{
  ModInfo::getByIndex(m_ContextRow)->setNeverEndorse();
}


void MainWindow::unendorse_clicked()
{
  QString username, password;
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->loggedIn()) {
    ModInfo::getByIndex(m_ContextRow)->endorse(false);
  } else {
    if (m_OrganizerCore.settings().getNexusLogin(username, password)) {
      m_OrganizerCore.doAfterLogin([this] () { this->unendorse_clicked(); });
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->login(username, password);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to endorse"), this);
    }
  }
}

void MainWindow::loginFailed(const QString &error)
{
  qDebug("login failed: %s", qPrintable(error));
  statusBar()->hide();
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

    dialog.restoreTabState(m_OrganizerCore.settings().directInterface().value("mod_info_tabs").toByteArray());

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
    m_OrganizerCore.settings().directInterface().setValue("mod_info_tabs", dialog.saveTabState());

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
      (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end())) {
    // skip overwrite and backups
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
      (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end())) {
    // skip overwrite and backups
    modOpenPrev(tab);
  } else {
    displayModInformation(m_ContextRow,tab);
  }
}

void MainWindow::displayModInformation(const QString &modName, int tab)
{
  unsigned int index = ModInfo::getIndex(modName);
  if (index == UINT_MAX) {
    qCritical("failed to resolve mod name %s", modName.toUtf8().constData());
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
      QDir(info->absolutePath()).mkdir("textures");
      info->testValid();
      connect(this, SIGNAL(modListDataChanged(QModelIndex, QModelIndex)), m_OrganizerCore.modList(), SIGNAL(dataChanged(QModelIndex, QModelIndex)));

      emit modListDataChanged(m_OrganizerCore.modList()->index(row_idx, 0), m_OrganizerCore.modList()->index(row_idx, m_OrganizerCore.modList()->columnCount() - 1));
    }
  } else {
    ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
    QDir(info->absolutePath()).mkdir("textures");
    info->testValid();
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
  int modID = m_OrganizerCore.modList()->data(m_OrganizerCore.modList()->index(m_ContextRow, 0), Qt::UserRole).toInt();
  QString gameName = m_OrganizerCore.modList()->data(m_OrganizerCore.modList()->index(m_ContextRow, 0), Qt::UserRole + 4).toString();
  if (modID > 0)  {
    linkClicked(NexusInterface::instance(&m_PluginContainer)->getModURL(modID, gameName));
  } else {
    MessageDialog::showMessage(tr("Nexus ID for this Mod is unknown"), this);
  }
}

void MainWindow::visitWebPage_clicked()
{
  ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
  if (info->getURL() != "") {
    linkClicked(info->getURL());
  } else {
    MessageDialog::showMessage(tr("Web page for this mod is unknown"), this);
  }
}

void MainWindow::openExplorer_clicked()
{
  QItemSelectionModel *selection = ui->modList->selectionModel();
  if (selection->hasSelection() && selection->selectedRows().count() > 1) {
    for (QModelIndex idx : selection->selectedRows()) {
      ModInfo::Ptr info = ModInfo::getByIndex(idx.data(Qt::UserRole + 1).toInt());
      ::ShellExecuteW(nullptr, L"explore", ToWString(info->absolutePath()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
  }
  else {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
    ::ShellExecuteW(nullptr, L"explore", ToWString(modInfo->absolutePath()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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
				::ShellExecuteW(nullptr, L"explore", ToWString(modInfo->absolutePath()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			}

		}
	}

	if (ui->espList->hasFocus()) {
		QItemSelectionModel *selection = ui->espList->selectionModel();

		if (selection->hasSelection() && selection->selectedRows().count() == 1) {

			QModelIndex idx = selection->currentIndex();
			QString fileName = idx.data().toString();



			ModInfo::Ptr modInfo = ModInfo::getByIndex(ModInfo::getIndex(m_OrganizerCore.pluginList()->origin(fileName)));
			std::vector<ModInfo::EFlag> flags = modInfo->getFlags();

			if (modInfo->isRegular() || (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end())) {
				::ShellExecuteW(nullptr, L"explore", ToWString(modInfo->absolutePath()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			}

		}
	}
}

void MainWindow::refreshProfile_activated()
{
	m_OrganizerCore.profileRefresh();
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

  IModInterface *newMod = m_OrganizerCore.createMod(name);
  if (newMod == nullptr) {
    return;
  }

  m_OrganizerCore.refreshModList();
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

  IModInterface *newMod = m_OrganizerCore.createMod(name);
  if (newMod == nullptr) {
    return;
  }

  unsigned int overwriteIndex = ModInfo::findMod([](ModInfo::Ptr mod) -> bool {
    std::vector<ModInfo::EFlag> flags = mod->getFlags();
    return std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end(); });

  ModInfo::Ptr overwriteInfo = ModInfo::getByIndex(overwriteIndex);
  shellMove(QStringList(QDir::toNativeSeparators(overwriteInfo->absolutePath()) + "\\*"),
            QStringList(QDir::toNativeSeparators(newMod->absolutePath())), this);

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
      shellDelete(delList, true);
      updateProblemsButton();
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
      displayModInformation(sourceIdx.row());
      // workaround to cancel the editor that might have opened because of
      // selection-click
      ui->modList->closePersistentEditor(index);
    }
    catch (const std::exception &e) {
      reportError(e.what());
    }
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
      qDebug("change categories on: %s", qPrintable(idx.data().toString()));
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
      qDebug("%s saved", qPrintable(QDir::toNativeSeparators(m_OrganizerCore.currentProfile()->getArchivesFileName())));
    }
  } else {
    qWarning("archive list not initialised");
  }
}

void MainWindow::checkModsForUpdates()
{
  statusBar()->show();
  if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->loggedIn()) {
    m_ModsToUpdate = ModInfo::checkAllForUpdate(&m_PluginContainer, this);
    m_RefreshProgress->setRange(0, m_ModsToUpdate);
  } else {
    QString username, password;
    if (m_OrganizerCore.settings().getNexusLogin(username, password)) {
      m_OrganizerCore.doAfterLogin([this] () { this->checkModsForUpdates(); });
      NexusInterface::instance(&m_PluginContainer)->getAccessManager()->login(username, password);
    } else { // otherwise there will be no endorsement info
      MessageDialog::showMessage(tr("Not logged in, endorsement information will be wrong"),
                                  this, true);
      m_ModsToUpdate = ModInfo::checkAllForUpdate(&m_PluginContainer, this);
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
	::ShellExecuteW(nullptr, L"explore", ToWString(m_OrganizerCore.settings().getBaseDirectory()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::openLogsFolder()
{
	QString logsPath = qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::logPath());
	::ShellExecuteW(nullptr, L"explore", ToWString(logsPath).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::openInstallFolder()
{
	::ShellExecuteW(nullptr, L"explore", ToWString(qApp->applicationDirPath()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::openPluginsFolder()
{
	QString pluginsPath = QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::pluginPath());
	::ShellExecuteW(nullptr, L"explore", ToWString(pluginsPath).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}


void MainWindow::openProfileFolder()
{
	::ShellExecuteW(nullptr, L"explore", ToWString(m_OrganizerCore.currentProfile()->absolutePath()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::openDownloadsFolder()
{
	::ShellExecuteW(nullptr, L"explore", ToWString(m_OrganizerCore.settings().getDownloadDirectory()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::openModsFolder()
{
  ::ShellExecuteW(nullptr, L"explore", ToWString(m_OrganizerCore.settings().getModDirectory()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::openGameFolder()
{
	::ShellExecuteW(nullptr, L"explore", ToWString(m_OrganizerCore.managedGame()->gameDirectory().absolutePath()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::openMyGamesFolder()
{
	::ShellExecuteW(nullptr, L"explore", ToWString(m_OrganizerCore.managedGame()->documentsDirectory().absolutePath()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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

QMenu *MainWindow::modListContextMenu()
{
  QMenu *menu = new QMenu(this);
  menu->addAction(tr("Install Mod..."), this, SLOT(installMod_clicked()));

  menu->addAction(tr("Create empty mod"), this, SLOT(createEmptyMod_clicked()));

  menu->addSeparator();

  menu->addAction(tr("Enable all visible"), this, SLOT(enableVisibleMods()));
  menu->addAction(tr("Disable all visible"), this, SLOT(disableVisibleMods()));

  menu->addAction(tr("Check all for update"), this, SLOT(checkModsForUpdates()));

  menu->addAction(tr("Refresh"), &m_OrganizerCore, SLOT(profileRefresh()));

  menu->addAction(tr("Export to csv..."), this, SLOT(exportModListCSV()));


  return menu;
}

void MainWindow::on_modList_customContextMenuRequested(const QPoint &pos)
{
  try {
    QTreeView *modList = findChild<QTreeView*>("modList");

    m_ContextIdx = mapToModel(m_OrganizerCore.modList(), modList->indexAt(pos));
    m_ContextRow = m_ContextIdx.row();

    QMenu *menu = nullptr;
    QMenu *allMods = modListContextMenu();
    if (m_ContextRow == -1) {
      // no selection
      menu = allMods;
    } else {
      menu = new QMenu(this);
      allMods->setTitle(tr("All Mods"));
      menu->addMenu(allMods);
      ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
      std::vector<ModInfo::EFlag> flags = info->getFlags();
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
        if (QDir(info->absolutePath()).count() > 2) {
          menu->addAction(tr("Sync to Mods..."), &m_OrganizerCore, SLOT(syncOverwrite()));
          menu->addAction(tr("Create Mod..."), this, SLOT(createModFromOverwrite()));
          menu->addAction(tr("Clear Overwrite..."), this, SLOT(clearOverwrite()));
        }
        menu->addAction(tr("Open in Explorer"), this, SLOT(openExplorer_clicked()));
      } else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end()) {
        menu->addAction(tr("Restore Backup"), this, SLOT(restoreBackup_clicked()));
        menu->addAction(tr("Remove Backup..."), this, SLOT(removeMod_clicked()));
      } else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) != flags.end()) {
        // nop, nothing to do with this mod
      } else {
        QMenu *addRemoveCategoriesMenu = new QMenu(tr("Change Categories"));
        populateMenuCategories(addRemoveCategoriesMenu, 0);
        connect(addRemoveCategoriesMenu, SIGNAL(aboutToHide()), this, SLOT(addRemoveCategories_MenuHandler()));
        addMenuAsPushButton(menu, addRemoveCategoriesMenu);

		//Removed as it was redundant, just making the categories look more complicated.
		/*
        QMenu *replaceCategoriesMenu = new QMenu(tr("Replace Categories"));
        populateMenuCategories(replaceCategoriesMenu, 0);
        connect(replaceCategoriesMenu, SIGNAL(aboutToHide()), this, SLOT(replaceCategories_MenuHandler()));
        addMenuAsPushButton(menu, replaceCategoriesMenu);
		*/

        QMenu *primaryCategoryMenu = new QMenu(tr("Primary Category"));
        connect(primaryCategoryMenu, SIGNAL(aboutToShow()), this, SLOT(addPrimaryCategoryCandidates()));
        addMenuAsPushButton(menu, primaryCategoryMenu);

        menu->addSeparator();
        if (info->downgradeAvailable()) {
          menu->addAction(tr("Change versioning scheme"), this, SLOT(changeVersioningScheme()));
        }
        if (info->updateAvailable() || info->downgradeAvailable()) {
          if (info->updateIgnored()) {
            menu->addAction(tr("Un-ignore update"), this, SLOT(unignoreUpdate()));
          } else {
            menu->addAction(tr("Ignore update"), this, SLOT(ignoreUpdate()));
          }
        }

        menu->addSeparator();

        menu->addAction(tr("Enable selected"), this, SLOT(enableSelectedMods_clicked()));
        menu->addAction(tr("Disable selected"), this, SLOT(disableSelectedMods_clicked()));

        menu->addSeparator();

        menu->addAction(tr("Rename Mod..."), this, SLOT(renameMod_clicked()));
        menu->addAction(tr("Reinstall Mod"), this, SLOT(reinstallMod_clicked()));
		    menu->addAction(tr("Remove Mod..."), this, SLOT(removeMod_clicked()));

		    menu->addSeparator();

        if (info->getNexusID() > 0) {
          switch (info->endorsedState()) {
            case ModInfo::ENDORSED_TRUE: {
              menu->addAction(tr("Un-Endorse"), this, SLOT(unendorse_clicked()));
            } break;
            case ModInfo::ENDORSED_FALSE: {
              menu->addAction(tr("Endorse"), this, SLOT(endorse_clicked()));
              menu->addAction(tr("Won't endorse"), this, SLOT(dontendorse_clicked()));
            } break;
            case ModInfo::ENDORSED_NEVER: {
              menu->addAction(tr("Endorse"), this, SLOT(endorse_clicked()));
            } break;
            default: {
              QAction *action = new QAction(tr("Endorsement state unknown"), menu);
              action->setEnabled(false);
              menu->addAction(action);
            } break;
          }
        }

		menu->addSeparator();

        std::vector<ModInfo::EFlag> flags = info->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
          menu->addAction(tr("Ignore missing data"), this, SLOT(ignoreMissingData_clicked()));
        }

        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_ALTERNATE_GAME) != flags.end()) {
          menu->addAction(tr("Mark as converted/working"), this, SLOT(markConverted_clicked()));
        }

        if (info->getNexusID() > 0)  {
          menu->addAction(tr("Visit on Nexus"), this, SLOT(visitOnNexus_clicked()));
        } else if ((info->getURL() != "")) {
          menu->addAction(tr("Visit web page"), this, SLOT(visitWebPage_clicked()));
        }

        menu->addAction(tr("Open in Explorer"), this, SLOT(openExplorer_clicked()));
      }

      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) == flags.end()) {
        QAction *infoAction = menu->addAction(tr("Information..."), this, SLOT(information_clicked()));
        menu->setDefaultAction(infoAction);
      }
    }

    menu->exec(modList->mapToGlobal(pos));
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
  //ui->clearFiltersButton->setStyleSheet("border:5px solid #ff0000;");
  ui->clearFiltersButton->setVisible(categories.size() > 0 || content.size() > 0);

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
  bool oldDisplayForeign(settings.displayForeign());
  bool proxy = settings.useProxy();

  settings.query(&m_PluginContainer, this);

  InstallationManager *instManager = m_OrganizerCore.installationManager();
  instManager->setModsDirectory(settings.getModDirectory());
  instManager->setDownloadDirectory(settings.getDownloadDirectory());

  fixCategories();
  refreshFilters();

  if (settings.getProfileDirectory() != oldProfilesDirectory) {
    refreshProfiles();
  }

  DownloadManager *dlManager = m_OrganizerCore.downloadManager();

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

  NexusInterface::instance(&m_PluginContainer)->setNMMVersion(settings.getNMMVersion());

  updateDownloadListDelegate();

  m_OrganizerCore.updateVFSParams(settings.logLevel(), settings.crashDumpsType());
  m_OrganizerCore.cycleDiagnostics();
}


void MainWindow::on_actionNexus_triggered()
{
  QDesktopServices::openUrl(QUrl(NexusInterface::instance(&m_PluginContainer)->getGameURL(m_OrganizerCore.managedGame()->gameShortName())));
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
    if ((m_CurrentLanguage != "en-US")
        && (m_CurrentLanguage != "en_US")
        && (m_CurrentLanguage != "en-GB")) {
      qDebug("localization file %s not found", qPrintable(fileName));
    } // we don't actually expect localization files for english
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
  qDebug("loaded language %s", qPrintable(newLanguage));

  ui->profileBox->setItemText(0, QObject::tr("<Manage...>"));

  createHelpWidget();

  updateDownloadListDelegate();
  updateProblemsButton();

  ui->listOptionsBtn->setMenu(modListContextMenu());

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
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly)) {
    reportError(tr("failed to write to file %1").arg(fileName));
  }

  writeDataToFile(file, "data", *m_OrganizerCore.directoryStructure());
  file.close();

  MessageDialog::showMessage(tr("%1 written").arg(QDir::toNativeSeparators(fileName)), this);
}


int MainWindow::getBinaryExecuteInfo(const QFileInfo &targetInfo, QFileInfo &binaryInfo, QString &arguments)
{
  QString extension = targetInfo.suffix();
  if ((extension.compare("cmd", Qt::CaseInsensitive) == 0) ||
      (extension.compare("com", Qt::CaseInsensitive) == 0) ||
      (extension.compare("bat", Qt::CaseInsensitive) == 0)) {
    binaryInfo = QFileInfo("C:\\Windows\\System32\\cmd.exe");
    arguments = QString("/C \"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
    return 1;
  } else if (extension.compare("exe", Qt::CaseInsensitive) == 0) {
    binaryInfo = targetInfo;
    return 1;
  } else if (extension.compare("jar", Qt::CaseInsensitive) == 0) {
    // types that need to be injected into
    std::wstring targetPathW = ToWString(targetInfo.absoluteFilePath());
    QString binaryPath;

    { // try to find java automatically
      WCHAR buffer[MAX_PATH];
      if (::FindExecutableW(targetPathW.c_str(), nullptr, buffer) > (HINSTANCE)32) {
        DWORD binaryType = 0UL;
        if (!::GetBinaryTypeW(buffer, &binaryType)) {
          qDebug("failed to determine binary type of \"%ls\": %lu", buffer, ::GetLastError());
        } else if (binaryType == SCS_32BIT_BINARY) {
          binaryPath = ToQString(buffer);
        }
      }
    }
    if (binaryPath.isEmpty() && (extension == "jar")) {
      // second attempt: look to the registry
      QSettings javaReg("HKEY_LOCAL_MACHINE\\Software\\JavaSoft\\Java Runtime Environment", QSettings::NativeFormat);
      if (javaReg.contains("CurrentVersion")) {
        QString currentVersion = javaReg.value("CurrentVersion").toString();
        binaryPath = javaReg.value(QString("%1/JavaHome").arg(currentVersion)).toString().append("\\bin\\javaw.exe");
      }
    }
    if (binaryPath.isEmpty()) {
      binaryPath = QFileDialog::getOpenFileName(this, tr("Select binary"), QString(), tr("Binary") + " (*.exe)");
    }
    if (binaryPath.isEmpty()) {
      return 0;
    }
    binaryInfo = QFileInfo(binaryPath);
    if (extension == "jar") {
      arguments = QString("-jar \"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
    } else {
      arguments = QString("\"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
    }
    return 1;
  } else {
    return 2;
  }
}


void MainWindow::addAsExecutable()
{
  if (m_ContextItem != nullptr) {
    QFileInfo targetInfo(m_ContextItem->data(0, Qt::UserRole).toString());
    QFileInfo binaryInfo;
    QString arguments;
    switch (getBinaryExecuteInfo(targetInfo, binaryInfo, arguments)) {
      case 1: {
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
      } break;
      case 2: {
        QMessageBox::information(this, tr("Not an executable"), tr("This is not a recognized executable."));
      } break;
      default: {
        // nop
      } break;
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


void MainWindow::enableSelectedMods_clicked()
{
  m_OrganizerCore.modList()->enableSelected(ui->modList->selectionModel());
}


void MainWindow::disableSelectedMods_clicked()
{
  m_OrganizerCore.modList()->disableSelected(ui->modList->selectionModel());
}


void MainWindow::previewDataFile()
{
  QString fileName = QDir::fromNativeSeparators(m_ContextItem->data(0, Qt::UserRole).toString());

  // what we have is an absolute path to the file in its actual location (for the primary origin)
  // what we want is the path relative to the virtual data directory

  // we need to look in the virtual directory for the file to make sure the info is up to date.

  // check if the file comes from the actual data folder instead of a mod
  QDir gameDirectory = m_OrganizerCore.managedGame()->dataDirectory().absolutePath();
  QString relativePath = gameDirectory.relativeFilePath(fileName);
  QDir dirRelativePath = gameDirectory.relativeFilePath(fileName);
  // if the file is on a different drive the dirRelativePath will actually be an absolute path so we make sure that is not the case
  if (!dirRelativePath.isAbsolute() && !relativePath.startsWith("..")) {
	  fileName = relativePath;
  }
  else {
	  // crude: we search for the next slash after the base mod directory to skip everything up to the data-relative directory
	  int offset = m_OrganizerCore.settings().getModDirectory().size() + 1;
	  offset = fileName.indexOf("/", offset);
	  fileName = fileName.mid(offset + 1);
  }



  const FileEntry::Ptr file = m_OrganizerCore.directoryStructure()->searchFile(ToWString(fileName), nullptr);

  if (file.get() == nullptr) {
    reportError(tr("file not found: %1").arg(fileName));
    return;
  }

  // set up preview dialog
  PreviewDialog preview(fileName);
  auto addFunc = [&] (int originId) {
      FilesOrigin &origin = m_OrganizerCore.directoryStructure()->getOriginByID(originId);
      QString filePath = QDir::fromNativeSeparators(ToQString(origin.getPath())) + "/" + fileName;
      if (QFile::exists(filePath)) {
        // it's very possible the file doesn't exist, because it's inside an archive. we don't support that
        QWidget *wid = m_PluginContainer.previewGenerator().genPreview(filePath);
        if (wid == nullptr) {
          reportError(tr("failed to generate preview for %1").arg(filePath));
        } else {
          preview.addVariant(ToQString(origin.getName()), wid);
        }
      }
    };

  addFunc(file->getOrigin());
  for (auto alt : file->getAlternatives()) {
    addFunc(alt.first);
  }
  if (preview.numVariants() > 0) {
    preview.exec();
  } else {
    QMessageBox::information(this, tr("Sorry"), tr("Sorry, can't preview anything. This function currently does not support extracting from bsas."));
  }
}

void MainWindow::openDataFile()
{
  if (m_ContextItem != nullptr) {
    QFileInfo targetInfo(m_ContextItem->data(0, Qt::UserRole).toString());
    QFileInfo binaryInfo;
    QString arguments;
    switch (getBinaryExecuteInfo(targetInfo, binaryInfo, arguments)) {
      case 1: {
        m_OrganizerCore.spawnBinaryDirect(
            binaryInfo, arguments, m_OrganizerCore.currentProfile()->name(),
            targetInfo.absolutePath(), "", "");
      } break;
      case 2: {
        ::ShellExecuteW(nullptr, L"open",
                        ToWString(targetInfo.absoluteFilePath()).c_str(),
                        nullptr, nullptr, SW_SHOWNORMAL);
      } break;
      default: {
        // nop
      } break;
    }
  }
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

  ui->actionEndorseMO->setVisible(false);
}


void MainWindow::notEndorsedYet()
{
  ui->actionEndorseMO->setVisible(true);
}


void MainWindow::on_dataTree_customContextMenuRequested(const QPoint &pos)
{
  QTreeWidget *dataTree = findChild<QTreeWidget*>("dataTree");
  m_ContextItem = dataTree->itemAt(pos.x(), pos.y());

  QMenu menu;
  if ((m_ContextItem != nullptr) && (m_ContextItem->childCount() == 0)) {
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
  refreshDataTree();
}


void MainWindow::on_actionUpdate_triggered()
{
  m_OrganizerCore.startMOUpdate();
}


void MainWindow::on_actionEndorseMO_triggered()
{
  // Normally this would be the managed game but MO2 is only uploaded to the Skyrim SE site right now
  IPluginGame * game = m_OrganizerCore.getGame("skyrimse");
  if (!game) return;

  if (QMessageBox::question(this, tr("Endorse Mod Organizer"),
                            tr("Do you want to endorse Mod Organizer on %1 now?").arg(
                                      NexusInterface::instance(&m_PluginContainer)->getGameURL(game->gameShortName())),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    NexusInterface::instance(&m_PluginContainer)->requestToggleEndorsement(
      game->gameShortName(), game->nexusModOrganizerID(), true, this, QVariant(), QString());
  }
}


void MainWindow::updateDownloadListDelegate()
{
  if (m_OrganizerCore.settings().compactDownloads()) {
    ui->downloadView->setItemDelegate(
          new DownloadListWidgetCompactDelegate(m_OrganizerCore.downloadManager(),
                                                m_OrganizerCore.settings().metaDownloads(),
                                                ui->downloadView,
                                                ui->downloadView));
  } else {
    ui->downloadView->setItemDelegate(
          new DownloadListWidgetDelegate(m_OrganizerCore.downloadManager(),
                                         m_OrganizerCore.settings().metaDownloads(),
                                         ui->downloadView,
                                         ui->downloadView));
  }

  DownloadListSortProxy *sortProxy = new DownloadListSortProxy(m_OrganizerCore.downloadManager(), ui->downloadView);
  sortProxy->setSourceModel(new DownloadList(m_OrganizerCore.downloadManager(), ui->downloadView));
  connect(ui->downloadFilterEdit, SIGNAL(textChanged(QString)), sortProxy, SLOT(updateFilter(QString)));
  connect(ui->downloadFilterEdit, SIGNAL(textChanged(QString)), this, SLOT(downloadFilterChanged(QString)));

  ui->downloadView->setModel(sortProxy);
  //ui->downloadView->sortByColumn(1, Qt::DescendingOrder);
  ui->downloadView->header()->resizeSections(QHeaderView::Stretch);

  connect(ui->downloadView->itemDelegate(), SIGNAL(installDownload(int)), &m_OrganizerCore, SLOT(installDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(queryInfo(int)), m_OrganizerCore.downloadManager(), SLOT(queryInfo(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(visitOnNexus(int)), m_OrganizerCore.downloadManager(), SLOT(visitOnNexus(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(openFile(int)), m_OrganizerCore.downloadManager(), SLOT(openFile(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(openInDownloadsFolder(int)), m_OrganizerCore.downloadManager(), SLOT(openInDownloadsFolder(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(removeDownload(int, bool)), m_OrganizerCore.downloadManager(), SLOT(removeDownload(int, bool)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(restoreDownload(int)), m_OrganizerCore.downloadManager(), SLOT(restoreDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(cancelDownload(int)), m_OrganizerCore.downloadManager(), SLOT(cancelDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(pauseDownload(int)), m_OrganizerCore.downloadManager(), SLOT(pauseDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(resumeDownload(int)), this, SLOT(resumeDownload(int)));
}


void MainWindow::modDetailsUpdated(bool)
{
  if (--m_ModsToUpdate == 0) {
    statusBar()->hide();
    m_ModListSortProxy->setCategoryFilter(boost::assign::list_of(CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE));
    for (int i = 0; i < ui->categoriesList->topLevelItemCount(); ++i) {
      if (ui->categoriesList->topLevelItem(i)->data(0, Qt::UserRole) == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
        ui->categoriesList->setCurrentItem(ui->categoriesList->topLevelItem(i));
        break;
      }
    }
    m_RefreshProgress->setVisible(false);
  } else {
    m_RefreshProgress->setValue(m_RefreshProgress->maximum() - m_ModsToUpdate);
  }
}

void MainWindow::nxmUpdatesAvailable(const std::vector<int> &modIDs, QVariant userData, QVariant resultData, int)
{
  m_ModsToUpdate -= static_cast<int>(modIDs.size());
  QVariantList resultList = resultData.toList();
  for (auto iter = resultList.begin(); iter != resultList.end(); ++iter) {
    QVariantMap result = iter->toMap();
    // Normally this would be the managed game but MO2 is only uploaded to the Skyrim SE site right now
    IPluginGame * game = m_OrganizerCore.getGame("skyrimse");
    if (game
          && result["id"].toInt() == game->nexusModOrganizerID()
          && result["game_id"].toInt() == game->nexusGameID()) {
      if (!result["voted_by_user"].toBool()) {
        ui->actionEndorseMO->setVisible(true);
      }
    } else {
      QString gameName = m_OrganizerCore.managedGame()->gameShortName();
      bool sameNexus = false;
      for (IPluginGame *game : m_PluginContainer.plugins<IPluginGame>()) {
        if (game->nexusGameID() == result["game_id"].toInt()) {
          gameName = game->gameShortName();
          if (game->nexusGameID() == m_OrganizerCore.managedGame()->nexusGameID())
            sameNexus = true;
          break;
        }
      }
      std::vector<ModInfo::Ptr> info = ModInfo::getByModID(gameName, result["id"].toInt());
      if (sameNexus) {
        std::vector<ModInfo::Ptr> mainInfo = ModInfo::getByModID(m_OrganizerCore.managedGame()->gameShortName(), result["id"].toInt());
        info.reserve(info.size() + mainInfo.size());
        info.insert(info.end(), mainInfo.begin(), mainInfo.end());
      }
      for (auto iter = info.begin(); iter != info.end(); ++iter) {
        (*iter)->setNewestVersion(result["version"].toString());
        (*iter)->setNexusDescription(result["description"].toString());
        if (NexusInterface::instance(&m_PluginContainer)->getAccessManager()->loggedIn() &&
            result.contains("voted_by_user")) {
          // don't use endorsement info if we're not logged in or if the response doesn't contain it
          (*iter)->setIsEndorsed(result["voted_by_user"].toBool());
        }
      }
    }
  }

  if (m_ModsToUpdate <= 0) {
    statusBar()->hide();
    m_ModListSortProxy->setCategoryFilter(boost::assign::list_of(CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE));
    for (int i = 0; i < ui->categoriesList->topLevelItemCount(); ++i) {
      if (ui->categoriesList->topLevelItem(i)->data(0, Qt::UserRole) == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
        ui->categoriesList->setCurrentItem(ui->categoriesList->topLevelItem(i));
        break;
      }
    }
  } else {
    m_RefreshProgress->setValue(m_RefreshProgress->maximum() - m_ModsToUpdate);
  }
}

void MainWindow::nxmEndorsementToggled(QString, int, QVariant, QVariant resultData, int)
{
  if (resultData.toBool()) {
    ui->actionEndorseMO->setVisible(false);
    QMessageBox::information(this, tr("Thank you!"), tr("Thank you for your endorsement!"));
  }

  if (!disconnect(sender(), SIGNAL(nxmEndorsementToggled(QString, int, QVariant, QVariant, int)),
             this, SLOT(nxmEndorsementToggled(QString, int, QVariant, QVariant, int)))) {
    qCritical("failed to disconnect endorsement slot");
  }
}

void MainWindow::nxmDownloadURLs(QString, int, int, QVariant, QVariant resultData, int)
{
  QVariantList serverList = resultData.toList();

  QList<ServerInfo> servers;
  for (const QVariant &server : serverList) {
    QVariantMap serverInfo = server.toMap();
    ServerInfo info;
    info.name = serverInfo["Name"].toString();
    info.premium = serverInfo["IsPremium"].toBool();
    info.lastSeen = QDate::currentDate();
    info.preferred = 0;
    // other keys: ConnectedUsers, Country, URI
    servers.append(info);
  }
  m_OrganizerCore.settings().updateServers(servers);
}


void MainWindow::nxmRequestFailed(QString, int modID, int, QVariant, int, const QString &errorString)
{
  if (modID == -1) {
    // must be the update-check that failed
    m_ModsToUpdate = 0;
    statusBar()->hide();
  }
  MessageDialog::showMessage(tr("Request to Nexus failed: %1").arg(errorString), this);
}


BSA::EErrorCode MainWindow::extractBSA(BSA::Archive &archive, BSA::Folder::Ptr folder, const QString &destination,
                           QProgressDialog &progress)
{
  QDir().mkdir(destination);
  BSA::EErrorCode result = BSA::ERROR_NONE;
  QString errorFile;

  for (unsigned int i = 0; i < folder->getNumFiles(); ++i) {
    BSA::File::Ptr file = folder->getFile(i);
    BSA::EErrorCode res = archive.extract(file, destination.toUtf8().constData());
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

void MainWindow::on_actionProblems_triggered()
{
  ProblemsDialog problems(m_PluginContainer.plugins<IPluginDiagnose>(), this);
  if (problems.hasProblems()) {
    problems.exec();
    updateProblemsButton();
  }
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
  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
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
      m_OrganizerCore.pluginList()->lockESPIndex(mapToModel(m_OrganizerCore.pluginList(), idx).row(), locked);
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


void MainWindow::toolBar_customContextMenuRequested(const QPoint &point)
{
  QAction *action = ui->toolBar->actionAt(point);
  if (action != nullptr) {
    if (action->objectName().startsWith("custom_")) {
      m_ContextAction = action;
      QMenu menu;
      menu.addAction(tr("Remove"), this, SLOT(removeFromToolbar()));
      menu.exec(ui->toolBar->mapToGlobal(point));
    }
  }
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

  menu.addSeparator();

  if (hasLocked) {
    menu.addAction(tr("Unlock load order"), this, SLOT(unlockESPIndex()));
  }
  if (hasUnlocked) {
    menu.addAction(tr("Lock load order"), this, SLOT(lockESPIndex()));
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
    qWarning("invalid source file %s", qPrintable(file.absoluteFilePath()));
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
    qCritical("file operation failed: %s", qPrintable(windowsErrorString(::GetLastError())));
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
	deselectFilters();
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
