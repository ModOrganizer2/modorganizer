#include "organizerproxy.h"

#include "shared/appconfig.h"
#include "organizercore.h"
#include "plugincontainer.h"
#include "settings.h"
#include "glob_matching.h"

#include <QObject>
#include <QApplication>

using namespace MOBase;
using namespace MOShared;


OrganizerProxy::OrganizerProxy(OrganizerCore *organizer, PluginContainer *pluginContainer, const QString &pluginName)
  : m_Proxied(organizer)
  , m_PluginContainer(pluginContainer)
  , m_PluginName(pluginName)
{
}

IModRepositoryBridge *OrganizerProxy::createNexusBridge() const
{
  return new NexusBridge(m_PluginContainer, m_PluginName);
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

VersionInfo OrganizerProxy::appVersion() const
{
  return m_Proxied->appVersion();
}

IModInterface *OrganizerProxy::getMod(const QString &name) const
{
  return m_Proxied->getMod(name);
}

IPluginGame *OrganizerProxy::getGame(const QString &gameName) const
{
  return m_Proxied->getGame(gameName);
}

IModInterface *OrganizerProxy::createMod(MOBase::GuessedValue<QString> &name)
{
  return m_Proxied->createMod(name);
}

bool OrganizerProxy::removeMod(IModInterface *mod)
{
  return m_Proxied->removeMod(mod);
}

void OrganizerProxy::modDataChanged(IModInterface *mod)
{
  m_Proxied->modDataChanged(mod);
}

QVariant OrganizerProxy::pluginSetting(const QString &pluginName, const QString &key) const
{
  return m_Proxied->pluginSetting(pluginName, key);
}

void OrganizerProxy::setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value)
{
  m_Proxied->setPluginSetting(pluginName, key, value);
}

QVariant OrganizerProxy::persistent(const QString &pluginName, const QString &key, const QVariant &def) const
{
  return m_Proxied->persistent(pluginName, key, def);
}

void OrganizerProxy::setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync)
{
  m_Proxied->setPersistent(pluginName, key, value, sync);
}

QString OrganizerProxy::pluginDataPath() const
{
  return m_Proxied->pluginDataPath();
}

HANDLE OrganizerProxy::startApplication(
  const QString& exe, const QStringList& args, const QString &cwd,
  const QString& profile, const QString &overwrite, bool ignoreOverwrite)
{
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

bool OrganizerProxy::onAboutToRun(const std::function<bool (const QString &)> &func)
{
  return m_Proxied->onAboutToRun(func);
}

bool OrganizerProxy::onFinishedRun(const std::function<void (const QString &, unsigned int)> &func)
{
  return m_Proxied->onFinishedRun(func);
}

bool OrganizerProxy::onModInstalled(const std::function<void (const QString &)> &func)
{
  return m_Proxied->onModInstalled(func);
}

bool OrganizerProxy::onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func)
{
  return m_Proxied->onUserInterfaceInitialized(func);
}

bool OrganizerProxy::onProfileChanged(std::function<void(MOBase::IProfile*, MOBase::IProfile*)> const& func)
{
  return m_Proxied->onProfileChanged(func);
}

bool OrganizerProxy::onPluginSettingChanged(std::function<void(QString const&, const QString& key, const QVariant&, const QVariant&)> const& func)
{
  return m_Proxied->onPluginSettingChanged(func);
}

void OrganizerProxy::refreshModList(bool saveChanges)
{
  m_Proxied->refreshModList(saveChanges);
}

IModInterface *OrganizerProxy::installMod(const QString &fileName, const QString &nameSuggestion)
{
  return m_Proxied->installMod(fileName, nameSuggestion);
}

QString OrganizerProxy::resolvePath(const QString &fileName) const
{
  return m_Proxied->resolvePath(fileName);
}

QStringList OrganizerProxy::listDirectories(const QString &directoryName) const
{
  return m_Proxied->listDirectories(directoryName);
}

QStringList OrganizerProxy::findFiles(const QString &path, const std::function<bool(const QString&)> &filter) const
{
  return m_Proxied->findFiles(path, filter);
}

QStringList OrganizerProxy::findFiles(const QString& path, const QStringList& globFilters) const
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

QStringList OrganizerProxy::getFileOrigins(const QString &fileName) const
{
  return m_Proxied->getFileOrigins(fileName);
}

QList<MOBase::IOrganizer::FileInfo> OrganizerProxy::findFileInfos(const QString &path, const std::function<bool (const MOBase::IOrganizer::FileInfo &)> &filter) const
{
  return m_Proxied->findFileInfos(path, filter);
}

MOBase::IDownloadManager *OrganizerProxy::downloadManager() const
{
  return m_Proxied->downloadManager();
}

MOBase::IPluginList *OrganizerProxy::pluginList() const
{
  return m_Proxied->pluginList();
}

MOBase::IModList *OrganizerProxy::modList() const
{
  return m_Proxied->modList();
}

MOBase::IProfile *OrganizerProxy::profile() const
{
  return m_Proxied->currentProfile();
}

MOBase::IPluginGame const *OrganizerProxy::managedGame() const
{
  return m_Proxied->managedGame();
}

QStringList OrganizerProxy::modsSortedByProfilePriority() const
{
  return m_Proxied->modsSortedByProfilePriority();
}
