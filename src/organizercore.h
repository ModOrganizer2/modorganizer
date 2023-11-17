#ifndef ORGANIZERCORE_H
#define ORGANIZERCORE_H

#include "downloadmanager.h"
#include "envdump.h"
#include "executableinfo.h"
#include "executableslist.h"
#include "guessedvalue.h"
#include "installationmanager.h"
#include "memoizedlock.h"
#include "moddatacontent.h"
#include "modinfo.h"
#include "modlist.h"
#include "moshortcut.h"
#include "pluginlist.h"
#include "processrunner.h"
#include "selfupdater.h"
#include "settings.h"
#include "uilocker.h"
#include "usvfsconnector.h"
#include <boost/signals2.hpp>
#include <delayedfilewriter.h>
#include <imoinfo.h>
#include <iplugindiagnose.h>
#include <log.h>
#include <versioninfo.h>

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>

class IniBakery;
class ModListSortProxy;
class PluginListSortProxy;
class Profile;
class IUserInterface;
class PluginManager;
class DirectoryRefresher;

namespace MOBase
{
template <typename T>
class GuessedValue;
class IModInterface;
class IPluginGame;
}  // namespace MOBase

namespace MOShared
{
class DirectoryEntry;
}

class OrganizerCore : public QObject, public MOBase::IPluginDiagnose
{

  Q_OBJECT
  Q_INTERFACES(MOBase::IPluginDiagnose)

private:
  friend class OrganizerProxy;

  struct SignalCombinerAnd
  {
    using result_type = bool;

    template <typename InputIterator>
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
  using SignalAboutToRunApplication =
      boost::signals2::signal<bool(const QString&, const QDir&, const QString&),
                              SignalCombinerAnd>;
  using SignalFinishedRunApplication =
      boost::signals2::signal<void(const QString&, unsigned int)>;
  using SignalUserInterfaceInitialized = boost::signals2::signal<void(QMainWindow*)>;
  using SignalProfileCreated = boost::signals2::signal<void(MOBase::IProfile*)>;
  using SignalProfileRenamed =
      boost::signals2::signal<void(MOBase::IProfile*, QString const&, QString const&)>;
  using SignalProfileRemoved = boost::signals2::signal<void(QString const&)>;
  using SignalProfileChanged =
      boost::signals2::signal<void(MOBase::IProfile*, MOBase::IProfile*)>;
  using SignalPluginSettingChanged = boost::signals2::signal<void(
      QString const&, const QString& key, const QVariant&, const QVariant&)>;
  using SignalPluginEnabled = boost::signals2::signal<void(const MOBase::IPlugin*)>;

public:
  /**
   * Small holder for the game content returned by the ModDataContent feature (the
   * list of all possible contents, not the per-mod content).
   */
  struct ModDataContentHolder
  {

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
    const Content* findById(int id) const
    {
      auto it = std::find_if(std::begin(m_Contents), std::end(m_Contents),
                             [&id](auto const& content) {
                               return content.id() == id;
                             });
      return it == std::end(m_Contents) ? nullptr : &(*it);
    }

    /**
     * Apply the given function to each content whose ID is in the given set.
     *
     * @param ids The set of content IDs.
     * @param fn The function to apply.
     * @param includeFilter true to also apply the function to filter-only contents,
     * false otherwise.
     */
    template <class Fn>
    void forEachContentIn(std::set<int> const& ids, Fn const& fn,
                          bool includeFilter = false) const
    {
      for (const auto& content : m_Contents) {
        if ((includeFilter || !content.isOnlyForFilter()) &&
            ids.find(content.id()) != ids.end()) {
          fn(content);
        }
      }
    }

    /**
     * @brief Apply fnIn to each content whose ID is in the given set, and fnOut to each
     * content not in the given set, excluding filter-only content (from both cases)
     * unless includeFilter is true.
     *
     * @param ids The set of content IDs.
     * @param fnIn Function to apply to content whose IDs are in ids.
     * @param fnOut Function to apply to content whose IDs are not in ids.
     * @param includeFilter true to also apply the function to filter-only contents,
     * false otherwise.
     */
    template <class FnIn, class FnOut>
    void forEachContentInOrOut(std::set<int> const& ids, FnIn const& fnIn,
                               FnOut const& fnOut, bool includeFilter = false) const
    {
      for (const auto& content : m_Contents) {
        if ((includeFilter || !content.isOnlyForFilter())) {
          if (ids.find(content.id()) != ids.end()) {
            fnIn(content);
          } else {
            fnOut(content);
          }
        }
      }
    }

