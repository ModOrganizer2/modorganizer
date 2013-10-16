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
#include <archive.h>
#include "spawn.h"
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
#include "utility.h"
#include "modinfodialog.h"
#include "overwriteinfodialog.h"
#include "activatemodsdialog.h"
#include "downloadlist.h"
#include "downloadlistwidget.h"
#include "downloadlistwidgetcompact.h"
#include "messagedialog.h"
#include "installationmanager.h"
#include "lockeddialog.h"
#include "syncoverwritedialog.h"
#include "logbuffer.h"
#include "downloadlistsortproxy.h"
#include "motddialog.h"
#include "filedialogmemory.h"
#include "questionboxmemory.h"
#include "tutorialmanager.h"
#include "icondelegate.h"
#include "credentialsdialog.h"
#include "selectiondialog.h"
#include "csvbuilder.h"
#include "gameinfoimpl.h"
#include "savetextasdialog.h"
#include "problemsdialog.h"
#include <gameinfo.h>
#include <appconfig.h>
#include <utility.h>
#include <ipluginproxy.h>
#include <map>
#include <ctime>
#include <QTime>
#include <util.h>
#include <QInputDialog>
#include <QSettings>
#include <QWhatsThis>
#include <wchar.h>
#include <sstream>
#include <QProcess>
#include <QMenu>
#include <QBuffer>
#include <QInputDialog>
#include <QDirIterator>
#include <QHelpEvent>
#include <QToolTip>
#include <QFileDialog>
#include <QTimer>
#include <QMessageBox>
#include <QDebug>
#include <QBuffer>
#include <QWidgetAction>
#include <QToolButton>
#include <QGraphicsObject>
#include <QPluginLoader>
#include <QRadioButton>
#include <QDesktopWidget>
#include <QtPlugin>
#include <QIdentityProxyModel>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/assign.hpp>
#include <Psapi.h>
#include <shlobj.h>
#include <ShellAPI.h>
#include <TlHelp32.h>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QtConcurrentRun>
#include <QCoreApplication>


#ifdef TEST_MODELS
#include "modeltest.h"
#endif // TEST_MODELS

#pragma warning( disable : 4428 )

using namespace MOBase;
using namespace MOShared;




static bool isOnline()
{
  QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

  bool connected = false;
  for (auto iter = interfaces.begin(); iter != interfaces.end() && !connected; ++iter) {
    if ( (iter->flags() & QNetworkInterface::IsUp) &&
         (iter->flags() & QNetworkInterface::IsRunning) &&
        !(iter->flags() & QNetworkInterface::IsLoopBack)) {
      auto addresses = iter->addressEntries();
      if (addresses.count() == 0) {
        continue;
      }
      qDebug("interface %s seems to be up (address: %s)",
             qPrintable(iter->humanReadableName()),
             qPrintable(addresses[0].ip().toString()));
      connected = true;
    }
  }

  return connected;
}


MainWindow::MainWindow(const QString &exeName, QSettings &initSettings, QWidget *parent)
  : QMainWindow(parent), ui(new Ui::MainWindow), m_Tutorial(this, "MainWindow"),
    m_ExeName(exeName), m_OldProfileIndex(-1),
    m_DirectoryStructure(new DirectoryEntry(L"data", NULL, 0)),
    m_ModList(this), m_ModListGroupingProxy(NULL), m_ModListSortProxy(NULL),
    m_PluginList(this), m_OldExecutableIndex(-1), m_GamePath(ToQString(GameInfo::instance().getGameDirectory())),
    m_DownloadManager(NexusInterface::instance(), this),
    m_InstallationManager(this), m_Translator(NULL), m_TranslatorQt(NULL),
    m_Updater(NexusInterface::instance(), this), m_CategoryFactory(CategoryFactory::instance()),
    m_CurrentProfile(NULL), m_AskForNexusPW(false), m_LoginAttempted(false),
    m_ArchivesInit(false), m_ContextItem(NULL), m_ContextAction(NULL), m_CurrentSaveView(NULL),
    m_GameInfo(new GameInfoImpl()), m_AboutToRun()
{
  ui->setupUi(this);
  this->setWindowTitle(ToQString(GameInfo::instance().getGameName()) + " Mod Organizer v" + m_Updater.getVersion().displayString());

  m_RefreshProgress = new QProgressBar(statusBar());
  m_RefreshProgress->setTextVisible(true);
  m_RefreshProgress->setRange(0, 100);
  m_RefreshProgress->setValue(0);
  statusBar()->addWidget(m_RefreshProgress, 1000);
  statusBar()->clearMessage();

  ui->actionEndorseMO->setVisible(false);

  updateProblemsButton();

  updateToolBar();

  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure);

  // set up mod list
  m_ModListSortProxy = new ModListSortProxy(m_CurrentProfile, this);
  m_ModListSortProxy->setSourceModel(&m_ModList);

#ifdef TEST_MODELS
  new ModelTest(&m_ModList, this);
  new ModelTest(m_ModListSortProxy, this);
#endif //TEST_MODELS

  ui->modList->setModel(m_ModListSortProxy);

  ui->modList->sortByColumn(ModList::COL_PRIORITY, Qt::AscendingOrder);
  ui->modList->setItemDelegateForColumn(ModList::COL_FLAGS, new IconDelegate(ui->modList));
  //ui->modList->setAcceptDrops(true);
  ui->modList->header()->installEventFilter(&m_ModList);
  if (initSettings.contains("mod_list_state")) {
    ui->modList->header()->restoreState(initSettings.value("mod_list_state").toByteArray());
  }
  ui->modList->header()->setSectionHidden(0, false); // prevent the name-column from being hidden
  ui->modList->installEventFilter(&m_ModList);

  // set up plugin list
  m_PluginListSortProxy = new PluginListSortProxy(this);
  m_PluginListSortProxy->setSourceModel(&m_PluginList);

  ui->espList->setModel(m_PluginListSortProxy);
  ui->espList->sortByColumn(PluginList::COL_PRIORITY, Qt::AscendingOrder);
  if (initSettings.contains("plugin_list_state")) {
    ui->espList->header()->restoreState(initSettings.value("plugin_list_state").toByteArray());
  }
  ui->espList->installEventFilter(&m_PluginList);

  resizeLists(initSettings.contains("mod_list_state"), initSettings.contains("plugin_list_state"));


  QMenu *linkMenu = new QMenu(this);
  linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Toolbar"), this, SLOT(linkToolbar()));
  linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Desktop"), this, SLOT(linkDesktop()));
  linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Start Menu"), this, SLOT(linkMenu()));
  ui->linkButton->setMenu(linkMenu);

  m_DownloadManager.setOutputDirectory(m_Settings.getDownloadDirectory());
  m_DownloadManager.setPreferredServers(m_Settings.getPreferredServers());

  NexusInterface::instance()->setCacheDirectory(m_Settings.getCacheDirectory());
  NexusInterface::instance()->setNMMVersion(m_Settings.getNMMVersion());

  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());

  updateDownloadListDelegate();

  ui->savegameList->installEventFilter(this);
  ui->savegameList->setMouseTracking(true);

  connect(&m_DownloadManager, SIGNAL(showMessage(QString)), this, SLOT(showMessage(QString)));
  connect(&m_DownloadManager, SIGNAL(downloadSpeed(QString,int)), this, SLOT(downloadSpeed(QString,int)));

  connect(ui->savegameList, SIGNAL(itemEntered(QListWidgetItem*)), this, SLOT(saveSelectionChanged(QListWidgetItem*)));

  connect(&m_ModList, SIGNAL(modorder_changed()), this, SLOT(modorder_changed()));
  connect(&m_ModList, SIGNAL(removeOrigin(QString)), this, SLOT(removeOrigin(QString)));
  connect(&m_ModList, SIGNAL(showMessage(QString)), this, SLOT(showMessage(QString)));
  connect(&m_ModList, SIGNAL(modRenamed(QString,QString)), this, SLOT(modRenamed(QString,QString)));
  connect(&m_ModList, SIGNAL(modUninstalled(QString)), this, SLOT(modRemoved(QString)));
  connect(&m_ModList, SIGNAL(modlist_changed(QModelIndex, int)), this, SLOT(modlistChanged(QModelIndex, int)));
  connect(&m_ModList, SIGNAL(removeSelectedMods()), this, SLOT(removeMod_clicked()));
  connect(&m_ModList, SIGNAL(requestColumnSelect(QPoint)), this, SLOT(displayColumnSelection(QPoint)));
  connect(&m_ModList, SIGNAL(fileMoved(QString, QString, QString)), this, SLOT(fileMoved(QString, QString, QString)));
  connect(ui->modList, SIGNAL(dropModeUpdate(bool)), &m_ModList, SLOT(dropModeUpdate(bool)));
  connect(m_ModListSortProxy, SIGNAL(filterActive(bool)), this, SLOT(modFilterActive(bool)));
  connect(ui->modFilterEdit, SIGNAL(textChanged(QString)), m_ModListSortProxy, SLOT(updateFilter(QString)));

  connect(ui->espFilterEdit, SIGNAL(textChanged(QString)), m_PluginListSortProxy, SLOT(updateFilter(QString)));
  connect(ui->espFilterEdit, SIGNAL(textChanged(QString)), this, SLOT(espFilterChanged(QString)));
  connect(&m_PluginList, SIGNAL(saveTimer()), this, SLOT(savePluginList()));

  connect(ui->bsaWarning, SIGNAL(linkActivated(QString)), this, SLOT(linkClicked(QString)));

  connect(&m_DirectoryRefresher, SIGNAL(refreshed()), this, SLOT(directory_refreshed()));
  connect(&m_DirectoryRefresher, SIGNAL(progress(int)), this, SLOT(refresher_progress(int)));
  connect(&m_DirectoryRefresher, SIGNAL(error(QString)), this, SLOT(showError(QString)));

  connect(&m_Settings, SIGNAL(languageChanged(QString)), this, SLOT(languageChange(QString)));
  connect(&m_Settings, SIGNAL(styleChanged(QString)), this, SIGNAL(styleChanged(QString)));

  connect(&m_Updater, SIGNAL(restart()), this, SLOT(close()));
  connect(&m_Updater, SIGNAL(updateAvailable()), this, SLOT(updateAvailable()));
  connect(&m_Updater, SIGNAL(motdAvailable(QString)), this, SLOT(motdReceived(QString)));

//  connect(ExitProxy::instance(), SIGNAL(exit()), this, SLOT(close()));

  connect(NexusInterface::instance()->getAccessManager(), SIGNAL(loginSuccessful(bool)), this, SLOT(loginSuccessful(bool)));
  connect(NexusInterface::instance()->getAccessManager(), SIGNAL(loginFailed(QString)), this, SLOT(loginFailed(QString)));
  connect(NexusInterface::instance(), SIGNAL(requestNXMDownload(QString)), this, SLOT(downloadRequestedNXM(QString)));
  connect(NexusInterface::instance(), SIGNAL(nxmDownloadURLsAvailable(int,int,QVariant,QVariant,int)), this, SLOT(nxmDownloadURLs(int,int,QVariant,QVariant,int)));

  connect(&TutorialManager::instance(), SIGNAL(windowTutorialFinished(QString)), this, SLOT(windowTutorialFinished(QString)));

  connect(ui->toolBar, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(toolBar_customContextMenuRequested(QPoint)));

  connect(this, SIGNAL(styleChanged(QString)), this, SLOT(updateStyle(QString)));

  m_CheckBSATimer.setSingleShot(true);
  connect(&m_CheckBSATimer, SIGNAL(timeout()), this, SLOT(checkBSAList()));

  m_DirectoryRefresher.moveToThread(&m_RefresherThread);
  m_RefresherThread.start();

  setCompactDownloads(initSettings.value("compact_downloads", false).toBool());
  m_AskForNexusPW = initSettings.value("ask_for_nexuspw", true).toBool();
  setCategoryListVisible(initSettings.value("categorylist_visible", true).toBool());
  FileDialogMemory::restore(initSettings);

  fixCategories();

  if (isOnline() && !m_Settings.offlineMode()) {
    m_Updater.testForUpdate();
  } else {
    qDebug("user doesn't seem to be connected to the internet");
  }

  m_StartTime = QTime::currentTime();

  m_Tutorial.expose("modList", &m_ModList);
  m_Tutorial.expose("espList", &m_PluginList);
  loadPlugins();
}


MainWindow::~MainWindow()
{
  m_RefresherThread.exit();
  m_RefresherThread.wait();
  delete ui;
  delete m_GameInfo;
  delete m_DirectoryStructure;
}


