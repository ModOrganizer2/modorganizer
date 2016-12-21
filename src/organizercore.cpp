#include "organizercore.h"

#include "delayedfilewriter.h"
#include "guessedvalue.h"
#include "imodinterface.h"
#include "imoinfo.h"
#include "iplugingame.h"
#include "iuserinterface.h"
#include "loadmechanism.h"
#include "messagedialog.h"
#include "modlistsortproxy.h"
#include "modrepositoryfileinfo.h"
#include "nexusinterface.h"
#include "plugincontainer.h"
#include "pluginlistsortproxy.h"
#include "profile.h"
#include "logbuffer.h"
#include "credentialsdialog.h"
#include "filedialogmemory.h"
#include "modinfodialog.h"
#include "spawn.h"
#include "syncoverwritedialog.h"
#include "nxmaccessmanager.h"
#include <ipluginmodpage.h>
#include <dataarchives.h>
#include <localsavegames.h>
#include <directoryentry.h>
#include <scopeguard.h>
#include <utility.h>
#include <usvfs.h>
#include "appconfig.h"
#include <report.h>
#include <questionboxmemory.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <QtDebug>
#include <QtGlobal> // for qPrintable, etc

#include <Psapi.h>
#include <tchar.h> // for _tcsicmp

#include <limits.h>
#include <stddef.h>
#include <string.h> // for memset, wcsrchr

#include <exception>
#include <functional>
#include <boost/algorithm/string/predicate.hpp>
#include <memory>
#include <set>
#include <string> //for wstring
#include <tuple>
#include <utility>


using namespace MOShared;
using namespace MOBase;

static bool isOnline()
{
  QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

  bool connected = false;
  for (auto iter = interfaces.begin(); iter != interfaces.end() && !connected;
       ++iter) {
    if ((iter->flags() & QNetworkInterface::IsUp)
        && (iter->flags() & QNetworkInterface::IsRunning)
        && !(iter->flags() & QNetworkInterface::IsLoopBack)) {
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

static bool renameFile(const QString &oldName, const QString &newName,
                       bool overwrite = true)
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
  QSettings steamSettings("HKEY_CURRENT_USER\\Software\\Valve\\Steam",
                          QSettings::NativeFormat);
  QString exe = steamSettings.value("SteamExe", "").toString();
  if (!exe.isEmpty()) {
    exe = QString("\"%1\"").arg(exe);
    // See if username and password supplied. If so, pass them into steam.
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
      QMessageBox::information(
          widget, QObject::tr("Waiting"),
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
  : m_UserInterface(nullptr)
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

  MOBase::QuestionBoxMemory::init(initSettings.fileName());

  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
  m_InstallationManager.setDownloadDirectory(m_Settings.getDownloadDirectory());

  connect(&m_DownloadManager, SIGNAL(downloadSpeed(QString, int)), this,
          SLOT(downloadSpeed(QString, int)));
  connect(&m_DirectoryRefresher, SIGNAL(refreshed()), this,
          SLOT(directory_refreshed()));

  connect(&m_ModList, SIGNAL(removeOrigin(QString)), this,
          SLOT(removeOrigin(QString)));

  connect(NexusInterface::instance()->getAccessManager(),
          SIGNAL(loginSuccessful(bool)), this, SLOT(loginSuccessful(bool)));
  connect(NexusInterface::instance()->getAccessManager(),
          SIGNAL(loginFailed(QString)), this, SLOT(loginFailed(QString)));

  // This seems awfully imperative
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          &m_Settings, SLOT(managedGameChanged(MOBase::IPluginGame const *)));
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          &m_DownloadManager,
          SLOT(managedGameChanged(MOBase::IPluginGame const *)));
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          &m_PluginList, SLOT(managedGameChanged(MOBase::IPluginGame const *)));
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          NexusInterface::instance(),
          SLOT(managedGameChanged(MOBase::IPluginGame const *)));

  connect(&m_PluginList, &PluginList::writePluginsList, &m_PluginListsWriter,
          &DelayedFileWriterBase::write);

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
  //  NexusInterface::instance()->cleanup();

  delete m_DirectoryStructure;
}

