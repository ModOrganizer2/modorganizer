#include "organizercore.h"
#include "mainwindow.h"
#include "gameinfoimpl.h"
#include "messagedialog.h"
#include "logbuffer.h"
#include "credentialsdialog.h"
#include "filedialogmemory.h"
#include "lockeddialog.h"
#include "modinfodialog.h"
#include "spawn.h"
#include "safewritefile.h"
#include "syncoverwritedialog.h"
#include "nxmaccessmanager.h"
#include <ipluginmodpage.h>
#include <dataarchives.h>
#include <directoryentry.h>
#include <scopeguard.h>
#include <utility.h>
#include <appconfig.h>
#include <report.h>
#include <questionboxmemory.h>
#include <QNetworkInterface>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QApplication>
#include <Psapi.h>
#include <functional>


using namespace MOShared;
using namespace MOBase;


static bool isOnline()
{
  QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

  bool connected = false;
  for (auto iter = interfaces.begin(); iter != interfaces.end() && !connected; ++iter) {
    if ( (iter->flags() & QNetworkInterface::IsUp) &&
         (iter->flags() & QNetworkInterface::IsRunning) &&
        !(iter->flags() & QNetworkInterface::IsLoopBack)) {
      auto addresses = iter->addressEntries();
      if (addresses.count() == 0) {
        continue;
      }
      qDebug("interface %s seems to be up (address: %s)",
             qPrintable(iter->humanReadableName()),
             qPrintable(addresses[0].ip().toString()));
      connected = true;
    }
  }

  return connected;
}

static bool renameFile(const QString &oldName, const QString &newName, bool overwrite = true)
{
  if (overwrite && QFile::exists(newName)) {
    QFile::remove(newName);
  }
  return QFile::rename(oldName, newName);
}

static std::wstring getProcessName(DWORD processId)
{
  HANDLE process = ::OpenProcess(PROCESS_QUERY_INFORMATION, false, processId);

  wchar_t buffer[MAX_PATH];
  if (::GetProcessImageFileNameW(process, buffer, MAX_PATH) != 0) {
    wchar_t *fileName = wcsrchr(buffer, L'\\');
    if (fileName == nullptr) {
      fileName = buffer;
    } else {
      fileName += 1;
    }
    return fileName;
  } else {
    return std::wstring(L"unknown");
  }
}

static void startSteam(QWidget *widget)
{
  QSettings steamSettings("HKEY_CURRENT_USER\\Software\\Valve\\Steam", QSettings::NativeFormat);
  QString exe = steamSettings.value("SteamExe", "").toString();
  if (!exe.isEmpty()) {
    exe = QString("\"%1\"").arg(exe);
    //See if username and password supplied. If so, pass them into steam.
    QStringList args;
    QString username;
    QString password;
    if (Settings::instance().getSteamLogin(username, password)) {
      args << "-login";
      args << username;
      if (password != "") {
        args << password;
      }
    }
    if (!QProcess::startDetached(exe, args)) {
      reportError(QObject::tr("Failed to start \"%1\"").arg(exe));
    } else {
      QMessageBox::information(widget, QObject::tr("Waiting"),
                               QObject::tr("Please press OK once you're logged into steam."));
    }
  }
}

template <typename InputIterator>
QStringList toStringList(InputIterator current, InputIterator end)
{
  QStringList result;
  for (; current != end; ++current) {
    result.append(*current);
  }
  return result;
}


OrganizerCore::OrganizerCore(const QSettings &initSettings)
  : m_GameInfo(new GameInfoImpl())
  , m_UserInterface(nullptr)
  , m_PluginContainer(nullptr)
  , m_GameName()
  , m_CurrentProfile(nullptr)
  , m_Settings(initSettings)
  , m_Updater(NexusInterface::instance())
  , m_AboutToRun()
  , m_FinishedRun()
  , m_ModInstalled()
  , m_ModList(this)
  , m_PluginList(this)
  , m_DirectoryRefresher()
  , m_DirectoryStructure(new DirectoryEntry(L"data", nullptr, 0))
  , m_DownloadManager(NexusInterface::instance(), this)
  , m_InstallationManager()
  , m_RefresherThread()
  , m_AskForNexusPW(false)
  , m_DirectoryUpdate(false)
  , m_ArchivesInit(false)
  , m_PluginListsWriter(std::bind(&OrganizerCore::savePluginList, this))
{
  m_DownloadManager.setOutputDirectory(m_Settings.getDownloadDirectory());
  m_DownloadManager.setPreferredServers(m_Settings.getPreferredServers());

  NexusInterface::instance()->setCacheDirectory(m_Settings.getCacheDirectory());
  NexusInterface::instance()->setNMMVersion(m_Settings.getNMMVersion());

  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
  m_InstallationManager.setDownloadDirectory(m_Settings.getDownloadDirectory());

  connect(&m_DownloadManager, SIGNAL(downloadSpeed(QString,int)), this, SLOT(downloadSpeed(QString,int)));
  connect(&m_DirectoryRefresher, SIGNAL(refreshed()), this, SLOT(directory_refreshed()));

  connect(&m_ModList, SIGNAL(removeOrigin(QString)), this, SLOT(removeOrigin(QString)));

  connect(NexusInterface::instance()->getAccessManager(), SIGNAL(loginSuccessful(bool)), this, SLOT(loginSuccessful(bool)));
  connect(NexusInterface::instance()->getAccessManager(), SIGNAL(loginFailed(QString)), this, SLOT(loginFailed(QString)));

  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame*)), &m_Settings, SLOT(managedGameChanged(MOBase::IPluginGame*)));

  connect(&m_PluginList, &PluginList::writePluginsList, &m_PluginListsWriter, &DelayedFileWriterBase::write);

  // make directory refresher run in a separate thread
  m_RefresherThread.start();
  m_DirectoryRefresher.moveToThread(&m_RefresherThread);

  m_AskForNexusPW = initSettings.value("ask_for_nexuspw", true).toBool();
}

OrganizerCore::~OrganizerCore()
{
  m_RefresherThread.exit();
  m_RefresherThread.wait();

  prepareStart();

  // profile has to be cleaned up before the modinfo-buffer is cleared
  delete m_CurrentProfile;
  m_CurrentProfile = nullptr;

  ModInfo::clear();
  LogBuffer::cleanQuit();
  m_ModList.setProfile(nullptr);
  NexusInterface::instance()->cleanup();

  delete m_GameInfo;
  delete m_DirectoryStructure;
}

