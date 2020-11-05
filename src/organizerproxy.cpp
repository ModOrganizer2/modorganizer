#include "organizerproxy.h"

#include "shared/appconfig.h"
#include "organizercore.h"
#include "plugincontainer.h"
#include "settings.h"
#include "glob_matching.h"
#include "downloadmanagerproxy.h"
#include "modlistproxy.h"
#include "pluginlistproxy.h"
#include "proxyutils.h"
#include "shared/util.h"

#include <QObject>
#include <QApplication>

using namespace MOBase;
using namespace MOShared;


OrganizerProxy::OrganizerProxy(OrganizerCore* organizer, PluginContainer* pluginContainer, MOBase::IPlugin* plugin)
  : m_Proxied(organizer)
  , m_PluginContainer(pluginContainer)
  , m_Plugin(plugin)
{
  if (m_Proxied) {
    m_DownloadManagerProxy = std::make_unique<DownloadManagerProxy>(
      this, m_Proxied->downloadManager());

    m_ModListProxy = std::make_unique<ModListProxy>(
      this, m_Proxied->modList());

    m_PluginListProxy = std::make_unique<PluginListProxy>(
      this, m_Proxied->pluginList());
  }
}

IModRepositoryBridge *OrganizerProxy::createNexusBridge() const
{
  return new NexusBridge(m_PluginContainer, m_Plugin->name());
}

QString OrganizerProxy::profileName() const
{
  if (m_Proxied) {
    return m_Proxied->profileName();
  } else {
    return {};
  }
}

QString OrganizerProxy::profilePath() const
{
  if (m_Proxied) {
    return m_Proxied->profilePath();
  } else {
    return {};
  }
}

QString OrganizerProxy::downloadsPath() const
{
  if (m_Proxied) {
    return m_Proxied->downloadsPath();
  } else {
    return {};
  }
}

QString OrganizerProxy::overwritePath() const
{
  if (m_Proxied) {
    return m_Proxied->overwritePath();
  } else {
    return {};
  }
}

QString OrganizerProxy::basePath() const
{
  if (m_Proxied) {
    return m_Proxied->basePath();
  } else {
    return {};
  }
}

QString OrganizerProxy::modsPath() const
{
  if (m_Proxied) {
    return m_Proxied->modsPath();
  } else {
    return {};
  }
}

VersionInfo OrganizerProxy::appVersion() const
{
  return createVersionInfo();
}

IPluginGame *OrganizerProxy::getGame(const QString &gameName) const
{
  if (m_Proxied) {
    return m_Proxied->getGame(gameName);
  } else {
    return nullptr;
  }
}

IModInterface *OrganizerProxy::createMod(MOBase::GuessedValue<QString> &name)
{
  if (m_Proxied) {
    return m_Proxied->createMod(name);
  } else {
    return nullptr;
  }
}

void OrganizerProxy::modDataChanged(IModInterface *mod)
{
  if (m_Proxied) {
    m_Proxied->modDataChanged(mod);
  }
}

QVariant OrganizerProxy::pluginSetting(const QString &pluginName, const QString &key) const
{
  if (m_Proxied) {
    return m_Proxied->pluginSetting(pluginName, key);
  } else {
    return {};
  }
}

void OrganizerProxy::setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value)
{
  if (m_Proxied) {
    m_Proxied->setPluginSetting(pluginName, key, value);
  }
}

QVariant OrganizerProxy::persistent(const QString &pluginName, const QString &key, const QVariant &def) const
{
  if (m_Proxied) {
    return m_Proxied->persistent(pluginName, key, def);
  } else {
    return {};
  }
}

void OrganizerProxy::setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync)
{
  if (m_Proxied) {
    m_Proxied->setPersistent(pluginName, key, value, sync);
  }
}

QString OrganizerProxy::pluginDataPath() const
{
  return OrganizerCore::pluginDataPath();
}

HANDLE OrganizerProxy::startApplication(
  const QString& exe, const QStringList& args, const QString &cwd,
  const QString& profile, const QString &overwrite, bool ignoreOverwrite)
{
  if (!m_Proxied) {
    return INVALID_HANDLE_VALUE;
  }

  log::debug(
    "a plugin has requested to start an application:\n"
    " . executable: '{}'\n"
    " . args: '{}'\n"
    " . cwd: '{}'\n"
    " . profile: '{}'\n"
    " . overwrite: '{}'\n"
    " . ignore overwrite: {}",
    exe, args.join(" "), cwd, profile, overwrite, ignoreOverwrite);

  auto runner = m_Proxied->processRunner();

  // don't wait for completion
  runner
    .setFromFileOrExecutable(exe, args, cwd, profile, overwrite, ignoreOverwrite)
    .run();

  // the plugin is in charge of closing the handle, unless waitForApplication()
  // is called on it
  return runner.stealProcessHandle().release();
}