void MainWindow::resizeLists(bool modListCustom, bool pluginListCustom)
{
  if (!modListCustom) {
    // resize mod list to fit content
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    for (int i = 0; i < ui->modList->header()->count(); ++i) {
      ui->modList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    ui->modList->header()->setSectionResizeMode(ModList::COL_NAME, QHeaderView::Stretch);
#else
    for (int i = 0; i < ui->modList->header()->count(); ++i) {
      ui->modList->header()->setResizeMode(i, QHeaderView::ResizeToContents);
    }
    ui->modList->header()->setResizeMode(ModList::COL_NAME, QHeaderView::Stretch);
#endif
  }

  if (!pluginListCustom) {
    // resize plugin list to fit content
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    for (int i = 0; i < ui->espList->header()->count(); ++i) {
      ui->espList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    ui->espList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
#else
    for (int i = 0; i < ui->espList->header()->count(); ++i) {
      ui->espList->header()->setResizeMode(i, QHeaderView::ResizeToContents);
    }
    ui->espList->header()->setResizeMode(0, QHeaderView::Stretch);
#endif
  }
}


void MainWindow::allowListResize()
{
  // allow resize on mod list
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  for (int i = 0; i < ui->modList->header()->count(); ++i) {
    ui->modList->header()->setSectionResizeMode(i, QHeaderView::Interactive);
  }
  ui->modList->header()->setSectionResizeMode(ui->modList->header()->count() - 1, QHeaderView::Stretch);
#else
  for (int i = 0; i < ui->modList->header()->count(); ++i) {
    ui->modList->header()->setResizeMode(i, QHeaderView::Interactive);
  }
  ui->modList->header()->setResizeMode(ui->modList->header()->count() - 1, QHeaderView::Stretch);
#endif


  // allow resize on plugin list
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  for (int i = 0; i < ui->espList->header()->count(); ++i) {
    ui->espList->header()->setSectionResizeMode(i, QHeaderView::Interactive);
  }
  ui->espList->header()->setSectionResizeMode(ui->espList->header()->count() - 1, QHeaderView::Stretch);
#else
  for (int i = 0; i < ui->espList->header()->count(); ++i) {
    ui->espList->header()->setResizeMode(i, QHeaderView::Interactive);
  }
  ui->espList->header()->setResizeMode(ui->espList->header()->count() - 1, QHeaderView::Stretch);
#endif

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
    if (model == NULL) {
      return QModelIndex();
    }
    const QAbstractProxyModel *proxyModel = qobject_cast<const QAbstractProxyModel*>(model);
    if (proxyModel == NULL) {
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
  foreach (QAction *action, ui->toolBar->actions()) {
    if (action->objectName().startsWith("custom__")) {
      ui->toolBar->removeAction(action);
    }
  }

  QWidget *spacer = new QWidget(ui->toolBar);
  spacer->setObjectName("custom__spacer");
  spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
  QWidget *widget = ui->toolBar->widgetForAction(ui->actionTool);
  QToolButton *toolBtn = qobject_cast<QToolButton*>(widget);

  if (toolBtn->menu() == NULL) {
    actionToToolButton(ui->actionTool);
  }

  actionToToolButton(ui->actionHelp);
  createHelpWidget();

  foreach (QAction *action, ui->toolBar->actions()) {
    if (action->isSeparator()) {
      // insert spacers
      ui->toolBar->insertWidget(action, spacer);

      std::vector<Executable>::iterator begin, end;
      m_ExecutablesList.getExecutables(begin, end);
      for (auto iter = begin; iter != end; ++iter) {
        if (iter->m_Toolbar) {
          QAction *exeAction = new QAction(iconForExecutable(iter->m_BinaryInfo.filePath()),
                                           iter->m_Title,
                                           ui->toolBar);
          QVariant temp;
          temp.setValue(*iter);
          exeAction->setData(temp);
          exeAction->setObjectName(QString("custom__") + iter->m_Title);
          if (!connect(exeAction, SIGNAL(triggered()), this, SLOT(startExeAction()))) {
            qDebug("failed to connect trigger?");
          }
          ui->toolBar->insertAction(action, exeAction);
        }
      }
    }
  }
}


void MainWindow::updateProblemsButton()
{
  if (checkForProblems()) {
    ui->actionProblems->setEnabled(true);
    ui->actionProblems->setIconText(tr("Problems"));
    ui->actionProblems->setToolTip(tr("There are potential problems with your setup"));
  } else {
    ui->actionProblems->setIconText(tr("No Problems"));
    ui->actionProblems->setToolTip(tr("Everything seems to be in order"));
  }
}


bool MainWindow::errorReported(QString &logFile)
{
  QDir dir(ToQString(GameInfo::instance().getLogDir()));
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


bool MainWindow::checkForProblems()
{
  foreach (IPluginDiagnose *diagnose, m_DiagnosisPlugins) {
    std::vector<unsigned int> activeProblems = diagnose->activeProblems();
    if (activeProblems.size() > 0) {
      return true;
    }
  }
  return false;
}


void MainWindow::createHelpWidget()
{
  QToolButton *toolBtn = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionHelp));
  QMenu *buttonMenu = toolBtn->menu();

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
            [](const ActionList::value_type &LHS, const ActionList::value_type &RHS) {
              return LHS.first < RHS.first; } );

  for (auto iter = tutorials.begin(); iter != tutorials.end(); ++iter) {
    connect(iter->second, SIGNAL(triggered()), this, SLOT(tutorialTriggered()));
    tutorialMenu->addAction(iter->second);
  }

  buttonMenu->addMenu(tutorialMenu);
}


void MainWindow::saveArchiveList()
{
  if (m_ArchivesInit) {
    QFile archiveFile(m_CurrentProfile->getArchivesFileName());
    if (archiveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      for (int i = 0; i < ui->bsaList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *tlItem = ui->bsaList->topLevelItem(i);
        for (int j = 0; j < tlItem->childCount(); ++j) {
          QTreeWidgetItem *item = tlItem->child(j);
          if (item->checkState(0) == Qt::Checked) {
            archiveFile.write(item->text(0).toUtf8().append("\r\n"));
          }
        }
      }
    } else {
      reportError(tr("failed to save archives order, do you have write access "
                     "to \"%1\"?").arg(m_CurrentProfile->getArchivesFileName()));
    }
    archiveFile.close();
  } else {
    qWarning("archive list not initialised");
  }
}

void MainWindow::savePluginList()
{
  m_PluginList.saveTo(m_CurrentProfile->getPluginsFileName(),
                      m_CurrentProfile->getLoadOrderFileName(),
                      m_CurrentProfile->getLockedOrderFileName(),
                      m_CurrentProfile->getDeleterFileName(),
                      m_Settings.hideUncheckedPlugins());
  m_PluginList.saveLoadOrder(*m_DirectoryStructure);
}

void MainWindow::modFilterActive(bool active)
{
  if (active) {
    ui->modList->setStyleSheet("QTreeView { border: 2px ridge #f00; }");
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

bool MainWindow::saveCurrentLists()
{
  if (m_DirectoryUpdate) {
    qWarning("not saving lists during directory update");
    return false;
  }

  // save plugin list
  try {
    savePluginList();
  } catch (const std::exception &e) {
    reportError(tr("failed to save load order: %1").arg(e.what()));
  }

  // ensure the archive list exists
  if (!QFile::exists(m_CurrentProfile->getArchivesFileName())) {
    saveArchiveList();
  }

  return true;
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
      if (!m_Settings.directInterface().value("CompletedWindowTutorials/" + windowName, false).toBool()) {
        TutorialManager::instance().activateTutorial(windowName, fileName);
      }
    }
  }
}


void MainWindow::showEvent(QShowEvent *event)
{
  refreshFilters();

  QMainWindow::showEvent(event);
  m_Tutorial.registerControl();

  hookUpWindowTutorials();

  if (m_Settings.directInterface().value("first_start", true).toBool()) {
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

    m_Settings.directInterface().setValue("first_start", false);
  }

  // this has no visible impact when called before the ui is visible
  int grouping = m_Settings.directInterface().value("group_state").toInt();
  ui->groupCombo->setCurrentIndex(grouping);

  allowListResize();
}


void MainWindow::closeEvent(QCloseEvent* event)
{
  if (m_DownloadManager.downloadsInProgress()) {
    if (QMessageBox::question(this, tr("Downloads in progress"),
                          tr("There are still downloads in progress, do you really want to quit?"),
                          QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Cancel) {
      event->ignore();
      return;
    } else {
      m_DownloadManager.pauseAll();
    }
  }

  setCursor(Qt::WaitCursor);

  storeSettings();

  // profile has to be cleaned up before the modinfo-buffer is cleared
  delete m_CurrentProfile;
  m_CurrentProfile = NULL;

  ModInfo::clear();
  LogBuffer::cleanQuit();
  m_ModList.setProfile(NULL);
}


void MainWindow::createFirstProfile()
{
  if (!refreshProfiles(false)) {
    qDebug("creating default profile");
    Profile newProf("Default", false);
    refreshProfiles(false);
  }
}


SaveGameGamebryo *MainWindow::getSaveGame(const QString &name)
{
  return new SaveGameGamebryo(this, name);
}


SaveGameGamebryo *MainWindow::getSaveGame(QListWidgetItem *item)
{
  try {
    SaveGameGamebryo *saveGame = getSaveGame(item->data(Qt::UserRole).toString());
    saveGame->setParent(item->listWidget());
    return saveGame;
  } catch (const std::exception &e) {
    reportError(tr("failed to read savegame: %1").arg(e.what()));
    return NULL;
  }
}


void MainWindow::displaySaveGameInfo(const SaveGameGamebryo *save, QPoint pos)
{
  if (m_CurrentSaveView == NULL) {
    m_CurrentSaveView = new SaveGameInfoWidgetGamebryo(save, &m_PluginList, this);
  } else {
    m_CurrentSaveView->setSave(save);
  }

  QRect screenRect = QApplication::desktop()->availableGeometry(m_CurrentSaveView);

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
  ui->savegameList->activateWindow();
  connect(m_CurrentSaveView, SIGNAL(closeSaveInfo()), this, SLOT(hideSaveGameInfo()));
}


void MainWindow::saveSelectionChanged(QListWidgetItem *newItem)
{
  if (newItem == NULL) {
    hideSaveGameInfo();
  } else if ((m_CurrentSaveView == NULL) || (newItem != m_CurrentSaveView->property("displayItem").value<void*>())) {
    const SaveGameGamebryo *save = getSaveGame(newItem);
    if (save != NULL) {
      displaySaveGameInfo(save, QCursor::pos());
      m_CurrentSaveView->setProperty("displayItem", qVariantFromValue((void*)newItem));
    }
  }
}



void MainWindow::hideSaveGameInfo()
{
  if (m_CurrentSaveView != NULL) {
    disconnect(m_CurrentSaveView, SIGNAL(closeSaveInfo()), this, SLOT(hideSaveGameInfo()));
    m_CurrentSaveView->deleteLater();
    m_CurrentSaveView = NULL;
  }
}


bool MainWindow::eventFilter(QObject* object, QEvent *event)
{
  if ((object == ui->savegameList) &&
      ((event->type() == QEvent::Leave) || (event->type() == QEvent::WindowDeactivate))) {
    hideSaveGameInfo();
  }

  return false;
}


bool MainWindow::testForSteam()
{
  DWORD processIDs[1024];
  DWORD bytesReturned;
  if (!::EnumProcesses(processIDs, sizeof(processIDs), &bytesReturned)) {
    qWarning("failed to determine if steam is running");
    return true;
  }

  TCHAR processName[MAX_PATH];
  for (unsigned int i = 0; i < bytesReturned / sizeof(DWORD); ++i) {
    memset(processName, '\0', sizeof(TCHAR) * MAX_PATH);
    if (processIDs[i] != 0) {
      HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIDs[i]);

      if (process != NULL) {
        HMODULE module;
        DWORD ignore;

        // first module in a process is always the binary
        if (EnumProcessModules(process, &module, sizeof(HMODULE) * 1, &ignore)) {
          GetModuleBaseName(process, module, processName, MAX_PATH);
          if ((_tcsicmp(processName, TEXT("steam.exe")) == 0) ||
              (_tcsicmp(processName, TEXT("steamservice.exe")) == 0)) {
            return true;
          }
        }
      }
    }
  }

  return false;
}


bool MainWindow::verifyPlugin(IPlugin *plugin)
{
  if (plugin == NULL) {
    return false;
  } else if (!plugin->init(this)) {
    qWarning("plugin failed to initialize");
    return false;
  }
  return true;
}


void MainWindow::toolPluginInvoke()
{
  QAction *triggeredAction = qobject_cast<QAction*>(sender());
  IPluginTool *plugin = (IPluginTool*)triggeredAction->data().value<void*>();
  try {
    plugin->display();
  } catch (const std::exception &e) {
    reportError(tr("Plugin \"%1\" failed: %2").arg(plugin->name()).arg(e.what()));
  } catch (...) {
    reportError(tr("Plugin \"%1\" failed").arg(plugin->name()));
  }
}


void MainWindow::registerPluginTool(IPluginTool *tool)
{
  QAction *action = new QAction(tool->icon(), tool->displayName(), ui->toolBar);
  action->setToolTip(tool->tooltip());
  tool->setParentWidget(this);
  action->setData(qVariantFromValue((void*)tool));
  connect(action, SIGNAL(triggered()), this, SLOT(toolPluginInvoke()), Qt::QueuedConnection);
  QToolButton *toolBtn = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionTool));
  toolBtn->menu()->addAction(action);
}


bool MainWindow::registerPlugin(QObject *plugin)
{
  { // generic treatment for all plugins
    IPlugin *pluginObj = qobject_cast<IPlugin*>(plugin);
    if (pluginObj == NULL) {
      qDebug("not an IPlugin");
      return false;
    }
    m_Settings.registerPlugin(pluginObj);
  }

  { // diagnosis plugins
    IPluginDiagnose *diagnose = qobject_cast<IPluginDiagnose*>(plugin);
    if (diagnose != NULL) {
      m_DiagnosisPlugins.push_back(diagnose);
    }
  }
  { // tool plugins
    IPluginTool *tool = qobject_cast<IPluginTool*>(plugin);
    if (verifyPlugin(tool)) {
      registerPluginTool(tool);
      return true;
    }
  }
  { // installer plugins
    IPluginInstaller *installer = qobject_cast<IPluginInstaller*>(plugin);
    if (verifyPlugin(installer)) {
      installer->setParentWidget(this);
      m_InstallationManager.registerInstaller(installer);
      return true;
    }
  }
  { // proxy plugins
    IPluginProxy *proxy = qobject_cast<IPluginProxy*>(plugin);
    if (verifyPlugin(proxy)) {
      proxy->setParentWidget(this);
      QStringList pluginNames = proxy->pluginList(QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::pluginPath()));
      foreach (const QString &pluginName, pluginNames) {
        try {
          QObject *proxiedPlugin = proxy->instantiate(pluginName);
          if (proxiedPlugin != NULL) {
            if (registerPlugin(proxiedPlugin)) {
              qDebug("loaded plugin \"%s\"", QDir::toNativeSeparators(pluginName).toUtf8().constData());
            } else {
              qWarning("plugin \"%s\" failed to load", pluginName.toUtf8().constData());
            }
          }
        } catch (const std::exception &e) {
          reportError(tr("failed to init plugin %1: %2").arg(pluginName).arg(e.what()));
        }
      }
      return true;
    }
  }

  { // dummy plugins
    // only initialize these, no processing otherwise
    IPlugin *dummy = qobject_cast<IPlugin*>(plugin);
    if (verifyPlugin(dummy)) {
      return true;
    }
  }

  qDebug("no matching plugin interface");

  return false;
}


void MainWindow::loadPlugins()
{
  m_DiagnosisPlugins.clear();

  m_Settings.clearPlugins();

  foreach (QObject *plugin, QPluginLoader::staticInstances()) {
    registerPlugin(plugin);
  }

  QFile loadCheck(QCoreApplication::applicationDirPath() + "/plugin_loadcheck.tmp");
  if (loadCheck.exists() && loadCheck.open(QIODevice::ReadOnly)) {
    // oh, there was a failed plugin load last time. Find out which plugin was loaded last
    QString fileName;
    while (!loadCheck.atEnd()) {
      fileName = QString::fromUtf8(loadCheck.readLine().constData()).trimmed();
    }
    if (QMessageBox::question(this, tr("Plugin error"),
      tr("It appears the plugin \"%1\" failed to load last startup and caused MO to crash. Do you want to disable it?\n"
         "(Please note: If this is the first time you see this message for this plugin you may want to give it another try. "
         "The plugin may be able to recover from the problem)").arg(fileName),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
      m_Settings.addBlacklistPlugin(fileName);
    }
    loadCheck.close();
  }

  loadCheck.open(QIODevice::WriteOnly);

  QString pluginPath = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getOrganizerDirectory())) + "/" + ToQString(AppConfig::pluginPath());
  qDebug("looking for plugins in %s", QDir::toNativeSeparators(pluginPath).toUtf8().constData());
  QDirIterator iter(pluginPath, QDir::Files | QDir::NoDotAndDotDot);

  while (iter.hasNext()) {
    iter.next();
    if (m_Settings.pluginBlacklisted(iter.fileName())) {
      qDebug("plugin \"%s\" blacklisted", qPrintable(iter.fileName()));
      continue;
    }
    loadCheck.write(iter.fileName().toUtf8());
    loadCheck.write("\n");
    loadCheck.flush();
    QString pluginName = iter.filePath();
    if (QLibrary::isLibrary(pluginName)) {
      QPluginLoader pluginLoader(pluginName);
      if (pluginLoader.instance() == NULL) {
        m_UnloadedPlugins.push_back(pluginName);
        qCritical("failed to load plugin %s: %s",
                  pluginName.toUtf8().constData(), pluginLoader.errorString().toUtf8().constData());
      } else {
        if (registerPlugin(pluginLoader.instance())) {
          qDebug("loaded plugin \"%s\"", QDir::toNativeSeparators(pluginName).toUtf8().constData());
        } else {
          m_UnloadedPlugins.push_back(pluginName);
          qWarning("plugin \"%s\" failed to load", pluginName.toUtf8().constData());
        }
      }
    }
  }

  // remove the load check file on success
  loadCheck.remove();

  m_DownloadManager.setSupportedExtensions(m_InstallationManager.getSupportedExtensions());

  m_DiagnosisPlugins.push_back(this);
}

IGameInfo &MainWindow::gameInfo() const
{
  return *m_GameInfo;
}


IModRepositoryBridge *MainWindow::createNexusBridge() const
{
  return new NexusBridge();
}


QString MainWindow::profileName() const
{
  if (m_CurrentProfile != NULL) {
    return m_CurrentProfile->getName();
  } else {
    return "";
  }
}

QString MainWindow::profilePath() const
{
  if (m_CurrentProfile != NULL) {
    return m_CurrentProfile->getPath();
  } else {
    return "";
  }
}

QString MainWindow::downloadsPath() const
{
  return QDir::fromNativeSeparators(m_Settings.getDownloadDirectory());
}

VersionInfo MainWindow::appVersion() const
{
  return m_Updater.getVersion();
}


IModInterface *MainWindow::getMod(const QString &name)
{
  unsigned int index = ModInfo::getIndex(name);
  if (index == UINT_MAX) {
    return NULL;
  } else {
    return ModInfo::getByIndex(index).data();
  }
}


IModInterface *MainWindow::createMod(GuessedValue<QString> &name)
{
  if (!m_InstallationManager.testOverwrite(name)) {
    return NULL;
  }

  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());

/*  QString fixedName = name;
  fixDirectoryName(fixedName);
  unsigned int index = ModInfo::getIndex(fixedName);
  if (index != UINT_MAX) {
    ModInfo::Ptr result = ModInfo::getByIndex(index);
    if (!result->isEmpty()) {
      throw MyException(tr("The mod \"%1\" already exists!").arg(fixedName));
    }
    return result.data();
  } else {*/
    QString targetDirectory = QDir::fromNativeSeparators(m_Settings.getModDirectory()).append("/").append(name);

    QSettings settingsFile(targetDirectory.mid(0).append("/meta.ini"), QSettings::IniFormat);

    settingsFile.setValue("modid", 0);
    settingsFile.setValue("version", 0);
    settingsFile.setValue("newestVersion", 0);
    settingsFile.setValue("category", 0);
    settingsFile.setValue("installationFile", 0);
    return ModInfo::createFrom(QDir(targetDirectory), &m_DirectoryStructure).data();
//  }
}

bool MainWindow::removeMod(IModInterface *mod)
{
  unsigned int index = ModInfo::getIndex(mod->name());
  if (index == UINT_MAX) {
    return mod->remove();
  } else {
    return ModInfo::removeMod(index);
  }
}


void MainWindow::modDataChanged(IModInterface*)
{
  refreshModList();
}

QVariant MainWindow::pluginSetting(const QString &pluginName, const QString &key) const
{
  return m_Settings.pluginSetting(pluginName, key);
}

void MainWindow::setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value)
{
  m_Settings.setPluginSetting(pluginName, key, value);
}

QVariant MainWindow::persistent(const QString &pluginName, const QString &key, const QVariant &def) const
{
  return m_Settings.pluginPersistent(pluginName, key, def);
}

void MainWindow::setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync)
{
  m_Settings.setPluginPersistent(pluginName, key, value, sync);
}