QString OrganizerCore::commitSettings(const QString &iniFile)
{
  if (!shellRename(iniFile + ".new", iniFile, true, qApp->activeWindow())) {
    DWORD err = ::GetLastError();
    // make a second attempt using qt functions but if that fails print the error from the first attempt
    if (!renameFile(iniFile + ".new", iniFile)) {
      return windowsErrorString(err);
    }
  }
  return QString();
}

QSettings::Status OrganizerCore::storeSettings(const QString &fileName)
{
  QSettings settings(fileName, QSettings::IniFormat);
  if (m_UserInterface != nullptr) {
    m_UserInterface->storeSettings(settings);
  }
  if (m_CurrentProfile != nullptr) {
    settings.setValue("selected_profile", m_CurrentProfile->name().toUtf8().constData());
  }
  settings.setValue("ask_for_nexuspw", m_AskForNexusPW);

  settings.remove("customExecutables");
  settings.beginWriteArray("customExecutables");
  std::vector<Executable>::const_iterator current, end;
  m_ExecutablesList.getExecutables(current, end);
  int count = 0;
  for (; current != end; ++current) {
    const Executable &item = *current;
    settings.setArrayIndex(count++);
    settings.setValue("title", item.m_Title);
    settings.setValue("custom", item.m_Custom);
    settings.setValue("toolbar", item.m_Toolbar);
    if (item.m_Custom) {
      settings.setValue("binary", item.m_BinaryInfo.absoluteFilePath());
      settings.setValue("arguments", item.m_Arguments);
      settings.setValue("workingDirectory", item.m_WorkingDirectory);
      settings.setValue("closeOnStart", item.m_CloseMO == ExecutableInfo::CloseMOStyle::DEFAULT_CLOSE);
      settings.setValue("steamAppID", item.m_SteamAppID);
    }
  }
  settings.endArray();

  FileDialogMemory::save(settings);

  settings.sync();
  return settings.status();
}

void OrganizerCore::storeSettings()
{
  QString iniFile = qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::iniFileName());
  if (!shellCopy(iniFile, iniFile + ".new", true, qApp->activeWindow())) {
    QMessageBox::critical(qApp->activeWindow(), tr("Failed to write settings"),
                          tr("An error occured trying to update MO settings to %1: %2").arg(
                            iniFile, windowsErrorString(::GetLastError())));
    return;
  }

  QSettings::Status result = storeSettings(iniFile + ".new");

  if (result == QSettings::NoError) {
    QString errMsg = commitSettings(iniFile);
    if (!errMsg.isEmpty()) {
      qWarning("settings file not writable, may be locked by another application, trying direct write");
      result = storeSettings(iniFile);
    }
  }
  if (result != QSettings::NoError) {
    QString reason = result == QSettings::AccessError ? tr("File is write protected")
                   : result == QSettings::FormatError ? tr("Invalid file format (probably a bug)")
                   : tr("Unknown error %1").arg(result);
    QMessageBox::critical(qApp->activeWindow(), tr("Failed to write settings"),
                          tr("An error occured trying to write back MO settings to %1: %2").arg(iniFile + ".new", reason));
  }
}

