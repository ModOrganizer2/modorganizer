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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "bsafolder.h"
#include "delayedfilewriter.h"
#include "errorcodes.h"
#include "imoinfo.h"
#include "iplugingame.h"  //namespace MOBase { class IPluginGame; }
#include "iuserinterface.h"
#include "modinfo.h"
#include "modlistbypriorityproxy.h"
#include "modlistsortproxy.h"
#include "plugincontainer.h"  //class PluginContainer;
#include "shared/fileregisterfwd.h"
#include "thememanager.h"
#include "tutorialcontrol.h"
#include <log.h>

class Executable;
class CategoryFactory;
class OrganizerCore;
class FilterList;
class DataTab;
class DownloadsTab;
class SavesTab;
class BrowserDialog;

class PluginListSortProxy;
namespace BSA
{
class Archive;
}

namespace MOBase
{
class IPluginModPage;
}
namespace MOBase
{
class IPluginTool;
}

namespace MOShared
{
class DirectoryEntry;
}

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QList>
#include <QMainWindow>
#include <QObject>
#include <QPersistentModelIndex>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QVariant>
#include <Qt>
#include <QtConcurrent/QtConcurrentRun>

class QAction;
class QAbstractItemModel;
class QDateTime;
class QEvent;
class QFile;
class QListWidgetItem;
class QMenu;
class QModelIndex;
class QPoint;
class QProgressDialog;
class QTranslator;
class QTreeWidgetItem;
class QUrl;
class QWidget;

#ifndef Q_MOC_RUN
#include <boost/signals2.hpp>
#endif

// Sigh - just for HANDLE
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace Ui
{
class MainWindow;
}

class Settings;

class MainWindow : public QMainWindow, public IUserInterface
{
  Q_OBJECT

  friend class OrganizerProxy;

public:
  explicit MainWindow(Settings& settings, OrganizerCore& organizerCore,
                      PluginContainer& pluginContainer, ThemeManager& manager,
                      QWidget* parent = 0);
  ~MainWindow();

  void processUpdates();

  QMainWindow* mainWindow() override;

  bool addProfile();
  void updateBSAList(const QStringList& defaultArchives,
                     const QStringList& activeArchives);

  void saveArchiveList();

  void installTranslator(const QString& name);

  void displayModInformation(ModInfo::Ptr modInfo, unsigned int modIndex,
                             ModInfoTabIDs tabID) override;

  bool canExit();
  void onBeforeClose();

  virtual bool closeWindow();
  virtual void setWindowEnabled(bool enabled);

  virtual MOBase::DelayedFileWriterBase& archivesWriter() override
  {
    return m_ArchiveListWriter;
  }

public slots:
  void refresherProgress(const DirectoryRefreshProgress* p);

signals:
  // emitted after the information dialog has been closed, used by tutorials
  //
  void modInfoDisplayed();

  /**
   * @brief emitted when the selected style changes
   */
  void themeChanged(const QString& themeIdentifier);

  void checkForProblemsDone();

protected:
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;

private slots:
  void on_actionChange_Game_triggered();

private:
  // update data tab and schedule a problem check after a directory
  // structure update
  //
  void onDirectoryStructureChanged();

  void cleanup();

  void setupToolbar();
  void setupActionMenu(QAction* a);
  void createHelpMenu();
  void createEndorseMenu();

  void updatePinnedExecutables();
  void setToolbarSize(const QSize& s);
  void setToolbarButtonStyle(Qt::ToolButtonStyle s);

  void registerModPage(MOBase::IPluginModPage* modPage);
  bool registerNexusPage(const QString& gameName);
  void registerPluginTool(MOBase::IPluginTool* tool, QString name = QString(),
                          QMenu* menu = nullptr);

  void updateToolbarMenu();
  void updateToolMenu();
  void updateModPageMenu();
  void updateViewMenu();

  QMenu* createPopupMenu() override;
  void activateSelectedProfile();

  bool refreshProfiles(bool selectProfile = true, QString newProfile = QString());
  void refreshExecutablesList();

  bool modifyExecutablesDialog(int selection);

  // remove invalid category-references from mods
  void fixCategories();