    /**
     * Apply the given function to each content.
     *
     * @param fn The function to apply.
     * @param includeFilter true to also apply the function to filter-only contents,
     * false otherwise.
     */
    template <class Fn>
    void forEachContent(Fn const& fn, bool includeFilter = false) const
    {
      for (const auto& content : m_Contents) {
        if (includeFilter || !content.isOnlyForFilter()) {
          fn(content);
        }
      }
    }

    ModDataContentHolder& operator=(ModDataContentHolder const&) = delete;
    ModDataContentHolder& operator=(ModDataContentHolder&&)      = default;

  private:
    std::vector<Content> m_Contents;

    /**
     * @brief Construct a ModDataContentHolder without any contents (e.g., if the
     * feature is missing).
     */
    ModDataContentHolder() {}

    /**
     * @brief Construct a ModDataContentHold holding the given list of contents.
     */
    ModDataContentHolder(std::vector<ModDataContent::Content> contents)
        : m_Contents(std::move(contents))
    {}

    friend class OrganizerCore;
  };

  // enumeration for the mode when adding refresh callbacks
  //
  enum class RefreshCallbackMode : int
  {
    // run the callbacks immediately if no refresh is running
    RUN_NOW_IF_POSSIBLE = 0,

    // wait for the next refresh if none is running
    FORCE_WAIT_FOR_REFRESH = 1
  };

  // enumeration for the groups where refresh callbacks can be put
  //
  enum class RefreshCallbackGroup : int
  {
    // for callbacks by the core itself, highest priority
    CORE = 0,

    // internal MO2 callbacks
    INTERNAL = 1,

    // external callbacks, typically MO2 plugins
    EXTERNAL = 2
  };

public:
  OrganizerCore(Settings& settings);

  ~OrganizerCore();

  void setUserInterface(IUserInterface* ui);
  void connectPlugins(PluginManager* manager);

  void setManagedGame(MOBase::IPluginGame* game);

  void updateExecutablesList();
  void updateModInfoFromDisc();

  void checkForUpdates();
  void startMOUpdate();

  Settings& settings();
  SelfUpdater* updater() { return &m_Updater; }
  InstallationManager* installationManager();
  MOShared::DirectoryEntry* directoryStructure() { return m_DirectoryStructure; }
  DirectoryRefresher* directoryRefresher() { return m_DirectoryRefresher.get(); }
  ExecutablesList* executablesList() { return &m_ExecutablesList; }
  void setExecutablesList(const ExecutablesList& executablesList)
  {
    m_ExecutablesList = executablesList;
  }

  Profile* currentProfile() const { return m_CurrentProfile.get(); }
  void setCurrentProfile(const QString& profileName);

  std::vector<QString> enabledArchives();

  MOBase::VersionInfo getVersion() const { return m_Updater.getVersion(); }

  // return the plugin manager
  //
  PluginManager& pluginManager() const;

  MOBase::IPluginGame const* managedGame() const;

  /**
   * @brief Retrieve the organizer proxy of the currently managed game.
   *
   */
  MOBase::IOrganizer const* managedGameOrganizer() const;

  /**
   * @return the list of contents for the currently managed game, or an empty vector
   *     if the game plugin does not implement the ModDataContent feature.
   */
  const ModDataContentHolder& modDataContents() const { return m_Contents; }

  bool isArchivesInit() const { return m_ArchivesInit; }

  bool saveCurrentLists();

  ProcessRunner processRunner();

  bool beforeRun(const QFileInfo& binary, const QDir& cwd, const QString& arguments,
                 const QString& profileName, const QString& customOverwrite,
                 const QList<MOBase::ExecutableForcedLoadSetting>& forcedLibraries);

  void afterRun(const QFileInfo& binary, DWORD exitCode);

  ProcessRunner::Results
  waitForAllUSVFSProcesses(UILocker::Reasons reason = UILocker::PreventExit);

  void refreshESPList(bool force = false);
  void refreshBSAList();

  void refreshDirectoryStructure();
  void updateModInDirectoryStructure(unsigned int index, ModInfo::Ptr modInfo);
  void updateModsInDirectoryStructure(QMap<unsigned int, ModInfo::Ptr> modInfos);

  void doAfterLogin(const std::function<void()>& function)
  {
    m_PostLoginTasks.append(function);
  }
  void loggedInAction(QWidget* parent, std::function<void()> f);

  bool previewFileWithAlternatives(QWidget* parent, QString filename,
                                   int selectedOrigin = -1);
  bool previewFile(QWidget* parent, const QString& originName, const QString& path);

  void loginSuccessfulUpdate(bool necessary);
  void loginFailedUpdate(const QString& message);