bool OrganizerCore::testForSteam()
{
  size_t currentSize = 1024;
  std::unique_ptr<DWORD[]> processIDs;
  DWORD bytesReturned;
  bool success = false;
  while (!success) {
    processIDs.reset(new DWORD[currentSize]);
    if (!::EnumProcesses(processIDs.get(), currentSize * sizeof(DWORD), &bytesReturned)) {
      qWarning("failed to determine if steam is running");
      return true;
    }
    if (bytesReturned == (currentSize * sizeof(DWORD))) {
      // maximum size used, list probably truncated
      currentSize *= 2;
    } else {
      success = true;
    }
  }
  TCHAR processName[MAX_PATH];
  for (unsigned int i = 0; i < bytesReturned / sizeof(DWORD); ++i) {
    memset(processName, '\0', sizeof(TCHAR) * MAX_PATH);
    if (processIDs[i] != 0) {
      HANDLE process = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIDs[i]);

      if (process != nullptr) {
        HMODULE module;
        DWORD ignore;

        // first module in a process is always the binary
        if (EnumProcessModules(process, &module, sizeof(HMODULE) * 1, &ignore)) {
          ::GetModuleBaseName(process, module, processName, MAX_PATH);
          if ((_tcsicmp(processName, TEXT("steam.exe")) == 0) ||
              (_tcsicmp(processName, TEXT("steamservice.exe")) == 0)) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

void OrganizerCore::updateExecutablesList(QSettings &settings)
{
  if (m_PluginContainer == nullptr) {
    qCritical("can't update executables list now");
    return;
  }

  m_ExecutablesList.init(m_PluginContainer->managedGame(ToQString(GameInfo::instance().getGameName())));

  qDebug("setting up configured executables");

  int numCustomExecutables = settings.beginReadArray("customExecutables");
  for (int i = 0; i < numCustomExecutables; ++i) {
    settings.setArrayIndex(i);
    ExecutableInfo::CloseMOStyle closeMO =
        settings.value("closeOnStart").toBool() ? ExecutableInfo::CloseMOStyle::DEFAULT_CLOSE
                                                : ExecutableInfo::CloseMOStyle::DEFAULT_STAY;
    m_ExecutablesList.addExecutable(settings.value("title").toString(),
                                    settings.value("binary").toString(),
                                    settings.value("arguments").toString(),
                                    settings.value("workingDirectory", "").toString(),
                                    closeMO,
                                    settings.value("steamAppID", "").toString(),
                                    settings.value("custom", true).toBool(),
                                    settings.value("toolbar", false).toBool());
  }

  settings.endArray();


  // TODO this has nothing to do with executables list move to an appropriate function!
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure, m_Settings.displayForeign());
}

void OrganizerCore::setUserInterface(IUserInterface *userInterface, QWidget *widget)
{
  storeSettings();

  m_UserInterface = userInterface;

  if (widget != nullptr) {
    connect(&m_ModList, SIGNAL(modlist_changed(QModelIndex, int)), widget, SLOT(modlistChanged(QModelIndex, int)));
    connect(&m_ModList, SIGNAL(showMessage(QString)), widget, SLOT(showMessage(QString)));
    connect(&m_ModList, SIGNAL(modRenamed(QString,QString)), widget, SLOT(modRenamed(QString,QString)));
    connect(&m_ModList, SIGNAL(modUninstalled(QString)), widget, SLOT(modRemoved(QString)));
    connect(&m_ModList, SIGNAL(removeSelectedMods()), widget, SLOT(removeMod_clicked()));
    connect(&m_ModList, SIGNAL(requestColumnSelect(QPoint)), widget, SLOT(displayColumnSelection(QPoint)));
    connect(&m_ModList, SIGNAL(fileMoved(QString, QString, QString)), widget, SLOT(fileMoved(QString, QString, QString)));
    connect(&m_ModList, SIGNAL(modorder_changed()), widget, SLOT(modorder_changed()));
    connect(&m_DownloadManager, SIGNAL(showMessage(QString)), widget, SLOT(showMessage(QString)));
  }

  m_InstallationManager.setParentWidget(widget);
  m_Updater.setUserInterface(widget);

  // this currently wouldn't work reliably if the ui isn't initialized yet to display the result
  if (isOnline() && !m_Settings.offlineMode()) {
    m_Updater.testForUpdate();
  } else {
    qDebug("user doesn't seem to be connected to the internet");
  }
}

void OrganizerCore::connectPlugins(PluginContainer *container)
{
  m_DownloadManager.setSupportedExtensions(m_InstallationManager.getSupportedExtensions());
  m_PluginContainer = container;

  if (!m_GameName.isEmpty()) {
    m_GamePlugin = m_PluginContainer->managedGame(m_GameName);
    emit managedGameChanged(m_GamePlugin);
  }
}

void OrganizerCore::disconnectPlugins()
{
  m_AboutToRun.disconnect_all_slots();
  m_FinishedRun.disconnect_all_slots();
  m_ModInstalled.disconnect_all_slots();
  m_ModList.disconnectSlots();
  m_PluginList.disconnectSlots();

  m_Settings.clearPlugins();
  m_GamePlugin = nullptr;
  m_PluginContainer = nullptr;
}

void OrganizerCore::setManagedGame(const QString &gameName, const QString &gamePath)
{
  m_GameName = gameName;
  if (m_PluginContainer != nullptr) {
    m_GamePlugin = m_PluginContainer->managedGame(m_GameName);
    m_GamePlugin->setGamePath(gamePath);
    qApp->setProperty("managed_game", QVariant::fromValue(m_GamePlugin));
    emit managedGameChanged(m_GamePlugin);
  }
}

Settings &OrganizerCore::settings()
{
  return m_Settings;
}

bool OrganizerCore::nexusLogin()
{
  QString username, password;

  NXMAccessManager *accessManager = NexusInterface::instance()->getAccessManager();

  if (!accessManager->loginAttempted()
      && !accessManager->loggedIn()
      && (m_Settings.getNexusLogin(username, password)
          || (m_AskForNexusPW
              && queryLogin(username, password)))) {
    accessManager->login(username, password);
    return true;
  } else {
    return false;
  }
}

bool OrganizerCore::queryLogin(QString &username, QString &password)
{
  CredentialsDialog dialog(qApp->activeWindow());
  int res = dialog.exec();
  if (dialog.neverAsk()) {
    m_AskForNexusPW = false;
  }
  if (res == QDialog::Accepted) {
    username = dialog.username();
    password = dialog.password();
    if (dialog.store()) {
      m_Settings.setNexusLogin(username, password);
    }
    return true;
  } else {
    return false;
  }
}

void OrganizerCore::startMOUpdate()
{
  if (nexusLogin()) {
    m_PostLoginTasks.append([&]() { m_Updater.startUpdate(); });
  } else {
    m_Updater.startUpdate();
  }
}

void OrganizerCore::downloadRequestedNXM(const QString &url)
{
  qDebug("download requested: %s", qPrintable(url));
  if (nexusLogin()) {
    m_PendingDownloads.append(url);
  } else {
    m_DownloadManager.addNXMDownload(url);
  }
}

void OrganizerCore::externalMessage(const QString &message)
{
  if (message.left(6).toLower() == "nxm://") {
    MessageDialog::showMessage(tr("Download started"), qApp->activeWindow());
    downloadRequestedNXM(message);
  }
}

void OrganizerCore::downloadRequested(QNetworkReply *reply, int modID, const QString &fileName)
{
  try {
    if (m_DownloadManager.addDownload(reply, QStringList(), fileName, modID, 0, new ModRepositoryFileInfo(modID))) {
      MessageDialog::showMessage(tr("Download started"), qApp->activeWindow());
    }
  } catch (const std::exception &e) {
    MessageDialog::showMessage(tr("Download failed"), qApp->activeWindow());
    qCritical("exception starting download: %s", e.what());
  }
}

void OrganizerCore::removeOrigin(const QString &name)
{
  FilesOrigin &origin = m_DirectoryStructure->getOriginByName(ToWString(name));
  origin.enable(false);
  refreshLists();
}

void OrganizerCore::downloadSpeed(const QString &serverName, int bytesPerSecond)
{
  m_Settings.setDownloadSpeed(serverName, bytesPerSecond);
}

InstallationManager *OrganizerCore::installationManager()
{
  return &m_InstallationManager;
}

void OrganizerCore::createDefaultProfile()
{
  QString profilesPath = qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::profilesPath());
  if (QDir(profilesPath).entryList(QDir::AllDirs | QDir::NoDotAndDotDot).size() == 0) {
    Profile newProf("Default", managedGame(), false);
  }
}

void OrganizerCore::setCurrentProfile(const QString &profileName)
{
  if ((m_CurrentProfile != nullptr) &&
      (profileName == m_CurrentProfile->name())) {
    return;
  }
  QString profileDir = qApp->property("dataPath").toString() + "/" + ToQString(AppConfig::profilesPath()) + "/" + profileName;
  Profile *newProfile = new Profile(QDir(profileDir), managedGame());

  delete m_CurrentProfile;
  m_CurrentProfile = newProfile;
  m_ModList.setProfile(newProfile);

  connect(m_CurrentProfile, SIGNAL(modStatusChanged(uint)), this, SLOT(modStatusChanged(uint)));

  refreshDirectoryStructure();
}

MOBase::IGameInfo &OrganizerCore::gameInfo() const
{
  return *m_GameInfo;
}

MOBase::IModRepositoryBridge *OrganizerCore::createNexusBridge() const
{
  return new NexusBridge();
}

QString OrganizerCore::profileName() const
{
  if (m_CurrentProfile != nullptr) {
    return m_CurrentProfile->name();
  } else {
    return "";
  }
}

QString OrganizerCore::profilePath() const
{
  if (m_CurrentProfile != nullptr) {
    return m_CurrentProfile->absolutePath();
  } else {
    return "";
  }
}

QString OrganizerCore::downloadsPath() const
{
  return QDir::fromNativeSeparators(m_Settings.getDownloadDirectory());
}

MOBase::VersionInfo OrganizerCore::appVersion() const
{
  return m_Updater.getVersion();
}

MOBase::IModInterface *OrganizerCore::getMod(const QString &name)
{
  unsigned int index = ModInfo::getIndex(name);
  if (index == UINT_MAX) {
    return nullptr;
  } else {
    return ModInfo::getByIndex(index).data();
  }
}

MOBase::IModInterface *OrganizerCore::createMod(GuessedValue<QString> &name)
{
  bool merge = false;
  if (!m_InstallationManager.testOverwrite(name, &merge)) {
    return nullptr;
  }

  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());

  QString targetDirectory = QDir::fromNativeSeparators(m_Settings.getModDirectory()).append("/").append(name);

  QSettings settingsFile(targetDirectory + "/meta.ini", QSettings::IniFormat);

  if (!merge) {
    settingsFile.setValue("modid", 0);
    settingsFile.setValue("version", "");
    settingsFile.setValue("newestVersion", "");
    settingsFile.setValue("category", 0);
    settingsFile.setValue("installationFile", "");

    settingsFile.beginWriteArray("installedFiles", 0);
    settingsFile.endArray();
  }

  return ModInfo::createFrom(QDir(targetDirectory), &m_DirectoryStructure).data();
}

bool OrganizerCore::removeMod(MOBase::IModInterface *mod)
{
  unsigned int index = ModInfo::getIndex(mod->name());
  if (index == UINT_MAX) {
    return mod->remove();
  } else {
    return ModInfo::removeMod(index);
  }
}

void OrganizerCore::modDataChanged(MOBase::IModInterface*)
{
  refreshModList(false);
}

QVariant OrganizerCore::pluginSetting(const QString &pluginName, const QString &key) const
{
  return m_Settings.pluginSetting(pluginName, key);
}

void OrganizerCore::setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value)
{
  m_Settings.setPluginSetting(pluginName, key, value);
}