  bool extractProgress(QProgressDialog& extractProgress, int percentage,
                       std::string fileName);

  // Performs checks, sets the m_NumberOfProblems and signals checkForProblemsDone().
  void checkForProblemsImpl();

  void setCategoryListVisible(bool visible);

  bool errorReported(QString& logFile);

  static void setupNetworkProxy(bool activate);
  void activateProxy(bool activate);

  bool createBackup(const QString& filePath, const QDateTime& time);
  QString queryRestore(const QString& filePath);

  QMenu* openFolderMenu();

  void dropLocalFile(const QUrl& url, const QString& outputDir, bool move);

  void toggleMO2EndorseState();
  void toggleUpdateAction();

  void updateSortButton();

  // update info
  struct NxmUpdateInfoData
  {
    QString game;
    std::set<ModInfo::Ptr> finalMods;
  };
  void finishUpdateInfo(const NxmUpdateInfoData& data);

private:
  static const char* PATTERN_BACKUP_GLOB;
  static const char* PATTERN_BACKUP_REGEX;
  static const char* PATTERN_BACKUP_DATE;

private:
  Ui::MainWindow* ui;

  bool m_WasVisible;
  bool m_FirstPaint;

  // last separator on the toolbar, used to add spacer for right-alignment and
  // as an insert point for executables
  QAction* m_linksSeparator;

  MOBase::TutorialControl m_Tutorial;

  std::unique_ptr<DataTab> m_DataTab;
  std::unique_ptr<DownloadsTab> m_DownloadsTab;
  std::unique_ptr<SavesTab> m_SavesTab;

  int m_OldProfileIndex;

  std::vector<QString>
      m_ModNameList;  // the mod-list to go with the directory structure

  QStringList m_DefaultArchives;

  int m_OldExecutableIndex;

  QAction* m_ContextAction;

  CategoryFactory& m_CategoryFactory;

  QTimer m_CheckBSATimer;
  QTimer m_SaveMetaTimer;
  QTimer m_UpdateProblemsTimer;

  QFuture<void> m_MetaSave;

  QTime m_StartTime;

  OrganizerCore& m_OrganizerCore;
  PluginContainer& m_PluginContainer;
  ThemeManager& m_ThemeManager;

  QString m_CurrentLanguage;
  std::vector<QTranslator*> m_Translators;

  std::unique_ptr<BrowserDialog> m_IntegratedBrowser;

  QByteArray m_ArchiveListHash;

  MOBase::DelayedFileWriter m_ArchiveListWriter;

  QAction* m_LinkToolbar;
  QAction* m_LinkDesktop;
  QAction* m_LinkStartMenu;

  // icon set by the stylesheet, used to remember its original appearance
  // when painting the count
  QIcon m_originalNotificationIcon;

  std::atomic<std::size_t> m_NumberOfProblems;
  std::atomic<bool> m_ProblemsCheckRequired;
  std::mutex m_CheckForProblemsMutex;

  QVersionNumber m_LastVersion;

  Executable* getSelectedExecutable();

private slots:
  void updateWindowTitle(const APIUserAccount& user);
  void showMessage(const QString& message);
  void showError(const QString& message);

  // main window actions
  void helpTriggered();
  void issueTriggered();
  void wikiTriggered();
  void gameSupportTriggered();
  void discordTriggered();
  void tutorialTriggered();
  void extractBSATriggered(QTreeWidgetItem* item);

  void refreshProfile_activated();

  void linkToolbar();
  void linkDesktop();
  void linkMenu();

  void languageChange(const QString& newLanguage);

  void windowTutorialFinished(const QString& windowName);

  BSA::EErrorCode extractBSA(BSA::Archive& archive, BSA::Folder::Ptr folder,
                             const QString& destination,
                             QProgressDialog& extractProgress);

  // nexus related
  void updateAvailable();

  void actionEndorseMO();
  void actionWontEndorseMO();

  void motdReceived(const QString& motd);

  void originModified(int originID);

  void modInstalled(const QString& modName);

  void importCategories(bool);

  void refreshNexusCategories(CategoriesDialog* dialog);
  void categoriesSaved();