  static bool createAndMakeWritable(const QString& path);
  bool checkPathSymlinks();
  bool bootstrap();
  void createDefaultProfile();

  MOBase::DelayedFileWriter& pluginsWriter() { return m_PluginListsWriter; }

  void prepareVFS();

  void updateVFSParams(MOBase::log::Levels logLevel, env::CoreDumpTypes coreDumpType,
                       const QString& coreDumpsPath, std::chrono::seconds spawnDelay,
                       QString executableBlacklist);

  void setLogLevel(MOBase::log::Levels level);

  bool cycleDiagnostics();

  static env::CoreDumpTypes getGlobalCoreDumpType();
  static void setGlobalCoreDumpType(env::CoreDumpTypes type);
  static std::wstring getGlobalCoreDumpPath();

public:
  MOBase::IModRepositoryBridge* createNexusBridge() const;
  QString profileName() const;
  QString profilePath() const;
  QString downloadsPath() const;
  QString overwritePath() const;
  QString basePath() const;
  QString modsPath() const;
  MOBase::VersionInfo appVersion() const;
  MOBase::IPluginGame* getGame(const QString& gameName) const;
  MOBase::IModInterface* createMod(MOBase::GuessedValue<QString>& name);
  void modDataChanged(MOBase::IModInterface* mod);
  QVariant pluginSetting(const QString& pluginName, const QString& key) const;
  void setPluginSetting(const QString& pluginName, const QString& key,
                        const QVariant& value);
  QVariant persistent(const QString& pluginName, const QString& key,
                      const QVariant& def) const;
  void setPersistent(const QString& pluginName, const QString& key,
                     const QVariant& value, bool sync);
  static QString pluginDataPath();
  virtual MOBase::IModInterface* installMod(const QString& fileName, int priority,
                                            bool reinstallation,
                                            ModInfo::Ptr currentMod,
                                            const QString& initModName);
  QString resolvePath(const QString& fileName) const;
  QStringList listDirectories(const QString& directoryName) const;
  QStringList findFiles(const QString& path,
                        const std::function<bool(const QString&)>& filter) const;
  QStringList getFileOrigins(const QString& fileName) const;
  QList<MOBase::IOrganizer::FileInfo> findFileInfos(
      const QString& path,
      const std::function<bool(const MOBase::IOrganizer::FileInfo&)>& filter) const;
  DownloadManager* downloadManager();
  PluginList* pluginList();
  ModList* modList();
  void refresh(bool saveChanges = true);

  boost::signals2::connection onAboutToRun(
      const std::function<bool(const QString&, const QDir&, const QString&)>& func);
  boost::signals2::connection
  onFinishedRun(const std::function<void(const QString&, unsigned int)>& func);
  boost::signals2::connection
  onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func);
  boost::signals2::connection
  onProfileCreated(std::function<void(MOBase::IProfile*)> const& func);
  boost::signals2::connection onProfileRenamed(
      std::function<void(MOBase::IProfile*, QString const&, QString const&)> const&
          func);
  boost::signals2::connection
  onProfileRemoved(std::function<void(QString const&)> const& func);
  boost::signals2::connection onProfileChanged(
      std::function<void(MOBase::IProfile*, MOBase::IProfile*)> const& func);
  boost::signals2::connection onPluginSettingChanged(
      std::function<void(QString const&, const QString& key, const QVariant&,
                         const QVariant&)> const& func);
  boost::signals2::connection
  onPluginEnabled(std::function<void(const MOBase::IPlugin*)> const& func);
  boost::signals2::connection
  onPluginDisabled(std::function<void(const MOBase::IPlugin*)> const& func);

  // add a function to be called after the next refresh is done
  //
  // - group to add the function to
  // - if immediateIfReady is true, the function will be called immediately if no
  //   directory update is running
  boost::signals2::connection onNextRefresh(std::function<void()> const& func,
                                            RefreshCallbackGroup group,
                                            RefreshCallbackMode mode);

public:  // IPluginDiagnose interface
  virtual std::vector<unsigned int> activeProblems() const;
  virtual QString shortDescription(unsigned int key) const;
  virtual QString fullDescription(unsigned int key) const;
  virtual bool hasGuidedFix(unsigned int key) const;
  virtual void startGuidedFix(unsigned int key) const;

public slots:

  void syncOverwrite();

  void savePluginList();

  void refreshLists();

  ModInfo::Ptr installDownload(int downloadIndex, int priority = -1);
  ModInfo::Ptr installArchive(const QString& archivePath, int priority = -1,
                              bool reinstallation     = false,
                              ModInfo::Ptr currentMod = nullptr,
                              const QString& modName  = QString());