QVariant OrganizerCore::persistent(const QString &pluginName, const QString &key, const QVariant &def) const
{
  return m_Settings.pluginPersistent(pluginName, key, def);
}

void OrganizerCore::setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync)
{
  m_Settings.setPluginPersistent(pluginName, key, value, sync);
}

QString OrganizerCore::pluginDataPath() const
{
  return qApp->applicationDirPath() + "/" + ToQString(AppConfig::pluginPath()) + "/data";
}

MOBase::IModInterface *OrganizerCore::installMod(const QString &fileName, const QString &initModName)
{
  if (m_CurrentProfile == nullptr) {
    return nullptr;
  }

  bool hasIniTweaks = false;
  GuessedValue<QString> modName;
  if (!initModName.isEmpty()) {
    modName.update(initModName, GUESS_USER);
  }
  m_CurrentProfile->writeModlistNow();
  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
  if (m_InstallationManager.install(fileName, modName, hasIniTweaks)) {
    MessageDialog::showMessage(tr("Installation successful"), qApp->activeWindow());
    refreshModList();

    int modIndex = ModInfo::getIndex(modName);
    if (modIndex != UINT_MAX) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
      if (hasIniTweaks
          && (m_UserInterface != nullptr)
          && (QMessageBox::question(qApp->activeWindow(), tr("Configure Mod"),
              tr("This mod contains ini tweaks. Do you want to configure them now?"),
              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
        m_UserInterface->displayModInformation(modInfo, modIndex, ModInfoDialog::TAB_INIFILES);
      }
      m_ModInstalled(modName);
      return modInfo.data();
    } else {
      reportError(tr("mod \"%1\" not found").arg(modName));
    }
  } else if (m_InstallationManager.wasCancelled()) {
    QMessageBox::information(qApp->activeWindow(), tr("Installation cancelled"),
                             tr("The mod was not installed completely."), QMessageBox::Ok);
  }
  return nullptr;
}

void OrganizerCore::installDownload(int index)
{
  try {
    QString fileName = m_DownloadManager.getFilePath(index);
    int modID = m_DownloadManager.getModID(index);
    int fileID = m_DownloadManager.getFileInfo(index)->fileID;
    GuessedValue<QString> modName;

    // see if there already are mods with the specified mod id
    if (modID != 0) {
      std::vector<ModInfo::Ptr> modInfo = ModInfo::getByModID(modID);
      for (auto iter = modInfo.begin(); iter != modInfo.end(); ++iter) {
        std::vector<ModInfo::EFlag> flags = (*iter)->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) == flags.end()) {
          modName.update((*iter)->name(), GUESS_PRESET);
          (*iter)->saveMeta();
        }
      }
    }

    m_CurrentProfile->writeModlistNow();

    bool hasIniTweaks = false;
    m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
    if (m_InstallationManager.install(fileName, modName, hasIniTweaks)) {
      MessageDialog::showMessage(tr("Installation successful"), qApp->activeWindow());
      refreshModList();

      int modIndex = ModInfo::getIndex(modName);
      if (modIndex != UINT_MAX) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
        modInfo->addInstalledFile(modID, fileID);

        if (hasIniTweaks
            && m_UserInterface != nullptr
            && (QMessageBox::question(qApp->activeWindow(), tr("Configure Mod"),
                                      tr("This mod contains ini tweaks. Do you want to configure them now?"),
                                      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
          m_UserInterface->displayModInformation(modInfo, modIndex, ModInfoDialog::TAB_INIFILES);
        }

        m_ModInstalled(modName);
      } else {
        reportError(tr("mod \"%1\" not found").arg(modName));
      }
      m_DownloadManager.markInstalled(index);

      emit modInstalled(modName);
    } else if (m_InstallationManager.wasCancelled()) {
      QMessageBox::information(qApp->activeWindow(), tr("Installation cancelled"),
                               tr("The mod was not installed completely."), QMessageBox::Ok);
    }
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}

QString OrganizerCore::resolvePath(const QString &fileName) const
{
  if (m_DirectoryStructure == nullptr) {
    return QString();
  }
  const FileEntry::Ptr file = m_DirectoryStructure->searchFile(ToWString(fileName), nullptr);
  if (file.get() != nullptr) {
    return ToQString(file->getFullPath());
  } else {
    return QString();
  }
}

QStringList OrganizerCore::listDirectories(const QString &directoryName) const
{
  QStringList result;
  DirectoryEntry *dir = m_DirectoryStructure->findSubDirectoryRecursive(ToWString(directoryName));
  if (dir != nullptr) {
    std::vector<DirectoryEntry*>::iterator current, end;
    dir->getSubDirectories(current, end);
    for (; current != end; ++current) {
      result.append(ToQString((*current)->getName()));
    }
  }
  return result;
}

QStringList OrganizerCore::findFiles(const QString &path, const std::function<bool (const QString &)> &filter) const
{
  QStringList result;
  DirectoryEntry *dir = m_DirectoryStructure->findSubDirectoryRecursive(ToWString(path));
  if (dir != nullptr) {
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

QStringList OrganizerCore::getFileOrigins(const QString &fileName) const
{
  QStringList result;
  const FileEntry::Ptr file = m_DirectoryStructure->searchFile(ToWString(QFileInfo(fileName).fileName()), nullptr);

  if (file.get() != nullptr) {
    result.append(ToQString(m_DirectoryStructure->getOriginByID(file->getOrigin()).getName()));
    foreach (int i, file->getAlternatives()) {
      result.append(ToQString(m_DirectoryStructure->getOriginByID(i).getName()));
    }
  } else {
    qDebug("%s not found", qPrintable(fileName));
  }
  return result;
}

QList<MOBase::IOrganizer::FileInfo> OrganizerCore::findFileInfos(const QString &path, const std::function<bool (const MOBase::IOrganizer::FileInfo &)> &filter) const
{
  QList<IOrganizer::FileInfo> result;
  DirectoryEntry *dir = m_DirectoryStructure->findSubDirectoryRecursive(ToWString(path));
  if (dir != nullptr) {
    std::vector<FileEntry::Ptr> files = dir->getFiles();
    foreach (FileEntry::Ptr file, files) {
      IOrganizer::FileInfo info;
      info.filePath = ToQString(file->getFullPath());
      bool fromArchive = false;
      info.origins.append(ToQString(m_DirectoryStructure->getOriginByID(file->getOrigin(fromArchive)).getName()));
      info.archive = fromArchive ? ToQString(file->getArchive()) : "";
      foreach (int idx, file->getAlternatives()) {
        info.origins.append(ToQString(m_DirectoryStructure->getOriginByID(idx).getName()));
      }

      if (filter(info)) {
        result.append(info);
      }
    }
  }
  return result;
}

DownloadManager *OrganizerCore::downloadManager()
{
  return &m_DownloadManager;
}

PluginList *OrganizerCore::pluginList()
{
  return &m_PluginList;
}

ModList *OrganizerCore::modList()
{
  return &m_ModList;
}

void OrganizerCore::spawnBinary(const QFileInfo &binary, const QString &arguments, const QDir &currentDirectory, bool closeAfterStart, const QString &steamAppID)
{
  LockedDialog *dialog = new LockedDialog(qApp->activeWindow());
  dialog->show();
  ON_BLOCK_EXIT([&] () { dialog->hide(); dialog->deleteLater(); });

  HANDLE processHandle = spawnBinaryDirect(binary, arguments, m_CurrentProfile->name(), currentDirectory, steamAppID);
  if (processHandle != INVALID_HANDLE_VALUE) {
    if (closeAfterStart && (m_UserInterface != nullptr)) {
      m_UserInterface->closeWindow();
    } else {
      if (m_UserInterface != nullptr) {
        m_UserInterface->setWindowEnabled(false);
      }
      // re-enable the locked dialog because what'd be the point otherwise?
      dialog->setEnabled(true);

      QCoreApplication::processEvents();

      DWORD processExitCode;
      DWORD retLen;
      JOBOBJECT_BASIC_PROCESS_ID_LIST info;

      {
        DWORD currentProcess = 0UL;
        bool isJobHandle = true;

        DWORD res = ::MsgWaitForMultipleObjects(1, &processHandle, false, 1000, QS_KEY | QS_MOUSE);
        while ((res != WAIT_FAILED) && (res != WAIT_OBJECT_0) && !dialog->unlockClicked()) {
          if (isJobHandle) {
            if (::QueryInformationJobObject(processHandle, JobObjectBasicProcessIdList, &info, sizeof(info), &retLen) > 0) {
              if (info.NumberOfProcessIdsInList == 0) {
                break;
              } else {
                if (info.ProcessIdList[0] != currentProcess) {
                  currentProcess = info.ProcessIdList[0];
                  dialog->setProcessName(ToQString(getProcessName(currentProcess)));
                }
              }
            } else {
              // the info-object I passed only provides space for 1 process id. but since this code only cares about whether there
              // is more than one that's good enough. ERROR_MORE_DATA simply signals there are at least two processes running.
              // any other error probably means the handle is a regular process handle, probably caused by running MO in a job without
              // the right to break out.
              if (::GetLastError() != ERROR_MORE_DATA) {
                isJobHandle = false;
              }
            }
          }

          // keep processing events so the app doesn't appear dead
          QCoreApplication::processEvents();

          res = ::MsgWaitForMultipleObjects(1, &processHandle, false, 1000, QS_KEY | QS_MOUSE);
        }
        ::GetExitCodeProcess(processHandle, &processExitCode);
      }
      ::CloseHandle(processHandle);

      if (m_UserInterface != nullptr) {
        m_UserInterface->setWindowEnabled(true);
      }
      refreshDirectoryStructure();
      // need to remove our stored load order because it may be outdated if a foreign tool changed the
      // file time. After removing that file, refreshESPList will use the file time as the order
      if (GameInfo::instance().getLoadOrderMechanism() == GameInfo::TYPE_FILETIME) {
        qDebug("removing loadorder.txt");
        QFile::remove(m_CurrentProfile->getLoadOrderFileName());
      }
      refreshESPList();
      if (GameInfo::instance().getLoadOrderMechanism() == GameInfo::TYPE_FILETIME) {
        // the load order should have been retrieved from file time, now save it to our own format
        savePluginList();
      }

      m_FinishedRun(binary.absoluteFilePath(), processExitCode);
    }
  }
}

HANDLE OrganizerCore::spawnBinaryDirect(const QFileInfo &binary, const QString &arguments, const QString &profileName,
                                        const QDir &currentDirectory, const QString &steamAppID)
{
  prepareStart();

  if (!binary.exists()) {
    reportError(tr("Executable \"%1\" not found").arg(binary.fileName()));
    return INVALID_HANDLE_VALUE;
  }

  if (!steamAppID.isEmpty()) {
    ::SetEnvironmentVariableW(L"SteamAPPId", ToWString(steamAppID).c_str());
  } else {
    ::SetEnvironmentVariableW(L"SteamAPPId", ToWString(m_Settings.getSteamAppID()).c_str());
  }

  if ((GameInfo::instance().requiresSteam())
      && (m_Settings.getLoadMechanism() == LoadMechanism::LOAD_MODORGANIZER)) {
    if (!testForSteam()) {
      QWidget *window = qApp->activeWindow();
      if ((window != nullptr) && (!window->isVisible())) {
        window = nullptr;
      }
      if (QuestionBoxMemory::query(window, "steamQuery",
            tr("Start Steam?"),
            tr("Steam is required to be running already to correctly start the game. "
               "Should MO try to start steam now?"),
            QDialogButtonBox::Yes | QDialogButtonBox::No) == QDialogButtonBox::Yes) {
        startSteam(qApp->activeWindow());
      }
    }
  }

  while (m_DirectoryUpdate) {
    ::Sleep(100);
    QCoreApplication::processEvents();
  }

  // need to make sure all data is saved before we start the application
  if (m_CurrentProfile != nullptr) {
    m_CurrentProfile->modlistWriter().writeImmediately(true);
  }

  // TODO: should also pass arguments
  if (m_AboutToRun(binary.absoluteFilePath())) {
    return startBinary(binary, arguments, profileName, m_Settings.logLevel(), currentDirectory, true);
  } else {
    qDebug("start of \"%s\" canceled by plugin", qPrintable(binary.absoluteFilePath()));
    return INVALID_HANDLE_VALUE;
  }
}

HANDLE OrganizerCore::startApplication(const QString &executable, const QStringList &args, const QString &cwd, const QString &profile)
{
  QFileInfo binary;
  QString arguments = args.join(" ");
  QString currentDirectory = cwd;
  QString profileName = profile;
  if (profile.length() == 0) {
    if (m_CurrentProfile != nullptr) {
      profileName = m_CurrentProfile->name();
    } else {
      throw MyException(tr("No profile set"));
    }
  }
  QString steamAppID;
  if (executable.contains('\\') || executable.contains('/')) {
    // file path

    binary = QFileInfo(executable);
    if (binary.isRelative()) {
      // relative path, should be relative to game directory
      binary = QFileInfo(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory())) + "/" + executable);
    }
    if (cwd.length() == 0) {
      currentDirectory = binary.absolutePath();
    }
    try {
      const Executable &exe = m_ExecutablesList.findByBinary(binary);
      steamAppID = exe.m_SteamAppID;
    } catch (const std::runtime_error&)  {
      // nop
    }
  } else {
    // only a file name, search executables list
    try {
      const Executable &exe = m_ExecutablesList.find(executable);
      steamAppID = exe.m_SteamAppID;
      if (arguments == "") {
        arguments = exe.m_Arguments;
      }
      binary = exe.m_BinaryInfo;
      if (cwd.length() == 0) {
        currentDirectory = exe.m_WorkingDirectory;
      }
    } catch (const std::runtime_error&) {
      qWarning("\"%s\" not set up as executable", executable.toUtf8().constData());
      binary = QFileInfo(executable);
    }
  }

  return spawnBinaryDirect(binary, arguments, profileName, currentDirectory, steamAppID);
}

bool OrganizerCore::waitForApplication(HANDLE handle, LPDWORD exitCode)
{
  if (m_UserInterface != nullptr) {
    m_UserInterface->lock();
    ON_BLOCK_EXIT([&] () { m_UserInterface->unlock(); });
  }

  DWORD retLen;
  JOBOBJECT_BASIC_PROCESS_ID_LIST info;

  bool isJobHandle = true;

  ULONG lastProcessID = ULONG_MAX;
  HANDLE processHandle = handle;

  DWORD res = ::MsgWaitForMultipleObjects(1, &handle, false, 500, QS_KEY | QS_MOUSE);
  while ((res != WAIT_FAILED)
         && (res != WAIT_OBJECT_0)
         && ((m_UserInterface == nullptr) || !m_UserInterface->unlockClicked())) {
    if (isJobHandle) {
      if (::QueryInformationJobObject(handle, JobObjectBasicProcessIdList, &info, sizeof(info), &retLen) > 0) {
        if (info.NumberOfProcessIdsInList == 0) {
          // fake signaled state
          res = WAIT_OBJECT_0;
          break;
        } else {
          // this is indeed a job handle. Figure out one of the process handles as well.
          if (lastProcessID != info.ProcessIdList[0]) {
            lastProcessID = info.ProcessIdList[0];
            if (processHandle != handle) {
              ::CloseHandle(processHandle);
            }
            processHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, lastProcessID);
          }
        }
      } else {
        // the info-object I passed only provides space for 1 process id. but since this code only cares about whether there
        // is more than one that's good enough. ERROR_MORE_DATA simply signals there are at least two processes running.
        // any other error probably means the handle is a regular process handle, probably caused by running MO in a job without
        // the right to break out.
        if (::GetLastError() != ERROR_MORE_DATA) {
          isJobHandle = false;
        }
      }
    }

    // keep processing events so the app doesn't appear dead
    QCoreApplication::processEvents();

    res = ::MsgWaitForMultipleObjects(1, &handle, false, 500, QS_KEY | QS_MOUSE);
  }

  if (exitCode != nullptr) {
    ::GetExitCodeProcess(processHandle, exitCode);
  }
  ::CloseHandle(processHandle);

  return res == WAIT_OBJECT_0;
}