QString MainWindow::pluginDataPath() const
{
  QString pluginPath = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getOrganizerDirectory())) + "/" + ToQString(AppConfig::pluginPath());
  return pluginPath + "/data";
}


void MainWindow::startSteam()
{
  QSettings steamSettings("HKEY_CURRENT_USER\\Software\\Valve\\Steam",
                          QSettings::NativeFormat);
  QString exe = steamSettings.value("SteamExe", "").toString();
  if (!exe.isEmpty()) {
    QString temp = QString("\"%1\"").arg(exe);
    if (!QProcess::startDetached(temp)) {
      reportError(tr("Failed to start \"%1\"").arg(temp));
    } else {
      QMessageBox::information(this, tr("Waiting"), tr("Please press OK once you're logged into steam."));
    }
  }
}


HANDLE MainWindow::spawnBinaryDirect(const QFileInfo &binary, const QString &arguments, const QString &profileName,
                                    const QDir &currentDirectory, const QString &steamAppID)
{
  storeSettings();

  if (!binary.exists()) {
    reportError(tr("\"%1\" not found").arg(binary.fileName()));
    return INVALID_HANDLE_VALUE;
  }

  if (!steamAppID.isEmpty()) {
    ::SetEnvironmentVariableW(L"SteamAPPId", ToWString(steamAppID).c_str());
  } else {
    ::SetEnvironmentVariableW(L"SteamAPPId", ToWString(m_Settings.getSteamAppID()).c_str());
  }

  if ((GameInfo::instance().requiresSteam()) &&
      (m_Settings.getLoadMechanism() == LoadMechanism::LOAD_MODORGANIZER)) {
    if (!testForSteam()) {
      if (QuestionBoxMemory::query(this->isVisible() ? this : NULL,
            m_Settings.directInterface(), "steamQuery", tr("Start Steam?"),
            tr("Steam is required to be running already to correctly start the game. "
               "Should MO try to start steam now?"),
            QDialogButtonBox::Yes | QDialogButtonBox::No) == QDialogButtonBox::Yes) {
        startSteam();
      }
    }
  }

  while (m_RefreshProgress->isVisible()) {
    ::Sleep(1000);
    QCoreApplication::processEvents();
  }

  if (m_AboutToRun(binary.absoluteFilePath())) {
    return startBinary(binary, arguments, profileName, m_Settings.logLevel(), currentDirectory, true);
  } else {
    qDebug("start of \"%s\" canceled by plugin", qPrintable(binary.absoluteFilePath()));
    return INVALID_HANDLE_VALUE;
  }
}

/*
void MainWindow::spawnProgram(const QString &fileName, const QString &argumentsArg,
                              const QString &profileName, const QDir &currentDirectory)
{
  QFileInfo binary;
  QString arguments = argumentsArg;
  QString steamAppID;
  try {
    const Executable &exe = m_ExecutablesList.find(fileName);
    steamAppID = exe.m_SteamAppID;
    if (arguments == "") {
      arguments = exe.m_Arguments;
    }
    binary = exe.m_BinaryInfo;
  } catch (const std::runtime_error&) {
    qWarning("\"%s\" not set up as executable", fileName.toUtf8().constData());
    binary = QFileInfo(fileName);
  }
  spawnBinaryDirect(binary, arguments, profileName, currentDirectory, steamAppID);
}
*/

void MainWindow::spawnBinary(const QFileInfo &binary, const QString &arguments, const QDir &currentDirectory, bool closeAfterStart, const QString &steamAppID)
{
  storeSettings();

  HANDLE processHandle = spawnBinaryDirect(binary, arguments, m_CurrentProfile->getName(), currentDirectory, steamAppID);
  if (processHandle != INVALID_HANDLE_VALUE) {
    if (closeAfterStart) {
      close();
    } else {
      this->setEnabled(false);

      LockedDialog *dialog = new LockedDialog(this);
      dialog->show();

      QCoreApplication::processEvents();

      while ((::WaitForSingleObject(processHandle, 1000) == WAIT_TIMEOUT) &&
              !dialog->unlockClicked()) {
        // keep processing events so the app doesn't appear dead
        QCoreApplication::processEvents();
      }

      this->setEnabled(true);
      refreshDirectoryStructure();
      if (GameInfo::instance().getLoadOrderMechanism() == GameInfo::TYPE_FILETIME) {
        QFile::remove(m_CurrentProfile->getLoadOrderFileName());
      }
      dialog->hide();
      dialog->deleteLater();
    }
  }
}


void MainWindow::startExeAction()
{
  QAction *action = qobject_cast<QAction*>(sender());
  if (action != NULL) {
    Executable selectedExecutable = action->data().value<Executable>();
    spawnBinary(selectedExecutable.m_BinaryInfo,
                selectedExecutable.m_Arguments,
                selectedExecutable.m_WorkingDirectory.length() != 0 ? selectedExecutable.m_WorkingDirectory
                                                                    : selectedExecutable.m_BinaryInfo.absolutePath(),
                selectedExecutable.m_CloseMO == DEFAULT_CLOSE,
                selectedExecutable.m_SteamAppID);
  } else {
    qCritical("not an action?");
  }
}


void MainWindow::refreshModList()
{
  // don't lose changes!
  m_CurrentProfile->writeModlistNow(true);
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure);
  m_CurrentProfile->refreshModStatus();

  m_ModList.notifyChange(-1);

  refreshDirectoryStructure();
}


void MainWindow::setExecutablesList(const ExecutablesList &executablesList)
{
  m_ExecutablesList = executablesList;
  refreshExecutablesList();
  updateToolBar();
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
  QString profileName = ui->profileBox->currentText();
  qDebug("activate profile \"%s\"", qPrintable(profileName));
  QString profileDir = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getProfilesDir()))
                          .append("/").append(profileName);
  delete m_CurrentProfile;
  m_CurrentProfile = new Profile(QDir(profileDir));
  m_ModList.setProfile(m_CurrentProfile);

  m_ModListSortProxy->setProfile(m_CurrentProfile);

  connect(m_CurrentProfile, SIGNAL(modStatusChanged(uint)), this, SLOT(modStatusChanged(uint)));

  refreshSaveList();
  refreshModList();
}


void MainWindow::on_profileBox_currentIndexChanged(int index)
{
  if (ui->profileBox->isEnabled()) {
    int previousIndex = m_OldProfileIndex;
    m_OldProfileIndex = index;

    if ((previousIndex != -1) &&
        (m_CurrentProfile != NULL) &&
        m_CurrentProfile->exists()) {
      saveCurrentLists();
    }

    if (ui->profileBox->currentIndex() == 0) {
      ProfilesDialog(m_GamePath).exec();
      while (!refreshProfiles()) {
        ProfilesDialog(m_GamePath).exec();
      }
      ui->profileBox->setCurrentIndex(previousIndex);
    } else {
      activateSelectedProfile();
    }
  }
}


void MainWindow::updateTo(QTreeWidgetItem *subTree, const std::wstring &directorySoFar, const DirectoryEntry &directoryEntry, bool conflictsOnly)
{
  {
    std::vector<FileEntry::Ptr> files = directoryEntry.getFiles();
    for (auto iter = files.begin(); iter != files.end(); ++iter) {
      FileEntry::Ptr current = *iter;
      if (conflictsOnly && (current->getAlternatives().size() == 0)) {
        continue;
      }

      QString fileName = ToQString(current->getName());
      QStringList columns(fileName);
      bool isArchive = false;
      int originID = current->getOrigin(isArchive);
      QString source = ToQString(m_DirectoryStructure->getOriginByID(originID).getName());
      std::wstring archive = current->getArchive();
      if (archive.length() != 0) {
        source.append(" (").append(ToQString(archive)).append(")");
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
      fileChild->setData(0, Qt::UserRole + 1, isArchive);
      fileChild->setData(1, Qt::UserRole, source);
      fileChild->setData(1, Qt::UserRole + 1, originID);

      std::vector<int> alternatives = current->getAlternatives();

      if (!alternatives.empty()) {
        std::wostringstream altString;
        altString << ToWString(tr("Also in: <br>"));
        for (std::vector<int>::iterator altIter = alternatives.begin();
             altIter != alternatives.end(); ++altIter) {
          if (altIter != alternatives.begin()) {
            altString << " , ";
          }
          altString << "<span style=\"white-space: nowrap;\"><i>" << m_DirectoryStructure->getOriginByID(*altIter).getName() << "</font></span>";
        }
        fileChild->setToolTip(1, QString("%1").arg(ToQString(altString.str())));
        fileChild->setForeground(1, QBrush(Qt::red));
      } else {
        fileChild->setToolTip(1, tr("No conflict"));
      }
      subTree->addChild(fileChild);
    }
  }

  std::wostringstream temp;
  temp << directorySoFar << "\\" << directoryEntry.getName();
  {
    std::vector<DirectoryEntry*>::const_iterator current, end;
    directoryEntry.getSubDirectories(current, end);
    for (; current != end; ++current) {
      QStringList columns(ToQString((*current)->getName()));
      columns.append("");
      QTreeWidgetItem *directoryChild = new QTreeWidgetItem(columns);
      updateTo(directoryChild, temp.str(), **current, conflictsOnly);
      if (directoryChild->childCount() != 0) {
        subTree->addChild(directoryChild);
      } else {
        delete directoryChild;
      }
    }
  }
  subTree->sortChildren(0, Qt::AscendingOrder);
}


bool MainWindow::refreshProfiles(bool selectProfile)
{
  QComboBox* profileBox = findChild<QComboBox*>("profileBox");

  QString currentProfileName = profileBox->currentText();

  profileBox->blockSignals(true);
  profileBox->clear();
  profileBox->addItem(QObject::tr("<Manage...>"));

  QDir profilesDir(ToQString(GameInfo::instance().getProfilesDir()));
  profilesDir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);

  QDirIterator profileIter(profilesDir);

  int newIndex = profileIter.hasNext() ? 1 : 0;
  int currentIndex = 0;
  while (profileIter.hasNext()) {
    profileIter.next();
    ++currentIndex;
    try {
      profileBox->addItem(profileIter.fileName());
      if (currentProfileName == profileIter.fileName()) {
        newIndex = currentIndex;
      }
    } catch (const std::runtime_error& error) {
      reportError(QObject::tr("failed to parse profile %1: %2").arg(profileIter.fileName()).arg(error.what()));
    }
  }

  // now select one of the profiles, preferably the one that was selected before
  profileBox->blockSignals(false);

  if (selectProfile) {
    if (profileBox->count() > 1) {
      if (currentProfileName.length() != 0) {
        if ((newIndex != 0) && (profileBox->count() > newIndex)) {
          profileBox->setCurrentIndex(newIndex);
        } else {
          profileBox->setCurrentIndex(1);
        }
      }
      return true;
    } else {
      return false;
    }
  } else {
    return profileBox->count() > 1;
  }
}


void MainWindow::refreshDirectoryStructure()
{
  m_DirectoryUpdate = true;
  std::vector<std::tuple<QString, QString, int> > activeModList = m_CurrentProfile->getActiveMods();
  m_DirectoryRefresher.setMods(activeModList);

  statusBar()->show();
  m_RefreshProgress->setRange(0, 100);

  QTimer::singleShot(0, &m_DirectoryRefresher, SLOT(refresh()));
}


#if QT_VERSION >= 0x050000
extern QPixmap qt_pixmapFromWinHICON(HICON icon);
#else
#define qt_pixmapFromWinHICON(icon) QPixmap::fromWinHICON(icon)
#endif

QIcon MainWindow::iconForExecutable(const QString &filePath)
{
  HICON winIcon;
  UINT res = ::ExtractIconExW(ToWString(filePath).c_str(), 0, &winIcon, NULL, 1);
  if (res == 1) {
    QIcon result = QIcon(qt_pixmapFromWinHICON(winIcon));
    ::DestroyIcon(winIcon);
    return result;
  } else {
    return QIcon(":/MO/gui/executable");
  }
}


void MainWindow::refreshExecutablesList()
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");
  executablesList->setEnabled(false);
  executablesList->clear();
  executablesList->addItem(tr("<Edit...>"));

  QAbstractItemModel *model = executablesList->model();

  std::vector<Executable>::const_iterator current, end;
  m_ExecutablesList.getExecutables(current, end);
  for(int i = 0; current != end; ++current, ++i) {
    QVariant temp;
    temp.setValue(*current);
    QIcon icon = iconForExecutable(current->m_BinaryInfo.filePath());
    executablesList->addItem(icon, current->m_Title, temp);
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
  updateTo(subTree, L"", *m_DirectoryStructure, conflictsBox->isChecked());
  tree->insertTopLevelItem(0, subTree);
  subTree->setExpanded(true);
  tree->header()->resizeSection(0, 200);
}


void MainWindow::refreshSaveList()
{
  ui->savegameList->clear();

  QDir savesDir;
  if (m_CurrentProfile->localSavesEnabled()) {
    savesDir.setPath(m_CurrentProfile->getPath() + "/saves");
  } else {
    wchar_t path[MAX_PATH];
    ::GetPrivateProfileStringW(L"General", L"SLocalSavePath", L"Saves",
                               path, MAX_PATH,
                               (ToWString(m_CurrentProfile->getPath()) + L"\\" + GameInfo::instance().getIniFileNames().at(0)).c_str());
    savesDir.setPath(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getDocumentsDir() + L"\\" + path)));
  }

  QStringList filters;
  filters << ToQString(GameInfo::instance().getSaveGameExtension());
  savesDir.setNameFilters(filters);

  QFileInfoList files = savesDir.entryInfoList(QDir::Files, QDir::Time);

  foreach (const QFileInfo &file, files) {
    QListWidgetItem *item = new QListWidgetItem(file.fileName());
    item->setData(Qt::UserRole, file.absoluteFilePath());
    ui->savegameList->addItem(item);
  }
}


void MainWindow::refreshLists()
{
  if (m_DirectoryStructure->isPopulated()) {
    refreshESPList();
    refreshBSAList();
  } // no point in refreshing lists if no files have been added to the directory tree
}


void MainWindow::refreshESPList()
{
  m_CurrentProfile->writeModlist();

  // clear list
  try {
    m_PluginList.refresh(m_CurrentProfile->getName(),
                         *m_DirectoryStructure,
                         m_CurrentProfile->getPluginsFileName(),
                         m_CurrentProfile->getLoadOrderFileName(),
                         m_CurrentProfile->getLockedOrderFileName());
  } catch (const std::exception &e) {
    reportError(tr("Failed to refresh list of esps: %s").arg(e.what()));
  }
}


static bool BySortValue(const std::pair<UINT32, QTreeWidgetItem*> &LHS, const std::pair<UINT32, QTreeWidgetItem*> &RHS)
{
  return LHS.first < RHS.first;
}


