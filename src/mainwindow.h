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
#include <QPluginLoader>
#include "modlist.h"
#include "pluginlist.h"
#include "plugincontainer.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <archive.h>
#include "directoryrefresher.h"
#include <imoinfo.h>
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
#include "iuserinterface.h"
#include <guessedvalue.h>
#include <directoryentry.h>
#include <delayedfilewriter.h>
#ifndef Q_MOC_RUN
#include <boost/signals2.hpp>
#endif

namespace Ui {
    class MainWindow;
}

class LockedDialog;
class QToolButton;
class ModListSortProxy;
class ModListGroupCategoriesProxy;


class MainWindow : public QMainWindow, public IUserInterface
{
  Q_OBJECT

  friend class OrganizerProxy;


public:
  explicit MainWindow(const QString &exeName, QSettings &initSettings,
                      OrganizerCore &organizerCore, PluginContainer &pluginContainer,
                      QWidget *parent = 0);
  ~MainWindow();

  void storeSettings(QSettings &settings) override;
  void readSettings();

  virtual void lock() override;
  virtual void unlock() override;
  virtual bool unlockClicked() override;

  bool addProfile();
  void updateBSAList(const QStringList &defaultArchives, const QStringList &activeArchives);
  void refreshDataTree();
  void refreshSaveList();

  void setModListSorting(int index);
  void setESPListSorting(int index);

  void saveArchiveList();

  void registerPluginTool(MOBase::IPluginTool *tool);
  void registerModPage(MOBase::IPluginModPage *modPage);

  void addPrimaryCategoryCandidates(QMenu *primaryCategoryMenu, ModInfo::Ptr info);

  void createStdoutPipe(HANDLE *stdOutRead, HANDLE *stdOutWrite);
  std::string readFromPipe(HANDLE stdOutRead);
  void processLOOTOut(const std::string &lootOut, std::string &errorMessages, QProgressDialog &dialog);

  void updateModInDirectoryStructure(unsigned int index, ModInfo::Ptr modInfo);

  QString getOriginDisplayName(int originID);

  void installTranslator(const QString &name);

  virtual void disconnectPlugins();

  void displayModInformation(ModInfo::Ptr modInfo, unsigned int index, int tab);

  virtual bool closeWindow();
  virtual void setWindowEnabled(bool enabled);

  virtual MOBase::DelayedFileWriterBase &archivesWriter() override { return m_ArchiveListWriter; }

  void updateWindowTitle(const QString &accountName, bool premium);
public slots:

  void displayColumnSelection(const QPoint &pos);

  void modorder_changed();
  void refresher_progress(int percent);
  void directory_refreshed();

  void toolPluginInvoke();
  void modPagePluginInvoke();

signals:


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
  virtual void dragEnterEvent(QDragEnterEvent *event);
  virtual void dropEvent(QDropEvent *event);

private:

  void actionToToolButton(QAction *&sourceAction);

  void updateToolBar();
  void activateSelectedProfile();

  void setExecutableIndex(int index);

  void startSteam();

  void updateTo(QTreeWidgetItem *subTree, const std::wstring &directorySoFar, const MOShared::DirectoryEntry &directoryEntry, bool conflictsOnly);
  bool refreshProfiles(bool selectProfile = true);
  void refreshExecutablesList();
  void installMod(QString fileName = "");

  QList<MOBase::IOrganizer::FileInfo> findFileInfos(const QString &path, const std::function<bool (const MOBase::IOrganizer::FileInfo &)> &filter) const;

  bool modifyExecutablesDialog();
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

  void createHelpWidget();

  bool extractProgress(QProgressDialog &extractProgress, int percentage, std::string fileName);

  int checkForProblems();

  int getBinaryExecuteInfo(const QFileInfo &targetInfo, QFileInfo &binaryInfo, QString &arguments);
  QTreeWidgetItem *addFilterItem(QTreeWidgetItem *root, const QString &name, int categoryID, ModListSortProxy::FilterType type);
  void addContentFilters();
  void addCategoryFilters(QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID);

  void setCategoryListVisible(bool visible);

  SaveGameGamebryo *getSaveGame(const QString &name);
  SaveGameGamebryo *getSaveGame(QListWidgetItem *item);

  void displaySaveGameInfo(const SaveGameGamebryo *save, QPoint pos);

  HANDLE nextChildProcess();

  bool errorReported(QString &logFile);

  void updateESPLock(bool locked);

  static void setupNetworkProxy(bool activate);
  void activateProxy(bool activate);
  void setBrowserGeometry(const QByteArray &geometry);

  bool createBackup(const QString &filePath, const QDateTime &time);
  QString queryRestore(const QString &filePath);

