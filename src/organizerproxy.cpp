#include "organizerproxy.h"

#include "downloadmanagerproxy.h"
#include "gamefeaturesproxy.h"
#include "glob_matching.h"
#include "modlistproxy.h"
#include "organizercore.h"
#include "pluginlistproxy.h"
#include "pluginmanager.h"
#include "proxyutils.h"
#include "settings.h"
#include "shared/appconfig.h"
#include "shared/util.h"

#include <QApplication>
#include <QObject>

using namespace MOBase;
using namespace MOShared;

OrganizerProxy::OrganizerProxy(OrganizerCore* organizer, PluginManager* pluginManager,
                               MOBase::IPlugin* plugin)
    : m_Proxied(organizer), m_PluginManager(pluginManager), m_Plugin(plugin),
      m_DownloadManagerProxy(
          std::make_unique<DownloadManagerProxy>(this, organizer->downloadManager())),
      m_ModListProxy(std::make_unique<ModListProxy>(this, organizer->modList())),
      m_PluginListProxy(
          std::make_unique<PluginListProxy>(this, organizer->pluginList())),
      m_GameFeaturesProxy(
          std::make_unique<GameFeaturesProxy>(this, pluginManager->gameFeatures()))
{}

OrganizerProxy::~OrganizerProxy()
{
  disconnectSignals();
}

void OrganizerProxy::connectSignals()
{
  m_Connections.push_back(
      m_Proxied->onAboutToRun(callSignalIfPluginActive(this, m_AboutToRun, true)));
  m_Connections.push_back(
      m_Proxied->onFinishedRun(callSignalIfPluginActive(this, m_FinishedRun)));
  m_Connections.push_back(
      m_Proxied->onProfileCreated(callSignalIfPluginActive(this, m_ProfileCreated)));
  m_Connections.push_back(
      m_Proxied->onProfileRenamed(callSignalIfPluginActive(this, m_ProfileRenamed)));
  m_Connections.push_back(
      m_Proxied->onProfileRemoved(callSignalIfPluginActive(this, m_ProfileRemoved)));
  m_Connections.push_back(
      m_Proxied->onProfileChanged(callSignalIfPluginActive(this, m_ProfileChanged)));

  m_Connections.push_back(m_Proxied->onUserInterfaceInitialized(
      callSignalAlways(m_UserInterfaceInitialized)));
  m_Connections.push_back(
      m_Proxied->onPluginSettingChanged(callSignalAlways(m_PluginSettingChanged)));
  m_Connections.push_back(
      m_Proxied->onPluginEnabled(callSignalAlways(m_PluginEnabled)));
  m_Connections.push_back(
      m_Proxied->onPluginDisabled(callSignalAlways(m_PluginDisabled)));

  // Connect the child proxies.
  m_DownloadManagerProxy->connectSignals();
  m_ModListProxy->connectSignals();
  m_PluginListProxy->connectSignals();
}

void OrganizerProxy::disconnectSignals()
{
  // Disconnect the child proxies.
  m_DownloadManagerProxy->disconnectSignals();
  m_ModListProxy->disconnectSignals();
  m_PluginListProxy->disconnectSignals();

  for (auto& conn : m_Connections) {
    conn.disconnect();
  }
  m_Connections.clear();
}

IModRepositoryBridge* OrganizerProxy::createNexusBridge() const
{
  return new NexusBridge(m_Plugin->name());
}

QString OrganizerProxy::profileName() const
{
  return m_Proxied->profileName();
}

QString OrganizerProxy::profilePath() const
{
  return m_Proxied->profilePath();
}

QString OrganizerProxy::downloadsPath() const
{
  return m_Proxied->downloadsPath();
}

QString OrganizerProxy::overwritePath() const
{
  return m_Proxied->overwritePath();
}

QString OrganizerProxy::basePath() const
{
  return m_Proxied->basePath();
}

QString OrganizerProxy::modsPath() const
{
  return m_Proxied->modsPath();
}

Version OrganizerProxy::version() const
{
  return m_Proxied->version();
}

VersionInfo OrganizerProxy::appVersion() const
{
  const auto version = m_Proxied->version();
  const int major = version.major(), minor = version.minor(),
            subminor                       = version.patch();
  int subsubminor                          = 0;
  VersionInfo::ReleaseType infoReleaseType = VersionInfo::RELEASE_FINAL;

  // make a copy
  auto prereleases = version.preReleases();

  if (!prereleases.empty()) {
    // check if the first pre-release entry is a number
    if (prereleases.front().index() == 0) {
      subsubminor = std::get<int>(prereleases.front());
      prereleases.erase(prereleases.begin());
    }

    if (!prereleases.empty()) {
      const auto releaseType = std::get<Version::ReleaseType>(prereleases.front());
      switch (releaseType) {
      case Version::Development:
        infoReleaseType = VersionInfo::RELEASE_PREALPHA;
        break;
      case Version::Alpha:
        infoReleaseType = VersionInfo::RELEASE_ALPHA;
        break;
      case Version::Beta:
        infoReleaseType = VersionInfo::RELEASE_BETA;
        break;
      case Version::ReleaseCandidate:
        infoReleaseType = VersionInfo::RELEASE_CANDIDATE;
        break;
      default:
        infoReleaseType = VersionInfo::RELEASE_PREALPHA;
      }
    }

    // there is no way to differentiate two pre-releases?
  }

  return VersionInfo(major, minor, subminor, subsubminor, infoReleaseType);
}