void MainWindow::refreshBSAList()
{
  m_ArchivesInit = false;
  ui->bsaList->clear();
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  ui->bsaList->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
  ui->bsaList->header()->setResizeMode(QHeaderView::ResizeToContents);
#endif

  m_DefaultArchives.clear();

  wchar_t buffer[256];
  std::wstring iniFileName = ToWString(QDir::toNativeSeparators(m_CurrentProfile->getIniFileName()));
  if (::GetPrivateProfileStringW(L"Archive", GameInfo::instance().archiveListKey().c_str(),
                                 L"", buffer, 256, iniFileName.c_str()) != 0) {
    m_DefaultArchives = ToQString(buffer).split(',');
  } else {
    std::vector<std::wstring> vanillaBSAs = GameInfo::instance().getVanillaBSAs();
    for (auto iter = vanillaBSAs.begin(); iter != vanillaBSAs.end(); ++iter) {
      m_DefaultArchives.append(ToQString(*iter));
    }
  }

  if (::GetPrivateProfileStringW(L"Archive", GameInfo::instance().archiveListKey().append(L"2").c_str(),
                                 L"", buffer, 256, iniFileName.c_str()) != 0) {
    m_DefaultArchives.append(ToQString(buffer).split(','));
  }

  for (int i = 0; i < m_DefaultArchives.count(); ++i) {
    m_DefaultArchives[i] = m_DefaultArchives[i].trimmed();
  }

  m_ActiveArchives.clear();

  QFile archiveFile(m_CurrentProfile->getArchivesFileName());
  if (archiveFile.open(QIODevice::ReadOnly)) {
    while (!archiveFile.atEnd()) {
      m_ActiveArchives.append(QString::fromUtf8(archiveFile.readLine()));
    }
    archiveFile.close();

    for (int i = 0; i < m_ActiveArchives.count(); ++i) {
      m_ActiveArchives[i] = m_ActiveArchives[i].trimmed();
    }
  } else {
    m_ActiveArchives = m_DefaultArchives;
  }

  std::vector<std::pair<UINT32, QTreeWidgetItem*> > items;

  std::vector<FileEntry::Ptr> files = m_DirectoryStructure->getFiles();
  for (auto iter = files.begin(); iter != files.end(); ++iter) {
    FileEntry::Ptr current = *iter;

    QString filename = ToQString(current->getName().c_str());
    QString extension = filename.right(3).toLower();

    if (extension == "bsa") {
      if (filename.compare("update.bsa", Qt::CaseInsensitive) == 0) {
        // hack: ignore update.bsa because it confuses people
        continue;
      }
      int index = m_ActiveArchives.indexOf(filename);
      QStringList strings(filename);
      bool isArchive = false;
      int origin = current->getOrigin(isArchive);
      strings.append(ToQString(m_DirectoryStructure->getOriginByID(origin).getName()));
      QTreeWidgetItem *newItem = new QTreeWidgetItem(strings);
      newItem->setData(0, Qt::UserRole, index);
      newItem->setData(1, Qt::UserRole, origin);
//      newItem->setFlags(newItem->flags() & ~Qt::ItemIsDropEnabled | Qt::ItemIsUserCheckable);
      newItem->setFlags(newItem->flags() | Qt::ItemIsUserCheckable);
      newItem->setCheckState(0, (index != -1) ? Qt::Checked : Qt::Unchecked);
      if (m_Settings.forceEnableCoreFiles() && m_DefaultArchives.contains(filename)) {
        newItem->setCheckState(0, Qt::Checked);
        newItem->setDisabled(true);
      } else {
        newItem->setCheckState(0, (index != -1) ? Qt::Checked : Qt::Unchecked);
      }

      if (index < 0) index = 0;

      UINT32 sortValue = ((m_DirectoryStructure->getOriginByID(origin).getPriority() & 0xFFFF) << 16) | (index & 0xFFFF);
      items.push_back(std::make_pair(sortValue, newItem));
    }
  }

  std::sort(items.begin(), items.end(), BySortValue);

  for (std::vector<std::pair<UINT32, QTreeWidgetItem*> >::iterator iter = items.begin(); iter != items.end(); ++iter) {
    int originID = iter->second->data(1, Qt::UserRole).toInt();
    FilesOrigin origin = m_DirectoryStructure->getOriginByID(originID);
    QList<QTreeWidgetItem*> items = ui->bsaList->findItems(ToQString(origin.getName()), Qt::MatchFixedString);
    QTreeWidgetItem *subItem = NULL;
    if (items.length() > 0) {
      subItem = items.at(0);
    } else {
      subItem = new QTreeWidgetItem(QStringList(ToQString(origin.getName())));
      ui->bsaList->addTopLevelItem(subItem);
    }
    subItem->addChild(iter->second);
    //ui->bsaList->addTopLevelItem(iter->second);
  }

  checkBSAList();
  m_ArchivesInit = true;
}


