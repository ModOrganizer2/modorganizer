#ifndef ORGANIZERPROXY_H
#define ORGANIZERPROXY_H

#include <memory>

#include <iplugin.h>
#include <imoinfo.h>

class OrganizerCore;
class PluginContainer;
class DownloadManagerProxy;
class ModListProxy;
class PluginListProxy;

class OrganizerProxy : public MOBase::IOrganizer
{

public:

  OrganizerProxy(OrganizerCore *organizer, PluginContainer *pluginContainer, MOBase::IPlugin *plugin);

  /**
   * @return the plugin corresponding to this proxy.
   */
  MOBase::IPlugin* plugin() const { return m_Plugin;  }

  virtual MOBase::IModRepositoryBridge *createNexusBridge() const;
  virtual QString profileName() const;
  virtual QString profilePath() const;
  virtual QString downloadsPath() const;
  virtual QString overwritePath() const;
  virtual QString basePath() const;
  virtual QString modsPath() const;
  virtual MOBase::VersionInfo appVersion() const;
  virtual MOBase::IPluginGame *getGame(const QString &gameName) const;
  virtual MOBase::IModInterface *createMod(MOBase::GuessedValue<QString> &name);
  virtual void modDataChanged(MOBase::IModInterface *mod);
  virtual QVariant pluginSetting(const QString &pluginName, const QString &key) const;
  virtual void setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value);
  virtual QVariant persistent(const QString &pluginName, const QString &key, const QVariant &def = QVariant()) const;
  virtual void setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync = true);
  virtual QString pluginDataPath() const;
  virtual MOBase::IModInterface *installMod(const QString &fileName, const QString &nameSuggestion = QString());
  virtual QString resolvePath(const QString &fileName) const;
  virtual QStringList listDirectories(const QString &directoryName) const;
  virtual QStringList findFiles(const QString &path, const std::function<bool(const QString &)> &filter) const override;
  virtual QStringList findFiles(const QString &path, const QStringList &globFilters) const override;
  virtual QStringList getFileOrigins(const QString &fileName) const;
  virtual QList<FileInfo> findFileInfos(const QString &path, const std::function<bool(const FileInfo&)> &filter) const;

  virtual MOBase::IDownloadManager *downloadManager() const;
  virtual MOBase::IPluginList *pluginList() const;
  virtual MOBase::IModList *modList() const;
  virtual MOBase::IProfile *profile() const override;
  virtual HANDLE startApplication(const QString &executable, const QStringList &args = QStringList(), const QString &cwd = "",
                                  const QString &profile = "", const QString &forcedCustomOverwrite = "", bool ignoreCustomOverwrite = false);
  virtual bool waitForApplication(HANDLE handle, LPDWORD exitCode = nullptr) const;
  virtual void refresh(bool saveChanges);

  virtual bool onAboutToRun(const std::function<bool(const QString&)> &func) override;
  virtual bool onFinishedRun(const std::function<void (const QString&, unsigned int)> &func) override;
  virtual bool onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func) override;
  virtual bool onProfileCreated(std::function<void(MOBase::IProfile*)> const& func) override;
  virtual bool onProfileRenamed(std::function<void(MOBase::IProfile*, QString const&, QString const&)> const& func) override;
  virtual bool onProfileRemoved(std::function<void(QString const&)> const& func) override;
  virtual bool onProfileChanged(std::function<void(MOBase::IProfile*, MOBase::IProfile*)> const& func) override;
  virtual bool onPluginSettingChanged(std::function<void(QString const&, const QString& key, const QVariant&, const QVariant&)> const& func) override;

  virtual MOBase::IPluginGame const *managedGame() const;

private:

  OrganizerCore *m_Proxied;
  PluginContainer *m_PluginContainer;

  MOBase::IPlugin *m_Plugin;

  std::unique_ptr<DownloadManagerProxy> m_DownloadManagerProxy;
  std::unique_ptr<ModListProxy> m_ModListProxy;
  std::unique_ptr<PluginListProxy> m_PluginListProxy;

};


class DummyOrganizerProxy : public MOBase::IOrganizer
{
public:
  DummyOrganizerProxy(MOBase::IPlugin* plugin);
  ~DummyOrganizerProxy();

  MOBase::IModRepositoryBridge *createNexusBridge() const override;
  QString profileName() const override;
  QString profilePath() const override;
  QString downloadsPath() const override;
  QString overwritePath() const override;
  QString basePath() const override;
  QString modsPath() const override;
  MOBase::VersionInfo appVersion() const override;
  MOBase::IPluginGame *getGame(const QString &gameName) const override;
  MOBase::IModInterface *createMod(MOBase::GuessedValue<QString> &name) override;
  void modDataChanged(MOBase::IModInterface *mod) override;
  QVariant pluginSetting(const QString &pluginName, const QString &key) const override;
  void setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value) override;
  QVariant persistent(const QString &pluginName, const QString &key, const QVariant &def = QVariant()) const override;
  void setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync = true) override;
  QString pluginDataPath() const override;
  MOBase::IModInterface *installMod(const QString &fileName, const QString &nameSuggestion = QString()) override;
  QString resolvePath(const QString &fileName) const override;
  QStringList listDirectories(const QString &directoryName) const override;
  QStringList findFiles(const QString &path, const std::function<bool(const QString &)> &filter) const override;
  QStringList findFiles(const QString &path, const QStringList &globFilters) const override;
  QStringList getFileOrigins(const QString &fileName) const override;
  QList<FileInfo> findFileInfos(const QString &path, const std::function<bool(const FileInfo&)> &filter) const override;

  MOBase::IDownloadManager *downloadManager() const override;
  MOBase::IPluginList *pluginList() const override;
  MOBase::IModList *modList() const override;
  MOBase::IProfile *profile() const override;
  HANDLE startApplication(const QString &executable, const QStringList &args = QStringList(), const QString &cwd = "",
    const QString &profile = "", const QString &forcedCustomOverwrite = "", bool ignoreCustomOverwrite = false) override;
  bool waitForApplication(HANDLE handle, LPDWORD exitCode = nullptr) const override;
  void refresh(bool saveChanges = true) override;

  bool onAboutToRun(const std::function<bool(const QString&)> &func) override;
  bool onFinishedRun(const std::function<void (const QString&, unsigned int)> &func) override;
  bool onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func) override;
  bool onProfileCreated(std::function<void(MOBase::IProfile*)> const& func) override;
  bool onProfileRenamed(std::function<void(MOBase::IProfile*, QString const&, QString const&)> const& func) override;
  bool onProfileRemoved(std::function<void(QString const&)> const& func) override;
  bool onProfileChanged(std::function<void(MOBase::IProfile*, MOBase::IProfile*)> const& func) override;
  bool onPluginSettingChanged(std::function<void(QString const&, const QString& key, const QVariant&, const QVariant&)> const& func) override;

  MOBase::IPluginGame const *managedGame() const override;

private:
  std::unique_ptr<MOBase::IPluginList> m_plugins;
  std::unique_ptr<MOBase::IModList> m_mods;
};

#endif // ORGANIZERPROXY_H