IPluginGame* OrganizerProxy::getGame(const QString& gameName) const
{
  return m_Proxied->getGame(gameName);
}

IModInterface* OrganizerProxy::createMod(MOBase::GuessedValue<QString>& name)
{
  return m_Proxied->createMod(name);
}

void OrganizerProxy::modDataChanged(IModInterface* mod)
{
  m_Proxied->modDataChanged(mod);
}

bool OrganizerProxy::isPluginEnabled(QString const& pluginName) const
{
  return m_PluginManager->isEnabled(pluginName);
}

bool OrganizerProxy::isPluginEnabled(IPlugin* plugin) const
{
  return m_PluginManager->isEnabled(plugin);
}

QVariant OrganizerProxy::pluginSetting(const QString& pluginName,
                                       const QString& key) const
{
  return m_Proxied->pluginSetting(pluginName, key);
}

void OrganizerProxy::setPluginSetting(const QString& pluginName, const QString& key,
                                      const QVariant& value)
{
  m_Proxied->setPluginSetting(pluginName, key, value);
}

QVariant OrganizerProxy::persistent(const QString& pluginName, const QString& key,
                                    const QVariant& def) const
{
  return m_Proxied->persistent(pluginName, key, def);
}

void OrganizerProxy::setPersistent(const QString& pluginName, const QString& key,
                                   const QVariant& value, bool sync)
{
  m_Proxied->setPersistent(pluginName, key, value, sync);
}

QString OrganizerProxy::pluginDataPath() const
{
  return OrganizerCore::pluginDataPath();
}

HANDLE OrganizerProxy::startApplication(const QString& exe, const QStringList& args,
                                        const QString& cwd, const QString& profile,
                                        const QString& overwrite, bool ignoreOverwrite)
{
  log::debug("a plugin has requested to start an application:\n"
             " . executable: '{}'\n"
             " . args: '{}'\n"
             " . cwd: '{}'\n"
             " . profile: '{}'\n"
             " . overwrite: '{}'\n"
             " . ignore overwrite: {}",
             exe, args.join(" "), cwd, profile, overwrite, ignoreOverwrite);

  auto runner = m_Proxied->processRunner();

  // don't wait for completion
  runner.setFromFileOrExecutable(exe, args, cwd, profile, overwrite, ignoreOverwrite)
      .run();

  // the plugin is in charge of closing the handle, unless waitForApplication()
  // is called on it
  return runner.stealProcessHandle().release();
}

bool OrganizerProxy::waitForApplication(HANDLE handle, bool refresh,
                                        LPDWORD exitCode) const
{
  const auto pid = ::GetProcessId(handle);

  log::debug("a plugin wants to wait for an application to complete, pid {}{}", pid,
             (pid == 0 ? "unknown (probably already completed)" : ""));

  auto runner = m_Proxied->processRunner();

  ProcessRunner::WaitFlags waitFlags = ProcessRunner::ForceWait;

  if (refresh) {
    waitFlags |= ProcessRunner::TriggerRefresh | ProcessRunner::WaitForRefresh;
  }

  const auto r = runner.setWaitForCompletion(waitFlags, UILocker::OutputRequired)
                     .attachToProcess(handle);

  if (exitCode) {
    *exitCode = runner.exitCode();
  }

  switch (r) {
  case ProcessRunner::Completed:
    return true;

  case ProcessRunner::Cancelled:  // fall-through
  case ProcessRunner::ForceUnlocked:
    // this is always an error because the application should have run to
    // completion
    return false;

  case ProcessRunner::Error:  // fall-through
  default:
    return false;
  }
}

void OrganizerProxy::refresh(bool saveChanges)
{
  m_Proxied->refresh(saveChanges);
}

IModInterface* OrganizerProxy::installMod(const QString& fileName,
                                          const QString& nameSuggestion)
{
  return m_Proxied->installMod(fileName, -1, false, nullptr, nameSuggestion);
}

QString OrganizerProxy::resolvePath(const QString& fileName) const
{
  return m_Proxied->resolvePath(fileName);
}

QStringList OrganizerProxy::listDirectories(const QString& directoryName) const
{
  return m_Proxied->listDirectories(directoryName);
}

QStringList
OrganizerProxy::findFiles(const QString& path,
                          const std::function<bool(const QString&)>& filter) const
{
  return m_Proxied->findFiles(path, filter);
}

QStringList OrganizerProxy::findFiles(const QString& path,
                                      const QStringList& globFilters) const
{
  QList<GlobPattern<QChar>> patterns;
  for (auto& gfilter : globFilters) {
    patterns.append(GlobPattern(gfilter));
  }
  return findFiles(path, [&patterns](const QString& filename) {
    for (auto& p : patterns) {
      if (p.match(filename)) {
        return true;
      }
    }
    return false;
  });
}