void MainWindow::checkBSAList()
{
  ui->bsaList->blockSignals(true);

  bool warning = false;

  for (int i = 0; i < ui->bsaList->topLevelItemCount(); ++i) {
    bool modWarning = false;
    QTreeWidgetItem *tlItem = ui->bsaList->topLevelItem(i);
    for (int j = 0; j < tlItem->childCount(); ++j) {
      QTreeWidgetItem *item = tlItem->child(j);
      QString filename = item->text(0);
      item->setIcon(0, QIcon());
      item->setToolTip(0, QString());

      if (item->checkState(0) == Qt::Unchecked) {
        if (m_DefaultArchives.contains(filename)) {
          item->setIcon(0, QIcon(":/MO/gui/warning"));
          item->setToolTip(0, tr("This bsa is enabled in the ini file so it may be required!"));
          modWarning = true;
        } else {
          QString espName = filename.mid(0, filename.length() - 3).append("esp").toLower();
          QString esmName = filename.mid(0, filename.length() - 3).append("esm").toLower();
          if (m_PluginList.isEnabled(espName) || m_PluginList.isEnabled(esmName)) {
            item->setIcon(0, QIcon(":/MO/gui/warning"));
            item->setToolTip(0, tr("This archive will still be loaded since there is a plugin of the same name but "
                                      "its files will not follow installation order!"));
            modWarning = true;
          }
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

  ui->bsaList->blockSignals(false);
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
  QSettings settings(ToQString(GameInfo::instance().getIniFilename()), QSettings::IniFormat);

  if (settings.contains("window_geometry")) {
    restoreGeometry(settings.value("window_geometry").toByteArray());
  }

  if (settings.contains("window_split")) {
    ui->splitter->restoreState(settings.value("window_split").toByteArray());
  }

  bool filtersVisible = settings.value("filters_visible", false).toBool();
  setCategoryListVisible(filtersVisible);
  ui->displayCategoriesBtn->setChecked(filtersVisible);

  languageChange(m_Settings.language());
  int selectedExecutable = settings.value("selected_executable").toInt();
  setExecutableIndex(selectedExecutable);

  if (settings.value("Settings/use_proxy", false).toBool()) {
    activateProxy(true);
  }
}


void MainWindow::storeSettings()
{
  if (m_CurrentProfile == NULL) {
    return;
  }
  m_CurrentProfile->writeModlist();
  m_CurrentProfile->createTweakedIniFile();
  saveCurrentLists();
  m_Settings.setupLoadMechanism();
  QSettings settings(ToQString(GameInfo::instance().getIniFilename()), QSettings::IniFormat);
  if (m_CurrentProfile != NULL) {
    settings.setValue("selected_profile", m_CurrentProfile->getName().toUtf8().constData());
  } else {
    settings.remove("selected_profile");
  }

  settings.setValue("mod_list_state", ui->modList->header()->saveState());
  settings.setValue("plugin_list_state", ui->espList->header()->saveState());

  settings.setValue("group_state", ui->groupCombo->currentIndex());

  settings.setValue("compact_downloads", ui->compactBox->isChecked());
  settings.setValue("ask_for_nexuspw", m_AskForNexusPW);

  settings.setValue("window_geometry", saveGeometry());
  settings.setValue("window_split", ui->splitter->saveState());

  settings.setValue("filters_visible", ui->displayCategoriesBtn->isChecked());

  settings.remove("customExecutables");
  settings.beginWriteArray("customExecutables");
  std::vector<Executable>::const_iterator current, end;
  m_ExecutablesList.getExecutables(current, end);
  int count = 0;
  for (; current != end; ++current) {
    const Executable &item = *current;
    if (item.m_Custom || item.m_Toolbar) {
      settings.setArrayIndex(count++);
      settings.setValue("binary", item.m_BinaryInfo.absoluteFilePath());
      settings.setValue("title", item.m_Title);
      settings.setValue("arguments", item.m_Arguments);
      settings.setValue("workingDirectory", item.m_WorkingDirectory);
      settings.setValue("closeOnStart", item.m_CloseMO == DEFAULT_CLOSE);
      settings.setValue("steamAppID", item.m_SteamAppID);
      settings.setValue("custom", item.m_Custom);
      settings.setValue("toolbar", item.m_Toolbar);
    }
  }
  settings.endArray();

  QComboBox *executableBox = findChild<QComboBox*>("executablesListBox");
  settings.setValue("selected_executable", executableBox->currentIndex());

  FileDialogMemory::save(m_Settings.directInterface());
}


void MainWindow::on_btnRefreshData_clicked()
{
  if (!m_DirectoryUpdate) {
    refreshDirectoryStructure();
  } else {
    qDebug("directory update");
  }
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
  if (index == 0) {
    refreshESPList();
  } else if (index == 1) {
    refreshBSAList();
  } else if (index == 2) {
    refreshDataTree();
  } else if (index == 3) {
    refreshSaveList();
  } else if (index == 4) {
    ui->downloadView->scrollToBottom();
  }
}


void MainWindow::installMod(const QString &fileName)
{
  if (m_CurrentProfile == NULL) {
    return;
  }

  bool hasIniTweaks = false;
  GuessedValue<QString> modName;
  m_CurrentProfile->writeModlistNow();
  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
  if (m_InstallationManager.install(fileName, modName, hasIniTweaks)) {
    MessageDialog::showMessage(tr("Installation successful"), this);
    refreshModList();

    QModelIndexList posList = m_ModList.match(m_ModList.index(0, 0), Qt::DisplayRole, static_cast<const QString&>(modName));
    if (posList.count() == 1) {
      ui->modList->scrollTo(posList.at(0));
    }
    int modIndex = ModInfo::getIndex(modName);
    if (modIndex != UINT_MAX) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
      if (hasIniTweaks &&
          (QMessageBox::question(this, tr("Configure Mod"),
              tr("This mod contains ini tweaks. Do you want to configure them now?"),
              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
        displayModInformation(modInfo, modIndex, ModInfoDialog::TAB_INIFILES);
      }
      testExtractBSA(modIndex);
    } else {
      reportError(tr("mod \"%1\" not found").arg(modName));
    }
  } else if (m_InstallationManager.wasCancelled()) {
    QMessageBox::information(this, tr("Installation cancelled"), tr("The mod was not installed completely."), QMessageBox::Ok);
  }
}

QString MainWindow::resolvePath(const QString &fileName) const
{
  if (m_DirectoryStructure == NULL) {
    return QString();
  }
  const FileEntry::Ptr file = m_DirectoryStructure->searchFile(ToWString(fileName), NULL);
  if (file.get() != NULL) {
    return ToQString(file->getFullPath());
  } else {
    return QString();
  }
}

QStringList MainWindow::listDirectories(const QString &directoryName) const
{
  QStringList result;
  DirectoryEntry *dir = m_DirectoryStructure->findSubDirectoryRecursive(ToWString(directoryName));
  if (dir != NULL) {
    std::vector<DirectoryEntry*>::iterator current, end;
    dir->getSubDirectories(current, end);
    for (; current != end; ++current) {
      result.append(ToQString((*current)->getName()));
    }
  }
  return result;
}

QStringList MainWindow::findFiles(const QString &path, const std::function<bool(const QString&)> &filter) const
{
  QStringList result;
  DirectoryEntry *dir = m_DirectoryStructure->findSubDirectoryRecursive(ToWString(path));
  if (dir != NULL) {
    std::vector<FileEntry::Ptr> files = dir->getFiles();
    foreach (FileEntry::Ptr file, files) {
      if (filter(ToQString(file->getName()))) {
        result.append(ToQString(file->getFullPath()));
      }
    }
  } else {
    qWarning("directory %s not found", qPrintable(path));
  }
  return result;
}

IDownloadManager *MainWindow::downloadManager()
{
  return &m_DownloadManager;
}

IPluginList *MainWindow::pluginList()
{
  return &m_PluginList;
}

HANDLE MainWindow::startApplication(const QString &executable, const QStringList &args, const QString &cwd, const QString &profile)
{
  QFileInfo binary;
  QString arguments = args.join(" ");
  QString currentDirectory = cwd;
  QString profileName = (profile.length() > 0) ? profile : m_CurrentProfile->getName();
  QString steamAppID;
  if (executable.contains('\\') || executable.contains('/')) {
    // file path
    binary = QFileInfo(executable);
    if (binary.isRelative()) {
      // relative path, should be relative to game directory
      binary = QFileInfo(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory())) + "/" + executable);
    }
    if (cwd.length() == 0) {
      currentDirectory = binary.absolutePath();
    }
  } else {
    // only a file name, search executables list
    try {
      const Executable &exe = m_ExecutablesList.find(executable);
      steamAppID = exe.m_SteamAppID;
      if (arguments == "") {
        arguments = exe.m_Arguments;
      }
      binary = exe.m_BinaryInfo;
      if (cwd.length() == 0) {
        currentDirectory = exe.m_WorkingDirectory;
      }
    } catch (const std::runtime_error&) {
      qWarning("\"%s\" not set up as executable", executable.toUtf8().constData());
      return INVALID_HANDLE_VALUE;
    }
  }

  return spawnBinaryDirect(binary, arguments, profileName, currentDirectory, steamAppID);
}

bool MainWindow::onAboutToRun(const std::function<bool (const QString &)> &func)
{
  auto conn = m_AboutToRun.connect(func);
  return conn.connected();
}

std::vector<unsigned int> MainWindow::activeProblems() const
{
  std::vector<unsigned int> problems;
  if (m_UnloadedPlugins.size() != 0) {
    problems.push_back(PROBLEM_PLUGINSNOTLOADED);
  }
  return problems;
}

QString MainWindow::shortDescription(unsigned int key) const
{
  switch (key) {
    case PROBLEM_PLUGINSNOTLOADED: {
      return tr("Some plugins could not be loaded");
    } break;
    default: {
      return tr("Description missing");
    } break;
  }
}

QString MainWindow::fullDescription(unsigned int key) const
{
  switch (key) {
    case PROBLEM_PLUGINSNOTLOADED: {
      QString result = tr("The following Plugins could not be loaded. The reason may be missing dependencies (i.e. python) or an outdated version:<ul>");
      foreach (const QString &plugin, m_UnloadedPlugins) {
        result += "<li>" + plugin + "</li>";
      }
      result += "<ul>";
      return result;
    } break;
    default: {
      return tr("Description missing");
    } break;
  }
}

bool MainWindow::hasGuidedFix(unsigned int) const
{
  return false;
}

void MainWindow::startGuidedFix(unsigned int) const
{
}

void MainWindow::installMod()
{
  try {
    QStringList extensions = m_InstallationManager.getSupportedExtensions();
    for (auto iter = extensions.begin(); iter != extensions.end(); ++iter) {
      *iter = "*." + *iter;
    }

    QString fileName = FileDialogMemory::getOpenFileName("installMod", this, tr("Choose Mod"), QString(),
                                                         tr("Mod Archive").append(QString(" (%1)").arg(extensions.join(" "))));

    if (fileName.length() == 0) {
      return;
    } else {
      installMod(fileName);
    }
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}


void MainWindow::on_startButton_clicked()
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");

  Executable selectedExecutable = executablesList->itemData(executablesList->currentIndex()).value<Executable>();

  spawnBinary(selectedExecutable.m_BinaryInfo,
              selectedExecutable.m_Arguments,
              selectedExecutable.m_WorkingDirectory.length() != 0 ? selectedExecutable.m_WorkingDirectory
                                                                  : selectedExecutable.m_BinaryInfo.absolutePath(),
              selectedExecutable.m_CloseMO == DEFAULT_CLOSE,
              selectedExecutable.m_SteamAppID);
}


static HRESULT CreateShortcut(LPCWSTR targetFileName, LPCWSTR arguments,
                              LPCSTR linkFileName, LPCWSTR description,
                              LPCWSTR currentDirectory)
{
  HRESULT result = E_INVALIDARG;
  if ((targetFileName != NULL) && (wcslen(targetFileName) > 0) &&
       (arguments != NULL) &&
       (linkFileName != NULL) && (strlen(linkFileName) > 0) &&
       (description != NULL) &&
       (currentDirectory != NULL)) {

    IShellLink* shellLink;
    result = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                              IID_IShellLink, (LPVOID*)&shellLink);

    if (!SUCCEEDED(result)) {
      qCritical("failed to create IShellLink instance");
      return result;
    }
      if (!SUCCEEDED(result)) return result;

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

    IPersistFile* persistFile;
    result = shellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&persistFile);
    if (SUCCEEDED(result)) {
      wchar_t linkFileNameW[MAX_PATH];
      MultiByteToWideChar(CP_ACP, 0, linkFileName, -1, linkFileNameW, MAX_PATH);
      result = persistFile->Save(linkFileNameW, TRUE);
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
    EditExecutablesDialog dialog(m_ExecutablesList);
    if (dialog.exec() == QDialog::Accepted) {
      m_ExecutablesList = dialog.getExecutablesList();
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

    if (executablesList->itemData(index).isNull()) {
      if (modifyExecutablesDialog()) {
        setExecutableIndex(previousIndex);
//        executablesList->setCurrentIndex(previousIndex);
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
//  ::ShellExecuteW(NULL, L"open", L"http://issue.tannin.eu/tbg/wiki/Modorganizer%3AMainPage", NULL, NULL, SW_SHOWNORMAL);
  ::ShellExecuteW(NULL, L"open", L"http://wiki.step-project.com/Guide:Mod_Organizer", NULL, NULL, SW_SHOWNORMAL);
}

void MainWindow::issueTriggered()
{
  ::ShellExecuteW(NULL, L"open", L"http://issue.tannin.eu/tbg", NULL, NULL, SW_SHOWNORMAL);
}

void MainWindow::tutorialTriggered()
{
  QAction *tutorialAction = qobject_cast<QAction*>(sender());
  if (tutorialAction != NULL) {
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
  bool repeat = true;
  while (repeat) {
    ProfilesDialog profilesDialog(m_GamePath);
    profilesDialog.exec();
    if (refreshProfiles() && !profilesDialog.failed()) {
      repeat = false;
    }
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


void MainWindow::setCompactDownloads(bool compact)
{
  ui->compactBox->setChecked(compact);
}


bool MainWindow::queryLogin(QString &username, QString &password)
{
  CredentialsDialog dialog(this);
  int res = dialog.exec();
  if (dialog.neverAsk()) {
    m_AskForNexusPW = false;
  }
  if (res == QDialog::Accepted) {
    username = dialog.username();
    password = dialog.password();
    if (dialog.store()) {
      m_Settings.setNexusLogin(username, password);
    }
    return true;
  } else {
    return false;
  }
}


bool MainWindow::setCurrentProfile(int index)
{
  QComboBox *profilesBox = findChild<QComboBox*>("profileBox");
  if (index >= profilesBox->count()) {
    return false;
  } else {
    profilesBox->setCurrentIndex(index);
    return true;
  }
}

bool MainWindow::setCurrentProfile(const QString &name)
{
  QComboBox *profilesBox = findChild<QComboBox*>("profileBox");
  for (int i = 0; i < profilesBox->count(); ++i) {
    if (QString::compare(profilesBox->itemText(i), name, Qt::CaseInsensitive) == 0) {
      profilesBox->setCurrentIndex(i);
      return true;
    }
  }
  // profile not valid
  profilesBox->setCurrentIndex(1);
  return false;
}


void MainWindow::refresher_progress(int percent)
{
  m_RefreshProgress->setValue(percent);
}


void MainWindow::directory_refreshed()
{
  statusBar()->hide();

  DirectoryEntry *newStructure = m_DirectoryRefresher.getDirectoryStructure();
  if (newStructure != NULL) {
    DirectoryEntry *oldStructure = m_DirectoryStructure;
    m_DirectoryStructure = newStructure;
    delete oldStructure;

    refreshDataTree();
    refreshLists();
  } else {
    // TODO: don't know why this happens, this slot seems to get called twice with only one emit
    return;
  }
  m_DirectoryUpdate = false;
  if (m_CurrentProfile != NULL) {
    refreshLists();
  }
//  m_RefreshProgress->setVisible(false);

  // some problem-reports may rely on the virtual directory tree so they need to be updated
  // now
  updateProblemsButton();
}


void MainWindow::externalMessage(const QString &message)
{
  if (message.left(6).toLower() == "nxm://") {
    MessageDialog::showMessage(tr("Download started"), this);
    downloadRequestedNXM(message);
  }
}


void MainWindow::modStatusChanged(unsigned int index)
{
  try {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
    if (m_CurrentProfile->modEnabled(index)) {
      DirectoryRefresher::addModToStructure(m_DirectoryStructure, modInfo->name(), m_CurrentProfile->getModPriority(index), modInfo->absolutePath());
      DirectoryRefresher::cleanStructure(m_DirectoryStructure);
    } else {
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        FilesOrigin &origin = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
        origin.enable(false);
      }
    }

    for (unsigned int i = 0; i < m_CurrentProfile->numMods(); ++i) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      int priority = m_CurrentProfile->getModPriority(i);
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        m_DirectoryStructure->getOriginByName(ToWString(modInfo->name())).setPriority(priority);
      }
    }
    m_DirectoryStructure->getFileRegister()->sortOrigins();

    refreshLists();
  } catch (const std::exception& e) {
    reportError(tr("failed to update mod list: %1").arg(e.what()));
  }
}


void MainWindow::removeOrigin(const QString &name)
{
  FilesOrigin &origin = m_DirectoryStructure->getOriginByName(ToWString(name));
  origin.enable(false);
  refreshLists();
}


void MainWindow::modorder_changed()
{
  for (unsigned int i = 0; i < m_CurrentProfile->numMods(); ++i) {
    int priority = m_CurrentProfile->getModPriority(i);
    if (m_CurrentProfile->modEnabled(i)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      m_DirectoryStructure->getOriginByName(ToWString(modInfo->name())).setPriority(priority);
    }
  }
  m_DirectoryStructure->getFileRegister()->sortOrigins();
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


void MainWindow::on_profileRefreshBtn_clicked()
{
  m_CurrentProfile->writeModlist();

//  m_ModList.updateModCollection();
  refreshModList();
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


void MainWindow::renameModInList(QFile &modList, const QString &oldName, const QString &newName)
{
  //TODO this code needs to be merged with ModList::readFrom
  if (!modList.open(QIODevice::ReadWrite)) {
    reportError(tr("failed to open %1").arg(modList.fileName()));
    return;
  }

  QBuffer outBuffer;
  outBuffer.open(QIODevice::WriteOnly);

  while (!modList.atEnd()) {
    QByteArray line = modList.readLine();
    if (line.length() == 0) {
      break;
    }
    if (line.at(0) == '#') {
      outBuffer.write(line);
      continue;
    }
    QString modName = QString::fromUtf8(line.constData()).trimmed();
    if (modName.isEmpty()) {
      break;
    }

    bool disabled = false;
    if (modName.at(0) == '-') {
      disabled = true;
      modName = modName.mid(1);
    } else if (modName.at(0) == '+') {
      modName = modName.mid(1);
    }

    if (modName == oldName) {
      modName = newName;
    }

    if (disabled) {
      outBuffer.write("-");
    } else {
      outBuffer.write("+");
    }
    outBuffer.write(modName.toUtf8().constData());
    outBuffer.write("\r\n");
  }

  modList.resize(0);
  modList.write(outBuffer.buffer());
  modList.close();
}


void MainWindow::modRenamed(const QString &oldName, const QString &newName)
{
  // fix the profiles directly on disc
  for (int i = 0; i < ui->profileBox->count(); ++i) {
    QString profileName = ui->profileBox->itemText(i);

    //TODO this functionality should be in the Profile class
    QString modlistName = QString("%1/%2/modlist.txt")
                            .arg(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getProfilesDir())))
                            .arg(profileName);

    QFile modList(modlistName);
    if (modList.exists()) {
      qDebug("rewrite modlist %s", QDir::toNativeSeparators(modlistName).toUtf8().constData());
      renameModInList(modList, oldName, newName);
    }
  }

  // immediately refresh the active profile because the data in memory is invalid
  m_CurrentProfile->refreshModStatus();

  // also fix the directory structure
  try {
    if (m_DirectoryStructure->originExists(ToWString(oldName))) {
      FilesOrigin &origin = m_DirectoryStructure->getOriginByName(ToWString(oldName));
      origin.setName(ToWString(newName));
    } else {

    }
  } catch (const std::exception &e) {
    reportError(tr("failed to change origin name: %1").arg(e.what()));
  }
}


void MainWindow::modlistChanged(int)
{
  m_ModListSortProxy->invalidate();
}


void MainWindow::fileMoved(const QString &filePath, const QString &oldOriginName, const QString &newOriginName)
{
  const FileEntry::Ptr filePtr = m_DirectoryStructure->findFile(ToWString(filePath));
  if (filePtr.get() != NULL) {
    try {
      FilesOrigin &oldOrigin = m_DirectoryStructure->getOriginByName(ToWString(oldOriginName));
      FilesOrigin &newOrigin = m_DirectoryStructure->getOriginByName(ToWString(newOriginName));

      QString fullNewPath = ToQString(newOrigin.getPath()) + "\\" + filePath;
      WIN32_FIND_DATAW findData;
      ::FindFirstFileW(ToWString(fullNewPath).c_str(), &findData);

      filePtr->addOrigin(newOrigin.getID(), findData.ftCreationTime, L"");
      filePtr->removeOrigin(oldOrigin.getID());
    } catch (const std::exception &e) {
      reportError(tr("Failed to move \"%1\" from mod \"%2\" to \"%3\": %4").arg(filePath).arg(oldOriginName).arg(newOriginName).arg(e.what()));
    }
  } else {
    // this is probably not an error, the specified path is likely a directory
  }
}


QTreeWidgetItem *MainWindow::addFilterItem(QTreeWidgetItem *root, const QString &name, int categoryID)
{
  QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(name));
  item->setData(0, Qt::UserRole, categoryID);
  if (root != NULL) {
    root->addChild(item);
  } else {
    ui->categoriesList->addTopLevelItem(item);
  }
  return item;
}


void MainWindow::addCategoryFilters(QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID)
{
  for (unsigned i = 1; i < m_CategoryFactory.numCategories(); ++i) {
    if ((m_CategoryFactory.getParentID(i) == targetID)) {
      int categoryID = m_CategoryFactory.getCategoryID(i);
      if (categoriesUsed.find(categoryID) != categoriesUsed.end()) {
        QTreeWidgetItem *item = addFilterItem(root, m_CategoryFactory.getCategoryName(i), categoryID);
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

  ui->modList->setCurrentIndex(QModelIndex());

  QStringList selectedItems;
  foreach (QTreeWidgetItem *item, ui->categoriesList->selectedItems()) {
    selectedItems.append(item->text(0));
  }

  ui->categoriesList->clear();
  addFilterItem(NULL, tr("<Checked>"), CategoryFactory::CATEGORY_SPECIAL_CHECKED);
  addFilterItem(NULL, tr("<Unchecked>"), CategoryFactory::CATEGORY_SPECIAL_UNCHECKED);
  addFilterItem(NULL, tr("<Update>"), CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE);
  addFilterItem(NULL, tr("<No category>"), CategoryFactory::CATEGORY_SPECIAL_NOCATEGORY);
  addFilterItem(NULL, tr("<Conflicted>"), CategoryFactory::CATEGORY_SPECIAL_CONFLICT);
  addFilterItem(NULL, tr("<Not Endorsed>"), CategoryFactory::CATEGORY_SPECIAL_NOTENDORSED);

  std::set<int> categoriesUsed;
  for (unsigned int modIdx = 0; modIdx < ModInfo::getNumMods(); ++modIdx) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIdx);
    BOOST_FOREACH (int categoryID, modInfo->getCategories()) {
      int currentID = categoryID;
      // also add parents so they show up in the tree
      while (currentID != 0) {
        categoriesUsed.insert(currentID);
        currentID = m_CategoryFactory.getParentID(m_CategoryFactory.getCategoryIndex(currentID));
      }
    }
  }

  addCategoryFilters(NULL, categoriesUsed, 0);

  foreach (const QString &item, selectedItems) {
    QList<QTreeWidgetItem*> matches = ui->categoriesList->findItems(item, Qt::MatchFixedString | Qt::MatchRecursive);
    if (matches.size() > 0) {
      matches.at(0)->setSelected(true);
    }
  }

  ui->modList->selectionModel()->select(currentSelection, QItemSelectionModel::Select);
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
    QDir modDir(QDir::fromNativeSeparators(m_Settings.getModDirectory()));
    if (!modDir.exists(regName) ||
        (QMessageBox::question(this, tr("Overwrite?"),
          tr("This will replace the existing mod \"%1\". Continue?").arg(regName),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
      if (modDir.exists(regName) && !shellDelete(QStringList(modDir.absoluteFilePath(regName)))) {
        reportError(tr("failed to remove mod \"%1\"").arg(regName));
      } else {
        QString destinationPath = QDir::fromNativeSeparators(m_Settings.getModDirectory()) + "/" + regName;
        if (!modDir.rename(modInfo->absolutePath(), destinationPath)) {
          reportError(tr("failed to rename \"%1\" to \"%2\"").arg(modInfo->absolutePath()).arg(destinationPath));
        }
        refreshModList();
      }
    }
  }
}


void MainWindow::modlistChanged(const QModelIndex &index, int role)
{
  if (role == Qt::CheckStateRole) {
    if (index.data(Qt::CheckStateRole).toBool()) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(index.row());

      QDir dir(modInfo->absolutePath());
      foreach (const QString &esm, dir.entryList(QStringList("*.esm"), QDir::Files)) {
        m_PluginList.enableESP(esm);
      }

      int enabled = 0;
      QStringList esps = dir.entryList(QStringList("*.esp"), QDir::Files);
      foreach (const QString &esp, esps) {
        if (!m_PluginList.isEnabled(esp)) {
          m_PluginList.enableESP(esp);
          ++enabled;
        }
      }
      if (enabled > 1) {
        MessageDialog::showMessage(tr("Multiple esps activated, please check that they don't conflict."), this);
      }
      m_PluginList.refreshLoadOrder();
      // immediately save plugin list
      savePluginList();
    }
  }
}


void MainWindow::removeMod_clicked()
{
  try {
    QItemSelectionModel *selection = ui->modList->selectionModel();
    if (selection->hasSelection() && selection->selectedRows().count() > 1) {
      QString mods;
      QStringList modNames;
      foreach (QModelIndex idx, selection->selectedRows()) {
//        QString name = ModInfo::getByIndex(m_ModListGroupProxy->mapToSource(idx).row())->name();
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
        foreach (QString name, modNames) {
          m_ModList.removeRowForce(ModInfo::getIndex(name));
        }
      }
    } else {
      m_ModList.removeRow(m_ContextRow, QModelIndex());
    }
  } catch (const std::exception &e) {
    reportError(tr("failed to remove mod: %1").arg(e.what()));
  }
}


void MainWindow::modRemoved(const QString &fileName)
{
  if (!fileName.isEmpty() && !QFileInfo(fileName).isAbsolute()) {
    int index = m_DownloadManager.indexByName(fileName);
    if (index >= 0) {
      m_DownloadManager.markUninstalled(index);
    }
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
        fullInstallationFile = m_DownloadManager.getOutputDirectory().append("/").append(fileInfo.fileName());
      }
    } else {
      fullInstallationFile = m_DownloadManager.getOutputDirectory().append("/").append(installationFile);
    }
    if (QFile::exists(fullInstallationFile)) {
      installMod(fullInstallationFile);
    } else {
      QMessageBox::information(this, tr("Failed"), tr("Installation file no longer exists"));
    }
  } else {
    QMessageBox::information(this, tr("Failed"),
                             tr("Mods installed with old versions of MO can't be reinstalled in this way."));
  }
}


void MainWindow::endorseMod(ModInfo::Ptr mod)
{
  if (NexusInterface::instance()->getAccessManager()->loggedIn()) {
    mod->endorse(true);
  } else {
    QString username, password;
    if (m_Settings.getNexusLogin(username, password)) {
      m_PostLoginTasks.push_back(boost::bind(&MainWindow::endorseMod, _1, mod));
      NexusInterface::instance()->getAccessManager()->login(username, password);
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
  if (NexusInterface::instance()->getAccessManager()->loggedIn()) {
    ModInfo::getByIndex(m_ContextRow)->endorse(false);
  } else {
    if (m_Settings.getNexusLogin(username, password)) {
      m_PostLoginTasks.push_back(boost::mem_fn(&MainWindow::unendorse_clicked));
      NexusInterface::instance()->getAccessManager()->login(username, password);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to endorse"), this);
    }
  }
}


void MainWindow::overwriteClosed(int)
{
  QDialog *dialog = this->findChild<QDialog*>("__overwriteDialog");
  if (dialog != NULL) {
    dialog->deleteLater();
  }
}


void MainWindow::displayModInformation(ModInfo::Ptr modInfo, unsigned int index, int tab)
{
  std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
    QDialog *dialog = this->findChild<QDialog*>("__overwriteDialog");
    if (dialog == NULL) {
      dialog = new OverwriteInfoDialog(modInfo, this);
      dialog->setObjectName("__overwriteDialog");
    }
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    connect(dialog, SIGNAL(finished(int)), this, SLOT(overwriteClosed(int)));
  } else {
    ModInfoDialog dialog(modInfo, m_DirectoryStructure, this);
    connect(&dialog, SIGNAL(nexusLinkActivated(QString)), this, SLOT(nexusLinkActivated(QString)));
    connect(&dialog, SIGNAL(downloadRequest(QString)), this, SLOT(downloadRequestedNXM(QString)));
    connect(&dialog, SIGNAL(modOpen(QString, int)), this, SLOT(displayModInformation(QString, int)), Qt::QueuedConnection);
    connect(&dialog, SIGNAL(modOpenNext()), this, SLOT(modOpenNext()), Qt::QueuedConnection);
    connect(&dialog, SIGNAL(modOpenPrev()), this, SLOT(modOpenPrev()), Qt::QueuedConnection);
    connect(&dialog, SIGNAL(originModified(int)), this, SLOT(originModified(int)));
    connect(&dialog, SIGNAL(endorseMod(ModInfo::Ptr)), this, SLOT(endorseMod(ModInfo::Ptr)));

    dialog.openTab(tab);
    dialog.exec();
    modInfo->saveMeta();
    emit modInfoDisplayed();
  }

  if (m_CurrentProfile->modEnabled(index)) {
    FilesOrigin& origin = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
    origin.enable(false);

    if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
      FilesOrigin& origin = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
      origin.enable(false);

      DirectoryRefresher::addModToStructure(m_DirectoryStructure,
                                            modInfo->name(), m_CurrentProfile->getModPriority(index),
                                            modInfo->absolutePath());
      DirectoryRefresher::cleanStructure(m_DirectoryStructure);
      refreshLists();
    }
  }
}


void MainWindow::modOpenNext()
{
  QModelIndex index = m_ModListSortProxy->mapFromSource(m_ModList.index(m_ContextRow, 0));
  index = m_ModListSortProxy->index((index.row() + 1) % m_ModListSortProxy->rowCount(), 0);

  m_ContextRow = m_ModListSortProxy->mapToSource(index).row();
  ModInfo::Ptr mod = ModInfo::getByIndex(m_ContextRow);
  std::vector<ModInfo::EFlag> flags = mod->getFlags();
  if ((std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) ||
      (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end())) {
    // skip overwrite and backups
    modOpenNext();
  } else {
    displayModInformation(m_ContextRow);
  }
}

void MainWindow::modOpenPrev()
{
  QModelIndex index = m_ModListSortProxy->mapFromSource(m_ModList.index(m_ContextRow, 0));
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
    modOpenPrev();
  } else {
    displayModInformation(m_ContextRow);
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


void MainWindow::testExtractBSA(int modIndex)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
  QDir dir(modInfo->absolutePath());

  QFileInfoList archives = dir.entryInfoList(QStringList("*.bsa"));
  if (archives.length() != 0 &&
      (QuestionBoxMemory::query(this, m_Settings.directInterface(), "unpackBSA", tr("Extract BSA"),
                             tr("This mod contains at least one BSA. Do you want to unpack it?\n"
                                "(This removes the BSA after completion. If you don't know about BSAs, just select no)"),
                             QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No) == QMessageBox::Yes)) {


    foreach (QFileInfo archiveInfo, archives) {
      BSA::Archive archive;

      BSA::EErrorCode result = archive.read(archiveInfo.absoluteFilePath().toLocal8Bit().constData());
      if ((result != BSA::ERROR_NONE) && (result != BSA::ERROR_INVALIDHASHES)) {
        reportError(tr("failed to read %1: %2").arg(archiveInfo.fileName()).arg(result));
        return;
      }

      QProgressDialog progress(this);
      progress.setMaximum(100);
      progress.setValue(0);
      progress.show();

      archive.extractAll(modInfo->absolutePath().toUtf8().constData(),
                         boost::bind(&MainWindow::extractProgress, this, boost::ref(progress), _1, _2));

      if (result == BSA::ERROR_INVALIDHASHES) {
        reportError(tr("This archive contains invalid hashes. Some files may be broken."));
      }

      archive.close();

      if (!QFile::remove(archiveInfo.absoluteFilePath())) {
        qCritical("failed to remove archive %s", archiveInfo.absoluteFilePath().toUtf8().constData());
      } else {
        m_DirectoryStructure->removeFile(ToWString(archiveInfo.fileName()));
      }
    }

    refreshBSAList();
  }
}


void MainWindow::ignoreMissingData_clicked()
{
  ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
  QDir(info->absolutePath()).mkdir("textures");
  info->testValid();
  connect(this, SIGNAL(modListDataChanged(QModelIndex,QModelIndex)), &m_ModList, SIGNAL(dataChanged(QModelIndex,QModelIndex)));

  emit modListDataChanged(m_ModList.index(m_ContextRow, 0), m_ModList.index(m_ContextRow, m_ModList.columnCount() - 1));
}


void MainWindow::visitOnNexus_clicked()
{
  int modID = m_ModList.data(m_ModList.index(m_ContextRow, 0), Qt::UserRole).toInt();
  if (modID > 0)  {
    nexusLinkActivated(QString("%1/downloads/file.php?id=%2").arg(ToQString(GameInfo::instance().getNexusPage())).arg(modID));
  } else {
    MessageDialog::showMessage(tr("Nexus ID for this Mod is unknown"), this);
  }
}

void MainWindow::openExplorer_clicked()
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);

  ::ShellExecuteW(NULL, L"explore", ToWString(modInfo->absolutePath()).c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void MainWindow::information_clicked()
{
  try {
    displayModInformation(m_ContextRow);
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}

void MainWindow::syncOverwrite()
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  SyncOverwriteDialog syncDialog(modInfo->absolutePath(), m_DirectoryStructure, this);
  if (syncDialog.exec() == QDialog::Accepted) {
    syncDialog.apply(QDir::fromNativeSeparators(m_Settings.getModDirectory()));
    modInfo->testValid();
    refreshDirectoryStructure();
  }
}

void MainWindow::createModFromOverwrite()
{
  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);

  while (name->isEmpty()) {
    bool ok;
    name.update(QInputDialog::getText(this, tr("Create Mod..."),
                                      tr("This will move all files from overwrite into a new, regular mod.\n"
                                         "Please enter a name: "), QLineEdit::Normal, "", &ok),
                GUESS_USER);
    if (!ok) {
      return;
    }
  }

  if (getMod(name) != NULL) {
    reportError(tr("A mod with this name already exists"));
    return;
  }

  IModInterface *newMod = createMod(name);
  if (newMod == NULL) {
    return;
  }

  ModInfo::Ptr overwriteInfo = ModInfo::getByIndex(m_ContextRow);
  shellMove(QStringList(QDir::toNativeSeparators(overwriteInfo->absolutePath()) + "\\*"),
            QStringList(QDir::toNativeSeparators(newMod->absolutePath())), this);

  refreshModList();
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
  QModelIndex sourceIdx = mapToModel(&m_ModList, index);
  if (!sourceIdx.isValid()) {
    return;
  }

  try {
    m_ContextRow = m_ModListSortProxy->mapToSource(index).row();
//    displayModInformation(m_ModListSortProxy->mapToSource(index).row());
    displayModInformation(sourceIdx.row());
    // workaround to cancel the editor that might have opened because of
    // selection-click
    ui->modList->closePersistentEditor(index);
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}

bool MainWindow::addCategories(QMenu *menu, int targetID)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  const std::set<int> &categories = modInfo->getCategories();

  bool childEnabled = false;

  for (unsigned i = 1; i < m_CategoryFactory.numCategories(); ++i) {
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
        if (addCategories(targetMenu, m_CategoryFactory.getCategoryID(i)) || enabled) {
          targetMenu->setIcon(QIcon(":/MO/gui/resources/check.png"));
        }
      }
    }
  }
  return childEnabled;
}

void MainWindow::saveCategoriesFromMenu(QMenu *menu, int modRow)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(modRow);
  foreach (QAction* action, menu->actions()) {
    if (action->menu() != NULL) {
      saveCategoriesFromMenu(action->menu(), modRow);
    } else {
      QWidgetAction *widgetAction = qobject_cast<QWidgetAction*>(action);
      if (widgetAction != NULL) {
        QCheckBox *checkbox = qobject_cast<QCheckBox*>(widgetAction->defaultWidget());
        modInfo->setCategory(widgetAction->data().toInt(), checkbox->isChecked());
      }
    }
  }
}