bool OrganizerCore::onAboutToRun(const std::function<bool (const QString &)> &func)
{
  auto conn = m_AboutToRun.connect(func);
  return conn.connected();
}

bool OrganizerCore::onFinishedRun(const std::function<void (const QString &, unsigned int)> &func)
{
  auto conn = m_FinishedRun.connect(func);
  return conn.connected();
}

bool OrganizerCore::onModInstalled(const std::function<void (const QString &)> &func)
{
  auto conn = m_ModInstalled.connect(func);
  return conn.connected();
}

void OrganizerCore::refreshModList(bool saveChanges)
{
  // don't lose changes!
  if (saveChanges) {
    m_CurrentProfile->modlistWriter().writeImmediately(true);
  }
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure, m_Settings.displayForeign());

  m_CurrentProfile->refreshModStatus();

  m_ModList.notifyChange(-1);

  refreshDirectoryStructure();
}

void OrganizerCore::refreshESPList()
{
  if (m_DirectoryUpdate) {
    // don't mess up the esp list if we're currently updating the directory structure
    m_PostRefreshTasks.append([this] () { this->refreshESPList(); });
    return;
  }
  m_CurrentProfile->modlistWriter().write();

  // clear list
  try {
    m_PluginList.refresh(m_CurrentProfile->name(),
                         *m_DirectoryStructure,
                         m_CurrentProfile->getPluginsFileName(),
                         m_CurrentProfile->getLoadOrderFileName(),
                         m_CurrentProfile->getLockedOrderFileName());
  } catch (const std::exception &e) {
    reportError(tr("Failed to refresh list of esps: %1").arg(e.what()));
  }
}

