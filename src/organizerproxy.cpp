#include "organizerproxy.h"
#include <gameinfo.h>
#include <appconfig.h>


using namespace MOBase;
using namespace MOShared;


OrganizerProxy::OrganizerProxy(MainWindow *window, const QString &pluginName)
  : m_Proxied(window)
  , m_PluginName(pluginName)
{
}

IGameInfo &OrganizerProxy::gameInfo() const
{
  return *m_Proxied->m_GameInfo;
}


IModRepositoryBridge *OrganizerProxy::createNexusBridge() const
{
  return new NexusBridge(m_PluginName);
}


QString OrganizerProxy::profileName() const
{
  if (m_Proxied->m_CurrentProfile != NULL) {
    return m_Proxied->m_CurrentProfile->getName();
  } else {
    return "";
  }
}

QString OrganizerProxy::profilePath() const
{
  if (m_Proxied->m_CurrentProfile != NULL) {
    return m_Proxied->m_CurrentProfile->getPath();
  } else {
    return "";
  }
}

QString OrganizerProxy::downloadsPath() const
{
  return QDir::fromNativeSeparators(m_Proxied->m_Settings.getDownloadDirectory());
}

QString OrganizerProxy::overwritePath() const
{
  return QDir::fromNativeSeparators(ToQString(GameInfo::instance().getOrganizerDirectory()))
         + "/"
         + ToQString(AppConfig::overwritePath());
}

VersionInfo OrganizerProxy::appVersion() const
{
  return m_Proxied->m_Updater.getVersion();
}

IModInterface *OrganizerProxy::getMod(const QString &name)
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

void OrganizerProxy::modDataChanged(IModInterface*)
{
  m_Proxied->refreshModList();
}

QVariant OrganizerProxy::pluginSetting(const QString &pluginName, const QString &key) const
{
  return m_Proxied->m_Settings.pluginSetting(pluginName, key);
}

void OrganizerProxy::setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value)
{
  m_Proxied->m_Settings.setPluginSetting(pluginName, key, value);
}

QVariant OrganizerProxy::persistent(const QString &pluginName, const QString &key, const QVariant &def) const
{
  return m_Proxied->m_Settings.pluginPersistent(pluginName, key, def);
}

void OrganizerProxy::setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync)
{
  m_Proxied->m_Settings.setPluginPersistent(pluginName, key, value, sync);
}

QString OrganizerProxy::pluginDataPath() const
{
  QString pluginPath = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getOrganizerDirectory())) + "/" + ToQString(AppConfig::pluginPath());
  return pluginPath + "/data";
}

HANDLE OrganizerProxy::startApplication(const QString &executable, const QStringList &args, const QString &cwd, const QString &profile)
{
  return m_Proxied->startApplication(executable, args, cwd, profile);
}

bool OrganizerProxy::waitForApplication(HANDLE handle, LPDWORD exitCode) const
{
  return m_Proxied->waitForProcessOrJob(handle, exitCode);
}

bool OrganizerProxy::onAboutToRun(const std::function<bool (const QString &)> &func)
{
  auto conn = m_Proxied->m_AboutToRun.connect(func);
  return conn.connected();
}

bool OrganizerProxy::onModInstalled(const std::function<void (const QString &)> &func)
{
  auto conn = m_Proxied->m_ModInstalled.connect(func);
  return conn.connected();
}

void OrganizerProxy::refreshModList(bool saveChanges)
{
  m_Proxied->refreshModList(saveChanges);
}

IModInterface *OrganizerProxy::installMod(const QString &fileName)
{
  return m_Proxied->installMod(fileName);
}

QString OrganizerProxy::resolvePath(const QString &fileName) const
{
  if (m_Proxied->m_DirectoryStructure == NULL) {
    return QString();
  }
  const FileEntry::Ptr file = m_Proxied->m_DirectoryStructure->searchFile(ToWString(fileName), NULL);
  if (file.get() != NULL) {
    return ToQString(file->getFullPath());
  } else {
    return QString();
  }
}

QStringList OrganizerProxy::listDirectories(const QString &directoryName) const
{
  QStringList result;
  DirectoryEntry *dir = m_Proxied->m_DirectoryStructure->findSubDirectoryRecursive(ToWString(directoryName));
  if (dir != NULL) {
    std::vector<DirectoryEntry*>::iterator current, end;
    dir->getSubDirectories(current, end);
    for (; current != end; ++current) {
      result.append(ToQString((*current)->getName()));
    }
  }
  return result;
}

QStringList OrganizerProxy::findFiles(const QString &path, const std::function<bool(const QString&)> &filter) const
{
  QStringList result;
  DirectoryEntry *dir = m_Proxied->m_DirectoryStructure->findSubDirectoryRecursive(ToWString(path));
  if (dir != NULL) {
    std::vector<FileEntry::Ptr> files = dir->getFiles();
    foreach (FileEntry::Ptr file, files) {
      if (filter(ToQString(file->getFullPath()))) {
        result.append(ToQString(file->getFullPath()));
      }
    }
  } else {
    qWarning("directory %s not found", qPrintable(path));
  }
  return result;
}

QStringList OrganizerProxy::getFileOrigins(const QString &fileName) const
{
  QStringList result;
  const FileEntry::Ptr file = m_Proxied->m_DirectoryStructure->searchFile(ToWString(QFileInfo(fileName).fileName()), NULL);

  if (file.get() != NULL) {
    result.append(ToQString(m_Proxied->m_DirectoryStructure->getOriginByID(file->getOrigin()).getName()));
    foreach (int i, file->getAlternatives()) {
      result.append(ToQString(m_Proxied->m_DirectoryStructure->getOriginByID(i).getName()));
    }
  } else {
    qDebug("%s not found", qPrintable(fileName));
  }
  return result;
}

QList<MOBase::IOrganizer::FileInfo> OrganizerProxy::findFileInfos(const QString &path, const std::function<bool (const MOBase::IOrganizer::FileInfo &)> &filter) const
{
  return m_Proxied->findFileInfos(path, filter);
}

MOBase::IDownloadManager *OrganizerProxy::downloadManager()
{
  return &m_Proxied->m_DownloadManager;
}

MOBase::IPluginList *OrganizerProxy::pluginList()
{
  return &m_Proxied->m_PluginList;
}

MOBase::IModList *OrganizerProxy::modList()
{
  return &m_Proxied->m_ModList;
}
