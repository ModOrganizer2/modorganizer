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

#include <QMainWindow>
#include <QFileInfo>
#include <QDir>
#include <QTreeWidget>
#include <QListWidgetItem>
#include <QProcess>
#include <QThread>
#include <QProgressBar>
#include <QTranslator>
#include "executableslist.h"
#include "modlist.h"
#include "pluginlist.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <archive.h>
#include "directoryrefresher.h"
#include <imoinfo.h>
#include <iplugintool.h>
#include <iplugindiagnose.h>
#include <ipluginmodpage.h>
#include "settings.h"
#include "downloadmanager.h"
#include "installationmanager.h"
#include "selfupdater.h"
#include "savegamegamebyro.h"
#include "modlistsortproxy.h"
#include "pluginlistsortproxy.h"
#include "tutorialcontrol.h"
#include "savegameinfowidgetgamebryo.h"
#include "previewgenerator.h"
#include "browserdialog.h"
#include <guessedvalue.h>
#include <directoryentry.h>
#include <boost/signals2.hpp>

namespace Ui {
    class MainWindow;
}

class QToolButton;
class ModListSortProxy;
class ModListGroupCategoriesProxy;


class MainWindow : public QMainWindow, public MOBase::IOrganizer, public MOBase::IPluginDiagnose
{
  Q_OBJECT
  Q_INTERFACES(MOBase::IPluginDiagnose)

private:

  struct SignalCombinerAnd
  {
    typedef bool result_type;
    template<typename InputIterator>
    bool operator()(InputIterator first, InputIterator last) const
    {
      while (first != last) {
        if (!(*first)) {
          return false;
        }
        ++first;
      }
      return true;
    }
  };

  typedef boost::signals2::signal<bool (const QString&), SignalCombinerAnd> SignalAboutToRunApplication;

public:
  explicit MainWindow(const QString &exeName, QSettings &initSettings, QWidget *parent = 0);
  ~MainWindow();

  void readSettings();

  bool addProfile();
  void refreshLists();
  void refreshESPList();
  void refreshBSAList();
  void refreshDataTree();
  void refreshSaveList();

  void setExecutablesList(const ExecutablesList &executablesList);

  void setModListSorting(int index);
  void setESPListSorting(int index);

  bool setCurrentProfile(int index);
  bool setCurrentProfile(const QString &name);

  void createFirstProfile();

/*  void spawnProgram(const QString &fileName, const QString &argumentsArg,
                    const QString &profileName, const QDir &currentDirectory);*/

  void loadPlugins();

  virtual MOBase::IGameInfo &gameInfo() const;
  virtual MOBase::IModRepositoryBridge *createNexusBridge() const;
  virtual QString profileName() const;
  virtual QString profilePath() const;
  virtual QString downloadsPath() const;
  virtual MOBase::VersionInfo appVersion() const;
  virtual MOBase::IModInterface *getMod(const QString &name);
  virtual MOBase::IModInterface *createMod(MOBase::GuessedValue<QString> &name);
  virtual bool removeMod(MOBase::IModInterface *mod);
  virtual void modDataChanged(MOBase::IModInterface *mod);
  virtual QVariant pluginSetting(const QString &pluginName, const QString &key) const;
  virtual void setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value);
  virtual QVariant persistent(const QString &pluginName, const QString &key, const QVariant &def = QVariant()) const;
  virtual void setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync = true);
  virtual QString pluginDataPath() const;
  virtual void installMod(const QString &fileName);
  virtual QString resolvePath(const QString &fileName) const;
  virtual QStringList listDirectories(const QString &directoryName) const;
  virtual QStringList findFiles(const QString &path, const std::function<bool(const QString &)> &filter) const;
  virtual QList<FileInfo> findFileInfos(const QString &path, const std::function<bool(const FileInfo&)> &filter) const;

  virtual MOBase::IDownloadManager *downloadManager();
  virtual MOBase::IPluginList *pluginList();
  virtual MOBase::IModList *modList();
  virtual HANDLE startApplication(const QString &executable, const QStringList &args = QStringList(), const QString &cwd = "", const QString &profile = "");
  virtual bool onAboutToRun(const std::function<bool(const QString&)> &func);
  virtual void refreshModList(bool saveChanges = true);

  virtual std::vector<unsigned int> activeProblems() const;
  virtual QString shortDescription(unsigned int key) const;
  virtual QString fullDescription(unsigned int key) const;
  virtual bool hasGuidedFix(unsigned int key) const;
  virtual void startGuidedFix(unsigned int key) const;

  void addPrimaryCategoryCandidates(QMenu *primaryCategoryMenu, ModInfo::Ptr info);

  void saveArchiveList();

  void createStdoutPipe(HANDLE *stdOutRead, HANDLE *stdOutWrite);
  std::string readFromPipe(HANDLE stdOutRead);
  void processLOOTOut(const std::string &lootOut, std::string &reportURL, std::string &errorMessages, QProgressDialog &dialog);
