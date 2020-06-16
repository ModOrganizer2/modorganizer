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
#include "credentialsdialog.h"
#include "filedialogmemory.h"
#include "modinfodialog.h"
#include "spawn.h"
#include "syncoverwritedialog.h"
#include "nxmaccessmanager.h"
#include <ipluginmodpage.h>
#include <dataarchives.h>
#include <localsavegames.h>
#include <scopeguard.h>
#include <utility.h>
#include <usvfs.h>
#include "shared/appconfig.h"
#include <report.h>
#include <questionboxmemory.h>
#include "instancemanager.h"
#include <scriptextender.h>
#include "previewdialog.h"
#include "env.h"
#include "envmodule.h"
#include "envfs.h"
#include "directoryrefresher.h"
#include "shared/directoryentry.h"
#include "shared/filesorigin.h"
#include "shared/fileentry.h"

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
#include <QtGlobal> // for qUtf8Printable, etc

#include <Psapi.h>
#include <Shlobj.h>
#include <tlhelp32.h>
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

//static
CrashDumpsType OrganizerCore::m_globalCrashDumpsType = CrashDumpsType::None;

template <typename InputIterator>
QStringList toStringList(InputIterator current, InputIterator end)
{
  QStringList result;
  for (; current != end; ++current) {
    result.append(*current);
  }
  return result;
}


OrganizerCore::OrganizerCore(Settings &settings)
  : m_UserInterface(nullptr)
  , m_PluginContainer(nullptr)
  , m_CurrentProfile(nullptr)
  , m_Settings(settings)
  , m_Updater(NexusInterface::instance(m_PluginContainer))
  , m_ModList(m_PluginContainer, this)
  , m_PluginList(this)
  , m_DirectoryRefresher(new DirectoryRefresher(settings.refreshThreadCount()))
  , m_DirectoryStructure(new DirectoryEntry(L"data", nullptr, 0))
  , m_DownloadManager(NexusInterface::instance(m_PluginContainer), this)
  , m_DirectoryUpdate(false)
  , m_ArchivesInit(false)
  , m_PluginListsWriter(std::bind(&OrganizerCore::savePluginList, this))
{
  env::setHandleCloserThreadCount(settings.refreshThreadCount());
  m_DownloadManager.setOutputDirectory(m_Settings.paths().downloads(), false);

  NexusInterface::instance(m_PluginContainer)->setCacheDirectory(
    m_Settings.paths().cache());

  m_InstallationManager.setModsDirectory(m_Settings.paths().mods());
  m_InstallationManager.setDownloadDirectory(m_Settings.paths().downloads());

  connect(&m_DownloadManager, SIGNAL(downloadSpeed(QString, int)), this,
          SLOT(downloadSpeed(QString, int)));
  connect(m_DirectoryRefresher.get(), SIGNAL(refreshed()), this,
          SLOT(directory_refreshed()));

  connect(&m_ModList, SIGNAL(removeOrigin(QString)), this,
          SLOT(removeOrigin(QString)));

  connect(NexusInterface::instance(m_PluginContainer)->getAccessManager(),
          SIGNAL(validateSuccessful(bool)), this, SLOT(loginSuccessful(bool)));
  connect(NexusInterface::instance(m_PluginContainer)->getAccessManager(),
          SIGNAL(validateFailed(QString)), this, SLOT(loginFailed(QString)));

  // This seems awfully imperative
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          &m_Settings, SLOT(managedGameChanged(MOBase::IPluginGame const *)));
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          &m_DownloadManager,
          SLOT(managedGameChanged(MOBase::IPluginGame const *)));
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          &m_PluginList, SLOT(managedGameChanged(MOBase::IPluginGame const *)));

  connect(this, &OrganizerCore::managedGameChanged, [this](IPluginGame const* gamePlugin) {
    ModDataContent* contentFeature = gamePlugin->feature<ModDataContent>();
    if (contentFeature) {
      m_Contents = ModDataContentHolder(contentFeature->getAllContents());
    }
    else {
      m_Contents = ModDataContentHolder();
    }
  });

  connect(&m_PluginList, &PluginList::writePluginsList, &m_PluginListsWriter,
          &DelayedFileWriterBase::write);

  // make directory refresher run in a separate thread
  m_RefresherThread.start();
  m_DirectoryRefresher->moveToThread(&m_RefresherThread);

  connect(&settings.plugins(), &PluginSettings::pluginSettingChanged, [this](auto const& ...args) {
    m_PluginSettingChanged(args...);
  });
}

OrganizerCore::~OrganizerCore()
{
  m_RefresherThread.exit();
  m_RefresherThread.wait();

  if (m_StructureDeleter.joinable()) {
    m_StructureDeleter.join();
  }

  saveCurrentProfile();

  // profile has to be cleaned up before the modinfo-buffer is cleared
  m_CurrentProfile.reset();

  ModInfo::clear();
  m_ModList.setProfile(nullptr);
  //  NexusInterface::instance()->cleanup();

  delete m_DirectoryStructure;
}

void OrganizerCore::storeSettings()
{
  if (m_CurrentProfile != nullptr) {
    m_Settings.game().setSelectedProfileName(m_CurrentProfile->name());
  }

  m_ExecutablesList.store(m_Settings);

  FileDialogMemory::save(m_Settings);

  const auto result = m_Settings.sync();

  if (result != QSettings::NoError) {
    QString reason;

    if (result == QSettings::AccessError) {
      reason = tr("File is write protected");
    } else if (result == QSettings::FormatError) {
      reason = tr("Invalid file format (probably a bug)");
    } else {
      reason = tr("Unknown error %1").arg(result);
    }

    QMessageBox::critical(
      qApp->activeWindow(), tr("Failed to write settings"),
      tr("An error occurred trying to write back MO settings to %1: %2")
      .arg(m_Settings.filename(), reason));
  }
}

void OrganizerCore::updateExecutablesList()
{
  if (m_PluginContainer == nullptr) {
    log::error("can't update executables list now");
    return;
  }

  m_ExecutablesList.load(managedGame(), m_Settings);
}

