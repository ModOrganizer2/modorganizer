#ifndef ORGANIZERCORE_H
#define ORGANIZERCORE_H

#include "selfupdater.h"
#include "settings.h"
#include "modlist.h"
#include "modinfo.h"
#include "pluginlist.h"
#include "installationmanager.h"
#include "downloadmanager.h"
#include "executableslist.h"
#include "usvfsconnector.h"
#include "moshortcut.h"
#include "processrunner.h"
#include "uilocker.h"
#include <imoinfo.h>
#include <iplugindiagnose.h>
#include <versioninfo.h>
#include <delayedfilewriter.h>
#include <boost/signals2.hpp>
#include "executableinfo.h"
#include "moddatacontent.h"
#include <log.h>

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>

class ModListSortProxy;
class PluginListSortProxy;
class Profile;
class IUserInterface;
class PluginContainer;
class DirectoryRefresher;

namespace MOBase
{
  template <typename T> class GuessedValue;
  class IModInterface;
  class IPluginGame;
}

namespace MOShared
{
  class DirectoryEntry;
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

  using SignalAboutToRunApplication = boost::signals2::signal<bool (const QString&), SignalCombinerAnd>;
  using SignalFinishedRunApplication = boost::signals2::signal<void (const QString&, unsigned int)>;
  using SignalModInstalled = boost::signals2::signal<void (const QString&)>;
  using SignalUserInterfaceInitialized = boost::signals2::signal<void (QMainWindow*)>;
  using SignalProfileChanged = boost::signals2::signal<void (MOBase::IProfile *, MOBase::IProfile *)>;
  using SignalPluginSettingChanged = boost::signals2::signal<void (QString const&, const QString& key, const QVariant&, const QVariant&)>;

public:

  /**
   * Small holder for the game content returned by the ModDataContent feature (the
   * list of all possible contents, not the per-mod content).
   */
  struct ModDataContentHolder {

    using Content = ModDataContent::Content;

    /**
     * @return true if the hold list of contents is empty, false otherwise.
     */
    bool empty() const { return m_Contents.empty(); }

    /**
     * @param id ID of the content to retrieve.
     *
     * @return the content with the given ID, or a null pointer if it is not found.
     */
    const Content* findById(int id) const {
      auto it = std::find_if(std::begin(m_Contents), std::end(m_Contents), [&id](auto const& content) { return content.id() == id; });
      return it == std::end(m_Contents) ? nullptr : &(*it);
    }

    /**
     * Apply the given function to each content whose ID is in the given set.
     *
     * @param ids The set of content IDs.
     * @param fn The function to apply.
     * @param includeFilter true to also apply the function to filter-only contents, false otherwise.
     */
    template <class Fn>
    void forEachContentIn(std::set<int> const& ids, Fn const& fn, bool includeFilter = false) const {
      for (const auto& content : m_Contents) {
        if ((includeFilter || !content.isOnlyForFilter())
          && ids.find(content.id()) != ids.end()) {
          fn(content);
        }
      }
    }

    /**
     * @brief Apply fnIn to each content whose ID is in the given set, and fnOut to each content not in the
     *   given set, excluding filter-only content (from both cases) unless includeFilter is true.
     *
     * @param ids The set of content IDs.
     * @param fnIn Function to apply to content whose IDs are in ids.
     * @param fnOut Function to apply to content whose IDs are not in ids.
     * @param includeFilter true to also apply the function to filter-only contents, false otherwise.
     */
    template <class FnIn, class FnOut>
    void forEachContentInOrOut(std::set<int> const& ids, FnIn const& fnIn, FnOut const& fnOut, bool includeFilter = false) const {
      for (const auto& content : m_Contents) {
        if ((includeFilter || !content.isOnlyForFilter())) {
          if (ids.find(content.id()) != ids.end()) {
            fnIn(content);
          }
          else {
            fnOut(content);
          }
        }
      }
    }

    /**
     * Apply the given function to each content.
     *
     * @param fn The function to apply.
     * @param includeFilter true to also apply the function to filter-only contents, false otherwise.
     */
    template <class Fn>
    void forEachContent(Fn const& fn, bool includeFilter = false) const {
      for (const auto& content : m_Contents) {
        if (includeFilter || !content.isOnlyForFilter()) {
          fn(content);
        }
      }
    }