  void modPrioritiesChanged(QModelIndexList const& indexes);
  void modStatusChanged(unsigned int index);
  void modStatusChanged(QList<unsigned int> index);
  void requestDownload(const QUrl& url, QNetworkReply* reply);
  void downloadRequestedNXM(const QString& url);

  void userInterfaceInitialized();

  void profileCreated(MOBase::IProfile* profile);
  void profileRenamed(MOBase::IProfile* profile, QString const& oldName,
                      QString const& newName);
  void profileRemoved(QString const& profileName);

  bool nexusApi(bool retry = false);

signals:

  // emitted after a mod has been installed
  //
  void modInstalled(const QString& modName);

  // emitted when the managed game changes
  //
  void managedGameChanged(MOBase::IPluginGame const* gamePlugin);

  // emitted when the profile is changed, before notifying plugins
  //
  // the new profile can be stored but the old one is temporary and
  // should not be
  //
  void profileChanged(Profile* oldProfile, Profile* newProfile);

  // Notify that the directory structure is ready to be used on the main thread
  // Use queued connections
  void directoryStructureReady();

  // Notify of a general UI refresh
  void refreshTriggered();

private:
  std::pair<unsigned int, ModInfo::Ptr> doInstall(const QString& archivePath,
                                                  MOBase::GuessedValue<QString> modName,
                                                  ModInfo::Ptr currentMod, int priority,
                                                  bool reinstallation);

  void saveCurrentProfile();
  void storeSettings();

  void updateModActiveState(int index, bool active);
  void updateModsActiveState(const QList<unsigned int>& modIndices, bool active);

  // clear the conflict caches of all the given mods, and the mods in conflict
  // with the given mods
  //
  void clearCaches(std::vector<unsigned int> const& indices) const;

  bool createDirectory(const QString& path);

  QString oldMO1HookDll() const;

  /**
   * @brief return a descriptor of the mappings real file->virtual file
   */
  std::vector<Mapping> fileMapping(const QString& profile,
                                   const QString& customOverwrite);

private slots:

  void onDirectoryRefreshed();
  void downloadRequested(QNetworkReply* reply, QString gameName, int modID,
                         const QString& fileName);
  void removeOrigin(const QString& name);
  void downloadSpeed(const QString& serverName, int bytesPerSecond);
  void loginSuccessful(bool necessary);
  void loginFailed(const QString& message);

private:
  static const unsigned int PROBLEM_MO1SCRIPTEXTENDERWORKAROUND = 1;

private:
  IUserInterface* m_UserInterface;
  PluginManager* m_PluginManager;
  std::unique_ptr<IniBakery> m_IniBakery;
  QString m_GameName;
  MOBase::IPluginGame* m_GamePlugin;
  ModDataContentHolder m_Contents;

  std::unique_ptr<Profile> m_CurrentProfile;

  Settings& m_Settings;

  SelfUpdater m_Updater;

  SignalAboutToRunApplication m_AboutToRun;
  SignalFinishedRunApplication m_FinishedRun;
  SignalUserInterfaceInitialized m_UserInterfaceInitialized;
  SignalProfileCreated m_ProfileCreated;
  SignalProfileRenamed m_ProfileRenamed;
  SignalProfileRemoved m_ProfileRemoved;
  SignalProfileChanged m_ProfileChanged;
  SignalPluginSettingChanged m_PluginSettingChanged;
  SignalPluginEnabled m_PluginEnabled;
  SignalPluginEnabled m_PluginDisabled;

  boost::signals2::signal<void()> m_OnNextRefreshCallbacks;

  ModList m_ModList;
  PluginList m_PluginList;

  QList<std::function<void()>> m_PostLoginTasks;

  ExecutablesList m_ExecutablesList;
  QStringList m_PendingDownloads;
  QStringList m_DefaultArchives;
  QStringList m_ActiveArchives;

  std::unique_ptr<DirectoryRefresher> m_DirectoryRefresher;
  MOShared::DirectoryEntry* m_DirectoryStructure;
  MOBase::MemoizedLocked<std::shared_ptr<const MOBase::IFileTree>> m_VirtualFileTree;

  DownloadManager m_DownloadManager;
  InstallationManager m_InstallationManager;

  QThread m_RefresherThread;

  std::thread m_StructureDeleter;

  std::atomic<bool> m_DirectoryUpdate;
  bool m_ArchivesInit;

  MOBase::DelayedFileWriter m_PluginListsWriter;
  UsvfsConnector m_USVFS;

  UILocker m_UILocker;
};

#endif  // ORGANIZERCORE_H