void OrganizerCore::refreshBSAList()
{
  DataArchives *archives = m_GamePlugin->feature<DataArchives>();

  if (archives != nullptr) {
    m_ArchivesInit = false;

    // default archives are the ones enabled outside MO. if the list can't be found (which might
    // happen if ini files are missing) use hard-coded defaults (preferrably the same the game would use)
    m_DefaultArchives = archives->archives(m_CurrentProfile);
    if (m_DefaultArchives.length() == 0) {
      m_DefaultArchives = archives->vanillaArchives();
    }

    m_ActiveArchives.clear();

    auto iter = enabledArchives();
    m_ActiveArchives = toStringList(iter.begin(), iter.end());
    if (m_ActiveArchives.isEmpty()) {
      m_ActiveArchives = m_DefaultArchives;
    }

    if (m_UserInterface != nullptr) {
      m_UserInterface->updateBSAList(m_DefaultArchives, m_ActiveArchives);
    }

    m_ArchivesInit = true;
  }
}

void OrganizerCore::refreshLists()
{
  if ((m_CurrentProfile != nullptr) && m_DirectoryStructure->isPopulated()) {
    refreshESPList();
    refreshBSAList();
  } // no point in refreshing lists if no files have been added to the directory tree
}

void OrganizerCore::updateModActiveState(int index, bool active)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  QDir dir(modInfo->absolutePath());
  foreach (const QString &esm, dir.entryList(QStringList() << "*.esm", QDir::Files)) {
    m_PluginList.enableESP(esm, active);
  }
  int enabled = 0;
  QStringList esps = dir.entryList(QStringList() << "*.esp", QDir::Files);
  for (const QString &esp : esps) {
    const FileEntry::Ptr file = m_DirectoryStructure->findFile(ToWString(esp));

    if (active != m_PluginList.isEnabled(esp)
        && file->getAlternatives().empty()) {
      m_PluginList.enableESP(esp, active);
      ++enabled;
    }
  }
  if (active && (enabled > 1)) {
    MessageDialog::showMessage(tr("Multiple esps activated, please check that they don't conflict."), qApp->activeWindow());
  }
  m_PluginList.refreshLoadOrder();
  // immediately save affected lists
  m_PluginListsWriter.writeImmediately(false);
}

