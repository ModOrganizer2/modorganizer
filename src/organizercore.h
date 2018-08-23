#ifndef ORGANIZERCORE_H
#define ORGANIZERCORE_H


#include "selfupdater.h"
#include "iuserinterface.h" //should be class IUserInterface;
#include "settings.h"
#include "modlist.h"
#include "modinfo.h"
#include "pluginlist.h"
#include "directoryrefresher.h"
#include "installationmanager.h"
#include "downloadmanager.h"
#include "executableslist.h"
#include "usvfsconnector.h"
#include "moshortcut.h"
#include <directoryentry.h>
#include <imoinfo.h>
#include <iplugindiagnose.h>
#include <versioninfo.h>
#include <delayedfilewriter.h>
#include <boost/signals2.hpp>

class ModListSortProxy;
class PluginListSortProxy;
class Profile;
namespace MOBase {
  template <typename T> class GuessedValue;
  class IModInterface;
}
namespace MOShared { class DirectoryEntry; }

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>

class QNetworkReply;
class QUrl;
class QWidget;

#include <Windows.h> //for HANDLE, LPDWORD

#include <functional>
#include <vector>

class PluginContainer;

namespace MOBase {
  class IPluginGame;
}

class OrganizerCore : public QObject, public MOBase::IPluginDiagnose
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

private:

  typedef boost::signals2::signal<bool (const QString&), SignalCombinerAnd> SignalAboutToRunApplication;
  typedef boost::signals2::signal<void (const QString&, unsigned int)> SignalFinishedRunApplication;
  typedef boost::signals2::signal<void (const QString&)> SignalModInstalled;

public:
  static bool isNxmLink(const QString &link) { return link.startsWith("nxm://", Qt::CaseInsensitive); }

  OrganizerCore(const QSettings &initSettings);

  ~OrganizerCore();

  void setUserInterface(IUserInterface *userInterface, QWidget *widget);
  void connectPlugins(PluginContainer *container);
  void disconnectPlugins();

  void setManagedGame(MOBase::IPluginGame *game);

  void updateExecutablesList(QSettings &settings);

  void startMOUpdate();

  Settings &settings();
  SelfUpdater *updater() { return &m_Updater; }
  InstallationManager *installationManager();
  MOShared::DirectoryEntry *directoryStructure() { return m_DirectoryStructure; }
  DirectoryRefresher *directoryRefresher() { return &m_DirectoryRefresher; }
  ExecutablesList *executablesList() { return &m_ExecutablesList; }
  void setExecutablesList(const ExecutablesList &executablesList) {
    m_ExecutablesList = executablesList;
  }

  Profile *currentProfile() const { return m_CurrentProfile; }
  void setCurrentProfile(const QString &profileName);

  std::vector<QString> enabledArchives();

  MOBase::VersionInfo getVersion() const { return m_Updater.getVersion(); }

  ModListSortProxy *createModListProxyModel();
  PluginListSortProxy *createPluginListProxyModel();

  MOBase::IPluginGame const *managedGame() const;

  bool isArchivesInit() const { return m_ArchivesInit; }

  bool saveCurrentLists();

  void prepareStart();

  void refreshESPList(bool force = false);
  void refreshBSAList();

  void refreshDirectoryStructure();
  void updateModInDirectoryStructure(unsigned int index, ModInfo::Ptr modInfo);

  void doAfterLogin(const std::function<void()> &function) { m_PostLoginTasks.append(function); }

  void spawnBinary(const QFileInfo &binary, const QString &arguments = "",
                   const QDir &currentDirectory = QDir(),
                   const QString &steamAppID = "",
                   const QString &customOverwrite = "");

  HANDLE spawnBinaryDirect(const QFileInfo &binary, const QString &arguments,
                           const QString &profileName,
                           const QDir &currentDirectory,
                           const QString &steamAppID,
                           const QString &customOverwrite,
                           LPDWORD exitCode = nullptr);

  HANDLE spawnBinaryProcess(const QFileInfo &binary, const QString &arguments,
                            const QString &profileName,
                            const QDir &currentDirectory,
                            const QString &steamAppID,
                            const QString &customOverwrite);

  void loginSuccessfulUpdate(bool necessary);
  void loginFailedUpdate(const QString &message);

  static bool createAndMakeWritable(const QString &path);
  bool bootstrap();
  void createDefaultProfile();

  MOBase::DelayedFileWriter &pluginsWriter() { return m_PluginListsWriter; }

  void prepareVFS();

  void updateVFSParams(int logLevel, int crashDumpsType);

  bool cycleDiagnostics();

  static CrashDumpsType getGlobalCrashDumpsType() { return m_globalCrashDumpsType; }
  static void setGlobalCrashDumpsType(int crashDumpsType);
  static std::wstring crashDumpsPath();