public slots:

  void displayColumnSelection(const QPoint &pos);

  void externalMessage(const QString &message);
  void modorder_changed();
  void refresher_progress(int percent);
  void directory_refreshed();

  void toolPluginInvoke();
  void modPagePluginInvoke();

signals:

  /**
   * @brief emitted after a mod has been installed
   * @node this is currently only used for tutorials
   */
  void modInstalled();

  /**
   * @brief emitted after the information dialog has been closed
   */
  void modInfoDisplayed();

  /**
   * @brief emitted when the selected style changes
   */
  void styleChanged(const QString &styleFile);


  void modListDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

protected:

  virtual void showEvent(QShowEvent *event);
  virtual void closeEvent(QCloseEvent *event);
  virtual bool eventFilter(QObject *obj, QEvent *event);
  virtual void resizeEvent(QResizeEvent *event);

private:

  void actionToToolButton(QAction *&sourceAction);
  bool verifyPlugin(MOBase::IPlugin *plugin);
  void registerPluginTool(MOBase::IPluginTool *tool);
  void registerModPage(MOBase::IPluginModPage *modPage);
  bool registerPlugin(QObject *pluginObj, const QString &fileName);

  void updateToolBar();
  void activateSelectedProfile();

  void setExecutableIndex(int index);

  bool nexusLogin();

  bool testForSteam();
  void startSteam();

  HANDLE spawnBinaryDirect(const QFileInfo &binary, const QString &arguments, const QString &profileName, const QDir &currentDirectory, const QString &steamAppID);
  void spawnBinary(const QFileInfo &binary, const QString &arguments = "", const QDir &currentDirectory = QDir(), bool closeAfterStart = true, const QString &steamAppID = "");

  void updateTo(QTreeWidgetItem *subTree, const std::wstring &directorySoFar, const MOShared::DirectoryEntry &directoryEntry, bool conflictsOnly);
  void refreshDirectoryStructure();
  bool refreshProfiles(bool selectProfile = true);
  void refreshExecutablesList();
  void installMod();
  bool modifyExecutablesDialog();
  void displayModInformation(ModInfo::Ptr modInfo, unsigned int index, int tab);
  void displayModInformation(int row, int tab = 0);
  void testExtractBSA(int modIndex);

  void writeDataToFile(QFile &file, const QString &directory, const MOShared::DirectoryEntry &directoryEntry);

  void renameModInList(QFile &modList, const QString &oldName, const QString &newName);

  void refreshFilters();

  /**
   * Sets category selections from menu; for multiple mods, this will only apply
   * the changes made in the menu (which is the delta between the current menu selection and the reference mod)
   * @param menu the menu after editing by the user
   * @param modRow index of the mod to edit
   * @param referenceRow row of the reference mod
   */
  void addRemoveCategoriesFromMenu(QMenu *menu, int modRow, int referenceRow);

  /**
   * Sets category selections from menu; for multiple mods, this will completely
   * replace the current set of categories on each selected with those selected in the menu
   * @param menu the menu after editing by the user
   * @param modRow index of the mod to edit
   */
  void replaceCategoriesFromMenu(QMenu *menu, int modRow);

  bool populateMenuCategories(QMenu *menu, int targetID);

  void updateDownloadListDelegate();

  // remove invalid category-references from mods
  void fixCategories();

  void storeSettings();

  bool queryLogin(QString &username, QString &password);

  void createHelpWidget();

  bool extractProgress(QProgressDialog &extractProgress, int percentage, std::string fileName);

  bool checkForProblems();

  int getBinaryExecuteInfo(const QFileInfo &targetInfo, QFileInfo &binaryInfo, QString &arguments);
  QTreeWidgetItem *addFilterItem(QTreeWidgetItem *root, const QString &name, int categoryID);
  void addCategoryFilters(QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID);

  void setCategoryListVisible(bool visible);

  void updateProblemsButton();

  SaveGameGamebryo *getSaveGame(const QString &name);
  SaveGameGamebryo *getSaveGame(QListWidgetItem *item);

  void displaySaveGameInfo(const SaveGameGamebryo *save, QPoint pos);

  HANDLE nextChildProcess();

  bool errorReported(QString &logFile);

  QIcon iconForExecutable(const QString &filePath);

  void updateESPLock(bool locked);

  static void setupNetworkProxy(bool activate);
  void activateProxy(bool activate);
  void installTranslator(const QString &name);
  void setBrowserGeometry(const QByteArray &geometry);

  bool createBackup(const QString &filePath, const QDateTime &time);
  QString queryRestore(const QString &filePath);

