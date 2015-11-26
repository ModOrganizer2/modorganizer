#include "organizerproxy.h"

#include <appconfig.h>

#include <QApplication>

using namespace MOBase;
using namespace MOShared;


OrganizerProxy::OrganizerProxy(OrganizerCore *organizer, const QString &pluginName)
  : m_Proxied(organizer)
  , m_PluginName(pluginName)
{
}

IModRepositoryBridge *OrganizerProxy::createNexusBridge() const
{
  return new NexusBridge(m_PluginName);
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
  return QDir::fromNativeSeparators(qApp->property("dataPath").toString())
         + "/"
         + ToQString(AppConfig::overwritePath());
}

VersionInfo OrganizerProxy::appVersion() const
{
  return m_Proxied->appVersion();
}

IModInterface *OrganizerProxy::getMod(const QString &name) const
{
  return m_Proxied->getMod(name);
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

HANDLE OrganizerProxy::startApplication(const QString &executable, const QStringList &args, const QString &cwd, const QString &profile)
{
  return m_Proxied->startApplication(executable, args, cwd, profile);
}

bool OrganizerProxy::waitForApplication(HANDLE handle, LPDWORD exitCode) const
{
  return m_Proxied->waitForApplication(handle, exitCode);
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

MOBase::IPluginGame const *OrganizerProxy::managedGame() const
{
  return m_Proxied->managedGame();
}