QStringList OrganizerProxy::getFileOrigins(const QString& fileName) const
{
  return m_Proxied->getFileOrigins(fileName);
}

QList<MOBase::IOrganizer::FileInfo> OrganizerProxy::findFileInfos(
    const QString& path,
    const std::function<bool(const MOBase::IOrganizer::FileInfo&)>& filter) const
{
  return m_Proxied->findFileInfos(path, filter);
}

std::shared_ptr<const MOBase::IFileTree> OrganizerProxy::virtualFileTree() const
{
  return m_Proxied->m_VirtualFileTree.value();
}

MOBase::IDownloadManager* OrganizerProxy::downloadManager() const
{
  return m_DownloadManagerProxy.get();
}

MOBase::IPluginList* OrganizerProxy::pluginList() const
{
  return m_PluginListProxy.get();
}

MOBase::IModList* OrganizerProxy::modList() const
{
  return m_ModListProxy.get();
}

MOBase::IGameFeatures* OrganizerProxy::gameFeatures() const
{
  return m_GameFeaturesProxy.get();
}

MOBase::IProfile* OrganizerProxy::profile() const
{
  return m_Proxied->currentProfile();
}

MOBase::IPluginGame const* OrganizerProxy::managedGame() const
{
  return m_Proxied->managedGame();
}

// CALLBACKS

bool OrganizerProxy::onAboutToRun(const std::function<bool(const QString&)>& func)
{
  return m_Proxied
      ->onAboutToRun(MOShared::callIfPluginActive(
          this,
          [func](const QString& binary, const QDir&, const QString&) {
            return func(binary);
          },
          true))
      .connected();
}

bool OrganizerProxy::onAboutToRun(
    const std::function<bool(const QString&, const QDir&, const QString&)>& func)
{
  return m_Proxied->onAboutToRun(MOShared::callIfPluginActive(this, func, true))
      .connected();
}

bool OrganizerProxy::onFinishedRun(
    const std::function<void(const QString&, unsigned int)>& func)
{
  return m_Proxied->onFinishedRun(MOShared::callIfPluginActive(this, func)).connected();
}

bool OrganizerProxy::onUserInterfaceInitialized(
    std::function<void(QMainWindow*)> const& func)
{
  // Always call this one to allow plugin to initialize themselves even when not active:
  return m_UserInterfaceInitialized.connect(func).connected();
}

bool OrganizerProxy::onNextRefresh(const std::function<void()>& func,
                                   bool immediateIfPossible)
{
  using enum OrganizerCore::RefreshCallbackMode;
  return m_Proxied
      ->onNextRefresh(MOShared::callIfPluginActive(this, func),
                      OrganizerCore::RefreshCallbackGroup::EXTERNAL,
                      immediateIfPossible ? RUN_NOW_IF_POSSIBLE
                                          : FORCE_WAIT_FOR_REFRESH)
      .connected();
}

bool OrganizerProxy::onProfileCreated(std::function<void(IProfile*)> const& func)
{
  return m_ProfileCreated.connect(func).connected();
}

bool OrganizerProxy::onProfileRenamed(
    std::function<void(IProfile*, QString const&, QString const&)> const& func)
{
  return m_ProfileRenamed.connect(func).connected();
}

bool OrganizerProxy::onProfileRemoved(std::function<void(QString const&)> const& func)
{
  return m_ProfileRemoved.connect(func).connected();
}

bool OrganizerProxy::onProfileChanged(
    std::function<void(MOBase::IProfile*, MOBase::IProfile*)> const& func)
{
  return m_ProfileChanged.connect(func).connected();
}
// Always call these one, otherwise plugin cannot detect they are being enabled /
// disabled:
bool OrganizerProxy::onPluginSettingChanged(
    std::function<void(QString const&, const QString& key, const QVariant&,
                       const QVariant&)> const& func)
{
  return m_PluginSettingChanged.connect(func).connected();
}

bool OrganizerProxy::onPluginEnabled(std::function<void(const IPlugin*)> const& func)
{
  return m_PluginEnabled.connect(func).connected();
}

bool OrganizerProxy::onPluginEnabled(const QString& pluginName,
                                     std::function<void()> const& func)
{
  return onPluginEnabled([=](const IPlugin* plugin) {
    if (plugin->name().compare(pluginName, Qt::CaseInsensitive) == 0) {
      func();
    }
  });
}

bool OrganizerProxy::onPluginDisabled(std::function<void(const IPlugin*)> const& func)
{
  return m_PluginDisabled.connect(func).connected();
}

bool OrganizerProxy::onPluginDisabled(const QString& pluginName,
                                      std::function<void()> const& func)
{
  return onPluginDisabled([=](const IPlugin* plugin) {
    if (plugin->name().compare(pluginName, Qt::CaseInsensitive) == 0) {
      func();
    }
  });
}