void MainWindow::saveCategories()
{
  QMenu *menu = qobject_cast<QMenu*>(sender());
  if (menu == NULL) {
    qCritical("not a menu?");
    return;
  }

  QModelIndexList selected = ui->modList->selectionModel()->selectedRows();

  if (selected.size() > 0) {
    int min = INT_MAX;
    int max = INT_MIN;

    QStringList selectedMods;
    for (int i = 0; i < selected.size(); ++i) {
      QModelIndex temp = mapToModel(&m_ModList, selected.at(i));
      selectedMods.append(temp.data().toString());
      if (temp.row() < min) min = temp.row();
      if (temp.row() > max) max = temp.row();
      saveCategoriesFromMenu(menu, mapToModel(&m_ModList, selected.at(i)).row());
    }
    //m_ModList.notifyChange(min, max);
    m_ModList.notifyChange(-1);
//    refreshModList();

    // find mods by their name because indices are invalidated
    QAbstractItemModel *model = ui->modList->model();
    Q_FOREACH(const QString &mod, selectedMods) {
      QModelIndexList matches = model->match(model->index(0, 0), Qt::DisplayRole, mod, 1,
                                             Qt::MatchFixedString | Qt::MatchCaseSensitive | Qt::MatchRecursive);
      if (matches.size() > 0) {
        ui->modList->selectionModel()->select(matches.at(0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
      }
    }
  } else {
    saveCategoriesFromMenu(menu, m_ContextRow);
    m_ModList.notifyChange(m_ContextRow);
  }

  refreshFilters();
}

void MainWindow::savePrimaryCategory()
{
  QMenu *menu = qobject_cast<QMenu*>(sender());
  if (menu == NULL) {
    qCritical("not a menu?");
    return;
  }

  foreach (QAction* action, menu->actions()) {
    QWidgetAction *widgetAction = qobject_cast<QWidgetAction*>(action);
    if (widgetAction != NULL) {
      QRadioButton *btn = qobject_cast<QRadioButton*>(widgetAction->defaultWidget());
      if (btn->isChecked()) {
        QModelIndexList selected = ui->modList->selectionModel()->selectedRows();
        for (int i = 0; i < selected.size(); ++i) {
          ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ModListSortProxy->mapToSource(selected.at(i)).row());
          modInfo->setPrimaryCategory(widgetAction->data().toInt());
        }
        break;
      }
    }
  }
}

void MainWindow::checkModsForUpdates()
{
  statusBar()->show();
  if (NexusInterface::instance()->getAccessManager()->loggedIn()) {
    m_ModsToUpdate = ModInfo::checkAllForUpdate(this);
    m_RefreshProgress->setRange(0, m_ModsToUpdate);
  } else {
    QString username, password;
    if (m_Settings.getNexusLogin(username, password)) {
      m_PostLoginTasks.push_back(boost::mem_fn(&MainWindow::checkModsForUpdates));
      NexusInterface::instance()->getAccessManager()->login(username, password);
    } else { // otherwise there will be no endorsement info
      m_ModsToUpdate = ModInfo::checkAllForUpdate(this);
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
  ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
  info->ignoreUpdate(true);
}

void MainWindow::unignoreUpdate()
{
  ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
  info->ignoreUpdate(false);
}

void MainWindow::addPrimaryCategoryCandidates(QMenu *primaryCategoryMenu, ModInfo::Ptr info)
{
  const std::set<int> &categories = info->getCategories();
  foreach (int categoryID, categories) {
    int catIdx = m_CategoryFactory.getCategoryIndex(categoryID);
    QWidgetAction *action = new QWidgetAction(primaryCategoryMenu);
    try {
      QRadioButton *categoryBox = new QRadioButton(m_CategoryFactory.getCategoryName(catIdx).replace('&', "&&"),
                                             primaryCategoryMenu);
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
  if (menu == NULL) {
    qCritical("not a menu?");
    return;
  }
  menu->clear();
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);

  addPrimaryCategoryCandidates(menu, modInfo);
}

void MainWindow::enableVisibleMods()
{
  if (QMessageBox::question(NULL, tr("Confirm"), tr("Really enable all visible mods?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    m_ModListSortProxy->enableAllVisible();
  }
}

void MainWindow::disableVisibleMods()
{
  if (QMessageBox::question(NULL, tr("Confirm"), tr("Really disable all visible mods?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    m_ModListSortProxy->disableAllVisible();
  }
}

void MainWindow::exportModListCSV()
{
  SelectionDialog selection(tr("Choose what to export"));

  selection.addChoice(tr("Everything"), tr("All installed mods are included in the list"), 0);
  selection.addChoice(tr("Active Mods"), tr("Only active (checked) mods from your current profile are included"), 1);
  selection.addChoice(tr("Visible"), tr("All mods visible in the mod list are included"), 2);

  if (selection.exec() == QDialog::Accepted) {
    unsigned int numMods = ModInfo::getNumMods();

    try {
      QBuffer buffer;
      buffer.open(QIODevice::ReadWrite);
      CSVBuilder builder(&buffer);
      builder.setEscapeMode(CSVBuilder::TYPE_STRING, CSVBuilder::QUOTE_ALWAYS);
      std::vector<std::pair<QString, CSVBuilder::EFieldType> > fields;
      fields.push_back(std::make_pair(QString("mod_id"), CSVBuilder::TYPE_INTEGER));
      fields.push_back(std::make_pair(QString("mod_installed_name"), CSVBuilder::TYPE_STRING));
      fields.push_back(std::make_pair(QString("mod_version"), CSVBuilder::TYPE_STRING));
      fields.push_back(std::make_pair(QString("file_installed_name"), CSVBuilder::TYPE_STRING));
//      fields.push_back(std::make_pair(QString("file_category"), CSVBuilder::TYPE_INTEGER));
      builder.setFields(fields);

      builder.writeHeader();

      for (unsigned int i = 0; i < numMods; ++i) {
        ModInfo::Ptr info = ModInfo::getByIndex(i);
        bool enabled = m_CurrentProfile->modEnabled(i);
        if ((selection.getChoiceData().toInt() == 1) && !enabled) {
          continue;
        } else if ((selection.getChoiceData().toInt() == 2) && !m_ModListSortProxy->filterMatches(info, enabled)) {
          continue;
        }
        std::vector<ModInfo::EFlag> flags = info->getFlags();
        if ((std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) == flags.end()) &&
            (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) == flags.end())) {
          builder.setRowField("mod_id", info->getNexusID());
          builder.setRowField("mod_installed_name", info->name());
          builder.setRowField("mod_version", info->getVersion().canonicalString());
          builder.setRowField("file_installed_name", info->getInstallationFile());
          builder.writeRow();
        }
      }

      SaveTextAsDialog saveDialog(this);
      saveDialog.setText(buffer.data());
      saveDialog.exec();
    } catch (const std::exception &e) {
      reportError(tr("export failed: %1").arg(e.what()));
    }
  }
}

void addMenuAsPushButton(QMenu *menu, QMenu *subMenu)
{
  QPushButton *pushBtn = new QPushButton(subMenu->title());
  pushBtn->setMenu(subMenu);
  QWidgetAction *action = new QWidgetAction(menu);
  action->setDefaultWidget(pushBtn);
  menu->addAction(action);
}

void MainWindow::on_modList_customContextMenuRequested(const QPoint &pos)
{
  try {
    QTreeView *modList = findChild<QTreeView*>("modList");

    QModelIndex index = mapToModel(&m_ModList, modList->indexAt(pos));
    m_ContextRow = index.row();

    QMenu menu;

    menu.addAction(tr("Install Mod..."), this, SLOT(installMod_clicked()));

    menu.addAction(tr("Enable all visible"), this, SLOT(enableVisibleMods()));
    menu.addAction(tr("Disable all visible"), this, SLOT(disableVisibleMods()));

    menu.addAction(tr("Check all for update"), this, SLOT(checkModsForUpdates()));

    menu.addAction(tr("Refresh"), this, SLOT(on_profileRefreshBtn_clicked()));

    menu.addAction(tr("Export to csv..."), this, SLOT(exportModListCSV()));

    if (m_ContextRow != -1) {
      menu.addSeparator();
      ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
      std::vector<ModInfo::EFlag> flags = info->getFlags();
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
        if (QDir(info->absolutePath()).count() > 2) {
          menu.addAction(tr("Sync to Mods..."), this, SLOT(syncOverwrite()));
          menu.addAction(tr("Create Mod..."), this, SLOT(createModFromOverwrite()));
        }
      } else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end()) {
        menu.addAction(tr("Restore Backup"), this, SLOT(restoreBackup_clicked()));
        menu.addAction(tr("Remove Backup..."), this, SLOT(removeMod_clicked()));
      } else {
        // Set categories is a separate menu connected to a push button. This way it doesn't simply close every time you hover the mouse outside
        QMenu *addCategoryMenu = new QMenu(tr("Set Category"));
        addCategories(addCategoryMenu, 0);
        connect(addCategoryMenu, SIGNAL(aboutToHide()), this, SLOT(saveCategories()));
        addMenuAsPushButton(&menu, addCategoryMenu);

        QMenu *primaryCategoryMenu = new QMenu(tr("Primary Category"));
        connect(primaryCategoryMenu, SIGNAL(aboutToShow()), this, SLOT(addPrimaryCategoryCandidates()));
        connect(primaryCategoryMenu, SIGNAL(aboutToHide()), this, SLOT(savePrimaryCategory()));
        addMenuAsPushButton(&menu, primaryCategoryMenu);

        menu.addSeparator();
        if (info->downgradeAvailable()) {
          menu.addAction(tr("Change versioning scheme"), this, SLOT(changeVersioningScheme()));
        }
        if (info->updateAvailable() || info->downgradeAvailable()) {
          if (info->updateIgnored()) {
            menu.addAction(tr("Un-ignore update"), this, SLOT(unignoreUpdate()));
          } else {
            menu.addAction(tr("Ignore update"), this, SLOT(ignoreUpdate()));
          }
        }
        menu.addSeparator();

        menu.addAction(tr("Rename Mod..."), this, SLOT(renameMod_clicked()));
        menu.addAction(tr("Remove Mod..."), this, SLOT(removeMod_clicked()));
        menu.addAction(tr("Reinstall Mod"), this, SLOT(reinstallMod_clicked()));
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
        std::vector<ModInfo::EFlag> flags = info->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
          menu.addAction(tr("Ignore missing data"), this, SLOT(ignoreMissingData_clicked()));
        }

        menu.addAction(tr("Visit on Nexus"), this, SLOT(visitOnNexus_clicked()));
        menu.addAction(tr("Open in explorer"), this, SLOT(openExplorer_clicked()));
      }

      QAction *infoAction = menu.addAction(tr("Information..."), this, SLOT(information_clicked()));
      menu.setDefaultAction(infoAction);
    }

    menu.exec(modList->mapToGlobal(pos));
  } catch (const std::exception &e) {
    reportError(tr("Exception: ").arg(e.what()));
  } catch (...) {
    reportError(tr("Unknown exception"));
  }
}


void MainWindow::on_categoriesList_itemSelectionChanged()
{
//    int filter = current->data(0, Qt::UserRole).toInt();
  QModelIndexList indices = ui->categoriesList->selectionModel()->selectedRows();
  std::vector<int> categories;
  foreach (const QModelIndex &index, indices) {
    int categoryId = index.data(Qt::UserRole).toInt();
    if (categoryId != CategoryFactory::CATEGORY_NONE) {
      categories.push_back(categoryId);
    }
  }

  m_ModListSortProxy->setCategoryFilter(categories);
//  ui->currentCategoryLabel->setText(QString("(%1)").arg(current->text(0)));
  if (indices.count() == 0) {
    ui->currentCategoryLabel->setText(QString("(%1)").arg(tr("<All>")));
  } else if (indices.count() > 1) {
    ui->currentCategoryLabel->setText(QString("(%1)").arg(tr("<Multiple>")));
  } else {
    ui->currentCategoryLabel->setText(QString("(%1)").arg(indices.first().data().toString()));
  }
  ui->modList->reset();
}


void MainWindow::fixMods_clicked()
{
  QListWidgetItem *selectedItem = ui->savegameList->item(m_SelectedSaveGame);
  if (selectedItem == NULL) {
    return;
  }

  // if required, parse the save game
  if (selectedItem->data(Qt::UserRole).isNull()) {
    QVariant temp;
    SaveGameGamebryo *save = getSaveGame(selectedItem->data(Qt::UserRole).toString());
    save->setParent(selectedItem->listWidget());
    temp.setValue(save);
    selectedItem->setData(Qt::UserRole, temp);
  }

  const SaveGameGamebryo *save = getSaveGame(selectedItem);

  // collect the list of missing plugins
  std::map<QString, std::vector<QString> > missingPlugins;

  for (int i = 0; i < save->numPlugins(); ++i) {
    const QString &pluginName = save->plugin(i);
    if (!m_PluginList.isEnabled(pluginName)) {
      missingPlugins[pluginName] = std::vector<QString>();
    }
  }

  // figure out, for each esp/esm, which mod, if any, contains it
  QStringList espFilter("*.esp");
  espFilter.append("*.esm");

  // search in data
  {
    QDir dataDir(m_GamePath + "/data");
    QStringList esps = dataDir.entryList(espFilter);
    foreach (const QString &esp, esps) {
      std::map<QString, std::vector<QString> >::iterator iter = missingPlugins.find(esp);
      if (iter != missingPlugins.end()) {
        iter->second.push_back("<data>");
      }
    }
  }

  // search in mods
  for (unsigned int i = 0; i < m_CurrentProfile->numRegularMods(); ++i) {
    int modIndex = m_CurrentProfile->modIndexByPriority(i);
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);

    QStringList esps = QDir(modInfo->absolutePath()).entryList(espFilter);
    foreach (const QString &esp, esps) {
      std::map<QString, std::vector<QString> >::iterator iter = missingPlugins.find(esp);
      if (iter != missingPlugins.end()) {
        iter->second.push_back(modInfo->name());
      }
    }
  }

  // search in overwrite
  {
    QDir overwriteDir(ToQString(GameInfo::instance().getOverwriteDir()));
    QStringList esps = overwriteDir.entryList(espFilter);
    foreach (const QString &esp, esps) {
      std::map<QString, std::vector<QString> >::iterator iter = missingPlugins.find(esp);
      if (iter != missingPlugins.end()) {
        iter->second.push_back("<overwrite>");
      }
    }
  }


  ActivateModsDialog dialog(missingPlugins, this);
  if (dialog.exec() == QDialog::Accepted) {
    // activate the required mods, then enable all esps
    std::set<QString> modsToActivate = dialog.getModsToActivate();
    for (std::set<QString>::iterator iter = modsToActivate.begin(); iter != modsToActivate.end(); ++iter) {
      if ((*iter != "<data>") && (*iter != "<overwrite>")) {
        unsigned int modIndex = ModInfo::getIndex(*iter);
        m_CurrentProfile->setModEnabled(modIndex, true);
      }
    }

    m_CurrentProfile->writeModlist();
    refreshLists();

    std::set<QString> espsToActivate = dialog.getESPsToActivate();
    for (std::set<QString>::iterator iter = espsToActivate.begin(); iter != espsToActivate.end(); ++iter) {
      m_PluginList.enableESP(*iter);
    }
    saveCurrentLists();
  }
}


void MainWindow::on_savegameList_customContextMenuRequested(const QPoint &pos)
{
  QListWidgetItem *selectedItem = ui->savegameList->itemAt(pos);
  if (selectedItem == NULL) {
    return;
  }

  m_SelectedSaveGame = ui->savegameList->row(selectedItem);

  QMenu menu;
  menu.addAction(tr("Fix Mods..."), this, SLOT(fixMods_clicked()));

  menu.exec(ui->savegameList->mapToGlobal(pos));
}

void MainWindow::linkToolbar()
{
  const Executable &selectedExecutable = ui->executablesListBox->itemData(ui->executablesListBox->currentIndex()).value<Executable>();
  Executable &exe = m_ExecutablesList.find(selectedExecutable.m_Title);
  exe.m_Toolbar = !exe.m_Toolbar;
  updateToolBar();
}

void MainWindow::linkDesktop()
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");

  const Executable &selectedExecutable = executablesList->itemData(executablesList->currentIndex()).value<Executable>();
  QString linkName = getDesktopDirectory() + "\\" + selectedExecutable.m_Title + ".lnk";

  if (QFile::exists(linkName)) {
    if (QFile::remove(linkName)) {
      ui->linkButton->menu()->actions().at(0)->setIcon(QIcon(":/MO/gui/link"));
    } else {
      reportError(tr("failed to remove %1").arg(linkName));
    }
  } else {
    QFileInfo exeInfo(m_ExeName);
    // create link
    std::wstring targetFile       = ToWString(exeInfo.absoluteFilePath());
    std::wstring parameter        = ToWString(QString("\"") + selectedExecutable.m_BinaryInfo.absoluteFilePath() + "\" " + selectedExecutable.m_Arguments);
    std::wstring description      = ToWString(selectedExecutable.m_BinaryInfo.fileName());
    std::wstring currentDirectory = ToWString(selectedExecutable.m_BinaryInfo.absolutePath());

    if (CreateShortcut(targetFile.c_str(), parameter.c_str(),
                       linkName.toUtf8().constData(),
                       description.c_str(), currentDirectory.c_str()) != E_INVALIDARG) {
      ui->linkButton->menu()->actions().at(0)->setIcon(QIcon(":/MO/gui/remove"));
    } else {
      reportError(tr("failed to create %1").arg(linkName));
    }
  }
}

void MainWindow::linkMenu()
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");

  const Executable &selectedExecutable = executablesList->itemData(executablesList->currentIndex()).value<Executable>();
  QString linkName = getStartMenuDirectory() + "\\" + selectedExecutable.m_Title + ".lnk";

  if (QFile::exists(linkName)) {
    if (QFile::remove(linkName)) {
      ui->linkButton->menu()->actions().at(1)->setIcon(QIcon(":/MO/gui/link"));
    } else {
      reportError(tr("failed to remove %1").arg(linkName));
    }
  } else {
    QFileInfo exeInfo(m_ExeName);
    // create link
    std::wstring targetFile       = ToWString(exeInfo.absoluteFilePath());
    std::wstring parameter        = ToWString(QString("\"") + selectedExecutable.m_BinaryInfo.absoluteFilePath() + "\" " + selectedExecutable.m_Arguments);
    std::wstring description      = ToWString(selectedExecutable.m_BinaryInfo.fileName());
    std::wstring currentDirectory = ToWString(selectedExecutable.m_BinaryInfo.absolutePath());

    if (CreateShortcut(targetFile.c_str(), parameter.c_str(),
                       linkName.toUtf8().constData(),
                       description.c_str(), currentDirectory.c_str()) != E_INVALIDARG) {
      ui->linkButton->menu()->actions().at(1)->setIcon(QIcon(":/MO/gui/remove"));
    } else {
      reportError(tr("failed to create %1").arg(linkName));
    }
  }
}