void OrganizerCore::updateModInDirectoryStructure(unsigned int index, ModInfo::Ptr modInfo)
{
  // add files of the bsa to the directory structure
  m_DirectoryRefresher.addModFilesToStructure(m_DirectoryStructure
                                              , modInfo->name()
                                              , m_CurrentProfile->getModPriority(index)
                                              , modInfo->absolutePath()
                                              , modInfo->stealFiles()
                                              );
  DirectoryRefresher::cleanStructure(m_DirectoryStructure);
  // need to refresh plugin list now so we can activate esps
  refreshESPList();
  // activate all esps of the specified mod so the bsas get activated along with it
  updateModActiveState(index, true);
  // now we need to refresh the bsa list and save it so there is no confusion about what archives are avaiable and active
  refreshBSAList();
  if (m_UserInterface != nullptr) {
    m_UserInterface->archivesWriter().write();
  }
  m_DirectoryRefresher.setMods(m_CurrentProfile->getActiveMods(), enabledArchives());

  // finally also add files from bsas to the directory structure
  m_DirectoryRefresher.addModBSAToStructure(m_DirectoryStructure
                                            , modInfo->name()
                                            , m_CurrentProfile->getModPriority(index)
                                            , modInfo->absolutePath()
                                            , modInfo->archives()
                                            );
}

void OrganizerCore::requestDownload(const QUrl &url, QNetworkReply *reply)
{
  if (m_PluginContainer != nullptr) {
    for (IPluginModPage *modPage : m_PluginContainer->plugins<MOBase::IPluginModPage>()) {
      ModRepositoryFileInfo *fileInfo = new ModRepositoryFileInfo();
      if (modPage->handlesDownload(url, reply->url(), *fileInfo)) {
        fileInfo->repository = modPage->name();
        m_DownloadManager.addDownload(reply, fileInfo);
        return;
      }
    }
  }

  // no mod found that could handle the download. Is it a nexus mod?
  if (url.host() == "www.nexusmods.com") {
    int modID = 0;
    int fileID = 0;
    QRegExp modExp("mods/(\\d+)");
    if (modExp.indexIn(url.toString()) != -1) {
      modID = modExp.cap(1).toInt();
    }
    QRegExp fileExp("fid=(\\d+)");
    if (fileExp.indexIn(reply->url().toString()) != -1) {
      fileID = fileExp.cap(1).toInt();
    }
    m_DownloadManager.addDownload(reply, new ModRepositoryFileInfo(modID, fileID));
  } else {
    if (QMessageBox::question(qApp->activeWindow(), tr("Download?"),
          tr("A download has been started but no installed page plugin recognizes it.\n"
             "If you download anyway no information (i.e. version) will be associated with the download.\n"
             "Continue?"),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      m_DownloadManager.addDownload(reply, new ModRepositoryFileInfo());
    }
  }
}

ModListSortProxy *OrganizerCore::createModListProxyModel()
{
  ModListSortProxy *result = new ModListSortProxy(m_CurrentProfile, this);
  result->setSourceModel(&m_ModList);
  return result;
}

PluginListSortProxy *OrganizerCore::createPluginListProxyModel()
{
  PluginListSortProxy *result = new PluginListSortProxy(this);
  result->setSourceModel(&m_PluginList);
  return result;
}

IPluginGame *OrganizerCore::managedGame() const
{
  return m_GamePlugin;
}

std::set<QString> OrganizerCore::enabledArchives()
{
  std::set<QString> result;
  QFile archiveFile(m_CurrentProfile->getArchivesFileName());
  if (archiveFile.open(QIODevice::ReadOnly)) {
    while (!archiveFile.atEnd()) {
      result.insert(QString::fromUtf8(archiveFile.readLine()).trimmed());
    }
    archiveFile.close();
  }
  return result;
}

void OrganizerCore::refreshDirectoryStructure()
{
  if (!m_DirectoryUpdate) {
    m_CurrentProfile->modlistWriter().writeImmediately(true);

    m_DirectoryUpdate = true;
    std::vector<std::tuple<QString, QString, int> > activeModList = m_CurrentProfile->getActiveMods();

    m_DirectoryRefresher.setMods(activeModList, enabledArchives());

    QTimer::singleShot(0, &m_DirectoryRefresher, SLOT(refresh()));
  }
}

void OrganizerCore::directory_refreshed()
{
  DirectoryEntry *newStructure = m_DirectoryRefresher.getDirectoryStructure();
  Q_ASSERT(newStructure != m_DirectoryStructure);
  if (newStructure != nullptr) {
    std::swap(m_DirectoryStructure, newStructure);
    delete newStructure;
  } else {
    // TODO: don't know why this happens, this slot seems to get called twice with only one emit
    return;
  }
  m_DirectoryUpdate = false;
  if (m_CurrentProfile != nullptr) {
    refreshLists();
  }

  for (int i = 0; i < m_ModList.rowCount(); ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
    modInfo->clearCaches();
  }
  for (auto task : m_PostRefreshTasks) {
    task();
  }
}

void OrganizerCore::profileRefresh()
{
  // have to refresh mods twice (again in refreshModList), otherwise the refresh isn't complete. Not sure why
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure, m_Settings.displayForeign());
  m_CurrentProfile->refreshModStatus();

  refreshModList();
}