void OrganizerCore::updateModInfoFromDisc() {
  ModInfo::updateFromDisc(
    m_Settings.paths().mods(), &m_DirectoryStructure,
    m_PluginContainer, m_Settings.interface().displayForeign(), 
    m_Settings.refreshThreadCount(), managedGame());
}

void OrganizerCore::setUserInterface(IUserInterface* ui)
{
  storeSettings();

  m_UserInterface = ui;

  QWidget* w = nullptr;
  if (m_UserInterface) {
    w = m_UserInterface->mainWindow();
  }

  if (w) {
    connect(&m_ModList, SIGNAL(modlistChanged(QModelIndex, int)), w,
            SLOT(modlistChanged(QModelIndex, int)));
    connect(&m_ModList, SIGNAL(modlistChanged(QModelIndexList, int)), w,
            SLOT(modlistChanged(QModelIndexList, int)));
    connect(&m_ModList, SIGNAL(showMessage(QString)), w,
            SLOT(showMessage(QString)));
    connect(&m_ModList, SIGNAL(modRenamed(QString, QString)), w,
            SLOT(modRenamed(QString, QString)));
    connect(&m_ModList, SIGNAL(modUninstalled(QString)), w,
            SLOT(modRemoved(QString)));
    connect(&m_ModList, SIGNAL(removeSelectedMods()), w,
            SLOT(removeMod_clicked()));
    connect(&m_ModList, SIGNAL(clearOverwrite()), w,
      SLOT(clearOverwrite()));
    connect(&m_ModList, SIGNAL(fileMoved(QString, QString, QString)), w,
            SLOT(fileMoved(QString, QString, QString)));
    connect(&m_ModList, SIGNAL(modorder_changed()), w,
            SLOT(modorder_changed()));
    connect(&m_PluginList, SIGNAL(writePluginsList()), w,
      SLOT(esplist_changed()));
    connect(&m_PluginList, SIGNAL(esplist_changed()), w,
      SLOT(esplist_changed()));
    connect(&m_DownloadManager, SIGNAL(showMessage(QString)), w,
            SLOT(showMessage(QString)));
  }

  m_InstallationManager.setParentWidget(w);
  m_Updater.setUserInterface(w);
  m_UILocker.setUserInterface(w);
  m_DownloadManager.setParentWidget(w);

  checkForUpdates();
}

void OrganizerCore::checkForUpdates()
{
  // this currently wouldn't work reliably if the ui isn't initialized yet to
  // display the result
  if (m_UserInterface != nullptr) {
    m_Updater.testForUpdate(m_Settings);
  }
}