public:
  MOBase::IModRepositoryBridge *createNexusBridge() const;
  QString profileName() const;
  QString profilePath() const;
  QString downloadsPath() const;
  QString overwritePath() const;
  QString basePath() const;
  MOBase::VersionInfo appVersion() const;
  MOBase::IModInterface *getMod(const QString &name) const;
  MOBase::IPluginGame *getGame(const QString &gameName) const;
  MOBase::IModInterface *createMod(MOBase::GuessedValue<QString> &name);
  bool removeMod(MOBase::IModInterface *mod);
  void modDataChanged(MOBase::IModInterface *mod);
  QVariant pluginSetting(const QString &pluginName, const QString &key) const;
  void setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value);
  QVariant persistent(const QString &pluginName, const QString &key, const QVariant &def) const;
  void setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync);
  QString pluginDataPath() const;
  virtual MOBase::IModInterface *installMod(const QString &fileName, const QString &initModName);
  QString resolvePath(const QString &fileName) const;
  QStringList listDirectories(const QString &directoryName) const;
  QStringList findFiles(const QString &path, const std::function<bool (const QString &)> &filter) const;
  QStringList getFileOrigins(const QString &fileName) const;
  QList<MOBase::IOrganizer::FileInfo> findFileInfos(const QString &path, const std::function<bool (const MOBase::IOrganizer::FileInfo &)> &filter) const;
  DownloadManager *downloadManager();
  PluginList *pluginList();
  ModList *modList();
  HANDLE runShortcut(const MOShortcut& shortcut);
  HANDLE startApplication(const QString &executable, const QStringList &args, const QString &cwd, const QString &profile);
  bool waitForApplication(HANDLE processHandle, LPDWORD exitCode = nullptr);
  HANDLE findAndOpenAUSVFSProcess(const std::vector<QString>& hiddenList, DWORD preferedParentPid);
  bool onModInstalled(const std::function<void (const QString &)> &func);
  bool onAboutToRun(const std::function<bool (const QString &)> &func);
  bool onFinishedRun(const std::function<void (const QString &, unsigned int)> &func);
  void refreshModList(bool saveChanges = true);
  QStringList modsSortedByProfilePriority() const;
  bool getArchiveParsing() const;
  void setArchiveParsing(bool archiveParsing);

public: // IPluginDiagnose interface

  virtual std::vector<unsigned int> activeProblems() const;
  virtual QString shortDescription(unsigned int key) const;
  virtual QString fullDescription(unsigned int key) const;
  virtual bool hasGuidedFix(unsigned int key) const;
  virtual void startGuidedFix(unsigned int key) const;

public slots:

  void profileRefresh();
  void externalMessage(const QString &message);

  void syncOverwrite();

  void savePluginList();

  void refreshLists();

  void installDownload(int downloadIndex);

  void modStatusChanged(unsigned int index);
  void requestDownload(const QUrl &url, QNetworkReply *reply);
  void downloadRequestedNXM(const QString &url);

  bool nexusLogin(bool retry = false);

signals:

  /**
   * @brief emitted after a mod has been installed
   * @node this is currently only used for tutorials
   */
  void modInstalled(const QString &modName);

  void managedGameChanged(MOBase::IPluginGame const *gamePlugin);

  void close();

private:

  void storeSettings();

  QSettings::Status storeSettings(const QString &fileName);

  QString commitSettings(const QString &iniFile);

  bool queryLogin(QString &username, QString &password);

  void updateModActiveState(int index, bool active);

  bool testForSteam();

  bool createDirectory(const QString &path);

  QString oldMO1HookDll() const;

  /**
   * @brief return a descriptor of the mappings real file->virtual file
   */
  std::vector<Mapping> fileMapping(const QString &profile,
                                   const QString &customOverwrite);

  std::vector<Mapping>
  fileMapping(const QString &dataPath, const QString &relPath,
              const MOShared::DirectoryEntry *base,
              const MOShared::DirectoryEntry *directoryEntry,
              int createDestination);

  bool waitForProcessCompletion(HANDLE handle, LPDWORD exitCode, ILockedWaitingForProcess* uilock);

private slots:

  void directory_refreshed();
  void downloadRequested(QNetworkReply *reply, QString gameName, int modID, const QString &fileName);
  void removeOrigin(const QString &name);
  void downloadSpeed(const QString &serverName, int bytesPerSecond);
  void loginSuccessful(bool necessary);
  void loginFailed(const QString &message);

private:
  static const unsigned int PROBLEM_MO1SCRIPTEXTENDERWORKAROUND = 1;

private:

  IUserInterface *m_UserInterface;
  PluginContainer *m_PluginContainer;
  QString m_GameName;
  MOBase::IPluginGame *m_GamePlugin;

  Profile *m_CurrentProfile;

  Settings m_Settings;

  SelfUpdater m_Updater;

  SignalAboutToRunApplication m_AboutToRun;
  SignalFinishedRunApplication m_FinishedRun;
  SignalModInstalled m_ModInstalled;

  ModList m_ModList;
  PluginList m_PluginList;


  QList<std::function<void()>> m_PostLoginTasks;
  QList<std::function<void()>> m_PostRefreshTasks;

  ExecutablesList m_ExecutablesList;
  QStringList m_PendingDownloads;
  QStringList m_DefaultArchives;
  QStringList m_ActiveArchives;

  DirectoryRefresher m_DirectoryRefresher;
  MOShared::DirectoryEntry *m_DirectoryStructure;

  DownloadManager m_DownloadManager;
  InstallationManager m_InstallationManager;

  QThread m_RefresherThread;

  bool m_AskForNexusPW;
  bool m_DirectoryUpdate;
  bool m_ArchivesInit;
  bool m_ArchiveParsing{ m_Settings.archiveParsing() };

  MOBase::DelayedFileWriter m_PluginListsWriter;
  UsvfsConnector m_USVFS;

  static CrashDumpsType m_globalCrashDumpsType;
};

#endif // ORGANIZERCORE_H