QString OrganizerCore::commitSettings(const QString &iniFile)
{
  if (!shellRename(iniFile + ".new", iniFile, true, qApp->activeWindow())) {
    DWORD err = ::GetLastError();
    // make a second attempt using qt functions but if that fails print the
    // error from the first attempt
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
    settings.setValue("selected_profile",
                      m_CurrentProfile->name().toUtf8().constData());
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
    settings.setValue("custom", item.isCustom());
    settings.setValue("toolbar", item.isShownOnToolbar());
    settings.setValue("ownicon", item.usesOwnIcon());
    if (item.isCustom()) {
      settings.setValue("binary", item.m_BinaryInfo.absoluteFilePath());
      settings.setValue("arguments", item.m_Arguments);
      settings.setValue("workingDirectory", item.m_WorkingDirectory);
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
  QString iniFile = qApp->property("dataPath").toString() + "/"
                    + QString::fromStdWString(AppConfig::iniFileName());
  if (QFileInfo(iniFile).exists()) {
    if (!shellCopy(iniFile, iniFile + ".new", true, qApp->activeWindow())) {
      QMessageBox::critical(
          qApp->activeWindow(), tr("Failed to write settings"),
          tr("An error occured trying to update MO settings to %1: %2")
              .arg(iniFile, windowsErrorString(::GetLastError())));
      return;
    }
  }

  QString writeTarget = iniFile + ".new";

  QSettings::Status result = storeSettings(writeTarget);

  if (result == QSettings::NoError) {
    QString errMsg = commitSettings(iniFile);
    if (!errMsg.isEmpty()) {
      qWarning("settings file not writable, may be locked by another "
               "application, trying direct write");
      writeTarget = iniFile;
      result = storeSettings(iniFile);
    }
  }
  if (result != QSettings::NoError) {
    QString reason = result == QSettings::AccessError
                         ? tr("File is write protected")
                         : result == QSettings::FormatError
                               ? tr("Invalid file format (probably a bug)")
                               : tr("Unknown error %1").arg(result);
    QMessageBox::critical(
        qApp->activeWindow(), tr("Failed to write settings"),
        tr("An error occured trying to write back MO settings to %1: %2")
            .arg(writeTarget, reason));
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
    if (!::EnumProcesses(processIDs.get(),
                         static_cast<DWORD>(currentSize) * sizeof(DWORD),
                         &bytesReturned)) {
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
      HANDLE process = ::OpenProcess(
          PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIDs[i]);

      if (process != nullptr) {
        HMODULE module;
        DWORD ignore;

        // first module in a process is always the binary
        if (EnumProcessModules(process, &module, sizeof(HMODULE) * 1,
                               &ignore)) {
          ::GetModuleBaseName(process, module, processName, MAX_PATH);
          if ((_tcsicmp(processName, TEXT("steam.exe")) == 0)
              || (_tcsicmp(processName, TEXT("steamservice.exe")) == 0)) {
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

  m_ExecutablesList.init(managedGame());

  qDebug("setting up configured executables");

  int numCustomExecutables = settings.beginReadArray("customExecutables");
  for (int i = 0; i < numCustomExecutables; ++i) {
    settings.setArrayIndex(i);

    Executable::Flags flags;
    if (settings.value("custom", true).toBool())
      flags |= Executable::CustomExecutable;
    if (settings.value("toolbar", false).toBool())
      flags |= Executable::ShowInToolbar;
    if (settings.value("ownicon", false).toBool())
      flags |= Executable::UseApplicationIcon;

    m_ExecutablesList.addExecutable(
        settings.value("title").toString(), settings.value("binary").toString(),
        settings.value("arguments").toString(),
        settings.value("workingDirectory", "").toString(),
        settings.value("steamAppID", "").toString(), flags);
  }

  settings.endArray();

  // TODO this has nothing to do with executables list move to an appropriate
  // function!
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure,
                          m_Settings.displayForeign(), managedGame());
}

void OrganizerCore::setUserInterface(IUserInterface *userInterface,
                                     QWidget *widget)
{
  storeSettings();

  m_UserInterface = userInterface;

  if (widget != nullptr) {
    connect(&m_ModList, SIGNAL(modlist_changed(QModelIndex, int)), widget,
            SLOT(modlistChanged(QModelIndex, int)));
    connect(&m_ModList, SIGNAL(showMessage(QString)), widget,
            SLOT(showMessage(QString)));
    connect(&m_ModList, SIGNAL(modRenamed(QString, QString)), widget,
            SLOT(modRenamed(QString, QString)));
    connect(&m_ModList, SIGNAL(modUninstalled(QString)), widget,
            SLOT(modRemoved(QString)));
    connect(&m_ModList, SIGNAL(removeSelectedMods()), widget,
            SLOT(removeMod_clicked()));
    connect(&m_ModList, SIGNAL(requestColumnSelect(QPoint)), widget,
            SLOT(displayColumnSelection(QPoint)));
    connect(&m_ModList, SIGNAL(fileMoved(QString, QString, QString)), widget,
            SLOT(fileMoved(QString, QString, QString)));
    connect(&m_ModList, SIGNAL(modorder_changed()), widget,
            SLOT(modorder_changed()));
    connect(&m_DownloadManager, SIGNAL(showMessage(QString)), widget,
            SLOT(showMessage(QString)));
  }

  m_InstallationManager.setParentWidget(widget);
  m_Updater.setUserInterface(widget);

  if (userInterface != nullptr) {
    // this currently wouldn't work reliably if the ui isn't initialized yet to
    // display the result
    if (isOnline() && !m_Settings.offlineMode()) {
      m_Updater.testForUpdate();
    } else {
      qDebug("user doesn't seem to be connected to the internet");
    }
  }
}

void OrganizerCore::connectPlugins(PluginContainer *container)
{
  m_DownloadManager.setSupportedExtensions(
      m_InstallationManager.getSupportedExtensions());
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
  m_GamePlugin      = nullptr;
  m_PluginContainer = nullptr;
}

void OrganizerCore::setManagedGame(MOBase::IPluginGame *game)
{
  m_GameName   = game->gameName();
  m_GamePlugin = game;
  qApp->setProperty("managed_game", QVariant::fromValue(m_GamePlugin));
  emit managedGameChanged(m_GamePlugin);
}

Settings &OrganizerCore::settings()
{
  return m_Settings;
}

bool OrganizerCore::nexusLogin(bool retry)
{
  NXMAccessManager *accessManager
      = NexusInterface::instance()->getAccessManager();

  if ((accessManager->loginAttempted() || accessManager->loggedIn())
      && !retry) {
    // previous attempt, maybe even successful
    return false;
  } else {
    QString username, password;
    if ((!retry && m_Settings.getNexusLogin(username, password))
        || (m_AskForNexusPW && queryLogin(username, password))) {
      // credentials stored or user entered them manually
      qDebug("attempt login with username %s", qPrintable(username));
      accessManager->login(username, password);
      return true;
    } else {
      // no credentials stored and user didn't enter them
      accessManager->refuseLogin();
      return false;
    }
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

void OrganizerCore::downloadRequested(QNetworkReply *reply, int modID,
                                      const QString &fileName)
{
  try {
    if (m_DownloadManager.addDownload(reply, QStringList(), fileName, modID, 0,
                                      new ModRepositoryFileInfo(modID))) {
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

bool OrganizerCore::createDirectory(const QString &path) {
  if (!QDir(path).exists() && !QDir().mkpath(path)) {
    QMessageBox::critical(nullptr, QObject::tr("Error"),
                          QObject::tr("Failed to create \"%1\". Your user "
                                      "account probably lacks permission.")
                              .arg(QDir::toNativeSeparators(path)));
    return false;
  } else {
    return true;
  }
}

bool OrganizerCore::bootstrap() {
  return createDirectory(m_Settings.getProfileDirectory()) &&
         createDirectory(m_Settings.getModDirectory()) &&
         createDirectory(m_Settings.getDownloadDirectory()) &&
         createDirectory(m_Settings.getOverwriteDirectory());
}

void OrganizerCore::createDefaultProfile()
{
  QString profilesPath = settings().getProfileDirectory();
  if (QDir(profilesPath).entryList(QDir::AllDirs | QDir::NoDotAndDotDot).size()
      == 0) {
    Profile newProf("Default", managedGame(), false);
  }
}

void OrganizerCore::prepareVFS()
{
  m_USVFS.updateMapping(fileMapping(m_CurrentProfile->name(), QString()));
}

void OrganizerCore::setLogLevel(int logLevel) {
  m_USVFS.setLogLevel(logLevel);
}

void OrganizerCore::setCurrentProfile(const QString &profileName)
{
  if ((m_CurrentProfile != nullptr)
      && (profileName == m_CurrentProfile->name())) {
    return;
  }

  QDir profileBaseDir(settings().getProfileDirectory());
  QString profileDir = profileBaseDir.absoluteFilePath(profileName);

  if (!QDir(profileDir).exists()) {
    // selected profile doesn't exist. Ensure there is at least one profile,
    // then pick any one
    createDefaultProfile();

    profileDir = profileBaseDir.absoluteFilePath(
        profileBaseDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot).at(0));
  }

  Profile *newProfile = new Profile(QDir(profileDir), managedGame());

  delete m_CurrentProfile;
  m_CurrentProfile = newProfile;
  m_ModList.setProfile(newProfile);

  if (m_CurrentProfile->invalidationActive(nullptr)) {
    m_CurrentProfile->activateInvalidation();
  } else {
    m_CurrentProfile->deactivateInvalidation();
  }

  connect(m_CurrentProfile, SIGNAL(modStatusChanged(uint)), this,
          SLOT(modStatusChanged(uint)));
  refreshDirectoryStructure();
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

MOBase::IModInterface *OrganizerCore::getMod(const QString &name) const
{
  unsigned int index = ModInfo::getIndex(name);
  return index == UINT_MAX ? nullptr : ModInfo::getByIndex(index).data();
}

MOBase::IModInterface *OrganizerCore::createMod(GuessedValue<QString> &name)
{
  bool merge = false;
  if (!m_InstallationManager.testOverwrite(name, &merge)) {
    return nullptr;
  }

  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());

  QString targetDirectory
      = QDir::fromNativeSeparators(m_Settings.getModDirectory())
            .append("/")
            .append(name);

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

  return ModInfo::createFrom(QDir(targetDirectory), &m_DirectoryStructure)
      .data();
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

void OrganizerCore::modDataChanged(MOBase::IModInterface *)
{
  refreshModList(false);
}

QVariant OrganizerCore::pluginSetting(const QString &pluginName,
                                      const QString &key) const
{
  return m_Settings.pluginSetting(pluginName, key);
}

void OrganizerCore::setPluginSetting(const QString &pluginName,
                                     const QString &key, const QVariant &value)
{
  m_Settings.setPluginSetting(pluginName, key, value);
}

QVariant OrganizerCore::persistent(const QString &pluginName,
                                   const QString &key,
                                   const QVariant &def) const
{
  return m_Settings.pluginPersistent(pluginName, key, def);
}

void OrganizerCore::setPersistent(const QString &pluginName, const QString &key,
                                  const QVariant &value, bool sync)
{
  m_Settings.setPluginPersistent(pluginName, key, value, sync);
}

QString OrganizerCore::pluginDataPath() const
{
  return qApp->applicationDirPath() + "/" + ToQString(AppConfig::pluginPath())
         + "/data";
}

MOBase::IModInterface *OrganizerCore::installMod(const QString &fileName,
                                                 const QString &initModName)
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
    MessageDialog::showMessage(tr("Installation successful"),
                               qApp->activeWindow());
    refreshModList();

    int modIndex = ModInfo::getIndex(modName);
    if (modIndex != UINT_MAX) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
      if (hasIniTweaks && (m_UserInterface != nullptr)
          && (QMessageBox::question(qApp->activeWindow(), tr("Configure Mod"),
                                    tr("This mod contains ini tweaks. Do you "
                                       "want to configure them now?"),
                                    QMessageBox::Yes | QMessageBox::No)
              == QMessageBox::Yes)) {
        m_UserInterface->displayModInformation(modInfo, modIndex,
                                               ModInfoDialog::TAB_INIFILES);
      }
      m_ModInstalled(modName);
      return modInfo.data();
    } else {
      reportError(tr("mod \"%1\" not found").arg(modName));
    }
  } else if (m_InstallationManager.wasCancelled()) {
    QMessageBox::information(qApp->activeWindow(), tr("Installation cancelled"),
                             tr("The mod was not installed completely."),
                             QMessageBox::Ok);
  }
  return nullptr;
}

void OrganizerCore::installDownload(int index)
{
  try {
    QString fileName = m_DownloadManager.getFilePath(index);
    int modID        = m_DownloadManager.getModID(index);
    int fileID       = m_DownloadManager.getFileInfo(index)->fileID;
    GuessedValue<QString> modName;

    // see if there already are mods with the specified mod id
    if (modID != 0) {
      std::vector<ModInfo::Ptr> modInfo = ModInfo::getByModID(modID);
      for (auto iter = modInfo.begin(); iter != modInfo.end(); ++iter) {
        std::vector<ModInfo::EFlag> flags = (*iter)->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP)
            == flags.end()) {
          modName.update((*iter)->name(), GUESS_PRESET);
          (*iter)->saveMeta();
        }
      }
    }

    m_CurrentProfile->writeModlistNow();

    bool hasIniTweaks = false;
    m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
    if (m_InstallationManager.install(fileName, modName, hasIniTweaks)) {
      MessageDialog::showMessage(tr("Installation successful"),
                                 qApp->activeWindow());
      refreshModList();

      int modIndex = ModInfo::getIndex(modName);
      if (modIndex != UINT_MAX) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
        modInfo->addInstalledFile(modID, fileID);

        if (hasIniTweaks && m_UserInterface != nullptr
            && (QMessageBox::question(qApp->activeWindow(), tr("Configure Mod"),
                                      tr("This mod contains ini tweaks. Do you "
                                         "want to configure them now?"),
                                      QMessageBox::Yes | QMessageBox::No)
                == QMessageBox::Yes)) {
          m_UserInterface->displayModInformation(modInfo, modIndex,
                                                 ModInfoDialog::TAB_INIFILES);
        }

        m_ModInstalled(modName);
      } else {
        reportError(tr("mod \"%1\" not found").arg(modName));
      }
      m_DownloadManager.markInstalled(index);

      emit modInstalled(modName);
    } else if (m_InstallationManager.wasCancelled()) {
      QMessageBox::information(
          qApp->activeWindow(), tr("Installation cancelled"),
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
  const FileEntry::Ptr file
      = m_DirectoryStructure->searchFile(ToWString(fileName), nullptr);
  if (file.get() != nullptr) {
    return ToQString(file->getFullPath());
  } else {
    return QString();
  }
}

QStringList OrganizerCore::listDirectories(const QString &directoryName) const
{
  QStringList result;
  DirectoryEntry *dir = m_DirectoryStructure->findSubDirectoryRecursive(
      ToWString(directoryName));
  if (dir != nullptr) {
    std::vector<DirectoryEntry *>::iterator current, end;
    dir->getSubDirectories(current, end);
    for (; current != end; ++current) {
      result.append(ToQString((*current)->getName()));
    }
  }
  return result;
}

QStringList OrganizerCore::findFiles(
    const QString &path,
    const std::function<bool(const QString &)> &filter) const
{
  QStringList result;
  DirectoryEntry *dir
      = m_DirectoryStructure->findSubDirectoryRecursive(ToWString(path));
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
  const FileEntry::Ptr file = m_DirectoryStructure->searchFile(
      ToWString(QFileInfo(fileName).fileName()), nullptr);

  if (file.get() != nullptr) {
    result.append(ToQString(
        m_DirectoryStructure->getOriginByID(file->getOrigin()).getName()));
    foreach (int i, file->getAlternatives()) {
      result.append(
          ToQString(m_DirectoryStructure->getOriginByID(i).getName()));
    }
  } else {
    qDebug("%s not found", qPrintable(fileName));
  }
  return result;
}

QList<MOBase::IOrganizer::FileInfo> OrganizerCore::findFileInfos(
    const QString &path,
    const std::function<bool(const MOBase::IOrganizer::FileInfo &)> &filter)
    const
{
  QList<IOrganizer::FileInfo> result;
  DirectoryEntry *dir
      = m_DirectoryStructure->findSubDirectoryRecursive(ToWString(path));
  if (dir != nullptr) {
    std::vector<FileEntry::Ptr> files = dir->getFiles();
    foreach (FileEntry::Ptr file, files) {
      IOrganizer::FileInfo info;
      info.filePath    = ToQString(file->getFullPath());
      bool fromArchive = false;
      info.origins.append(ToQString(
          m_DirectoryStructure->getOriginByID(file->getOrigin(fromArchive))
              .getName()));
      info.archive = fromArchive ? ToQString(file->getArchive()) : "";
      foreach (int idx, file->getAlternatives()) {
        info.origins.append(
            ToQString(m_DirectoryStructure->getOriginByID(idx).getName()));
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

QStringList OrganizerCore::modsSortedByProfilePriority() const
{
  QStringList res;
  for (unsigned int i = 0; i < currentProfile()->numRegularMods(); ++i) {
    int modIndex = currentProfile()->modIndexByPriority(i);
    res.push_back(ModInfo::getByIndex(modIndex)->name());
  }
  return res;
}

void OrganizerCore::spawnBinary(const QFileInfo &binary, const QString &arguments, const QDir &currentDirectory, const QString &steamAppID, const QString &customOverwrite)
{
  if (m_UserInterface != nullptr) {
    m_UserInterface->lock();
  }
  ON_BLOCK_EXIT([&] () {
    if (m_UserInterface != nullptr) { m_UserInterface->unlock(); }
  });

  HANDLE processHandle = spawnBinaryDirect(binary, arguments, m_CurrentProfile->name(), currentDirectory, steamAppID, customOverwrite);
  if (processHandle != INVALID_HANDLE_VALUE) {
    DWORD processExitCode;
    (void)waitForProcessCompletion(processHandle, &processExitCode);

    refreshDirectoryStructure();
    // need to remove our stored load order because it may be outdated if a foreign tool changed the
    // file time. After removing that file, refreshESPList will use the file time as the order
    if (managedGame()->loadOrderMechanism() == IPluginGame::LoadOrderMechanism::FileTime) {
      qDebug("removing loadorder.txt");
      QFile::remove(m_CurrentProfile->getLoadOrderFileName());
    }
    refreshDirectoryStructure();

    refreshESPList();
    savePluginList();

    //These callbacks should not fiddle with directoy structure and ESPs.
    m_FinishedRun(binary.absoluteFilePath(), processExitCode);
  }
}

HANDLE OrganizerCore::spawnBinaryDirect(const QFileInfo &binary,
                                        const QString &arguments,
                                        const QString &profileName,
                                        const QDir &currentDirectory,
                                        const QString &steamAppID,
                                        const QString &customOverwrite)
{
  prepareStart();

  if (!binary.exists()) {
    reportError(
        tr("Executable \"%1\" not found").arg(binary.absoluteFilePath()));
    return INVALID_HANDLE_VALUE;
  }

  if (!steamAppID.isEmpty()) {
    ::SetEnvironmentVariableW(L"SteamAPPId", ToWString(steamAppID).c_str());
  } else {
    ::SetEnvironmentVariableW(L"SteamAPPId",
                              ToWString(m_Settings.getSteamAppID()).c_str());
  }

  // This could possibly be extracted somewhere else but it's probably for when
  // we have more than one provider of game registration.
  if ((QFileInfo(
           managedGame()->gameDirectory().absoluteFilePath("steam_api.dll"))
           .exists()
       || QFileInfo(managedGame()->gameDirectory().absoluteFilePath(
                        "steam_api64.dll"))
              .exists())
      && (m_Settings.getLoadMechanism() == LoadMechanism::LOAD_MODORGANIZER)) {
    if (!testForSteam()) {
      QWidget *window = qApp->activeWindow();
      if ((window != nullptr) && (!window->isVisible())) {
        window = nullptr;
      }
      if (QuestionBoxMemory::query(window, "steamQuery", binary.fileName(),
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
    try {
      m_USVFS.updateMapping(fileMapping(profileName, customOverwrite));
    } catch (const std::exception &e) {
      QMessageBox::warning(qApp->activeWindow(), tr("Error"), e.what());
      return INVALID_HANDLE_VALUE;
    }

    QString modsPath = settings().getModDirectory();

    QString binPath = binary.absoluteFilePath();
    if (binPath.startsWith(modsPath, Qt::CaseInsensitive)) {
      // binary was installed as a MO mod. Need to start it through a (hooked)
      // proxy to ensure pathes are correct

      QString cwdPath = currentDirectory.absolutePath();

      int binOffset       = binPath.indexOf('/', modsPath.length() + 1);
      int cwdOffset       = cwdPath.indexOf('/', modsPath.length() + 1);
      QString dataBinPath = m_GamePlugin->dataDirectory().absolutePath()
                            + binPath.mid(binOffset, -1);
      QString dataCwd = m_GamePlugin->dataDirectory().absolutePath()
                        + cwdPath.mid(cwdOffset, -1);
      QString cmdline
          = QString("launch \"%1\" \"%2\" %3")
                .arg(QDir::toNativeSeparators(dataCwd),
                     QDir::toNativeSeparators(dataBinPath), arguments);
      return startBinary(QFileInfo(QCoreApplication::applicationFilePath()),
                         cmdline, QCoreApplication::applicationDirPath(), true);
    } else {
      return startBinary(binary, arguments, currentDirectory, true);
    }
  } else {
    qDebug("start of \"%s\" canceled by plugin",
           qPrintable(binary.absoluteFilePath()));
    return INVALID_HANDLE_VALUE;
  }
}

HANDLE OrganizerCore::startApplication(const QString &executable,
                                       const QStringList &args,
                                       const QString &cwd,
                                       const QString &profile)
{
  QFileInfo binary;
  QString arguments        = args.join(" ");
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
  QString customOverwrite;
  if (executable.contains('\\') || executable.contains('/')) {
    // file path

    binary = QFileInfo(executable);
    if (binary.isRelative()) {
      // relative path, should be relative to game directory
      binary = QFileInfo(
          managedGame()->gameDirectory().absoluteFilePath(executable));
    }
    if (cwd.length() == 0) {
      currentDirectory = binary.absolutePath();
    }
    try {
      const Executable &exe = m_ExecutablesList.findByBinary(binary);
      steamAppID = exe.m_SteamAppID;
      customOverwrite
          = m_CurrentProfile->setting("custom_overwrites", exe.m_Title)
                .toString();
    } catch (const std::runtime_error &) {
      // nop
    }
  } else {
    // only a file name, search executables list
    try {
      const Executable &exe = m_ExecutablesList.find(executable);
      steamAppID = exe.m_SteamAppID;
      customOverwrite
          = m_CurrentProfile->setting("custom_overwrites", exe.m_Title)
                .toString();
      if (arguments == "") {
        arguments = exe.m_Arguments;
      }
      binary = exe.m_BinaryInfo;
      if (cwd.length() == 0) {
        currentDirectory = exe.m_WorkingDirectory;
      }
    } catch (const std::runtime_error &) {
      qWarning("\"%s\" not set up as executable",
               executable.toUtf8().constData());
      binary = QFileInfo(executable);
    }
  }

  return spawnBinaryDirect(binary, arguments, profileName, currentDirectory,
                           steamAppID, customOverwrite);
}

bool OrganizerCore::waitForApplication(HANDLE handle, LPDWORD exitCode)
{
  if (m_UserInterface != nullptr) {
    m_UserInterface->lock();
  }

  ON_BLOCK_EXIT([&] () {
    if (m_UserInterface != nullptr) {
      m_UserInterface->unlock();
    } });
  return waitForProcessCompletion(handle, exitCode);
}

bool OrganizerCore::waitForProcessCompletion(HANDLE handle, LPDWORD exitCode)
{
  HANDLE processHandle = handle;

  static const DWORD maxCount = 5;
  size_t numProcesses         = maxCount;
  LPDWORD processes = new DWORD[maxCount];

  DWORD currentProcess = 0UL;
  bool tryAgain = true;

  DWORD res;
  // Wait for a an event on the handle, a key press, mouse click or timeout
  //TODO: Remove MOBase::isOneOf from this query as it was always returning true.
  while (
      res = ::MsgWaitForMultipleObjects(1, &handle, false, 500,
                                        QS_KEY | QS_MOUSE),
      ((res != WAIT_FAILED) && (res != WAIT_OBJECT_0)) &&
       ((m_UserInterface == nullptr) || !m_UserInterface->unlockClicked())) {

    if (!::GetVFSProcessList(&numProcesses, processes)) {
      break;
    }

    bool found = false;
    size_t count =
        std::min<size_t>(static_cast<size_t>(maxCount), numProcesses);
    for (size_t i = 0; i < count; ++i) {
      std::wstring processName = getProcessName(processes[i]);
      if (!boost::starts_with(processName, L"ModOrganizer.exe")) {
        currentProcess = processes[i];
        m_UserInterface->setProcessName(QString::fromStdWString(processName));
        processHandle = ::OpenProcess(SYNCHRONIZE, FALSE, currentProcess);
        found = true;
      }
    }
    if (!found) {
      // it's possible the previous process has deregistered before
      // the new one has registered, so we should try one more time
      // with a little delay
      if (tryAgain) {
        tryAgain = false;
        QThread::msleep(500);
        continue;
      } else {
        break;
      }
    } else {
      tryAgain = true;
    }

    // keep processing events so the app doesn't appear dead
    QCoreApplication::processEvents();
  }

  if (exitCode != nullptr) {
    //This is actually wrong if the process we started finished before we
    //got the event and so we end up with a job handle.
    if (! ::GetExitCodeProcess(processHandle, exitCode))
    {
      DWORD error = ::GetLastError();
      qDebug() << "Failed to get process exit code: Error " << error;
    }
  }

  ::CloseHandle(processHandle);
  if (handle != processHandle) {
    ::CloseHandle(handle);
  }

  return res == WAIT_OBJECT_0;
}

bool OrganizerCore::onAboutToRun(
    const std::function<bool(const QString &)> &func)
{
  auto conn = m_AboutToRun.connect(func);
  return conn.connected();
}

bool OrganizerCore::onFinishedRun(
    const std::function<void(const QString &, unsigned int)> &func)
{
  auto conn = m_FinishedRun.connect(func);
  return conn.connected();
}

bool OrganizerCore::onModInstalled(
    const std::function<void(const QString &)> &func)
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
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure,
                          m_Settings.displayForeign(), managedGame());

  m_CurrentProfile->refreshModStatus();

  m_ModList.notifyChange(-1);

  refreshDirectoryStructure();
}

void OrganizerCore::refreshESPList()
{
  if (m_DirectoryUpdate) {
    // don't mess up the esp list if we're currently updating the directory
    // structure
    m_PostRefreshTasks.append([this]() {
      this->refreshESPList();
    });
    return;
  }
  m_CurrentProfile->modlistWriter().write();

  // clear list
  try {
    m_PluginList.refresh(m_CurrentProfile->name(), *m_DirectoryStructure,
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

    // default archives are the ones enabled outside MO. if the list can't be
    // found (which might
    // happen if ini files are missing) use hard-coded defaults (preferrably the
    // same the game would use)
    m_DefaultArchives = archives->archives(m_CurrentProfile);
    if (m_DefaultArchives.length() == 0) {
      m_DefaultArchives = archives->vanillaArchives();
    }

    m_ActiveArchives.clear();

    auto iter        = enabledArchives();
    m_ActiveArchives = toStringList(iter.begin(), iter.end());
    if (m_ActiveArchives.isEmpty()) {
      m_ActiveArchives = m_DefaultArchives;
    }

    m_ArchivesInit = true;
  }
}

void OrganizerCore::refreshLists()
{
  if ((m_CurrentProfile != nullptr) && m_DirectoryStructure->isPopulated()) {
    refreshESPList();
    refreshBSAList();
  } // no point in refreshing lists if no files have been added to the directory
    // tree
}

void OrganizerCore::updateModActiveState(int index, bool active)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  QDir dir(modInfo->absolutePath());
  for (const QString &esm :
       dir.entryList(QStringList() << "*.esm", QDir::Files)) {
    m_PluginList.enableESP(esm, active);
  }
  int enabled      = 0;
  QStringList esps = dir.entryList(QStringList() << "*.esp", QDir::Files);
  for (const QString &esp : esps) {
    const FileEntry::Ptr file = m_DirectoryStructure->findFile(ToWString(esp));
    if (file.get() == nullptr) {
      qWarning("failed to activate %s", qPrintable(esp));
      continue;
    }

    if (active != m_PluginList.isEnabled(esp)
        && file->getAlternatives().empty()) {
      m_PluginList.enableESP(esp, active);
      ++enabled;
    }
  }
  if (active && (enabled > 1)) {
    MessageDialog::showMessage(
        tr("Multiple esps activated, please check that they don't conflict."),
        qApp->activeWindow());
  }
  m_PluginList.refreshLoadOrder();
  // immediately save affected lists
  m_PluginListsWriter.writeImmediately(false);
}

void OrganizerCore::updateModInDirectoryStructure(unsigned int index,
                                                  ModInfo::Ptr modInfo)
{
  // add files of the bsa to the directory structure
  m_DirectoryRefresher.addModFilesToStructure(
      m_DirectoryStructure, modInfo->name(),
      m_CurrentProfile->getModPriority(index), modInfo->absolutePath(),
      modInfo->stealFiles());
  DirectoryRefresher::cleanStructure(m_DirectoryStructure);
  // need to refresh plugin list now so we can activate esps
  refreshESPList();
  // activate all esps of the specified mod so the bsas get activated along with
  // it
  updateModActiveState(index, true);
  // now we need to refresh the bsa list and save it so there is no confusion
  // about what archives are avaiable and active
  refreshBSAList();

  std::vector<QString> archives = enabledArchives();
  m_DirectoryRefresher.setMods(
      m_CurrentProfile->getActiveMods(),
      std::set<QString>(archives.begin(), archives.end()));

  // finally also add files from bsas to the directory structure
  m_DirectoryRefresher.addModBSAToStructure(
      m_DirectoryStructure, modInfo->name(),
      m_CurrentProfile->getModPriority(index), modInfo->absolutePath(),
      modInfo->archives());
}

void OrganizerCore::requestDownload(const QUrl &url, QNetworkReply *reply)
{
  if (m_PluginContainer != nullptr) {
    for (IPluginModPage *modPage :
         m_PluginContainer->plugins<MOBase::IPluginModPage>()) {
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
    int modID  = 0;
    int fileID = 0;
    QRegExp modExp("mods/(\\d+)");
    if (modExp.indexIn(url.toString()) != -1) {
      modID = modExp.cap(1).toInt();
    }
    QRegExp fileExp("fid=(\\d+)");
    if (fileExp.indexIn(reply->url().toString()) != -1) {
      fileID = fileExp.cap(1).toInt();
    }
    m_DownloadManager.addDownload(reply,
                                  new ModRepositoryFileInfo(modID, fileID));
  } else {
    if (QMessageBox::question(qApp->activeWindow(), tr("Download?"),
                              tr("A download has been started but no installed "
                                 "page plugin recognizes it.\n"
                                 "If you download anyway no information (i.e. "
                                 "version) will be associated with the "
                                 "download.\n"
                                 "Continue?"),
                              QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
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

IPluginGame const *OrganizerCore::managedGame() const
{
  return m_GamePlugin;
}

std::vector<QString> OrganizerCore::enabledArchives()
{
  std::vector<QString> result;
  QFile archiveFile(m_CurrentProfile->getArchivesFileName());
  if (archiveFile.open(QIODevice::ReadOnly)) {
    while (!archiveFile.atEnd()) {
      result.push_back(QString::fromUtf8(archiveFile.readLine()).trimmed());
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
    std::vector<std::tuple<QString, QString, int>> activeModList
        = m_CurrentProfile->getActiveMods();
    auto archives = enabledArchives();
    m_DirectoryRefresher.setMods(
        activeModList, std::set<QString>(archives.begin(), archives.end()));

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
    // TODO: don't know why this happens, this slot seems to get called twice
    // with only one emit
    return;
  }
  m_DirectoryUpdate = false;

  for (int i = 0; i < m_ModList.rowCount(); ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
    modInfo->clearCaches();
  }
  for (auto task : m_PostRefreshTasks) {
    task();
  }
  m_PostRefreshTasks.clear();

  if (m_CurrentProfile != nullptr) {
    refreshLists();
  }
}

void OrganizerCore::profileRefresh()
{
  // have to refresh mods twice (again in refreshModList), otherwise the refresh
  // isn't complete. Not sure why
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure,
                          m_Settings.displayForeign(), managedGame());
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
        FilesOrigin &origin
            = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
        origin.enable(false);
      }
    }
    modInfo->clearCaches();

    for (unsigned int i = 0; i < m_CurrentProfile->numMods(); ++i) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      int priority = m_CurrentProfile->getModPriority(i);
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        // priorities in the directory structure are one higher because data is
        // 0
        m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()))
            .setPriority(priority + 1);
      }
    }
    m_DirectoryStructure->getFileRegister()->sortOrigins();

    refreshLists();
  } catch (const std::exception &e) {
    reportError(tr("failed to update mod list: %1").arg(e.what()));
  }
}

void OrganizerCore::loginSuccessful(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), qApp->activeWindow());
  }
  for (QString url : m_PendingDownloads) {
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
  if (QMessageBox::question(qApp->activeWindow(), tr("Login failed"),
                            tr("Login failed, try again?"))
      == QMessageBox::Yes) {
    if (nexusLogin(true)) {
      return;
    }
  }

  if (!m_PendingDownloads.isEmpty()) {
    MessageDialog::showMessage(
        tr("login failed: %1. Download will not be associated with an account")
            .arg(message),
        qApp->activeWindow());
    for (QString url : m_PendingDownloads) {
      downloadRequestedNXM(url);
    }
    m_PendingDownloads.clear();
  } else {
    MessageDialog::showMessage(tr("login failed: %1").arg(message),
                               qApp->activeWindow());
    m_PostLoginTasks.clear();
  }
  NexusInterface::instance()->loginCompleted();
}

void OrganizerCore::loginFailedUpdate(const QString &message)
{
  MessageDialog::showMessage(
      tr("login failed: %1. You need to log-in with Nexus to update MO.")
          .arg(message),
      qApp->activeWindow());
}

void OrganizerCore::syncOverwrite()
{
  unsigned int overwriteIndex         = ModInfo::findMod([](ModInfo::Ptr mod) -> bool {
    std::vector<ModInfo::EFlag> flags = mod->getFlags();
    return std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE)
           != flags.end();
  });

  ModInfo::Ptr modInfo = ModInfo::getByIndex(overwriteIndex);
  SyncOverwriteDialog syncDialog(modInfo->absolutePath(), m_DirectoryStructure,
                                 qApp->activeWindow());
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
      return tr("The game doesn't allow more than 255 active plugins "
                "(including the official ones) to be loaded. You have to "
                "disable some unused plugins or "
                "merge some plugins into one. You can find a guide here: <a "
                "href=\"http://wiki.step-project.com/"
                "Guide:Merging_Plugins\">http://wiki.step-project.com/"
                "Guide:Merging_Plugins</a>");
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
  } catch (const std::exception &e) {
    reportError(tr("failed to save load order: %1").arg(e.what()));
  }

  return true;
}

void OrganizerCore::savePluginList()
{
  if (m_DirectoryUpdate) {
    // delay save till after directory update
    m_PostRefreshTasks.append([this]() {
      this->savePluginList();
    });
    return;
  }
  m_PluginList.saveTo(m_CurrentProfile->getLockedOrderFileName(),
                      m_CurrentProfile->getDeleterFileName(),
                      m_Settings.hideUncheckedPlugins());
  m_PluginList.saveLoadOrder(*m_DirectoryStructure);
}

void OrganizerCore::prepareStart()
{
  if (m_CurrentProfile == nullptr) {
    return;
  }
  m_CurrentProfile->modlistWriter().write();
  m_CurrentProfile->createTweakedIniFile();
  saveCurrentLists();
  m_Settings.setupLoadMechanism();
  storeSettings();
}

std::vector<Mapping> OrganizerCore::fileMapping(const QString &profileName,
                                                const QString &customOverwrite)
{
  // need to wait until directory structure
  while (m_DirectoryUpdate) {
    ::Sleep(100);
    QCoreApplication::processEvents();
  }

  IPluginGame *game  = qApp->property("managed_game").value<IPluginGame *>();
  Profile profile(QDir(m_Settings.getProfileDirectory() + "/" + profileName),
                  game);

  MappingType result;

  QString dataPath
      = QDir::toNativeSeparators(game->dataDirectory().absolutePath());

  bool overwriteActive = false;

  for (auto mod : profile.getActiveMods()) {
    if (std::get<0>(mod).compare("overwrite", Qt::CaseInsensitive) == 0) {
      continue;
    }

    unsigned int modIndex = ModInfo::getIndex(std::get<0>(mod));
    ModInfo::Ptr modPtr   = ModInfo::getByIndex(modIndex);

    bool createTarget = customOverwrite == std::get<0>(mod);

    overwriteActive |= createTarget;

    if (modPtr->isRegular()) {
      result.insert(result.end(), {QDir::toNativeSeparators(std::get<1>(mod)),
                                   dataPath, true, createTarget});
    }
  }

  if (!overwriteActive && !customOverwrite.isEmpty()) {
    throw MyException(tr("The designated write target \"%1\" is not enabled.")
                          .arg(customOverwrite));
  }

  if (m_CurrentProfile->localSavesEnabled()) {
    LocalSavegames *localSaves = game->feature<LocalSavegames>();
    if (localSaves != nullptr) {
      MappingType saveMap
          = localSaves->mappings(currentProfile()->absolutePath() + "/saves");
      result.reserve(result.size() + saveMap.size());
      result.insert(result.end(), saveMap.begin(), saveMap.end());
    } else {
      qWarning("local save games not supported by this game plugin");
    }
  }

  result.insert(result.end(), {
                  QDir::toNativeSeparators(m_Settings.getOverwriteDirectory()),
                  dataPath,
                  true,
                  customOverwrite.isEmpty()
                });

  for (MOBase::IPluginFileMapper *mapper :
       m_PluginContainer->plugins<MOBase::IPluginFileMapper>()) {
    IPlugin *plugin = dynamic_cast<IPlugin *>(mapper);
    if (plugin->isActive()) {
      MappingType pluginMap = mapper->mappings();
      result.reserve(result.size() + pluginMap.size());
      result.insert(result.end(), pluginMap.begin(), pluginMap.end());
    }
  }

  return result;
}


std::vector<Mapping> OrganizerCore::fileMapping(
    const QString &dataPath, const QString &relPath, const DirectoryEntry *base,
    const DirectoryEntry *directoryEntry, int createDestination)
{
  std::vector<Mapping> result;

  for (FileEntry::Ptr current : directoryEntry->getFiles()) {
    bool isArchive = false;
    int origin = current->getOrigin(isArchive);
    if (isArchive || (origin == 0)) {
      continue;
    }

    QString originPath
        = QString::fromStdWString(base->getOriginByID(origin).getPath());
    QString fileName = QString::fromStdWString(current->getName());
//    QString fileName = ToQString(current->getName());
    QString source   = originPath + relPath + fileName;
    QString target   = dataPath + relPath + fileName;
    if (source != target) {
      result.push_back({source, target, false, false});
    }
  }

  // recurse into subdirectories
  std::vector<DirectoryEntry *>::const_iterator current, end;
  directoryEntry->getSubDirectories(current, end);
  for (; current != end; ++current) {
    int origin = (*current)->anyOrigin();

    QString originPath
        = QString::fromStdWString(base->getOriginByID(origin).getPath());
    QString dirName = QString::fromStdWString((*current)->getName());
    QString source  = originPath + relPath + dirName;
    QString target  = dataPath + relPath + dirName;

    bool writeDestination
        = (base == directoryEntry) && (origin == createDestination);

    result.push_back({source, target, true, writeDestination});
    std::vector<Mapping> subRes = fileMapping(
        dataPath, relPath + dirName + "\\", base, *current, createDestination);
    result.insert(result.end(), subRes.begin(), subRes.end());
  }
  return result;
}
