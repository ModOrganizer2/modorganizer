#ifndef ORGANIZERCORE_H
#define ORGANIZERCORE_H


#include "profile.h"
#include "selfupdater.h"
#include "iuserinterface.h"
#include "settings.h"
#include "modlist.h"
#include "pluginlist.h"
#include "directoryrefresher.h"
#include "installationmanager.h"
#include "downloadmanager.h"
#include "modlistsortproxy.h"
#include "pluginlistsortproxy.h"
#include "executableslist.h"
#include <directoryentry.h>
#include <imoinfo.h>
#include <iplugindiagnose.h>
#include <versioninfo.h>
#include <guessedvalue.h>
#include <boost/signals2.hpp>
#include <QSettings>
#include <QString>
#include <QThread>


class PluginContainer;


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

  OrganizerCore(const QSettings &initSettings);

  ~OrganizerCore();

  void setUserInterface(IUserInterface *userInterface, QWidget *widget);
  void connectPlugins(PluginContainer *container);
  void disconnectPlugins();

  void updateExecutablesList(QSettings &settings);

  void startMOUpdate();

  Settings &settings();
  SelfUpdater *updater() { return &m_Updater; }
  InstallationManager *installationManager();
  MOShared::DirectoryEntry *directoryStructure() { return m_DirectoryStructure; }
  DirectoryRefresher *directoryRefresher() { return &m_DirectoryRefresher; }
  ExecutablesList *executablesList() { return &m_ExecutablesList; }
  void setExecutablesDialog(const ExecutablesList &executablesList) { m_ExecutablesList = executablesList; }

  Profile *currentProfile() { return m_CurrentProfile; }
  void setCurrentProfile(Profile *profile);

  void setExecutablesList(const ExecutablesList &executablesList);

  std::set<QString> enabledArchives();

  MOBase::VersionInfo getVersion() const { return m_Updater.getVersion(); }

  ModListSortProxy *createModListProxyModel();
  PluginListSortProxy *createPluginListProxyModel();

  bool isArchivesInit() const { return m_ArchivesInit; }

  bool saveCurrentLists();
  void savePluginList();

  void prepareStart();

  void refreshESPList();
  void refreshBSAList();

  void refreshDirectoryStructure();
  void updateModInDirectoryStructure(unsigned int index, ModInfo::Ptr modInfo);

  void requestDownload(const QUrl &url, QNetworkReply *reply);

  void doAfterLogin(std::function<void()> &function) { m_PostLoginTasks.append(function); }

  void spawnBinary(const QFileInfo &binary, const QString &arguments = "", const QDir &currentDirectory = QDir(), bool closeAfterStart = true, const QString &steamAppID = "");

  void modStatusChanged(unsigned int index);

  void loginSuccessful(bool necessary);
  void loginSuccessfulUpdate(bool necessary);
  void loginFailed(const QString &message);
  void loginFailedUpdate(const QString &message);

public:
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
  virtual QVariant persistent(const QString &pluginName, const QString &key, const QVariant &def) const;
  virtual void setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync);
  virtual QString pluginDataPath() const;
  virtual MOBase::IModInterface *installMod(const QString &fileName);
  virtual QString resolvePath(const QString &fileName) const;
  virtual QStringList listDirectories(const QString &directoryName) const;
  virtual QStringList findFiles(const QString &path, const std::function<bool (const QString &)> &filter) const;
  virtual QStringList getFileOrigins(const QString &fileName) const;
  virtual QList<MOBase::IOrganizer::FileInfo> findFileInfos(const QString &path, const std::function<bool (const MOBase::IOrganizer::FileInfo &)> &filter) const;
  virtual DownloadManager *downloadManager();
  virtual PluginList *pluginList();
  virtual ModList *modList();
  virtual HANDLE startApplication(const QString &executable, const QStringList &args, const QString &cwd, const QString &profile);
  virtual bool waitForApplication(HANDLE handle, LPDWORD exitCode) const;
  virtual bool onModInstalled(const std::function<void (const QString &)> &func);
  virtual bool onAboutToRun(const std::function<bool (const QString &)> &func);
  virtual bool onFinishedRun(const std::function<void (const QString &, unsigned int)> &func);
  virtual void refreshModList(bool saveChanges = true);

public: // IPluginDiagnose interface

  virtual std::vector<unsigned int> activeProblems() const;
  virtual QString shortDescription(unsigned int key) const;
  virtual QString fullDescription(unsigned int key) const;
  virtual bool hasGuidedFix(unsigned int key) const;
  virtual void startGuidedFix(unsigned int key) const;

public slots:

  void profileRefresh();
  void externalMessage(const QString &message);

  void refreshLists();

signals:

  /**
   * @brief emitted after a mod has been installed
   * @node this is currently only used for tutorials
   */
  void modInstalled();

private:

  void storeSettings();

  bool queryLogin(QString &username, QString &password);

  bool nexusLogin();

  HANDLE spawnBinaryDirect(const QFileInfo &binary, const QString &arguments, const QString &profileName, const QDir &currentDirectory, const QString &steamAppID);
  void updateModActiveState(int index, bool active);

private slots:

  void directory_refreshed();
  void downloadRequestedNXM(const QString &url);
  void downloadRequested(QNetworkReply *reply, int modID, const QString &fileName);
  void removeOrigin(const QString &name);

private:

  static const unsigned int PROBLEM_TOOMANYPLUGINS = 1;

private:

  MOBase::IGameInfo *m_GameInfo;

  IUserInterface *m_UserInterface;
  PluginContainer *m_PluginContainer;

  Profile *m_CurrentProfile;

  Settings m_Settings;

  SelfUpdater m_Updater;

  SignalAboutToRunApplication m_AboutToRun;
  SignalFinishedRunApplication m_FinishedRun;
  SignalModInstalled m_ModInstalled;

  ModList m_ModList;
  PluginList m_PluginList;

  QList<std::function<void()>> m_PostLoginTasks;

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

};

#endif // ORGANIZERCORE_H