  QMenu *modListContextMenu();

  std::set<QString> enabledArchives();

  void scheduleUpdateButton();

  QDir currentSavesDir() const;

  void startMonitorSaves();
  void stopMonitorSaves();

  void dropLocalFile(const QUrl &url, const QString &outputDir, bool move);

private:

  static const char *PATTERN_BACKUP_GLOB;
  static const char *PATTERN_BACKUP_REGEX;
  static const char *PATTERN_BACKUP_DATE;

private:

  Ui::MainWindow *ui;

  bool m_WasVisible;

  MOBase::TutorialControl m_Tutorial;

  QString m_ExeName;

  int m_OldProfileIndex;

  std::vector<QString> m_ModNameList; // the mod-list to go with the directory structure
  QProgressBar *m_RefreshProgress;
  bool m_Refreshing;

  QStringList m_DefaultArchives;

  QAbstractItemModel *m_ModListGroupingProxy;
  ModListSortProxy *m_ModListSortProxy;

  PluginListSortProxy *m_PluginListSortProxy;

  int m_OldExecutableIndex;

  int m_ContextRow;
  QPersistentModelIndex m_ContextIdx;
  QTreeWidgetItem *m_ContextItem;
  QAction *m_ContextAction;

  CategoryFactory &m_CategoryFactory;

  int m_ModsToUpdate;

  bool m_LoginAttempted;

  QTimer m_CheckBSATimer;
  QTimer m_SaveMetaTimer;
  QTimer m_UpdateProblemsTimer;

  QTime m_StartTime;
  SaveGameInfoWidget *m_CurrentSaveView;

  OrganizerCore &m_OrganizerCore;
  PluginContainer &m_PluginContainer;

  QString m_CurrentLanguage;
  std::vector<QTranslator*> m_Translators;

  BrowserDialog m_IntegratedBrowser;

  QFileSystemWatcher m_SavesWatcher;

  std::vector<QTreeWidgetItem*> m_RemoveWidget;

  QByteArray m_ArchiveListHash;

  bool m_DidUpdateMasterList;

  LockedDialog *m_LockDialog { nullptr };

  MOBase::DelayedFileWriter m_ArchiveListWriter;

  enum class ShortcutType {
    Toolbar,
    Desktop,
    StartMenu
  };

  void addWindowsLink(ShortcutType const);

  Executable const &getSelectedExecutable() const;
  Executable &getSelectedExecutable();

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
  void visitWebPage_clicked();
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
  void saveSelectionChanged(QListWidgetItem *newItem);

  void windowTutorialFinished(const QString &windowName);

  BSA::EErrorCode extractBSA(BSA::Archive &archive, BSA::Folder::Ptr folder, const QString &destination, QProgressDialog &extractProgress);

  void createModFromOverwrite();

  void procError(QProcess::ProcessError error);
  void procFinished(int exitCode, QProcess::ExitStatus exitStatus);

  // nexus related
  void checkModsForUpdates();
  void nexusLinkActivated(const QString &link);

  void loginFailed(const QString &message);

  void linkClicked(const QString &url);

  void updateAvailable();

  void motdReceived(const QString &motd);
  void notEndorsedYet();

  void originModified(int originID);

  void addRemoveCategories_MenuHandler();
  void replaceCategories_MenuHandler();

  void savePrimaryCategory();
  void addPrimaryCategoryCandidates();

  void modDetailsUpdated(bool success);

  void modInstalled(const QString &modName);

  void nxmUpdatesAvailable(const std::vector<int> &modIDs, QVariant userData, QVariant resultData, int requestID);
  void nxmEndorsementToggled(int, QVariant, QVariant resultData, int);
  void nxmDownloadURLs(int modID, int fileID, QVariant userData, QVariant resultData, int requestID);
  void nxmRequestFailed(int modID, int fileID, QVariant userData, int requestID, const QString &errorString);

  void editCategories();
  void deselectFilters();

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

  void updateProblemsButton();

  void saveModMetas();

  void updateStyle(const QString &style);

  void modlistChanged(const QModelIndex &index, int role);
  void fileMoved(const QString &filePath, const QString &oldOriginName, const QString &newOriginName);


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

  void modlistSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
  void modListSortIndicatorChanged(int column, Qt::SortOrder order);

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
  void on_actionCopy_Log_to_Clipboard_triggered();
  void on_categoriesAndBtn_toggled(bool checked);
  void on_categoriesOrBtn_toggled(bool checked);
  void on_managedArchiveLabel_linkHovered(const QString &link);
  void on_manageArchivesBox_toggled(bool checked);
};



#endif // MAINWINDOW_H