bool OrganizerProxy::waitForApplication(HANDLE handle, LPDWORD exitCode) const
{
  if (!m_Proxied) {
    return false;
  }

  const auto pid = ::GetProcessId(handle);

  log::debug(
    "a plugin wants to wait for an application to complete, pid {}{}",
    pid, (pid == 0 ? "unknown (probably already completed)" : ""));

  auto runner = m_Proxied->processRunner();

  const auto r = runner
    .setWaitForCompletion(ProcessRunner::ForceWait, UILocker::OutputRequired)
    .attachToProcess(handle);

  if (exitCode) {
    *exitCode = runner.exitCode();
  }

  switch (r)
  {
    case ProcessRunner::Completed:
      return true;

    case ProcessRunner::Cancelled:     // fall-through
    case ProcessRunner::ForceUnlocked:
      // this is always an error because the application should have run to
      // completion
      return false;

    case ProcessRunner::Error: // fall-through
    default:
      return false;
  }
}

void OrganizerProxy::refresh(bool saveChanges)
{
  if (m_Proxied) {
    m_Proxied->refresh(saveChanges);
  }
}

IModInterface *OrganizerProxy::installMod(const QString &fileName, const QString &nameSuggestion)
{
  if (m_Proxied) {
    return m_Proxied->installMod(fileName, false, nullptr, nameSuggestion);
  } else {
    return nullptr;
  }
}

QString OrganizerProxy::resolvePath(const QString &fileName) const
{
  if (m_Proxied) {
    return m_Proxied->resolvePath(fileName);
  } else {
    return {};
  }
}

QStringList OrganizerProxy::listDirectories(const QString &directoryName) const
{
  if (m_Proxied) {
    return m_Proxied->listDirectories(directoryName);
  } else {
    return {};
  }
}

QStringList OrganizerProxy::findFiles(const QString &path, const std::function<bool(const QString&)> &filter) const
{
  if (m_Proxied) {
    return m_Proxied->findFiles(path, filter);
  } else {
    return {};
  }
}

QStringList OrganizerProxy::findFiles(const QString& path, const QStringList& globFilters) const
{
  if (!m_Proxied) {
    return {};
  }

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

QStringList OrganizerProxy::getFileOrigins(const QString &fileName) const
{
  if (m_Proxied) {
    return m_Proxied->getFileOrigins(fileName);
  } else {
    return {};
  }
}

QList<MOBase::IOrganizer::FileInfo> OrganizerProxy::findFileInfos(const QString &path, const std::function<bool (const MOBase::IOrganizer::FileInfo &)> &filter) const
{
  if (m_Proxied) {
    return m_Proxied->findFileInfos(path, filter);
  } else {
    return {};
  }
}

MOBase::IDownloadManager *OrganizerProxy::downloadManager() const
{
  return m_DownloadManagerProxy.get();
}

MOBase::IPluginList *OrganizerProxy::pluginList() const
{
  return m_PluginListProxy.get();
}

MOBase::IModList *OrganizerProxy::modList() const
{
  return m_ModListProxy.get();
}

MOBase::IProfile *OrganizerProxy::profile() const
{
  if (m_Proxied) {
    return m_Proxied->currentProfile();
  } else {
    return nullptr;
  }
}

MOBase::IPluginGame const *OrganizerProxy::managedGame() const
{
  if (m_Proxied) {
    return m_Proxied->managedGame();
  } else {
    return nullptr;
  }
}

// CALLBACKS

bool OrganizerProxy::onAboutToRun(const std::function<bool(const QString&)>& func)
{
  if (m_Proxied) {
    return m_Proxied->onAboutToRun(MOShared::callIfPluginActive(this, func, true));
  } else {
    return false;
  }
}

bool OrganizerProxy::onFinishedRun(const std::function<void(const QString&, unsigned int)>& func)
{
  if (m_Proxied) {
    return m_Proxied->onFinishedRun(MOShared::callIfPluginActive(this, func));
  } else {
    return false;
  }
}

bool OrganizerProxy::onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func)
{
  if (m_Proxied) {
    // Always call this one to allow plugin to initialize themselves even when not active:
    return m_Proxied->onUserInterfaceInitialized(func);
  } else {
    return false;
  }
}

bool OrganizerProxy::onProfileCreated(std::function<void(IProfile*)> const& func)
{
  if (m_Proxied) {
    return m_Proxied->onProfileCreated(MOShared::callIfPluginActive(this, func));
  } else {
    return false;
  }
}

bool OrganizerProxy::onProfileRenamed(std::function<void(IProfile*, QString const&, QString const&)> const& func)
{
  if (m_Proxied) {
    return m_Proxied->onProfileRenamed(MOShared::callIfPluginActive(this, func));
  } else {
    return false;
  }
}

bool OrganizerProxy::onProfileRemoved(std::function<void(QString const&)> const& func)
{
  if (m_Proxied) {
    return m_Proxied->onProfileRemoved(MOShared::callIfPluginActive(this, func));
  } else {
    return false;
  }
}

bool OrganizerProxy::onProfileChanged(std::function<void(MOBase::IProfile*, MOBase::IProfile*)> const& func)
{
  if (m_Proxied) {
    return m_Proxied->onProfileChanged(MOShared::callIfPluginActive(this, func));
  } else {
    return false;
  }
}

bool OrganizerProxy::onPluginSettingChanged(std::function<void(QString const&, const QString& key, const QVariant&, const QVariant&)> const& func)
{
  if (m_Proxied) {
    // Always call this one, otherwise plugin cannot detect they are being enabled / disabled:
    return m_Proxied->onPluginSettingChanged(func);
  } else {
    return false;
  }
}