    ModDataContentHolder& operator=(ModDataContentHolder const&) = delete;
    ModDataContentHolder& operator=(ModDataContentHolder&&) = default;

  private:

    std::vector<Content> m_Contents;

    /**
     * @brief Construct a ModDataContentHolder without any contents (e.g., if the feature is
     *     missing).
     */
    ModDataContentHolder() { }

    /**
     * @brief Construct a ModDataContentHold holding the given list of contents.
     */
    ModDataContentHolder(std::vector<ModDataContent::Content> contents) :
      m_Contents(std::move(contents)) { }

    friend class OrganizerCore;
  };

public:

  static bool isNxmLink(const QString &link) { return link.startsWith("nxm://", Qt::CaseInsensitive); }

  OrganizerCore(Settings &settings);

  ~OrganizerCore();

  void setUserInterface(IUserInterface* ui);
  void connectPlugins(PluginContainer *container);
  void disconnectPlugins();

  void setManagedGame(MOBase::IPluginGame *game);

  void updateExecutablesList();
  void updateModInfoFromDisc();

  void checkForUpdates();
  void startMOUpdate();

  Settings &settings();
  SelfUpdater *updater() { return &m_Updater; }
  InstallationManager *installationManager();
  MOShared::DirectoryEntry *directoryStructure() { return m_DirectoryStructure; }
  DirectoryRefresher *directoryRefresher() { return m_DirectoryRefresher.get(); }
  ExecutablesList *executablesList() { return &m_ExecutablesList; }
  void setExecutablesList(const ExecutablesList &executablesList) {
    m_ExecutablesList = executablesList;
  }

  Profile *currentProfile() const { return m_CurrentProfile.get(); }
  void setCurrentProfile(const QString &profileName);

  std::vector<QString> enabledArchives();

  MOBase::VersionInfo getVersion() const { return m_Updater.getVersion(); }

  ModListSortProxy *createModListProxyModel();
  PluginListSortProxy *createPluginListProxyModel();

  MOBase::IPluginGame const *managedGame() const;

  /**
   * @return the list of contents for the currently managed game, or an empty vector
   *     if the game plugin does not implement the ModDataContent feature.
   */
  const ModDataContentHolder& modDataContents() const { return m_Contents; }

  bool isArchivesInit() const { return m_ArchivesInit; }

  bool saveCurrentLists();

  ProcessRunner processRunner();

  bool beforeRun(
    const QFileInfo& binary, const QString& profileName,
    const QString& customOverwrite,
    const QList<MOBase::ExecutableForcedLoadSetting>& forcedLibraries);

  void afterRun(const QFileInfo& binary, DWORD exitCode);

  ProcessRunner::Results waitForAllUSVFSProcesses(
    UILocker::Reasons reason=UILocker::PreventExit);

  void refreshESPList(bool force = false);
  void refreshBSAList();

  void refreshDirectoryStructure();
  void updateModInDirectoryStructure(unsigned int index, ModInfo::Ptr modInfo);
  void updateModsInDirectoryStructure(QMap<unsigned int, ModInfo::Ptr> modInfos);

  void doAfterLogin(const std::function<void()> &function) { m_PostLoginTasks.append(function); }
  void loggedInAction(QWidget* parent, std::function<void ()> f);

  bool previewFileWithAlternatives(QWidget* parent, QString filename, int selectedOrigin=-1);
  bool previewFile(QWidget* parent, const QString& originName, const QString& path);

  void loginSuccessfulUpdate(bool necessary);
  void loginFailedUpdate(const QString &message);

  static bool createAndMakeWritable(const QString &path);
  bool checkPathSymlinks();
  bool bootstrap();
  void createDefaultProfile();

  MOBase::DelayedFileWriter &pluginsWriter() { return m_PluginListsWriter; }

  void prepareVFS();

  void updateVFSParams(
    MOBase::log::Levels logLevel, CrashDumpsType crashDumpsType,
    const QString& crashDumpsPath, std::chrono::seconds spawnDelay,
    QString executableBlacklist);