  // update info
  void nxmUpdateInfoAvailable(QString gameName, QVariant userData, QVariant resultData,
                              int requestID);

  void nxmEndorsementsAvailable(QVariant userData, QVariant resultData, int);
  void nxmUpdatesAvailable(QString gameName, int modID, QVariant userData,
                           QVariant resultData, int requestID);
  void nxmModInfoAvailable(QString gameName, int modID, QVariant userData,
                           QVariant resultData, int requestID);
  void nxmEndorsementToggled(QString, int, QVariant, QVariant resultData, int);
  void nxmTrackedModsAvailable(QVariant userData, QVariant resultData, int);
  void nxmDownloadURLs(QString, int modID, int fileID, QVariant userData,
                       QVariant resultData, int requestID);
  void nxmGameInfoAvailable(QString gameName, QVariant, QVariant resultData, int);
  void nxmRequestFailed(QString gameName, int modID, int fileID, QVariant userData,
                        int requestID, int errorCode, const QString& errorString);

  void onRequestsChanged(const APIStats& stats, const APIUserAccount& user);

  void modRenamed(const QString& oldName, const QString& newName);
  void modRemoved(const QString& fileName);

  void hookUpWindowTutorials();
  bool shouldStartTutorial() const;

  void openInstanceFolder();
  void openInstallFolder();
  void openPluginsFolder();
  void openStylesheetsFolder();
  void openDownloadsFolder();
  void openModsFolder();
  void openProfileFolder();
  void openIniFolder();
  void openGameFolder();
  void openMyGamesFolder();
  void startExeAction();

  void checkBSAList();

  // Only visually update the problems icon.
  void updateProblemsButton();

  // Queue a problem check to allow collapsing of multiple requests in short amount of
  // time.
  void scheduleCheckForProblems();

  // Perform the actual problem check in another thread.
  QFuture<void> checkForProblemsAsync();

  void saveModMetas();

  void updateStyle(const QString& style);

  void resizeLists(bool pluginListCustom);

  void fileMoved(const QString& filePath, const QString& oldOriginName,
                 const QString& newOriginName);

  /**
   * @brief allow columns in mod list and plugin list to be resized
   */
  void allowListResize();

  void toolBar_customContextMenuRequested(const QPoint& point);
  void removeFromToolbar(QAction* action);

  void about();

  void resetActionIcons();

private slots:  // ui slots
  // actions
  void on_actionAdd_Profile_triggered();
  void on_actionInstallMod_triggered();
  void on_action_Refresh_triggered();
  void on_actionModify_Executables_triggered();
  void on_actionNexus_triggered();
  void on_actionNotifications_triggered();
  void on_actionSettings_triggered();
  void on_actionUpdate_triggered();
  void on_actionExit_triggered();
  void on_actionMainMenuToggle_triggered();
  void on_actionToolBarMainToggle_triggered();
  void on_actionStatusBarToggle_triggered();
  void on_actionToolBarSmallIcons_triggered();
  void on_actionToolBarMediumIcons_triggered();
  void on_actionToolBarLargeIcons_triggered();
  void on_actionToolBarIconsOnly_triggered();
  void on_actionToolBarTextOnly_triggered();
  void on_actionToolBarIconsAndText_triggered();
  void on_actionViewLog_triggered();

  void on_centralWidget_customContextMenuRequested(const QPoint& pos);
  void on_bsaList_customContextMenuRequested(const QPoint& pos);
  void on_executablesListBox_currentIndexChanged(int index);
  void on_profileBox_currentIndexChanged(int index);
  void on_startButton_clicked();
  void on_tabWidget_currentChanged(int index);

  void on_displayCategoriesBtn_toggled(bool checked);
  void on_linkButton_pressed();
  void on_showHiddenBox_toggled(bool checked);
  void on_bsaList_itemChanged(QTreeWidgetItem* item, int column);

  void on_saveButton_clicked();
  void on_restoreButton_clicked();
  void on_restoreModsButton_clicked();
  void on_saveModsButton_clicked();
  void on_managedArchiveLabel_linkHovered(const QString& link);

  void onPluginRegistrationChanged();

  void storeSettings();
  void readSettings();

  void setupModList();
};

#endif  // MAINWINDOW_H