private:

  static const unsigned int PROBLEM_PLUGINSNOTLOADED = 1;
  static const unsigned int PROBLEM_TOOMANYPLUGINS = 2;

  static const char *PATTERN_BACKUP_GLOB;
  static const char *PATTERN_BACKUP_REGEX;
  static const char *PATTERN_BACKUP_DATE;

private:

  Ui::MainWindow *ui;

  MOBase::TutorialControl m_Tutorial;

  QString m_ExeName;

  int m_OldProfileIndex;

  QThread m_RefresherThread;
  DirectoryRefresher m_DirectoryRefresher;
  MOShared::DirectoryEntry *m_DirectoryStructure;
  std::vector<QString> m_ModNameList; // the mod-list to go with the directory structure
  QProgressBar *m_RefreshProgress;
  bool m_Refreshing;

  ModList m_ModList;
  QAbstractItemModel *m_ModListGroupingProxy;
  ModListSortProxy *m_ModListSortProxy;

  PluginList m_PluginList;
  PluginListSortProxy *m_PluginListSortProxy;

  ExecutablesList m_ExecutablesList;
  int m_OldExecutableIndex;

  QString m_GamePath;

  int m_ContextRow;
  QPersistentModelIndex m_ContextIdx;
  QTreeWidgetItem *m_ContextItem;
  QAction *m_ContextAction;

  int m_SelectedSaveGame;

  Settings m_Settings;

  DownloadManager m_DownloadManager;
  InstallationManager m_InstallationManager;

  SelfUpdater m_Updater;

  CategoryFactory &m_CategoryFactory;

  Profile *m_CurrentProfile;

  int m_ModsToUpdate;

  QStringList m_PendingDownloads;
  QList<boost::function<void (MainWindow*)> > m_PostLoginTasks;
  bool m_AskForNexusPW;
  bool m_LoginAttempted;

  QStringList m_DefaultArchives;
  QStringList m_ActiveArchives;
  bool m_DirectoryUpdate;
  bool m_ArchivesInit;
  QTimer m_CheckBSATimer;
  QTimer m_SaveMetaTimer;

  QTime m_StartTime;
  SaveGameInfoWidget *m_CurrentSaveView;

  MOBase::IGameInfo *m_GameInfo;

  std::vector<MOBase::IPluginDiagnose*> m_DiagnosisPlugins;
  std::vector<MOBase::IPluginModPage*> m_ModPages;
  std::vector<QString> m_UnloadedPlugins;

  QFile m_PluginsCheck;

  SignalAboutToRunApplication m_AboutToRun;

  QString m_CurrentLanguage;
  std::vector<QTranslator*> m_Translators;

  PreviewGenerator m_PreviewGenerator;
  BrowserDialog m_IntegratedBrowser;

  QFileSystemWatcher m_SavesWatcher;

  std::vector<QTreeWidgetItem*> m_RemoveWidget;

private slots:

  void showMessage(const QString &message);
  void showError(const QString &message);

  // main window actions
  void helpTriggered();
  void issueTriggered();
  void wikiTriggered();
  void tutorialTriggered();
  void extractBSATriggered();

  // modlist context menu
  void installMod_clicked();
  void restoreBackup_clicked();
  void renameMod_clicked();
  void removeMod_clicked();
  void reinstallMod_clicked();
  void endorse_clicked();
  void dontendorse_clicked();
  void unendorse_clicked();
  void ignoreMissingData_clicked();
  void visitOnNexus_clicked();
  void openExplorer_clicked();
  void information_clicked();
  // savegame context menu
  void deleteSavegame_clicked();
  void fixMods_clicked();
  // data-tree context menu
  void writeDataToFile();
  void openDataFile();
  void addAsExecutable();
  void previewDataFile();
  void hideFile();
  void unhideFile();

  void linkToolbar();
  void linkDesktop();
  void linkMenu();

  void languageChange(const QString &newLanguage);
  void modStatusChanged(unsigned int index);
  void saveSelectionChanged(QListWidgetItem *newItem);

  bool saveCurrentLists();

  void windowTutorialFinished(const QString &windowName);

  BSA::EErrorCode extractBSA(BSA::Archive &archive, BSA::Folder::Ptr folder, const QString &destination, QProgressDialog &extractProgress);

  void syncOverwrite();

  void createModFromOverwrite();

  void removeOrigin(const QString &name);

  void procError(QProcess::ProcessError error);
  void procFinished(int exitCode, QProcess::ExitStatus exitStatus);

  // nexus related
  void checkModsForUpdates();
  void nexusLinkActivated(const QString &link);

  void linkClicked(const QString &url);

  void loginSuccessful(bool necessary);
  void loginSuccessfulUpdate(bool necessary);
  void loginFailed(const QString &message);
  void loginFailedUpdate(const QString &message);

  void downloadRequestedNXM(const QString &url);
  void downloadRequested(QNetworkReply *reply, int modID, const QString &fileName);

  void installDownload(int index);
  void updateAvailable();

  void motdReceived(const QString &motd);
  void notEndorsedYet();

  void originModified(int originID);

  void addRemoveCategories_MenuHandler();
  void replaceCategories_MenuHandler();

  void savePrimaryCategory();
  void addPrimaryCategoryCandidates();

  void modDetailsUpdated(bool success);
  void modlistChanged(int row);

  void nxmUpdatesAvailable(const std::vector<int> &modIDs, QVariant userData, QVariant resultData, int requestID);