void OrganizerCore::connectPlugins(PluginContainer *container)
{
  m_DownloadManager.setSupportedExtensions(
      m_InstallationManager.getSupportedExtensions());
  m_PluginContainer = container;
  m_Updater.setPluginContainer(m_PluginContainer);
  m_DownloadManager.setPluginContainer(m_PluginContainer);
  m_ModList.setPluginContainer(m_PluginContainer);

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
  m_UserInterfaceInitialized.disconnect_all_slots();
  m_ProfileChanged.disconnect_all_slots();
  m_PluginSettingChanged.disconnect_all_slots();

  m_ModList.disconnectSlots();
  m_PluginList.disconnectSlots();
  m_Updater.setPluginContainer(nullptr);
  m_DownloadManager.setPluginContainer(nullptr);
  m_ModList.setPluginContainer(nullptr);

  m_Settings.plugins().clearPlugins();
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

bool OrganizerCore::nexusApi(bool retry)
{
  NXMAccessManager *accessManager
      = NexusInterface::instance(m_PluginContainer)->getAccessManager();

  if ((accessManager->validateAttempted() || accessManager->validated())
      && !retry) {
    // previous attempt, maybe even successful
    return false;
  } else {
    QString apiKey;
    if (m_Settings.nexus().apiKey(apiKey)) {
      // credentials stored or user entered them manually
      log::debug("attempt to verify nexus api key");
      accessManager->apiCheck(apiKey);
      return true;
    } else {
      // no credentials stored and user didn't enter them
      accessManager->refuseValidation();
      return false;
    }
  }
}

void OrganizerCore::startMOUpdate()
{
  if (nexusApi()) {
    m_PostLoginTasks.append([&]() { m_Updater.startUpdate(); });
  } else {
    m_Updater.startUpdate();
  }
}

void OrganizerCore::downloadRequestedNXM(const QString &url)
{
  log::debug("download requested: {}", url);
  if (nexusApi()) {
    m_PendingDownloads.append(url);
  } else {
    m_DownloadManager.addNXMDownload(url);
  }
}

void OrganizerCore::userInterfaceInitialized() 
{
  m_UserInterfaceInitialized(m_UserInterface->mainWindow());
}

void OrganizerCore::externalMessage(const QString &message)
{
  if (MOShortcut moshortcut{ message } ) {
    if(moshortcut.hasExecutable()) {
      processRunner()
        .setFromShortcut(moshortcut)
        .setWaitForCompletion(ProcessRunner::Refresh)
        .run();
    }
  }
  else if (isNxmLink(message)) {
    MessageDialog::showMessage(tr("Download started"), qApp->activeWindow());
    downloadRequestedNXM(message);
  }
}

void OrganizerCore::downloadRequested(QNetworkReply *reply, QString gameName, int modID,
                                      const QString &fileName)
{
  try {
    if (m_DownloadManager.addDownload(reply, QStringList(), fileName, gameName, modID, 0,
                                      new ModRepositoryFileInfo(gameName, modID))) {
      MessageDialog::showMessage(tr("Download started"), qApp->activeWindow());
    }
  } catch (const std::exception &e) {
    MessageDialog::showMessage(tr("Download failed"), qApp->activeWindow());
    log::error("exception starting download: {}", e.what());
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
  m_Settings.network().setDownloadSpeed(serverName, bytesPerSecond);
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

bool OrganizerCore::checkPathSymlinks() {
  const bool hasSymlink = (
    QFileInfo(m_Settings.paths().profiles()).isSymLink() ||
    QFileInfo(m_Settings.paths().mods()).isSymLink() ||
    QFileInfo(m_Settings.paths().overwrite()).isSymLink());

  if (hasSymlink) {
    log::warn("{}", QObject::tr(
      "One of the configured MO2 directories (profiles, mods, or overwrite) "
      "is on a path containing a symbolic (or other) link. This is likely to "
      "be incompatible with MO2's virtual filesystem."));

    return false;
  }

  return true;
}

bool OrganizerCore::bootstrap() {
  return createDirectory(m_Settings.paths().profiles()) &&
         createDirectory(m_Settings.paths().mods()) &&
         createDirectory(m_Settings.paths().downloads()) &&
         createDirectory(m_Settings.paths().overwrite()) &&
         createDirectory(QString::fromStdWString(crashDumpsPath())) &&
         checkPathSymlinks() && cycleDiagnostics();
}

void OrganizerCore::createDefaultProfile()
{
  QString profilesPath = settings().paths().profiles();
  if (QDir(profilesPath).entryList(QDir::AllDirs | QDir::NoDotAndDotDot).size()
      == 0) {
    Profile newProf("Default", managedGame(), false);
  }
}

void OrganizerCore::prepareVFS()
{
  m_USVFS.updateMapping(fileMapping(m_CurrentProfile->name(), QString()));
}

void OrganizerCore::updateVFSParams(
  log::Levels logLevel, CrashDumpsType crashDumpsType,
  const QString& crashDumpsPath,
  std::chrono::seconds spawnDelay, QString executableBlacklist)
{
  setGlobalCrashDumpsType(crashDumpsType);
  m_USVFS.updateParams(
    logLevel, crashDumpsType, crashDumpsPath, spawnDelay, executableBlacklist);
}

void OrganizerCore::setLogLevel(log::Levels level)
{
  m_Settings.diagnostics().setLogLevel(level);

  updateVFSParams(
    m_Settings.diagnostics().logLevel(),
    m_Settings.diagnostics().crashDumpsType(),
    QString::fromStdWString(crashDumpsPath()),
    m_Settings.diagnostics().spawnDelay(),
    m_Settings.executablesBlacklist());

  log::getDefault().setLevel(m_Settings.diagnostics().logLevel());
}

bool OrganizerCore::cycleDiagnostics()
{
  const auto maxDumps = settings().diagnostics().crashDumpsMax();
  const auto path = QString::fromStdWString(crashDumpsPath());

  if (maxDumps > 0) {
    removeOldFiles(path, "*.dmp", maxDumps, QDir::Time|QDir::Reversed);
  }

  return true;
}

void OrganizerCore::setGlobalCrashDumpsType(CrashDumpsType type)
{
  m_globalCrashDumpsType = type;
}

std::wstring OrganizerCore::crashDumpsPath() {
  return (
    qApp->property("dataPath").toString() + "/"
    + QString::fromStdWString(AppConfig::dumpsDir())
    ).toStdWString();
}

void OrganizerCore::setCurrentProfile(const QString &profileName)
{
  if ((m_CurrentProfile != nullptr)
      && (profileName == m_CurrentProfile->name())) {
    return;
  }

  log::debug("selecting profile '{}'", profileName);

  QDir profileBaseDir(settings().paths().profiles());
  QString profileDir = profileBaseDir.absoluteFilePath(profileName);

  if (!QDir(profileDir).exists()) {
    // selected profile doesn't exist. Ensure there is at least one profile,
    // then pick any one
    createDefaultProfile();

    profileDir = profileBaseDir.absoluteFilePath(
        profileBaseDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot).at(0));
  }

  // Keep the old profile to emit signal-changed:
  auto oldProfile = std::move(m_CurrentProfile);

  m_CurrentProfile = std::make_unique<Profile>(QDir(profileDir), managedGame());

  m_ModList.setProfile(m_CurrentProfile.get());

  if (m_CurrentProfile->invalidationActive(nullptr)) {
    m_CurrentProfile->activateInvalidation();
  } else {
    m_CurrentProfile->deactivateInvalidation();
  }

  m_Settings.game().setSelectedProfileName(m_CurrentProfile->name());

  connect(m_CurrentProfile.get(), SIGNAL(modStatusChanged(uint)), this, SLOT(modStatusChanged(uint)));
  connect(m_CurrentProfile.get(), SIGNAL(modStatusChanged(QList<uint>)), this, SLOT(modStatusChanged(QList<uint>)));
  refreshDirectoryStructure();

  m_CurrentProfile->debugDump();

  m_ProfileChanged(oldProfile.get(), m_CurrentProfile.get());
}

MOBase::IModRepositoryBridge *OrganizerCore::createNexusBridge() const
{
  return new NexusBridge(m_PluginContainer);
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
  return QDir::fromNativeSeparators(m_Settings.paths().downloads());
}

QString OrganizerCore::overwritePath() const
{
  return QDir::fromNativeSeparators(m_Settings.paths().overwrite());
}

QString OrganizerCore::basePath() const
{
  return QDir::fromNativeSeparators(m_Settings.paths().base());
}

QString OrganizerCore::modsPath() const
{
  return QDir::fromNativeSeparators(m_Settings.paths().mods());
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

MOBase::IPluginGame *OrganizerCore::getGame(const QString &name) const
{
  for (IPluginGame *game : m_PluginContainer->plugins<IPluginGame>()) {
    if (game != nullptr && game->gameShortName().compare(name, Qt::CaseInsensitive) == 0)
      return game;
  }
  return nullptr;
}

MOBase::IModInterface *OrganizerCore::createMod(GuessedValue<QString> &name)
{
  bool merge = false;
  if (!m_InstallationManager.testOverwrite(name, &merge)) {
    return nullptr;
  }

  m_InstallationManager.setModsDirectory(m_Settings.paths().mods());

  QString targetDirectory
      = QDir::fromNativeSeparators(m_Settings.paths().mods())
            .append("/")
            .append(name);

  QSettings settingsFile(targetDirectory + "/meta.ini", QSettings::IniFormat);

  if (!merge) {
    settingsFile.setValue("modid", 0);
    settingsFile.setValue("version", "");
    settingsFile.setValue("newestVersion", "");
    settingsFile.setValue("category", 0);
    settingsFile.setValue("installationFile", "");

    settingsFile.remove("installedFiles");
    settingsFile.beginWriteArray("installedFiles", 0);
    settingsFile.endArray();
  }

  return ModInfo::createFrom(m_PluginContainer, m_GamePlugin, QDir(targetDirectory), &m_DirectoryStructure)
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
  return m_Settings.plugins().setting(pluginName, key);
}

void OrganizerCore::setPluginSetting(const QString &pluginName,
                                     const QString &key, const QVariant &value)
{
  m_Settings.plugins().setSetting(pluginName, key, value);
}

QVariant OrganizerCore::persistent(const QString &pluginName,
                                   const QString &key,
                                   const QVariant &def) const
{
  return m_Settings.plugins().persistent(pluginName, key, def);
}

void OrganizerCore::setPersistent(const QString &pluginName, const QString &key,
                                  const QVariant &value, bool sync)
{
  m_Settings.plugins().setPersistent(pluginName, key, value, sync);
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

  if (m_InstallationManager.isRunning()) {
    QMessageBox::information(
      qApp->activeWindow(), tr("Installation cancelled"),
      tr("Another installation is currently in progress."), QMessageBox::Ok);
    return nullptr;
  }

  bool hasIniTweaks = false;
  GuessedValue<QString> modName;
  if (!initModName.isEmpty()) {
    modName.update(initModName, GUESS_USER);
  }
  m_CurrentProfile->writeModlistNow();
  m_InstallationManager.setModsDirectory(m_Settings.paths().mods());
  if (m_InstallationManager.install(fileName, modName, hasIniTweaks) == IPluginInstaller::RESULT_SUCCESS) {
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
        m_UserInterface->displayModInformation(
          modInfo, modIndex, ModInfoTabIDs::IniFiles);
      }
      m_ModInstalled(modName);
      m_DownloadManager.markInstalled(fileName);
      emit modInstalled(modName);
      return modInfo.data();
    } else {
      reportError(tr("mod not found: %1").arg(qUtf8Printable(modName)));
    }
  } else if (m_InstallationManager.wasCancelled()) {
    QMessageBox::information(qApp->activeWindow(), tr("Extraction cancelled"),
                             tr("The installation was cancelled while extracting files. "
                               "If this was prior to a FOMOD setup, this warning may be ignored. "
                               "However, if this was during installation, the mod will likely be missing files."),
                             QMessageBox::Ok);
    refreshModList();
  }
  return nullptr;
}

void OrganizerCore::installDownload(int index)
{
  if (m_InstallationManager.isRunning()) {
    QMessageBox::information(
      qApp->activeWindow(), tr("Installation cancelled"),
      tr("Another installation is currently in progress."), QMessageBox::Ok);
    return;
  }

  try {
    QString fileName = m_DownloadManager.getFilePath(index);
    QString gameName = m_DownloadManager.getGameName(index);
    int modID        = m_DownloadManager.getModID(index);
    int fileID       = m_DownloadManager.getFileInfo(index)->fileID;
    GuessedValue<QString> modName;

    // see if there already are mods with the specified mod id
    if (modID != 0) {
      std::vector<ModInfo::Ptr> modInfo = ModInfo::getByModID(gameName, modID);
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
    m_InstallationManager.setModsDirectory(m_Settings.paths().mods());
    if (m_InstallationManager.install(fileName, modName, hasIniTweaks) == IPluginInstaller::RESULT_SUCCESS) {
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
          m_UserInterface->displayModInformation(
            modInfo, modIndex, ModInfoTabIDs::IniFiles);
        }

        m_ModInstalled(modName);
      } else {
        reportError(tr("mod not found: %1").arg(qUtf8Printable(modName)));
      }
      m_DownloadManager.markInstalled(index);

      emit modInstalled(modName);
    } else if (m_InstallationManager.wasCancelled()) {
      QMessageBox::information(qApp->activeWindow(), tr("Extraction cancelled"),
        tr("The installation was cancelled while extracting files. "
          "If this was prior to a FOMOD setup, this warning may be ignored. "
          "However, if this was during installation, the mod will likely be missing files."),
        QMessageBox::Ok);
      refreshModList();
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
  const FileEntryPtr file
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
  DirectoryEntry *dir = m_DirectoryStructure;
  if (!directoryName.isEmpty())
    dir = dir->findSubDirectoryRecursive(ToWString(directoryName));
  if (dir != nullptr) {
    for (const auto& d : dir->getSubDirectories()) {
      result.append(ToQString(d->getName()));
    }
  }
  return result;
}

QStringList OrganizerCore::findFiles(
    const QString &path,
    const std::function<bool(const QString &)> &filter) const
{
  QStringList result;
  DirectoryEntry *dir = m_DirectoryStructure;
  if (!path.isEmpty())
    dir = dir->findSubDirectoryRecursive(ToWString(path));
  if (dir != nullptr) {
    std::vector<FileEntryPtr> files = dir->getFiles();
    for (FileEntryPtr &file: files) {
      QString fullPath = ToQString(file->getFullPath());
      if (filter(fullPath)) {
        result.append(fullPath);
      }
    }
  }
  return result;
}

QStringList OrganizerCore::getFileOrigins(const QString &fileName) const
{
  QStringList result;
  const FileEntryPtr file = m_DirectoryStructure->searchFile(ToWString(fileName), nullptr);

  if (file.get() != nullptr) {
    result.append(ToQString(
        m_DirectoryStructure->getOriginByID(file->getOrigin()).getName()));
    foreach (auto i, file->getAlternatives()) {
      result.append(
          ToQString(m_DirectoryStructure->getOriginByID(i.first).getName()));
    }
  }
  return result;
}

QList<MOBase::IOrganizer::FileInfo> OrganizerCore::findFileInfos(
    const QString &path,
    const std::function<bool(const MOBase::IOrganizer::FileInfo &)> &filter)
    const
{
  QList<IOrganizer::FileInfo> result;
  DirectoryEntry *dir = m_DirectoryStructure;
  if (!path.isEmpty())
    dir = dir->findSubDirectoryRecursive(ToWString(path));
  if (dir != nullptr) {
    std::vector<FileEntryPtr> files = dir->getFiles();
    foreach (FileEntryPtr file, files) {
      IOrganizer::FileInfo info;
      info.filePath    = ToQString(file->getFullPath());
      bool fromArchive = false;
      info.origins.append(ToQString(
          m_DirectoryStructure->getOriginByID(file->getOrigin(fromArchive))
              .getName()));
      info.archive = fromArchive ? ToQString(file->getArchive().first) : "";
      foreach (auto idx, file->getAlternatives()) {
        info.origins.append(
            ToQString(m_DirectoryStructure->getOriginByID(idx.first).getName()));
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
  for (int i = currentProfile()->getPriorityMinimum();
           i < currentProfile()->getPriorityMinimum() + (int)currentProfile()->numRegularMods();
           ++i) {
    int modIndex = currentProfile()->modIndexByPriority(i);
    auto modInfo = ModInfo::getByIndex(modIndex);
    if (!modInfo->hasFlag(ModInfo::FLAG_OVERWRITE) &&
        !modInfo->hasFlag(ModInfo::FLAG_BACKUP)) {
      res.push_back(ModInfo::getByIndex(modIndex)->internalName());
    }
  }
  return res;
}

bool OrganizerCore::previewFileWithAlternatives(
  QWidget* parent, QString fileName, int selectedOrigin)
{
  fileName = QDir::fromNativeSeparators(fileName);

  // what we have is an absolute path to the file in its actual location (for the primary origin)
  // what we want is the path relative to the virtual data directory

  // we need to look in the virtual directory for the file to make sure the info is up to date.

  // check if the file comes from the actual data folder instead of a mod
  QDir gameDirectory = managedGame()->dataDirectory().absolutePath();
  QString relativePath = gameDirectory.relativeFilePath(fileName);
  QDir dirRelativePath = gameDirectory.relativeFilePath(fileName);

  // if the file is on a different drive the dirRelativePath will actually be an
  // absolute path so we make sure that is not the case
  if (!dirRelativePath.isAbsolute() && !relativePath.startsWith("..")) {
    fileName = relativePath;
  }
  else {
    // crude: we search for the next slash after the base mod directory to skip
    // everything up to the data-relative directory
    int offset = settings().paths().mods().size() + 1;
    offset = fileName.indexOf("/", offset);
    fileName = fileName.mid(offset + 1);
  }



  const FileEntryPtr file = directoryStructure()->searchFile(ToWString(fileName), nullptr);

  if (file.get() == nullptr) {
    reportError(tr("file not found: %1").arg(qUtf8Printable(fileName)));
    return false;
  }

  // set up preview dialog
  PreviewDialog preview(fileName, parent);

  auto addFunc = [&](int originId) {
    FilesOrigin &origin = directoryStructure()->getOriginByID(originId);
    QString filePath = QDir::fromNativeSeparators(ToQString(origin.getPath())) + "/" + fileName;
    if (QFile::exists(filePath)) {
      // it's very possible the file doesn't exist, because it's inside an archive. we don't support that
      QWidget *wid = m_PluginContainer->previewGenerator().genPreview(filePath);
      if (wid == nullptr) {
        reportError(tr("failed to generate preview for %1").arg(filePath));
      }
      else {
        preview.addVariant(ToQString(origin.getName()), wid);
      }
    }
  };

  if (selectedOrigin == -1) {
    // don't bother with the vector of origins, just add them as they come
    addFunc(file->getOrigin());
    for (auto alt : file->getAlternatives()) {
      addFunc(alt.first);
    }
  } else {
    std::vector<int> origins;

    // start with the primary origin
    origins.push_back(file->getOrigin());

    // add other origins, push to front if it's the selected one
    for (auto alt : file->getAlternatives()) {
      if (alt.first == selectedOrigin) {
        origins.insert(origins.begin(), alt.first);
      } else {
        origins.push_back(alt.first);
      }
    }

    // can't be empty; either the primary origin was the selected one, or it
    // was one of the alternatives, which got inserted in front

    if (origins[0] != selectedOrigin) {
      // sanity check, this shouldn't happen unless the caller passed an
      // incorrect id

      log::warn(
        "selected preview origin {} not found in list of alternatives",
        selectedOrigin);
    }

    for (int id : origins) {
      addFunc(id);
    }
  }

  if (preview.numVariants() > 0) {
    preview.exec();
    return true;
  }
  else {
    QMessageBox::information(
      parent, tr("Sorry"),
      tr("Sorry, can't preview anything. This function currently does not support extracting from bsas."));

    return false;
  }
}

bool OrganizerCore::previewFile(
  QWidget* parent, const QString& originName, const QString& path)
{
  if (!QFile::exists(path)) {
    reportError(tr("File '%1' not found.").arg(path));
    return false;
  }

  PreviewDialog preview(path, parent);

  QWidget *wid = m_PluginContainer->previewGenerator().genPreview(path);
  if (wid == nullptr) {
    reportError(tr("Failed to generate preview for %1").arg(path));
    return false;
  }

  preview.addVariant(originName, wid);
  preview.exec();

  return true;
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

bool OrganizerCore::onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func)
{
  return m_UserInterfaceInitialized.connect(func).connected();
}

bool OrganizerCore::onProfileChanged(std::function<void(IProfile*, IProfile*)> const& func)
{
  return m_ProfileChanged.connect(func).connected();
}

bool OrganizerCore::onPluginSettingChanged(std::function<void(QString const&, const QString& key, const QVariant&, const QVariant&)> const& func)
{
  return m_PluginSettingChanged.connect(func).connected();
}

void OrganizerCore::refreshModList(bool saveChanges)
{
  // don't lose changes!
  if (saveChanges) {
    m_CurrentProfile->writeModlistNow(true);
  }

  ModInfo::updateFromDisc(
    m_Settings.paths().mods(), &m_DirectoryStructure,
    m_PluginContainer, m_Settings.interface().displayForeign(), 
    m_Settings.refreshThreadCount(), managedGame());

  m_CurrentProfile->refreshModStatus();

  m_ModList.notifyChange(-1);

  refreshDirectoryStructure();
}

void OrganizerCore::refreshESPList(bool force)
{
  if (m_DirectoryUpdate) {
    // don't mess up the esp list if we're currently updating the directory
    // structure
    m_PostRefreshTasks.append([=]() {
      this->refreshESPList(force);
    });
    return;
  }
  m_CurrentProfile->writeModlist();

  // clear list
  try {
    m_PluginList.refresh(m_CurrentProfile->name(), *m_DirectoryStructure,
                         m_CurrentProfile->getLockedOrderFileName(), force);
  } catch (const std::exception &e) {
    reportError(tr("Failed to refresh list of esps: %1").arg(e.what()));
  }
}

void OrganizerCore::refreshBSAList()
{
  TimeThis tt("OrganizerCore::refreshBSAList()");

  DataArchives *archives = m_GamePlugin->feature<DataArchives>();

  if (archives != nullptr) {
    m_ArchivesInit = false;

    // default archives are the ones enabled outside MO. if the list can't be
    // found (which might
    // happen if ini files are missing) use hard-coded defaults (preferrably the
    // same the game would use)
    m_DefaultArchives = archives->archives(m_CurrentProfile.get());
    if (m_DefaultArchives.length() == 0) {
      m_DefaultArchives = archives->vanillaArchives();
    }

    m_ActiveArchives.clear();

    auto iter        = enabledArchives();
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
    refreshESPList(true);
    refreshBSAList();
  } // no point in refreshing lists if no files have been added to the directory
    // tree
}

void OrganizerCore::updateModActiveState(int index, bool active)
{
  QList<unsigned int> modsToUpdate;
  modsToUpdate.append(index);
  updateModsActiveState(modsToUpdate, active);
}

void OrganizerCore::updateModsActiveState(const QList<unsigned int> &modIndices, bool active)
{
  int enabled = 0;
  for (auto index : modIndices) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
    QDir dir(modInfo->absolutePath());
    for (const QString &esm :
      dir.entryList(QStringList() << "*.esm", QDir::Files)) {
      const FileEntryPtr file = m_DirectoryStructure->findFile(ToWString(esm));
      if (file.get() == nullptr) {
        log::warn("failed to activate {}", esm);
        continue;
      }

      if (active != m_PluginList.isEnabled(esm)
        && file->getAlternatives().empty()) {
        m_PluginList.blockSignals(true);
        m_PluginList.enableESP(esm, active);
        m_PluginList.blockSignals(false);
      }
    }

    for (const QString &esl :
      dir.entryList(QStringList() << "*.esl", QDir::Files)) {
      const FileEntryPtr file = m_DirectoryStructure->findFile(ToWString(esl));
      if (file.get() == nullptr) {
        log::warn("failed to activate {}", esl);
        continue;
      }

      if (active != m_PluginList.isEnabled(esl)
        && file->getAlternatives().empty()) {
        m_PluginList.blockSignals(true);
        m_PluginList.enableESP(esl, active);
        m_PluginList.blockSignals(false);
        ++enabled;
      }
    }
    QStringList esps = dir.entryList(QStringList() << "*.esp", QDir::Files);
    for (const QString &esp : esps) {
      const FileEntryPtr file = m_DirectoryStructure->findFile(ToWString(esp));
      if (file.get() == nullptr) {
        log::warn("failed to activate {}", esp);
        continue;
      }

      if (active != m_PluginList.isEnabled(esp)
        && file->getAlternatives().empty()) {
        m_PluginList.blockSignals(true);
        m_PluginList.enableESP(esp, active);
        m_PluginList.blockSignals(false);
        ++enabled;
      }
    }
  }
  if (active && (enabled > 1)) {
    MessageDialog::showMessage(
      tr("Multiple esps/esls activated, please check that they don't conflict."),
      qApp->activeWindow());
  }
  m_PluginList.refreshLoadOrder();
  // immediately save affected lists
  m_PluginListsWriter.writeImmediately(false);
}

void OrganizerCore::updateModInDirectoryStructure(unsigned int index,
                                                  ModInfo::Ptr modInfo)
{
  QMap<unsigned int, ModInfo::Ptr> allModInfo;
  allModInfo[index] = modInfo;
  updateModsInDirectoryStructure(allModInfo);
}

void OrganizerCore::updateModsInDirectoryStructure(QMap<unsigned int, ModInfo::Ptr> modInfo)
{
  std::vector<DirectoryRefresher::EntryInfo> entries;

  for (auto idx : modInfo.keys()) {
    entries.push_back({
      modInfo[idx]->name(), modInfo[idx]->absolutePath(),
      modInfo[idx]->stealFiles(), {}, m_CurrentProfile->getModPriority(idx)});
  }

  m_DirectoryRefresher->addMultipleModsFilesToStructure(
    m_DirectoryStructure, entries);

  DirectoryRefresher::cleanStructure(m_DirectoryStructure);
  // need to refresh plugin list now so we can activate esps
  refreshESPList(true);
  // activate all esps of the specified mod so the bsas get activated along with
  // it
  m_PluginList.blockSignals(true);
  updateModsActiveState(modInfo.keys(), true);
  m_PluginList.blockSignals(false);
  // now we need to refresh the bsa list and save it so there is no confusion
  // about what archives are available and active
  refreshBSAList();
  if (m_UserInterface != nullptr) {
    m_UserInterface->archivesWriter().writeImmediately(false);
  }

  std::vector<QString> archives = enabledArchives();
  m_DirectoryRefresher->setMods(
    m_CurrentProfile->getActiveMods(),
    std::set<QString>(archives.begin(), archives.end()));

  // finally also add files from bsas to the directory structure
  for (auto idx : modInfo.keys()) {
    m_DirectoryRefresher->addModBSAToStructure(
      m_DirectoryStructure, modInfo[idx]->name(),
      m_CurrentProfile->getModPriority(idx), modInfo[idx]->absolutePath(),
      modInfo[idx]->archives());
  }
}

void OrganizerCore::loggedInAction(QWidget* parent, std::function<void ()> f)
{
  if (NexusInterface::instance(m_PluginContainer)->getAccessManager()->validated()) {
    f();
  } else {
    QString apiKey;
    if (settings().nexus().apiKey(apiKey)) {
      doAfterLogin([f]{ f(); });
      NexusInterface::instance(m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus"), parent);
    }
  }
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
    QString gameName = "";
    int modID  = 0;
    int fileID = 0;
    QRegExp nameExp("www\\.nexusmods\\.com/(\\a+)/");
    if (nameExp.indexIn(url.toString()) != -1) {
      gameName = nameExp.cap(1);
    }
    QRegExp modExp("mods/(\\d+)");
    if (modExp.indexIn(url.toString()) != -1) {
      modID = modExp.cap(1).toInt();
    }
    QRegExp fileExp("fid=(\\d+)");
    if (fileExp.indexIn(reply->url().toString()) != -1) {
      fileID = fileExp.cap(1).toInt();
    }
    m_DownloadManager.addDownload(reply,
                                  new ModRepositoryFileInfo(gameName, modID, fileID));
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
  ModListSortProxy *result = new ModListSortProxy(m_CurrentProfile.get(), this);
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
  if (m_ArchiveParsing) {
    QFile archiveFile(m_CurrentProfile->getArchivesFileName());
    if (archiveFile.open(QIODevice::ReadOnly)) {
      while (!archiveFile.atEnd()) {
        result.push_back(QString::fromUtf8(archiveFile.readLine()).trimmed());
      }
      archiveFile.close();
    }
  }
  return result;
}

void OrganizerCore::refreshDirectoryStructure()
{
  if (m_DirectoryUpdate) {
    log::debug("can't refresh, already in progress");
    return;
  }

  log::debug("refreshing structure");
  m_DirectoryUpdate = true;

  m_CurrentProfile->writeModlistNow(true);
  const auto activeModList = m_CurrentProfile->getActiveMods();
  const auto archives = enabledArchives();

  m_DirectoryRefresher->setMods(
      activeModList, std::set<QString>(archives.begin(), archives.end()));

  // runs refresh() in a thread
  QTimer::singleShot(0, m_DirectoryRefresher.get(), SLOT(refresh()));
}

void OrganizerCore::directory_refreshed()
{
  log::debug("directory refreshed, finishing up");
  TimeThis tt("OrganizerCore::directory_refreshed()");

  DirectoryEntry *newStructure = m_DirectoryRefresher->stealDirectoryStructure();
  Q_ASSERT(newStructure != m_DirectoryStructure);

  if (newStructure == nullptr) {
    // TODO: don't know why this happens, this slot seems to get called twice
    // with only one emit
    return;
  }

  std::swap(m_DirectoryStructure, newStructure);

  if (m_StructureDeleter.joinable()) {
    m_StructureDeleter.join();
  }

  m_StructureDeleter = std::thread([=]{
    log::debug("structure deleter thread start");
    delete newStructure;
    log::debug("structure deleter thread done");
  });

  m_DirectoryUpdate = false;

  log::debug("clearing caches");
  for (int i = 0; i < m_ModList.rowCount(); ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
    modInfo->clearCaches();
  }

  if (!m_PostRefreshTasks.empty()) {
    log::debug("running {} post refresh tasks", m_PostRefreshTasks.size());

    for (auto task : m_PostRefreshTasks) {
      task();
    }

    m_PostRefreshTasks.clear();
  }

  if (m_CurrentProfile != nullptr) {
    log::debug("refreshing lists");
    refreshLists();
  }

  emit directoryStructureReady();

  log::debug("refresh done");
}

void OrganizerCore::profileRefresh()
{
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
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        FilesOrigin &origin
            = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
        origin.enable(false);
      }
      if (m_UserInterface != nullptr) {
        m_UserInterface->archivesWriter().write();
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

void OrganizerCore::modStatusChanged(QList<unsigned int> index) {
  try {
    QMap<unsigned int, ModInfo::Ptr> modsToEnable;
    QMap<unsigned int, ModInfo::Ptr> modsToDisable;
    for (auto idx : index) {
      if (m_CurrentProfile->modEnabled(idx)) {
        modsToEnable[idx] = ModInfo::getByIndex(idx);
      } else {
        modsToDisable[idx] = ModInfo::getByIndex(idx);
      }
    }
    if (!modsToEnable.isEmpty()) {
      updateModsInDirectoryStructure(modsToEnable);
      for (auto modInfo : modsToEnable.values()) {
        modInfo->clearCaches();
      }
    }
    if (!modsToDisable.isEmpty()) {
      updateModsActiveState(modsToDisable.keys(), false);
      for (auto idx : modsToDisable.keys()) {
        if (m_DirectoryStructure->originExists(ToWString(modsToDisable[idx]->name()))) {
          FilesOrigin &origin
            = m_DirectoryStructure->getOriginByName(ToWString(modsToDisable[idx]->name()));
          origin.enable(false);
        }
      }
      if (m_UserInterface != nullptr) {
        m_UserInterface->archivesWriter().write();
      }
    }

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
  NexusInterface::instance(m_PluginContainer)->loginCompleted();
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
  qCritical().nospace().noquote()
    << "Nexus API validation failed: " << message;

  if (QMessageBox::question(qApp->activeWindow(), tr("Login failed"),
                            tr("Login failed, try again?"))
      == QMessageBox::Yes) {
    if (nexusApi(true)) {
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
  NexusInterface::instance(m_PluginContainer)->loginCompleted();
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
    syncDialog.apply(QDir::fromNativeSeparators(m_Settings.paths().mods()));
    modInfo->diskContentModified();
    refreshDirectoryStructure();
  }
}

QString OrganizerCore::oldMO1HookDll() const
{
  if (auto extender = managedGame()->feature<ScriptExtender>()) {
    QString hookdll = QDir::toNativeSeparators(
      managedGame()->dataDirectory().absoluteFilePath(extender->PluginPath() + "/hook.dll"));
    if (QFile(hookdll).exists())
      return hookdll;
  }
  return QString();
}

std::vector<unsigned int> OrganizerCore::activeProblems() const
{
  std::vector<unsigned int> problems;
  const auto& hookdll = oldMO1HookDll();
  if (!hookdll.isEmpty()) {
    // This warning will now be shown every time the problems are checked, which is a bit
    // of a "log spam". But since this is a sever error which will most likely make the
    // game crash/freeze/etc. and is very hard to diagnose,  this "log spam" will make it
    // easier for the user to notice the warning.
    log::warn("hook.dll found in game folder: {}", hookdll);
    problems.push_back(PROBLEM_MO1SCRIPTEXTENDERWORKAROUND);
  }
  return problems;
}

QString OrganizerCore::shortDescription(unsigned int key) const
{
  switch (key) {
    case PROBLEM_MO1SCRIPTEXTENDERWORKAROUND: {
      return tr("MO1 \"Script Extender\" load mechanism has left hook.dll in your game folder");
    } break;
    default: {
      return tr("Description missing");
    } break;
  }
}

QString OrganizerCore::fullDescription(unsigned int key) const
{
  switch (key) {
    case PROBLEM_MO1SCRIPTEXTENDERWORKAROUND: {
      return tr("<a href=\"%1\">hook.dll</a> has been found in your game folder (right click to copy the full path). "
                "This is most likely a leftover of setting the ModOrganizer 1 load mechanism to \"Script Extender\", "
                "in which case you must remove this file either by changing the load mechanism in ModOrganizer 1 or "
                "manually removing the file, otherwise the game is likely to crash and burn.").arg(oldMO1HookDll());
      break;
    }
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
    log::warn("not saving lists during directory update");
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
    m_PostRefreshTasks.append([this]() {
      this->savePluginList();
    });
    return;
  }
  m_PluginList.saveTo(m_CurrentProfile->getLockedOrderFileName(),
                      m_CurrentProfile->getDeleterFileName(),
                      m_Settings.game().hideUncheckedPlugins());
  m_PluginList.saveLoadOrder(*m_DirectoryStructure);
}

void OrganizerCore::saveCurrentProfile()
{
  if (m_CurrentProfile == nullptr) {
    return;
  }

  m_CurrentProfile->writeModlist();
  m_CurrentProfile->createTweakedIniFile();
  saveCurrentLists();
  m_Settings.game().loadMechanism().activate(m_Settings.game().loadMechanismType());
  storeSettings();
}

ProcessRunner OrganizerCore::processRunner()
{
  return ProcessRunner(*this, m_UserInterface);
}

bool OrganizerCore::beforeRun(
  const QFileInfo& binary, const QString& profileName,
  const QString& customOverwrite,
  const QList<MOBase::ExecutableForcedLoadSetting>& forcedLibraries)
{
  saveCurrentProfile();

  // need to wait until directory structure is ready
  if (m_DirectoryUpdate) {
    QEventLoop loop;
    connect(this, &OrganizerCore::directoryStructureReady, &loop, &QEventLoop::quit,
      Qt::ConnectionType::QueuedConnection);
    loop.exec();
  }

  // need to make sure all data is saved before we start the application
  if (m_CurrentProfile != nullptr) {
    m_CurrentProfile->writeModlistNow(true);
  }

  // TODO: should also pass arguments
  if (!m_AboutToRun(binary.absoluteFilePath())) {
    log::debug("start of \"{}\" cancelled by plugin", binary.absoluteFilePath());
    return false;
  }

  try
  {
    m_USVFS.updateMapping(fileMapping(profileName, customOverwrite));
    m_USVFS.updateForcedLibraries(forcedLibraries);
  }
  catch (const UsvfsConnectorException &e)
  {
    log::debug(e.what());
    return false;
  }
  catch (const std::exception &e)
  {
    QWidget* w = nullptr;
    if (m_UserInterface) {
      w = m_UserInterface->mainWindow();
    }
    QMessageBox::warning(w, tr("Error"), e.what());
    return false;
  }

  return true;
}

void OrganizerCore::afterRun(const QFileInfo& binary, DWORD exitCode)
{
  refreshDirectoryStructure();

  // need to remove our stored load order because it may be outdated if a
  // foreign tool changed the file time. After removing that file,
  // refreshESPList will use the file time as the order
  if (managedGame()->loadOrderMechanism() == IPluginGame::LoadOrderMechanism::FileTime) {
    log::debug("removing loadorder.txt");
    QFile::remove(m_CurrentProfile->getLoadOrderFileName());
  }

  refreshDirectoryStructure();

  refreshESPList(true);
  savePluginList();
  cycleDiagnostics();

  //These callbacks should not fiddle with directoy structure and ESPs.
  m_FinishedRun(binary.absoluteFilePath(), exitCode);
}

ProcessRunner::Results OrganizerCore::waitForAllUSVFSProcesses(
  UILocker::Reasons reason)
{
  return processRunner().waitForAllUSVFSProcessesWithLock(reason);
}

std::vector<Mapping> OrganizerCore::fileMapping(const QString &profileName,
                                                const QString &customOverwrite)
{
  // need to wait until directory structure is ready
  if (m_DirectoryUpdate) {
    QEventLoop loop;
    connect(this, &OrganizerCore::directoryStructureReady, &loop, &QEventLoop::quit,
      Qt::ConnectionType::QueuedConnection);
    loop.exec();
  }

  IPluginGame *game  = qApp->property("managed_game").value<IPluginGame *>();
  Profile profile(QDir(m_Settings.paths().profiles() + "/" + profileName),
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
      log::warn("local save games not supported by this game plugin");
    }
  }

  result.insert(result.end(), {
                  QDir::toNativeSeparators(m_Settings.paths().overwrite()),
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

  for (FileEntryPtr current : directoryEntry->getFiles()) {
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
  for (const auto& d : directoryEntry->getSubDirectories()) {
    int origin = d->anyOrigin();

    QString originPath
        = QString::fromStdWString(base->getOriginByID(origin).getPath());
    QString dirName = QString::fromStdWString(d->getName());
    QString source  = originPath + relPath + dirName;
    QString target  = dataPath + relPath + dirName;

    bool writeDestination
        = (base == directoryEntry) && (origin == createDestination);

    result.push_back({source, target, true, writeDestination});
    std::vector<Mapping> subRes = fileMapping(
        dataPath, relPath + dirName + "\\", base, d, createDestination);
    result.insert(result.end(), subRes.begin(), subRes.end());
  }
  return result;
}
