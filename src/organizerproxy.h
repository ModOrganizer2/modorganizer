#ifndef ORGANIZERPROXY_H
#define ORGANIZERPROXY_H

#include <memory>

#include <imoinfo.h>
#include <iplugin.h>

#include "organizercore.h"

class PluginManager;
class DownloadManagerProxy;
class ModListProxy;
class ExtensionManager;
class PluginListProxy;
class ExtensionListProxy;

class OrganizerProxy : public MOBase::IOrganizer
{

public:
  OrganizerProxy(OrganizerCore* organizer, const ExtensionManager& extensionManager,
                 PluginManager* pluginManager, MOBase::IPlugin* plugin);
  ~OrganizerProxy();

public:
  /**
   * @return the plugin corresponding to this proxy.
   */
  MOBase::IPlugin* plugin() const { return m_Plugin; }

public:  // IOrganizer interface
  virtual MOBase::IModRepositoryBridge* createNexusBridge() const;
  virtual QString profileName() const;
  virtual QString profilePath() const;
  virtual QString downloadsPath() const;
  virtual QString overwritePath() const;
  virtual QString basePath() const;
  virtual QString modsPath() const;
  virtual MOBase::VersionInfo appVersion() const;
  virtual MOBase::IPluginGame* getGame(const QString& gameName) const;
  virtual MOBase::IModInterface* createMod(MOBase::GuessedValue<QString>& name);
  virtual void modDataChanged(MOBase::IModInterface* mod);
  virtual QVariant persistent(const QString& pluginName, const QString& key,
                              const QVariant& def = QVariant()) const;
  virtual void setPersistent(const QString& pluginName, const QString& key,
                             const QVariant& value, bool sync = true);
  virtual QString pluginDataPath() const;
  virtual MOBase::IModInterface* installMod(const QString& fileName,
                                            const QString& nameSuggestion = QString());
  virtual QString resolvePath(const QString& fileName) const;
  virtual QStringList listDirectories(const QString& directoryName) const;
  virtual QStringList
  findFiles(const QString& path,
            const std::function<bool(const QString&)>& filter) const override;
  virtual QStringList findFiles(const QString& path,
                                const QStringList& globFilters) const override;
  virtual QStringList getFileOrigins(const QString& fileName) const override;
  virtual QList<FileInfo>
  findFileInfos(const QString& path,
                const std::function<bool(const FileInfo&)>& filter) const override;
  virtual std::shared_ptr<const MOBase::IFileTree> virtualFileTree() const override;

  virtual MOBase::IDownloadManager* downloadManager() const;
  virtual MOBase::IPluginList* pluginList() const;
  virtual MOBase::IModList* modList() const;
  virtual MOBase::IProfile* profile() const override;
  virtual HANDLE startApplication(const QString& executable,
                                  const QStringList& args = QStringList(),
                                  const QString& cwd = "", const QString& profile = "",
                                  const QString& forcedCustomOverwrite = "",
                                  bool ignoreCustomOverwrite           = false);
  virtual bool waitForApplication(HANDLE handle, bool refresh = true,
                                  LPDWORD exitCode = nullptr) const;
  virtual void refresh(bool saveChanges);

  virtual bool onAboutToRun(const std::function<bool(const QString&)>& func) override;
  virtual bool onAboutToRun(const std::function<bool(const QString&, const QDir&,
                                                     const QString&)>& func) override;
  virtual bool
  onFinishedRun(const std::function<void(const QString&, unsigned int)>& func) override;
  virtual bool
  onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func) override;
  virtual bool onNextRefresh(const std::function<void()>& func,
                             bool immediateIfPossible) override;
  virtual bool
  onProfileCreated(std::function<void(MOBase::IProfile*)> const& func) override;
  virtual bool onProfileRenamed(
      std::function<void(MOBase::IProfile*, QString const&, QString const&)> const&
          func) override;
  virtual bool
  onProfileRemoved(std::function<void(QString const&)> const& func) override;
  virtual bool onProfileChanged(
      std::function<void(MOBase::IProfile*, MOBase::IProfile*)> const& func) override;

  // Plugin/extension related:
  virtual MOBase::IExtensionList& extensionList() const override;

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
  std::unique_ptr<ExtensionListProxy> m_ExtensionListProxy;
  std::unique_ptr<ModListProxy> m_ModListProxy;
  std::unique_ptr<PluginListProxy> m_PluginListProxy;
};

#endif  // ORGANIZERPROXY_H