//  void nxmEndorsementToggled(int, QVariant, QVariant resultData, int);
  void nxmDownloadURLs(int modID, int fileID, QVariant userData, QVariant resultData, int requestID);
  void nxmRequestFailed(int modID, int fileID, QVariant userData, int requestID, const QString &errorString);

  void editCategories();

  void displayModInformation(const QString &modName, int tab);
  void modOpenNext();
  void modOpenPrev();

  void modRenamed(const QString &oldName, const QString &newName);
  void modRemoved(const QString &fileName);

  void hideSaveGameInfo();

  void hookUpWindowTutorials();

  void resumeDownload(int downloadIndex);
  void endorseMod(ModInfo::Ptr mod);
  void cancelModListEditor();

  void lockESPIndex();
  void unlockESPIndex();

  void enableVisibleMods();
  void disableVisibleMods();
  void exportModListCSV();

  void startExeAction();

  void checkBSAList();
  void saveModMetas();

  void updateStyle(const QString &style);

  void modlistChanged(const QModelIndex &index, int role);
  void fileMoved(const QString &filePath, const QString &oldOriginName, const QString &newOriginName);

  void savePluginList();

  void modFilterActive(bool active);
  void espFilterChanged(const QString &filter);
  void downloadFilterChanged(const QString &filter);

  void expandModList(const QModelIndex &index);

  /**
   * @brief resize columns in mod list and plugin list to content
   */
  void resizeLists(bool modListCustom, bool pluginListCustom);

  /**
   * @brief allow columns in mod list and plugin list to be resized
   */
  void allowListResize();

  void downloadSpeed(const QString &serverName, int bytesPerSecond);

  void toolBar_customContextMenuRequested(const QPoint &point);
  void removeFromToolbar();
  void overwriteClosed(int);

  void changeVersioningScheme();
  void ignoreUpdate();
  void unignoreUpdate();

  void refreshSavesIfOpen();
  void expandDataTreeItem(QTreeWidgetItem *item);
  void about();
  void delayedRemove();

  void requestDownload(const QUrl &url, QNetworkReply *reply);

private slots: // ui slots
  // actions
  void on_actionAdd_Profile_triggered();
  void on_actionInstallMod_triggered();
  void on_actionModify_Executables_triggered();
  void on_actionNexus_triggered();
  void on_actionProblems_triggered();
  void on_actionSettings_triggered();
  void on_actionUpdate_triggered();
  void on_actionEndorseMO_triggered();

  void on_bsaList_customContextMenuRequested(const QPoint &pos);
  void bsaList_itemMoved();
  void on_btnRefreshData_clicked();
  void on_categoriesList_customContextMenuRequested(const QPoint &pos);
  void on_conflictsCheckBox_toggled(bool checked);
  void on_dataTree_customContextMenuRequested(const QPoint &pos);
  void on_executablesListBox_currentIndexChanged(int index);
  void on_modList_customContextMenuRequested(const QPoint &pos);
  void on_modList_doubleClicked(const QModelIndex &index);
  void on_profileBox_currentIndexChanged(int index);
  void on_profileRefreshBtn_clicked();
  void on_savegameList_customContextMenuRequested(const QPoint &pos);
  void on_startButton_clicked();
  void on_tabWidget_currentChanged(int index);

  void on_espList_customContextMenuRequested(const QPoint &pos);
  void on_displayCategoriesBtn_toggled(bool checked);
  void on_groupCombo_currentIndexChanged(int index);
  void on_categoriesList_itemSelectionChanged();
  void on_linkButton_pressed();
  void on_showHiddenBox_toggled(bool checked);
  void on_bsaList_itemChanged(QTreeWidgetItem *item, int column);
  void on_bossButton_clicked();

  void on_saveButton_clicked();
  void on_restoreButton_clicked();
  void on_restoreModsButton_clicked();
  void on_saveModsButton_clicked();
};

#endif // MAINWINDOW_H