void MainWindow::downloadSpeed(const QString &serverName, int bytesPerSecond)
{
  m_Settings.setDownloadSpeed(serverName, bytesPerSecond);
}


void MainWindow::on_actionSettings_triggered()
{
  QString oldModDirectory(m_Settings.getModDirectory());
  QString oldCacheDirectory(m_Settings.getCacheDirectory());
  bool proxy = m_Settings.useProxy();
  m_Settings.query(this);
  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
  fixCategories();
  refreshFilters();
  if (QDir::fromNativeSeparators(m_DownloadManager.getOutputDirectory()) != QDir::fromNativeSeparators(m_Settings.getDownloadDirectory())) {
    if (m_DownloadManager.downloadsInProgress()) {
      MessageDialog::showMessage(tr("Can't change download directory while downloads are in progress!"), this);
    } else {
      m_DownloadManager.setOutputDirectory(m_Settings.getDownloadDirectory());
    }
  }
  m_DownloadManager.setPreferredServers(m_Settings.getPreferredServers());

  if (m_Settings.getModDirectory() != oldModDirectory) {
    refreshModList();
    refreshLists();
  }

  if (m_Settings.getCacheDirectory() != oldCacheDirectory) {
    NexusInterface::instance()->setCacheDirectory(m_Settings.getCacheDirectory());
  }

  if (proxy != m_Settings.useProxy()) {
    activateProxy(m_Settings.useProxy());
  }

  NexusInterface::instance()->setNMMVersion(m_Settings.getNMMVersion());
}

void MainWindow::on_actionNexus_triggered()
{
  std::wstring nxmPath = ToWString(QApplication::applicationDirPath() + "/nxmhandler.exe");
  std::wstring executable = ToWString(QApplication::applicationFilePath());
  ::ShellExecuteW(NULL, L"open", nxmPath.c_str(),
                  (std::wstring(L"reg ") + GameInfo::instance().getGameShortName() + L" \"" + executable + L"\"").c_str(), NULL, SW_SHOWNORMAL);

  ::ShellExecuteW(NULL, L"open", GameInfo::instance().getNexusPage().c_str(), NULL, NULL, SW_SHOWNORMAL);
  ui->tabWidget->setCurrentIndex(4);
}


void MainWindow::nexusLinkActivated(const QString &link)
{
  ::ShellExecuteW(NULL, L"open", ToWString(link).c_str(), NULL, NULL, SW_SHOWNORMAL);
  ui->tabWidget->setCurrentIndex(4);
}


void MainWindow::linkClicked(const QString &url)
{
  ::ShellExecuteW(NULL, L"open", ToWString(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
}


void MainWindow::downloadRequestedNXM(const QString &url)
{
  QString username, password;

  if (!m_LoginAttempted && !NexusInterface::instance()->getAccessManager()->loggedIn() &&
      (m_Settings.getNexusLogin(username, password) ||
      (m_AskForNexusPW && queryLogin(username, password)))) {
    m_PendingDownloads.append(url);
    NexusInterface::instance()->getAccessManager()->login(username, password);
    m_LoginAttempted = true;
  } else {
    m_DownloadManager.addNXMDownload(url);
  }
}


void MainWindow::downloadRequested(QNetworkReply *reply, int modID, const QString &fileName)
{
  try {
    if (m_DownloadManager.addDownload(reply, QStringList(), fileName, modID)) {
      MessageDialog::showMessage(tr("Download started"), this);
    }
  } catch (const std::exception &e) {
    MessageDialog::showMessage(tr("Download failed"), this);
    qCritical("exception starting download: %s", e.what());
  }
}


void MainWindow::languageChange(const QString &newLanguage)
{
  if (m_Translator != NULL) {
    QCoreApplication::removeTranslator(m_Translator);
    delete m_Translator;
    m_Translator = NULL;
  }
  if (m_TranslatorQt != NULL) {
    QCoreApplication::removeTranslator(m_TranslatorQt);
    delete m_TranslatorQt;
    m_TranslatorQt = NULL;
  }

  if (newLanguage != "en_US") {
    // add our own translations
    m_Translator = new QTranslator(this);
    QString locFile = ToQString(AppConfig::translationPrefix()) + "_" + newLanguage;
    if (!m_Translator->load(locFile, QCoreApplication::applicationDirPath() + "/translations")) {
      qDebug("localization %s not found", locFile.toUtf8().constData());
    }
    QCoreApplication::installTranslator(m_Translator);

    // also add the translations for qt default strings
    m_TranslatorQt = new QTranslator(this);
    locFile = QString("qt_") + newLanguage;
    if (!m_TranslatorQt->load(locFile, QCoreApplication::applicationDirPath() + "/translations")) {
      qDebug("localization %s not found", locFile.toUtf8().constData());
    }
    QCoreApplication::installTranslator(m_TranslatorQt);
  }
  ui->retranslateUi(this);
  ui->profileBox->setItemText(0, QObject::tr("<Manage...>"));
//  ui->toolBar->addWidget(createHelpWidget(ui->toolBar));

  updateDownloadListDelegate();
  updateProblemsButton();
}


void MainWindow::installDownload(int index)
{
  try {
    QString fileName = m_DownloadManager.getFilePath(index);
    int modID = m_DownloadManager.getModID(index);
    GuessedValue<QString> modName;

    // see if there already are mods with the specified mod id
    if (modID != 0) {
      std::vector<ModInfo::Ptr> modInfo = ModInfo::getByModID(modID);
      for (auto iter = modInfo.begin(); iter != modInfo.end(); ++iter) {
        std::vector<ModInfo::EFlag> flags = (*iter)->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) == flags.end()) {
          modName.update((*iter)->name(), GUESS_PRESET);
          (*iter)->saveMeta();
        }
      }
    }

    m_CurrentProfile->writeModlistNow();

    bool hasIniTweaks = false;
    m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
    if (m_InstallationManager.install(fileName, modName, hasIniTweaks)) {
      MessageDialog::showMessage(tr("Installation successful"), this);
      refreshModList();


      QModelIndexList posList = m_ModList.match(m_ModList.index(0, 0), Qt::DisplayRole, static_cast<const QString&>(modName));
      if (posList.count() == 1) {
        ui->modList->scrollTo(posList.at(0));
      }
      int modIndex = ModInfo::getIndex(modName);
      if (modIndex != UINT_MAX) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);

        if (hasIniTweaks &&
            (QMessageBox::question(this, tr("Configure Mod"),
                tr("This mod contains ini tweaks. Do you want to configure them now?"),
                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
          displayModInformation(modInfo, modIndex, ModInfoDialog::TAB_INIFILES);
        }
        testExtractBSA(modIndex);
      } else {
        reportError(tr("mod \"%1\" not found").arg(modName));
      }
      m_DownloadManager.markInstalled(index);

      emit modInstalled();
    } else if (m_InstallationManager.wasCancelled()) {
      QMessageBox::information(this, tr("Installation cancelled"), tr("The mod was not installed completely."), QMessageBox::Ok);
    }
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}


void MainWindow::writeDataToFile(QFile &file, const QString &directory, const DirectoryEntry &directoryEntry)
{
  { // list files
//    std::set<FileEntry>::const_iterator current, end;
//    directoryEntry.getFiles(current, end);
//    for (; current != end; ++current) {

    std::vector<FileEntry::Ptr> files = directoryEntry.getFiles();
    for (auto iter = files.begin(); iter != files.end(); ++iter) {
      FileEntry::Ptr current = *iter;
      bool isArchive = false;
      int origin = current->getOrigin(isArchive);
      if (isArchive) {
        // TODO: don't list files from archives. maybe make this an option?
        continue;
      }
      QString fullName = directory;
      fullName.append("\\").append(ToQString(current->getName()));
      file.write(fullName.toUtf8());

      file.write("\t(");
      file.write(ToQString(m_DirectoryStructure->getOriginByID(origin).getName()).toUtf8());
      file.write(")\r\n");
    }
  }

  { // recurse into subdirectories
    std::vector<DirectoryEntry*>::const_iterator current, end;
    directoryEntry.getSubDirectories(current, end);
    for (; current != end; ++current) {
      writeDataToFile(file, directory.mid(0).append("\\").append(ToQString((*current)->getName())), **current);
    }
  }
}


void MainWindow::writeDataToFile()
{
  QString fileName = QFileDialog::getSaveFileName(this);
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly)) {
    reportError(tr("failed to write to file %1").arg(fileName));
  }

  writeDataToFile(file, "data", *m_DirectoryStructure);
  file.close();

  MessageDialog::showMessage(tr("%1 written").arg(QDir::toNativeSeparators(fileName)), this);
}