  void setLogLevel(MOBase::log::Levels level);

  bool cycleDiagnostics();

  static CrashDumpsType getGlobalCrashDumpsType() { return m_globalCrashDumpsType; }
  static void setGlobalCrashDumpsType(CrashDumpsType crashDumpsType);
  static std::wstring crashDumpsPath();

public:
  MOBase::IModRepositoryBridge *createNexusBridge() const;
  QString profileName() const;
  QString profilePath() const;
  QString downloadsPath() const;
  QString overwritePath() const;
  QString basePath() const;
  QString modsPath() const;
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
  void refreshModList(bool saveChanges = true);
  QStringList modsSortedByProfilePriority() const;


  bool onModInstalled(const std::function<void(const QString&)>& func);
  bool onAboutToRun(const std::function<bool(const QString&)>& func);
  bool onFinishedRun(const std::function<void(const QString&, unsigned int)>& func);
  bool onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func);
  bool onProfileChanged(std::function<void(MOBase::IProfile*, MOBase::IProfile*)> const& func);
  bool onPluginSettingChanged(std::function<void(QString const&, const QString& key, const QVariant&, const QVariant&)> const& func);

  bool getArchiveParsing() const
  {
    return m_ArchiveParsing;
  }

  void setArchiveParsing(bool archiveParsing)
  {
    m_ArchiveParsing = archiveParsing;
  }

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
  void modStatusChanged(QList<unsigned int> index);
  void requestDownload(const QUrl &url, QNetworkReply *reply);
  void downloadRequestedNXM(const QString &url);

  void userInterfaceInitialized();

  bool nexusApi(bool retry = false);

signals:

  /**
   * @brief emitted after a mod has been installed
   * @node this is currently only used for tutorials
   */
  void modInstalled(const QString &modName);

  void managedGameChanged(MOBase::IPluginGame const *gamePlugin);

  void close();

  // Notify that the directory structure is ready to be used on the main thread
  // Use queued connections
  void directoryStructureReady();

private:

  void saveCurrentProfile();
  void storeSettings();

  bool queryApi(QString &apiKey);

  void updateModActiveState(int index, bool active);
  void updateModsActiveState(const QList<unsigned int> &modIndices, bool active);

  bool testForSteam(bool *found, bool *access);

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
  IUserInterface* m_UserInterface;
  PluginContainer *m_PluginContainer;
  QString m_GameName;
  MOBase::IPluginGame *m_GamePlugin;
  ModDataContentHolder m_Contents;

  std::unique_ptr<Profile> m_CurrentProfile;

  Settings& m_Settings;

  SelfUpdater m_Updater;

  SignalAboutToRunApplication m_AboutToRun;
  SignalFinishedRunApplication m_FinishedRun;
  SignalModInstalled m_ModInstalled;
  SignalUserInterfaceInitialized m_UserInterfaceInitialized;
  SignalProfileChanged m_ProfileChanged;
  SignalPluginSettingChanged m_PluginSettingChanged;

  ModList m_ModList;
  PluginList m_PluginList;


  QList<std::function<void()>> m_PostLoginTasks;
  QList<std::function<void()>> m_PostRefreshTasks;

  ExecutablesList m_ExecutablesList;
  QStringList m_PendingDownloads;
  QStringList m_DefaultArchives;
  QStringList m_ActiveArchives;

  std::unique_ptr<DirectoryRefresher> m_DirectoryRefresher;
  MOShared::DirectoryEntry *m_DirectoryStructure;

  DownloadManager m_DownloadManager;
  InstallationManager m_InstallationManager;

  QThread m_RefresherThread;

  std::thread m_StructureDeleter;

  bool m_DirectoryUpdate;
  bool m_ArchivesInit;
  bool m_ArchiveParsing{ m_Settings.archiveParsing() };

  MOBase::DelayedFileWriter m_PluginListsWriter;
  UsvfsConnector m_USVFS;

  UILocker m_UILocker;

  static CrashDumpsType m_globalCrashDumpsType;
};

#endif // ORGANIZERCORE_H
