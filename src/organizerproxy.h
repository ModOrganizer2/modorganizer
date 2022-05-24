#ifndef ORGANIZERPROXY_H
#define ORGANIZERPROXY_H

#include <memory>

#include <imoinfo.h>
#include <iplugin.h>

#include "organizercore.h"

class GameFeaturesProxy;
class PluginManager;
class DownloadManagerProxy;
class ModListProxy;
class PluginListProxy;

class OrganizerProxy : public MOBase::IOrganizer
{

public:
  OrganizerProxy(OrganizerCore* organizer, PluginManager* pluginManager,
                 MOBase::IPlugin* plugin);
  ~OrganizerProxy();

public:
  /**
   * @return the plugin corresponding to this proxy.
   */
  MOBase::IPlugin* plugin() const { return m_Plugin; }

public:  // IOrganizer interface
  MOBase::IModRepositoryBridge* createNexusBridge() const override;
  QString profileName() const override;
  QString profilePath() const override;
  QString downloadsPath() const override;
  QString overwritePath() const override;
  QString basePath() const override;
  QString modsPath() const override;
  MOBase::Version version() const override;
  MOBase::VersionInfo appVersion() const override;
  MOBase::IPluginGame* getGame(const QString& gameName) const override;
  MOBase::IModInterface* createMod(MOBase::GuessedValue<QString>& name) override;
  void modDataChanged(MOBase::IModInterface* mod) override;
  QVariant persistent(const QString& pluginName, const QString& key,
                      const QVariant& def = QVariant()) const override;
  void setPersistent(const QString& pluginName, const QString& key,
                     const QVariant& value, bool sync = true) override;
  QString pluginDataPath() const override;
  MOBase::IModInterface* installMod(const QString& fileName,
                                    const QString& nameSuggestion = QString());
  QString resolvePath(const QString& fileName) const override;
  QStringList listDirectories(const QString& directoryName) const override;
  QStringList
  findFiles(const QString& path,
            const std::function<bool(const QString&)>& filter) const override;
  QStringList findFiles(const QString& path,
                        const QStringList& globFilters) const override;
  QStringList getFileOrigins(const QString& fileName) const override;
  QList<FileInfo>
  findFileInfos(const QString& path,
                const std::function<bool(const FileInfo&)>& filter) const override;
  std::shared_ptr<const MOBase::IFileTree> virtualFileTree() const override;

  MOBase::IDownloadManager* downloadManager() const override;
  MOBase::IPluginList* pluginList() const override;
  MOBase::IModList* modList() const override;
  MOBase::IProfile* profile() const override;
  MOBase::IGameFeatures* gameFeatures() const override;

  HANDLE startApplication(const QString& executable,
                          const QStringList& args = QStringList(),
                          const QString& cwd = "", const QString& profile = "",
                          const QString& forcedCustomOverwrite = "",
                          bool ignoreCustomOverwrite           = false) override;
  bool waitForApplication(HANDLE handle, bool refresh = true,
                          LPDWORD exitCode = nullptr) const override;
  void refresh(bool saveChanges) override;

  bool onAboutToRun(const std::function<bool(const QString&)>& func) override;
  bool onAboutToRun(const std::function<bool(const QString&, const QDir&,
                                             const QString&)>& func) override;
  bool
  onFinishedRun(const std::function<void(const QString&, unsigned int)>& func) override;
  bool
  onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func) override;
  bool onNextRefresh(const std::function<void()>& func,
                     bool immediateIfPossible) override;
  bool onProfileCreated(std::function<void(MOBase::IProfile*)> const& func) override;
  bool onProfileRenamed(std::function<void(MOBase::IProfile*, QString const&,
                                           QString const&)> const& func) override;
  bool onProfileRemoved(std::function<void(QString const&)> const& func) override;
  bool onProfileChanged(
      std::function<void(MOBase::IProfile*, MOBase::IProfile*)> const& func) override;

  // Plugin related:
  virtual bool isPluginEnabled(QString const& pluginName) const override;
  virtual bool isPluginEnabled(MOBase::IPlugin* plugin) const override;
  virtual QVariant pluginSetting(const QString& pluginName,
                                 const QString& key) const override;
  virtual void setPluginSetting(const QString& pluginName, const QString& key,
                                const QVariant& value) override;
  virtual bool onPluginSettingChanged(
      std::function<void(QString const&, const QString& key, const QVariant&,
                         const QVariant&)> const& func) override;
  virtual bool
  onPluginEnabled(std::function<void(const MOBase::IPlugin*)> const& func) override;
  virtual bool onPluginEnabled(const QString& pluginName,
                               std::function<void()> const& func) override;
  virtual bool
  onPluginDisabled(std::function<void(const MOBase::IPlugin*)> const& func) override;
  virtual bool onPluginDisabled(const QString& pluginName,
                                std::function<void()> const& func) override;

  virtual MOBase::IPluginGame const* managedGame() const;

protected:
  // The container needs access to some callbacks to simulate startup.
  friend class PluginManager;

  /**
   * @brief Connect the signals from this proxy and all the child proxies (plugin list,
   * mod list, etc.) to the actual implementation. Before this call, plugins can
   * register signals but they won't be triggered.
   */
  void connectSignals();

  /**
   * @brief Disconnect the signals from this proxy and all the child proxies (plugin
   * list, mod list, etc.) from the actual implementation.
   */
  void disconnectSignals();

private:
  OrganizerCore* m_Proxied;
  PluginManager* m_PluginManager;

  MOBase::IPlugin* m_Plugin;

  OrganizerCore::SignalAboutToRunApplication m_AboutToRun;
  OrganizerCore::SignalFinishedRunApplication m_FinishedRun;
  OrganizerCore::SignalUserInterfaceInitialized m_UserInterfaceInitialized;
  OrganizerCore::SignalProfileCreated m_ProfileCreated;
  OrganizerCore::SignalProfileRenamed m_ProfileRenamed;
  OrganizerCore::SignalProfileRemoved m_ProfileRemoved;
  OrganizerCore::SignalProfileChanged m_ProfileChanged;
  OrganizerCore::SignalPluginSettingChanged m_PluginSettingChanged;
  OrganizerCore::SignalPluginEnabled m_PluginEnabled;
  OrganizerCore::SignalPluginEnabled m_PluginDisabled;

  std::vector<boost::signals2::connection> m_Connections;

  std::unique_ptr<DownloadManagerProxy> m_DownloadManagerProxy;
  std::unique_ptr<ModListProxy> m_ModListProxy;
  std::unique_ptr<PluginListProxy> m_PluginListProxy;
  std::unique_ptr<GameFeaturesProxy> m_GameFeaturesProxy;
};

#endif  // ORGANIZERPROXY_H