int MainWindow::getBinaryExecuteInfo(const QFileInfo &targetInfo,
                                    QFileInfo &binaryInfo, QString &arguments)
{
  QString extension = targetInfo.completeSuffix();
  if ((extension == "exe") ||
      (extension == "cmd") ||
      (extension == "com") ||
      (extension == "bat")) {
    binaryInfo = QFileInfo("C:\\Windows\\System32\\cmd.exe");
    arguments = QString("/C \"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
    return 1;
  } else if (extension == "jar") {
    // types that need to be injected into
    std::wstring targetPathW = ToWString(targetInfo.absoluteFilePath());
    QString binaryPath;

    { // try to find java automatically
      WCHAR buffer[MAX_PATH];
      if (::FindExecutableW(targetPathW.c_str(), NULL, buffer) > (HINSTANCE)32) {
        DWORD binaryType = 0UL;
        if (!::GetBinaryTypeW(targetPathW.c_str(), &binaryType)) {
          qDebug("failed to determine binary type: %lu", ::GetLastError());
        } else if (binaryType == SCS_32BIT_BINARY) {
          binaryPath = ToQString(buffer);
        }
      }
    }
    if (binaryPath.isEmpty() && (extension == "jar")) {
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
  if (m_ContextItem != NULL) {
    QFileInfo targetInfo(m_ContextItem->data(0, Qt::UserRole).toString());
    QFileInfo binaryInfo;
    QString arguments;
    switch (getBinaryExecuteInfo(targetInfo, binaryInfo, arguments)) {
      case 1: {
        QString name = QInputDialog::getText(this, tr("Enter Name"),
              tr("Please enter a name for the executable"), QLineEdit::Normal,
              targetInfo.baseName());
        if (!name.isEmpty()) {
          m_ExecutablesList.addExecutable(name, binaryInfo.absoluteFilePath(),
                                          arguments, targetInfo.absolutePath(),
                                          DEFAULT_STAY, QString(),
                                          true, false);
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
  FilesOrigin &origin = m_DirectoryStructure->getOriginByID(originID);
  origin.enable(false);
  m_DirectoryStructure->addFromOrigin(origin.getName(), origin.getPath(), origin.getPriority());
  DirectoryRefresher::cleanStructure(m_DirectoryStructure);
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
    refreshDataTree();
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
    refreshDataTree();
  } else {
    reportError(tr("failed to rename \"%1\" to \"%2\"").arg(QDir::toNativeSeparators(oldName)).arg(QDir::toNativeSeparators(newName)));
  }
}


void MainWindow::openDataFile()
{
  if (m_ContextItem != NULL) {
    QFileInfo targetInfo(m_ContextItem->data(0, Qt::UserRole).toString());
    QFileInfo binaryInfo;
    QString arguments;
    switch (getBinaryExecuteInfo(targetInfo, binaryInfo, arguments)) {
      case 1: {
        spawnBinaryDirect(binaryInfo, arguments, m_CurrentProfile->getName(), targetInfo.absolutePath(), "");
      } break;
      case 2: {
        ::ShellExecuteW(NULL, L"open", ToWString(targetInfo.absoluteFilePath()).c_str(), NULL, NULL, SW_SHOWNORMAL);
      } break;
      default: {
        // nop
      } break;
    }
  }
}


void MainWindow::updateAvailable()
{
  QToolBar *toolBar = findChild<QToolBar*>("toolBar");
  foreach (QAction *action, toolBar->actions()) {
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
    if (hash != m_Settings.getMotDHash()) {
      MotDDialog dialog(motd);
      dialog.exec();
      m_Settings.setMotDHash(hash);
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
  if ((m_ContextItem != NULL) && (m_ContextItem->childCount() == 0)) {
    menu.addAction(tr("Open/Execute"), this, SLOT(openDataFile()));
    menu.addAction(tr("Add as Executable"), this, SLOT(addAsExecutable()));
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
  QString username, password;

  if (!m_LoginAttempted && !NexusInterface::instance()->getAccessManager()->loggedIn() &&
      (m_Settings.getNexusLogin(username, password) ||
      (m_AskForNexusPW && queryLogin(username, password)))) {
    NexusInterface::instance()->getAccessManager()->login(username, password);
    m_LoginAttempted = true;
    m_PostLoginTasks.push_back([&](MainWindow*) { m_Updater.startUpdate(); });
  } else {
    m_Updater.startUpdate();
  }
}


void MainWindow::on_actionEndorseMO_triggered()
{
  if (QMessageBox::question(this, tr("Endorse Mod Organizer"),
                            tr("Do you want to endorse Mod Organizer on %1 now?").arg(ToQString(GameInfo::instance().getNexusPage())),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    NexusInterface::instance()->requestToggleEndorsement(GameInfo::instance().getNexusModID(), true, this, QVariant());
  }
}


void MainWindow::updateDownloadListDelegate()
{
  if (ui->compactBox->isChecked()) {
    ui->downloadView->setItemDelegate(new DownloadListWidgetCompactDelegate(&m_DownloadManager, ui->downloadView, ui->downloadView));
  } else {
    ui->downloadView->setItemDelegate(new DownloadListWidgetDelegate(&m_DownloadManager, ui->downloadView, ui->downloadView));
  }

  DownloadListSortProxy *sortProxy = new DownloadListSortProxy(&m_DownloadManager, ui->downloadView);
  sortProxy->setSourceModel(new DownloadList(&m_DownloadManager, ui->downloadView));
  connect(ui->downloadFilterEdit, SIGNAL(textChanged(QString)), sortProxy, SLOT(updateFilter(QString)));
  connect(ui->downloadFilterEdit, SIGNAL(textChanged(QString)), this, SLOT(downloadFilterChanged(QString)));

  ui->downloadView->setModel(sortProxy);
  ui->downloadView->sortByColumn(1, Qt::AscendingOrder);
  ui->downloadView->header()->resizeSections(QHeaderView::Fixed);

  connect(ui->downloadView->itemDelegate(), SIGNAL(installDownload(int)), this, SLOT(installDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(queryInfo(int)), &m_DownloadManager, SLOT(queryInfo(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(removeDownload(int, bool)), &m_DownloadManager, SLOT(removeDownload(int, bool)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(restoreDownload(int)), &m_DownloadManager, SLOT(restoreDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(cancelDownload(int)), &m_DownloadManager, SLOT(cancelDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(pauseDownload(int)), &m_DownloadManager, SLOT(pauseDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(resumeDownload(int)), &m_DownloadManager, SLOT(resumeDownload(int)));
}


void MainWindow::on_compactBox_toggled(bool)
{
  updateDownloadListDelegate();
}


void MainWindow::modDetailsUpdated(bool)
{
  --m_ModsToUpdate;
  if (m_ModsToUpdate == 0) {
    statusBar()->hide();
    m_ModListSortProxy->setCategoryFilter(boost::assign::list_of(CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE));
    for (int i = 0; i < ui->categoriesList->topLevelItemCount(); ++i) {
      if (ui->categoriesList->topLevelItem(i)->data(0, Qt::UserRole) == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
        ui->categoriesList->setCurrentItem(ui->categoriesList->topLevelItem(i));
        break;
      }
    }
//    m_RefreshProgress->setVisible(false);
  } else {
    m_RefreshProgress->setValue(m_RefreshProgress->maximum() - m_ModsToUpdate);
  }
}

void MainWindow::nxmUpdatesAvailable(const std::vector<int> &modIDs, QVariant userData, QVariant resultData, int)
{
  m_ModsToUpdate -= modIDs.size();
  QVariantList resultList = resultData.toList();
  for (auto iter = resultList.begin(); iter != resultList.end(); ++iter) {
    QVariantMap result = iter->toMap();
    if (result["id"].toInt() == GameInfo::instance().getNexusModID()) {
      if (!result["voted_by_user"].toBool()) {
        ui->actionEndorseMO->setVisible(true);
      }
    } else {
      std::vector<ModInfo::Ptr> info = ModInfo::getByModID(result["id"].toInt());
      for (auto iter = info.begin(); iter != info.end(); ++iter) {
        (*iter)->setNewestVersion(result["version"].toString());
        (*iter)->setNexusDescription(result["description"].toString());
        if (NexusInterface::instance()->getAccessManager()->loggedIn() &&
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

/*
void MainWindow::nxmEndorsementToggled(int, QVariant, QVariant resultData, int)
{
  if (resultData.toBool()) {
    ui->actionEndorseMO->setVisible(false);
    QMessageBox::question(this, tr("Thank you!"), tr("Thank you for your endorsement!"));
  }

  if (!disconnect(sender(), SIGNAL(nxmEndorsementToggled(int, QVariant, QVariant, int)),
             this, SLOT(nxmEndorsementToggled(int, QVariant, QVariant, int)))) {
    qCritical("failed to disconnect endorsement slot");
  }
}*/

void MainWindow::nxmDownloadURLs(int, int, QVariant, QVariant resultData, int)
{
  QVariantList serverList = resultData.toList();

  QList<ServerInfo> servers;
  foreach (const QVariant &server, serverList) {
    QVariantMap serverInfo = server.toMap();
    ServerInfo info;
    info.name = serverInfo["Name"].toString();
    info.premium = serverInfo["IsPremium"].toBool();
    info.lastSeen = QDate::currentDate();
    info.preferred = 0;
    // other keys: ConnectedUsers, Country, URI
    servers.append(info);
  }
  m_Settings.updateServers(servers);
}


void MainWindow::nxmRequestFailed(int modID, QVariant, int, const QString &errorString)
{
  if (modID == -1) {
    // must be the update-check that failed
    m_ModsToUpdate = 0;
    statusBar()->hide();
  }
  MessageDialog::showMessage(tr("Request to Nexus failed: %1").arg(errorString), this);
}


void MainWindow::loginSuccessful(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), this);
  }
  foreach (QString url, m_PendingDownloads) {
    downloadRequestedNXM(url);
  }
  m_PendingDownloads.clear();
  foreach (auto task, m_PostLoginTasks) {
    task(this);
  }

  m_PostLoginTasks.clear();
}


void MainWindow::loginSuccessfulUpdate(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), this);
  }
  m_Updater.startUpdate();
}


void MainWindow::loginFailed(const QString &message)
{
  if (!m_PendingDownloads.isEmpty()) {
    MessageDialog::showMessage(tr("login failed: %1. Trying to download anyway").arg(message), this);
    foreach (QString url, m_PendingDownloads) {
      downloadRequestedNXM(url);
    }
    m_PendingDownloads.clear();
  } else {
    MessageDialog::showMessage(tr("login failed: %1").arg(message), this);
    m_PostLoginTasks.clear();
    statusBar()->hide();
  }
}


void MainWindow::loginFailedUpdate(const QString &message)
{
  MessageDialog::showMessage(tr("login failed: %1. You need to log-in with Nexus to update MO.").arg(message), this);
}


void MainWindow::windowTutorialFinished(const QString &windowName)
{
  m_Settings.directInterface().setValue(QString("CompletedWindowTutorials/") + windowName, true);
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

  QString targetFolder = FileDialogMemory::getExistingDirectory("extractBSA", this, tr("Extract BSA"));
  if (!targetFolder.isEmpty()) {
    BSA::Archive archive;
    QString originPath = QDir::fromNativeSeparators(ToQString(m_DirectoryStructure->getOriginByName(ToWString(item->text(1))).getPath()));
    QString archivePath =  QString("%1\\%2").arg(originPath).arg(item->text(0));

    BSA::EErrorCode result = archive.read(archivePath.toLocal8Bit().constData());
    if ((result != BSA::ERROR_NONE) && (result != BSA::ERROR_INVALIDHASHES)) {
      reportError(tr("failed to read %1: %2").arg(archivePath).arg(result));
      return;
    }

    QProgressDialog progress(this);
    progress.setMaximum(100);
    progress.setValue(0);
    progress.show();
    archive.extractAll(QDir::toNativeSeparators(targetFolder).toUtf8().constData(),
                       boost::bind(&MainWindow::extractProgress, this, boost::ref(progress), _1, _2));
    if (result == BSA::ERROR_INVALIDHASHES) {
      reportError(tr("This archive contains invalid hashes. Some files may be broken."));
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
  foreach (const QAction *action, menu.actions()) {
    const QWidgetAction *widgetAction = qobject_cast<const QWidgetAction*>(action);
    if (widgetAction != NULL) {
      const QCheckBox *checkBox = qobject_cast<const QCheckBox*>(widgetAction->defaultWidget());
      if (checkBox != NULL) {
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
  saveArchiveList();
  m_CheckBSATimer.start(500);
}


void MainWindow::on_actionProblems_triggered()
{
//  QString problemDescription;
//  checkForProblems(problemDescription);
//  QMessageBox::information(this, tr("Problems"), problemDescription);
  ProblemsDialog problems(m_DiagnosisPlugins, this);
  if (problems.hasProblems()) {
    problems.exec();
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

void MainWindow::on_categoriesList_customContextMenuRequested(const QPoint &pos)
{
  QMenu menu;
  menu.addAction(tr("Edit Categories..."), this, SLOT(editCategories()));

  menu.exec(ui->categoriesList->mapToGlobal(pos));
}


void MainWindow::updateESPLock(bool locked)
{
  QItemSelection currentSelection = ui->espList->selectionModel()->selection();
  if (currentSelection.count() == 0) {
    // this path is probably useless
    m_PluginList.lockESPIndex(m_ContextRow, locked);
  } else {
    Q_FOREACH (const QModelIndex &idx, currentSelection.indexes()) {
      m_PluginList.lockESPIndex(mapToModel(&m_PluginList, idx).row(), locked);
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
  Executable &exe = m_ExecutablesList.find(m_ContextAction->text());
  exe.m_Toolbar = false;
  updateToolBar();
}


void MainWindow::toolBar_customContextMenuRequested(const QPoint &point)
{
  QAction *action = ui->toolBar->actionAt(point);
  if (action->objectName().startsWith("custom_")) {
    m_ContextAction = action;
    QMenu menu;
    menu.addAction(tr("Remove"), this, SLOT(removeFromToolbar()));
    menu.exec(ui->toolBar->mapToGlobal(point));
  }
}

void MainWindow::on_espList_customContextMenuRequested(const QPoint &pos)
{
  m_ContextRow = m_PluginListSortProxy->mapToSource(ui->espList->indexAt(pos)).row();

  QMenu menu;
  menu.addAction(tr("Enable all"), &m_PluginList, SLOT(enableAll()));
  menu.addAction(tr("Disable all"), &m_PluginList, SLOT(disableAll()));

  QItemSelection currentSelection = ui->espList->selectionModel()->selection();
  bool hasLocked = false;
  bool hasUnlocked = false;
  Q_FOREACH (const QModelIndex &idx, currentSelection.indexes()) {
    int row = m_PluginListSortProxy->mapToSource(idx).row();
    if (m_PluginList.isEnabled(row)) {
      if (m_PluginList.isESPLocked(row)) {
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
  QAbstractProxyModel *newModel = NULL;
  switch (index) {
    case 1: {
        newModel = new QtGroupingProxy(&m_ModList, QModelIndex(), ModList::COL_CATEGORY, Qt::UserRole,
                                       0, Qt::UserRole + 2);
      } break;
    case 2: {
        newModel = new QtGroupingProxy(&m_ModList, QModelIndex(), ModList::COL_MODID, Qt::DisplayRole,
                                       QtGroupingProxy::FLAG_NOGROUPNAME | QtGroupingProxy::FLAG_NOSINGLE,
                                       Qt::UserRole + 2);
      } break;
    default: {
        newModel = NULL;
      } break;
  }

  if (newModel != NULL) {
#ifdef TEST_MODELS
    new ModelTest(newModel, this);
#endif // TEST_MODELS
    m_ModListSortProxy->setSourceModel(newModel);
    connect(ui->modList, SIGNAL(expanded(QModelIndex)),newModel, SLOT(expanded(QModelIndex)));
    connect(ui->modList, SIGNAL(collapsed(QModelIndex)), newModel, SLOT(collapsed(QModelIndex)));
    connect(newModel, SIGNAL(expandItem(QModelIndex)), this, SLOT(expandModList(QModelIndex)));

  } else {
    m_ModListSortProxy->setSourceModel(&m_ModList);
  }
}

void MainWindow::on_linkButton_pressed()
{
  const Executable &selectedExecutable = ui->executablesListBox->itemData(ui->executablesListBox->currentIndex()).value<Executable>();

  QIcon addIcon(":/MO/gui/link");
  QIcon removeIcon(":/MO/gui/remove");

  QFileInfo linkDesktopFile(QDir::fromNativeSeparators(getDesktopDirectory()) + "/" + selectedExecutable.m_Title + ".lnk");
  QFileInfo linkMenuFile(QDir::fromNativeSeparators(getStartMenuDirectory()) + "/" + selectedExecutable.m_Title + ".lnk");

  ui->linkButton->menu()->actions().at(0)->setIcon(selectedExecutable.m_Toolbar ? removeIcon : addIcon);
  ui->linkButton->menu()->actions().at(1)->setIcon(linkDesktopFile.exists() ? removeIcon : addIcon);
  ui->linkButton->menu()->actions().at(2)->setIcon(linkMenuFile.exists() ? removeIcon : addIcon);
}

void MainWindow::on_showHiddenBox_toggled(bool checked)
{
  m_DownloadManager.setShowHidden(checked);
}