void OrganizerCore::modStatusChanged(unsigned int index)
{
  try {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
    if (m_CurrentProfile->modEnabled(index)) {
      updateModInDirectoryStructure(index, modInfo);
    } else {
      updateModActiveState(index, false);
      refreshESPList();
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        FilesOrigin &origin = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
        origin.enable(false);
      }
    }
    modInfo->clearCaches();

    for (unsigned int i = 0; i < m_CurrentProfile->numMods(); ++i) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      int priority = m_CurrentProfile->getModPriority(i);
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        // priorities in the directory structure are one higher because data is 0
        m_DirectoryStructure->getOriginByName(ToWString(modInfo->name())).setPriority(priority + 1);
      }
    }
    m_DirectoryStructure->getFileRegister()->sortOrigins();

    refreshLists();
  } catch (const std::exception& e) {
    reportError(tr("failed to update mod list: %1").arg(e.what()));
  }
}

void OrganizerCore::loginSuccessful(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), qApp->activeWindow());
  }
  foreach (QString url, m_PendingDownloads) {
    downloadRequestedNXM(url);
  }
  m_PendingDownloads.clear();
  for (auto task : m_PostLoginTasks) {
    task();
  }

  m_PostLoginTasks.clear();
  NexusInterface::instance()->loginCompleted();
}

void OrganizerCore::loginSuccessfulUpdate(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), qApp->activeWindow());
  }
  m_Updater.startUpdate();
}

void OrganizerCore::loginFailed(const QString &message)
{
  if (!m_PendingDownloads.isEmpty()) {
    MessageDialog::showMessage(tr("login failed: %1. Trying to download anyway").arg(message), qApp->activeWindow());
    foreach (QString url, m_PendingDownloads) {
      downloadRequestedNXM(url);
    }
    m_PendingDownloads.clear();
  } else {
    MessageDialog::showMessage(tr("login failed: %1").arg(message), qApp->activeWindow());
    m_PostLoginTasks.clear();
  }
  NexusInterface::instance()->loginCompleted();
}


void OrganizerCore::loginFailedUpdate(const QString &message)
{
  MessageDialog::showMessage(tr("login failed: %1. You need to log-in with Nexus to update MO.").arg(message), qApp->activeWindow());
}

void OrganizerCore::syncOverwrite()
{
  unsigned int overwriteIndex = ModInfo::findMod([](ModInfo::Ptr mod) -> bool {
    std::vector<ModInfo::EFlag> flags = mod->getFlags();
    return std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end(); });

  ModInfo::Ptr modInfo = ModInfo::getByIndex(overwriteIndex);
  SyncOverwriteDialog syncDialog(modInfo->absolutePath(), m_DirectoryStructure, qApp->activeWindow());
  if (syncDialog.exec() == QDialog::Accepted) {
    syncDialog.apply(QDir::fromNativeSeparators(m_Settings.getModDirectory()));
    modInfo->testValid();
    refreshDirectoryStructure();
  }
}

std::vector<unsigned int> OrganizerCore::activeProblems() const
{
  std::vector<unsigned int> problems;
  if (m_PluginList.enabledCount() > 255) {
    problems.push_back(PROBLEM_TOOMANYPLUGINS);
  }
  return problems;
}

QString OrganizerCore::shortDescription(unsigned int key) const
{
  switch (key) {
    case PROBLEM_TOOMANYPLUGINS: {
      return tr("Too many esps and esms enabled");
    } break;
    default: {
      return tr("Description missing");
    } break;
  }
}

QString OrganizerCore::fullDescription(unsigned int key) const
{
  switch (key) {
    case PROBLEM_TOOMANYPLUGINS: {
      return tr("The game doesn't allow more than 255 active plugins (including the official ones) to be loaded. You have to disable some unused plugins or "
                "merge some plugins into one. You can find a guide here: <a href=\"http://wiki.step-project.com/Guide:Merging_Plugins\">http://wiki.step-project.com/Guide:Merging_Plugins</a>");
    } break;
    default: {
      return tr("Description missing");
    } break;
  }
}

bool OrganizerCore::hasGuidedFix(unsigned int) const
{
  return false;
}

void OrganizerCore::startGuidedFix(unsigned int) const
{
}

bool OrganizerCore::saveCurrentLists()
{
  if (m_DirectoryUpdate) {
    qWarning("not saving lists during directory update");
    return false;
  }

  try {
    savePluginList();
    if (m_UserInterface != nullptr) {
      m_UserInterface->archivesWriter().write();
    }
  } catch (const std::exception &e) {
    reportError(tr("failed to save load order: %1").arg(e.what()));
  }

  return true;
}

void OrganizerCore::savePluginList()
{
  if (m_DirectoryUpdate) {
    // delay save till after directory update
    m_PostRefreshTasks.append([&] () { this->savePluginList(); });
    return;
  }
  m_PluginList.saveTo(m_CurrentProfile->getPluginsFileName(),
                      m_CurrentProfile->getLoadOrderFileName(),
                      m_CurrentProfile->getLockedOrderFileName(),
                      m_CurrentProfile->getDeleterFileName(),
                      m_Settings.hideUncheckedPlugins());
  m_PluginList.saveLoadOrder(*m_DirectoryStructure);
}

void OrganizerCore::prepareStart() {
  if (m_CurrentProfile == nullptr) {
    return;
  }
  m_CurrentProfile->modlistWriter().write();
  m_CurrentProfile->createTweakedIniFile();
  saveCurrentLists();
  m_Settings.setupLoadMechanism();
  storeSettings();
}

