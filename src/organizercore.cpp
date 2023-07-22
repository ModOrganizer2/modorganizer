#include "organizercore.h"
#include "categoriesdialog.h"
#include "credentialsdialog.h"
#include "delayedfilewriter.h"
#include "directoryrefresher.h"
#include "env.h"
#include "envfs.h"
#include "envmodule.h"
#include "filedialogmemory.h"
#include "guessedvalue.h"
#include "imodinterface.h"
#include "imoinfo.h"
#include "instancemanager.h"
#include "iplugingame.h"
#include "iuserinterface.h"
#include "messagedialog.h"
#include "modlistsortproxy.h"
#include "modrepositoryfileinfo.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "pluginmanager.h"
#include "previewdialog.h"
#include "profile.h"
#include "shared/appconfig.h"
#include "shared/directoryentry.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"
#include "shared/util.h"
#include "spawn.h"
#include "syncoverwritedialog.h"
#include "virtualfiletree.h"
#include <ipluginmodpage.h>
#include <questionboxmemory.h>
#include <uibase/game_features/dataarchives.h>
#include <uibase/game_features/localsavegames.h>
#include <uibase/game_features/scriptextender.h>
#include <uibase/report.h>
#include <uibase/scopeguard.h>
#include <uibase/utility.h>
#include <usvfs/usvfs.h>

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
#include <QtGlobal>  // for qUtf8Printable, etc

#include <Psapi.h>
#include <Shlobj.h>
#include <tchar.h>  // for _tcsicmp
#include <tlhelp32.h>

#include <limits.h>
#include <stddef.h>
#include <string.h>  // for memset, wcsrchr

#include <boost/algorithm/string/predicate.hpp>
#include <exception>
#include <functional>
#include <memory>
#include <set>
#include <string>  //for wstring
#include <tuple>
#include <utility>

#include <libbsarch/bs_archive.h>

#include "inibakery.h"
#include "organizerproxy.h"

using namespace MOShared;
using namespace MOBase;

static env::CoreDumpTypes g_coreDumpType = env::CoreDumpTypes::Mini;

template <typename InputIterator>
QStringList toStringList(InputIterator current, InputIterator end)
{
  QStringList result;
  for (; current != end; ++current) {
    result.append(*current);
  }
  return result;
}

OrganizerCore::OrganizerCore(Settings& settings)
    : m_UserInterface(nullptr), m_PluginManager(nullptr), m_GamePlugin(nullptr),
      m_CurrentProfile(nullptr), m_Settings(settings),
      m_Updater(&NexusInterface::instance()), m_ModList(m_PluginManager, this),
      m_PluginList(*this),
      m_DirectoryRefresher(new DirectoryRefresher(this, settings.refreshThreadCount())),
      m_DirectoryStructure(new DirectoryEntry(L"data", nullptr, 0)),
      m_VirtualFileTree([this]() {
        return VirtualFileTree::makeTree(m_DirectoryStructure);
      }),
      m_DownloadManager(&NexusInterface::instance(), this), m_DirectoryUpdate(false),
      m_ArchivesInit(false),
      m_PluginListsWriter(std::bind(&OrganizerCore::savePluginList, this))
{
  // need to initialize here for aboutToRun() to be callable
  m_IniBakery = std::make_unique<IniBakery>(*this);

  env::setHandleCloserThreadCount(settings.refreshThreadCount());
  m_DownloadManager.setOutputDirectory(m_Settings.paths().downloads(), false);

  NexusInterface::instance().setCacheDirectory(m_Settings.paths().cache());

  m_InstallationManager.setModsDirectory(m_Settings.paths().mods());
  m_InstallationManager.setDownloadDirectory(m_Settings.paths().downloads());

  connect(&m_DownloadManager, SIGNAL(downloadSpeed(QString, int)), this,
          SLOT(downloadSpeed(QString, int)));
  connect(m_DirectoryRefresher.get(), &DirectoryRefresher::refreshed, this,
          &OrganizerCore::onDirectoryRefreshed);

  connect(&m_ModList, SIGNAL(removeOrigin(QString)), this, SLOT(removeOrigin(QString)));
  connect(&m_ModList, &ModList::modStatesChanged, [=] {
    currentProfile()->writeModlist();
  });
  connect(&m_ModList, &ModList::modPrioritiesChanged, [this](auto&& indexes) {
    modPrioritiesChanged(indexes);
  });

  connect(NexusInterface::instance().getAccessManager(),
          SIGNAL(validateSuccessful(bool)), this, SLOT(loginSuccessful(bool)));
  connect(NexusInterface::instance().getAccessManager(),
          SIGNAL(validateFailed(QString)), this, SLOT(loginFailed(QString)));

  // This seems awfully imperative
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const*)), &m_Settings,
          SLOT(managedGameChanged(MOBase::IPluginGame const*)));
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const*)),
          &m_DownloadManager, SLOT(managedGameChanged(MOBase::IPluginGame const*)));
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const*)), &m_PluginList,
          SLOT(managedGameChanged(MOBase::IPluginGame const*)));

  connect(&m_PluginList, &PluginList::writePluginsList, &m_PluginListsWriter,
          &DelayedFileWriterBase::write);

  // make directory refresher run in a separate thread
  m_RefresherThread.start();
  m_DirectoryRefresher->moveToThread(&m_RefresherThread);

  connect(&settings.plugins(), &PluginSettings::pluginSettingChanged,
          [this](auto const&... args) {
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
  if (m_PluginManager == nullptr) {
    log::error("can't update executables list now");
    return;
  }

  m_ExecutablesList.load(managedGame(), m_Settings);
}

void OrganizerCore::updateModInfoFromDisc()
{
  ModInfo::updateFromDisc(m_Settings.paths().mods(), *this,
                          m_Settings.interface().displayForeign(),
                          m_Settings.refreshThreadCount());
}

void OrganizerCore::setUserInterface(IUserInterface* ui)
{
  storeSettings();

  m_UserInterface = ui;

  QWidget* w = nullptr;
  if (m_UserInterface) {
    w = m_UserInterface->mainWindow();
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

void OrganizerCore::connectPlugins(PluginManager* manager)
{
  m_PluginManager = manager;
  m_Updater.setPluginManager(m_PluginManager);
  m_InstallationManager.setPluginManager(m_PluginManager);
  m_DownloadManager.setPluginManager(m_PluginManager);
  m_ModList.setPluginManager(m_PluginManager);

  if (!m_GameName.isEmpty()) {
    m_GamePlugin = m_PluginManager->game(m_GameName);
    emit managedGameChanged(m_GamePlugin);
  }

  connect(m_PluginManager, &PluginManager::pluginEnabled, [&](IPlugin* plugin) {
    m_PluginEnabled(plugin);
  });
  connect(m_PluginManager, &PluginManager::pluginDisabled, [&](IPlugin* plugin) {
    m_PluginDisabled(plugin);
  });

  connect(&m_PluginManager->gameFeatures(), &GameFeatures::modDataContentUpdated,
          [this](ModDataContent const* contentFeature) {
            if (contentFeature) {
              m_Contents = ModDataContentHolder(contentFeature->getAllContents());
            } else {
              m_Contents = ModDataContentHolder();
            }
          });
}

void OrganizerCore::setManagedGame(MOBase::IPluginGame* game)
{
  m_GameName   = game->gameName();
  m_GamePlugin = game;
  qApp->setProperty("managed_game", QVariant::fromValue(m_GamePlugin));
  emit managedGameChanged(m_GamePlugin);
}

Settings& OrganizerCore::settings()
{
  return m_Settings;
}

bool OrganizerCore::nexusApi(bool retry)
{
  auto* accessManager = NexusInterface::instance().getAccessManager();

  if ((accessManager->validateAttempted() || accessManager->validated()) && !retry) {
    // previous attempt, maybe even successful
    return false;
  } else {
    QString apiKey;
    if (GlobalSettings::nexusApiKey(apiKey)) {
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
    m_PostLoginTasks.append([&]() {
      m_Updater.startUpdate();
    });
  } else {
    m_Updater.startUpdate();
  }
}

void OrganizerCore::downloadRequestedNXM(const QString& url)
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

void OrganizerCore::profileCreated(MOBase::IProfile* profile)
{
  m_ProfileCreated(profile);
}

void OrganizerCore::profileRenamed(MOBase::IProfile* profile, QString const& oldName,
                                   QString const& newName)
{
  m_ProfileRenamed(profile, oldName, newName);
}

void OrganizerCore::profileRemoved(QString const& profileName)
{
  m_ProfileRemoved(profileName);
}

void OrganizerCore::downloadRequested(QNetworkReply* reply, QString gameName, int modID,
                                      const QString& fileName)
{
  try {
    if (m_DownloadManager.addDownload(reply, QStringList(), fileName, gameName, modID,
                                      0, new ModRepositoryFileInfo(gameName, modID))) {
      MessageDialog::showMessage(tr("Download started"), qApp->activeWindow());
    }
  } catch (const std::exception& e) {
    MessageDialog::showMessage(tr("Download failed"), qApp->activeWindow());
    log::error("exception starting download: {}", e.what());
  }
}

void OrganizerCore::removeOrigin(const QString& name)
{
  FilesOrigin& origin = m_DirectoryStructure->getOriginByName(ToWString(name));
  origin.enable(false);
  refreshLists();
}

void OrganizerCore::downloadSpeed(const QString& serverName, int bytesPerSecond)
{
  m_Settings.network().setDownloadSpeed(serverName, bytesPerSecond);
}

InstallationManager* OrganizerCore::installationManager()
{
  return &m_InstallationManager;
}

bool OrganizerCore::createDirectory(const QString& path)
{
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

bool OrganizerCore::checkPathSymlinks()
{
  const bool hasSymlink = (QFileInfo(m_Settings.paths().profiles()).isSymLink() ||
                           QFileInfo(m_Settings.paths().mods()).isSymLink() ||
                           QFileInfo(m_Settings.paths().overwrite()).isSymLink());

  if (hasSymlink) {
    log::warn(
        "{}",
        QObject::tr(
            "One of the configured MO2 directories (profiles, mods, or overwrite) "
            "is on a path containing a symbolic (or other) link. This is likely to "
            "be incompatible with MO2's virtual filesystem."));

    return false;
  }

  return true;
}

bool OrganizerCore::bootstrap()
{
  const auto dirs = {m_Settings.paths().profiles(), m_Settings.paths().mods(),
                     m_Settings.paths().downloads(), m_Settings.paths().overwrite(),
                     QString::fromStdWString(getGlobalCoreDumpPath())};

  for (auto&& dir : dirs) {
    if (!createDirectory(dir)) {
      return false;
    }
  }

  if (!checkPathSymlinks()) {
    return false;
  }

  if (!cycleDiagnostics()) {
    return false;
  }

  // log if there are any dmp files
  const auto hasCrashDumps = !QDir(QString::fromStdWString(getGlobalCoreDumpPath()))
                                  .entryList({"*.dmp"}, QDir::Files)
                                  .empty();

  if (hasCrashDumps) {
    log::debug("there are crash dumps in '{}'",
               QString::fromStdWString(getGlobalCoreDumpPath()));
  }

  return true;
}

void OrganizerCore::createDefaultProfile()
{
  QString profilesPath = settings().paths().profiles();
  if (QDir(profilesPath).entryList(QDir::AllDirs | QDir::NoDotAndDotDot).size() == 0) {
    Profile newProf(QString::fromStdWString(AppConfig::defaultProfileName()),
                    managedGame(), gameFeatures(), false);

    m_ProfileCreated(&newProf);
  }
}

void OrganizerCore::prepareVFS()
{
  m_USVFS.updateMapping(fileMapping(m_CurrentProfile->name(), QString()));
}

void OrganizerCore::updateVFSParams(log::Levels logLevel,
                                    env::CoreDumpTypes coreDumpType,
                                    const QString& crashDumpsPath,
                                    std::chrono::seconds spawnDelay,
                                    QString executableBlacklist,
                                    const QStringList& skipFileSuffixes,
                                    const QStringList& skipDirectories)
{
  setGlobalCoreDumpType(coreDumpType);

  m_USVFS.updateParams(logLevel, coreDumpType, crashDumpsPath, spawnDelay,
                       executableBlacklist, skipFileSuffixes, skipDirectories);
}

void OrganizerCore::setLogLevel(log::Levels level)
{
  m_Settings.diagnostics().setLogLevel(level);

  updateVFSParams(
      m_Settings.diagnostics().logLevel(), m_Settings.diagnostics().coreDumpType(),
      QString::fromStdWString(getGlobalCoreDumpPath()),
      m_Settings.diagnostics().spawnDelay(), m_Settings.executablesBlacklist(),
      m_Settings.skipFileSuffixes(), m_Settings.skipDirectories());

  log::getDefault().setLevel(m_Settings.diagnostics().logLevel());
}

bool OrganizerCore::cycleDiagnostics()
{
  const auto maxDumps = settings().diagnostics().maxCoreDumps();
  const auto path     = QString::fromStdWString(getGlobalCoreDumpPath());

  if (maxDumps > 0) {
    removeOldFiles(path, "*.dmp", maxDumps, QDir::Time | QDir::Reversed);
  }

  return true;
}

env::CoreDumpTypes OrganizerCore::getGlobalCoreDumpType()
{
  return g_coreDumpType;
}

void OrganizerCore::setGlobalCoreDumpType(env::CoreDumpTypes type)
{
  g_coreDumpType = type;
}

std::wstring OrganizerCore::getGlobalCoreDumpPath()
{
  if (qApp) {
    const auto dp = qApp->property("dataPath");
    if (!dp.isNull()) {
      return dp.toString().toStdWString() + L"/" + AppConfig::dumpsDir();
    }
  }

  return {};
}

void OrganizerCore::setCurrentProfile(const QString& profileName)
{
  if ((m_CurrentProfile != nullptr) && (profileName == m_CurrentProfile->name())) {
    return;
  }

  log::debug("selecting profile '{}'", profileName);

  QDir profileBaseDir(settings().paths().profiles());

  const auto subdirs = profileBaseDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);

  QString profileDir;

  // the profile name may not have the correct case, which breaks other parts
  // of the ui like the profile combobox, which walks directories on its own
  //
  // find the real name with the correct case by walking the directories
  for (auto&& dirName : subdirs) {
    if (QString::compare(dirName, profileName, Qt::CaseInsensitive) == 0) {
      profileDir = profileBaseDir.absoluteFilePath(dirName);
      break;
    }
  }

  if (profileDir.isEmpty()) {
    log::error("profile '{}' does not exist", profileName);

    // selected profile doesn't exist. Ensure there is at least one profile,
    // then pick any one
    createDefaultProfile();

    profileDir = profileBaseDir.absoluteFilePath(
        profileBaseDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot).at(0));

    log::error("picked profile '{}' instead", QDir(profileDir).dirName());

    reportError(tr("The selected profile '%1' does not exist. The profile '%2' will be "
                   "used instead")
                    .arg(profileName)
                    .arg(QDir(profileDir).dirName()));
  }

  // Keep the old profile to emit signal-changed:
  auto oldProfile = std::move(m_CurrentProfile);

  m_CurrentProfile =
      std::make_unique<Profile>(QDir(profileDir), managedGame(), gameFeatures());

  m_ModList.setProfile(m_CurrentProfile.get());

  if (m_CurrentProfile->invalidationActive(nullptr)) {
    m_CurrentProfile->activateInvalidation();
  } else {
    m_CurrentProfile->deactivateInvalidation();
  }

  m_Settings.game().setSelectedProfileName(m_CurrentProfile->name());

  connect(m_CurrentProfile.get(), qOverload<uint>(&Profile::modStatusChanged),
          [this](auto&& index) {
            modStatusChanged(index);
          });
  connect(m_CurrentProfile.get(), qOverload<QList<uint>>(&Profile::modStatusChanged),
          [this](auto&& indexes) {
            modStatusChanged(indexes);
          });
  refreshDirectoryStructure();

  m_CurrentProfile->debugDump();

  emit profileChanged(oldProfile.get(), m_CurrentProfile.get());
  m_ProfileChanged(oldProfile.get(), m_CurrentProfile.get());
}

MOBase::IModRepositoryBridge* OrganizerCore::createNexusBridge() const
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

MOBase::Version OrganizerCore::version() const
{
  return m_Updater.getVersion();
}

MOBase::IPluginGame* OrganizerCore::getGame(const QString& name) const
{
  for (IPluginGame* game : m_PluginManager->plugins<IPluginGame>()) {
    if (game != nullptr &&
        game->gameShortName().compare(name, Qt::CaseInsensitive) == 0)
      return game;
  }
  return nullptr;
}

MOBase::IModInterface* OrganizerCore::createMod(GuessedValue<QString>& name)
{
  auto result = m_InstallationManager.testOverwrite(name);
  if (!result) {
    return nullptr;
  }

  m_InstallationManager.setModsDirectory(m_Settings.paths().mods());

  QString targetDirectory =
      QDir::fromNativeSeparators(m_Settings.paths().mods()).append("/").append(name);

  QSettings settingsFile(targetDirectory + "/meta.ini", QSettings::IniFormat);

  if (!result.merged()) {
    settingsFile.setValue("modid", 0);
    settingsFile.setValue("version", "");
    settingsFile.setValue("newestVersion", "");
    settingsFile.setValue("category", 0);
    settingsFile.setValue("installationFile", "");

    settingsFile.remove("installedFiles");
    settingsFile.beginWriteArray("installedFiles", 0);
    settingsFile.endArray();
  }

  // shouldn't this use the existing mod in case of a merge? also, this does not refresh
  // the indices in the ModInfo structure
  return ModInfo::createFrom(QDir(targetDirectory), *this).data();
}

void OrganizerCore::modDataChanged(MOBase::IModInterface*)
{
  refresh(false);
}

QVariant OrganizerCore::pluginSetting(const QString& pluginName,
                                      const QString& key) const
{
  return m_Settings.plugins().setting(pluginName, key);
}

void OrganizerCore::setPluginSetting(const QString& pluginName, const QString& key,
                                     const QVariant& value)
{
  m_Settings.plugins().setSetting(pluginName, key, value);
}

QVariant OrganizerCore::persistent(const QString& pluginName, const QString& key,
                                   const QVariant& def) const
{
  return m_Settings.plugins().persistent(pluginName, key, def);
}

void OrganizerCore::setPersistent(const QString& pluginName, const QString& key,
                                  const QVariant& value, bool sync)
{
  m_Settings.plugins().setPersistent(pluginName, key, value, sync);
}

QString OrganizerCore::pluginDataPath()
{
  return qApp->applicationDirPath() + "/" + ToQString(AppConfig::pluginPath()) +
         "/data";
}

MOBase::IModInterface* OrganizerCore::installMod(const QString& archivePath,
                                                 int priority, bool reinstallation,
                                                 ModInfo::Ptr currentMod,
                                                 const QString& initModName)
{
  return installArchive(archivePath, reinstallation ? -1 : priority, reinstallation,
                        currentMod, initModName)
      .get();
}

std::pair<unsigned int, ModInfo::Ptr>
OrganizerCore::doInstall(const QString& archivePath, GuessedValue<QString> modName,
                         ModInfo::Ptr currentMod, int priority, bool reinstallation)
{
  if (m_CurrentProfile == nullptr) {
    return {-1, nullptr};
  }

  if (m_InstallationManager.isRunning()) {
    QMessageBox::information(qApp->activeWindow(), tr("Installation cancelled"),
                             tr("Another installation is currently in progress."),
                             QMessageBox::Ok);
    return {-1, nullptr};
  }

  bool hasIniTweaks = false;
  m_CurrentProfile->writeModlistNow();
  m_InstallationManager.setModsDirectory(m_Settings.paths().mods());
  m_InstallationManager.notifyInstallationStart(archivePath, reinstallation,
                                                currentMod);
  auto result = m_InstallationManager.install(archivePath, modName, hasIniTweaks);

  if (result) {
    MessageDialog::showMessage(tr("Installation successful"), qApp->activeWindow());

    // we wait for the directory structure to be ready before notifying the mod list,
    // this prevents issue with third-party plugins, e.g., if the installed mod is
    // activated before the structure is ready
    //
    // we need to fetch modIndex() within the call back because the index is only
    // valid after the call to refresh(), but we do not want to connect after
    // refresh()
    //
    connect(
        this, &OrganizerCore::directoryStructureReady, this,
        [=] {
          const int modIndex = ModInfo::getIndex(modName);
          if (modIndex != UINT_MAX) {
            const auto modInfo = ModInfo::getByIndex(modIndex);
            m_ModList.notifyModInstalled(modInfo.get());
          }
        },
        Qt::SingleShotConnection);

    refresh();

    const auto modIndex  = ModInfo::getIndex(modName);
    ModInfo::Ptr modInfo = nullptr;
    if (modIndex != UINT_MAX) {
      modInfo = ModInfo::getByIndex(modIndex);

      if (priority != -1 && !result.mergedOrReplaced()) {
        m_ModList.changeModPriority(modIndex, priority);
      }

      if (hasIniTweaks && m_UserInterface != nullptr &&
          (QMessageBox::question(qApp->activeWindow(), tr("Configure Mod"),
                                 tr("This mod contains ini tweaks. Do you "
                                    "want to configure them now?"),
                                 QMessageBox::Yes | QMessageBox::No) ==
           QMessageBox::Yes)) {
        m_UserInterface->displayModInformation(modInfo, modIndex,
                                               ModInfoTabIDs::IniFiles);
      }

      m_InstallationManager.notifyInstallationEnd(result, modInfo);
    } else {
      reportError(tr("mod not found: %1").arg(qUtf8Printable(modName)));
    }
    emit modInstalled(modName);
    return {modIndex, modInfo};
  } else {
    if (result.result() == MOBase::IPluginInstaller::RESULT_CATEGORYREQUESTED) {
      CategoriesDialog dialog(qApp->activeWindow());

      if (dialog.exec() == QDialog::Accepted) {
        dialog.commitChanges();
        refresh();
      }
    } else {
      m_InstallationManager.notifyInstallationEnd(result, nullptr);
      if (m_InstallationManager.wasCancelled()) {
        QMessageBox::information(
            qApp->activeWindow(), tr("Extraction cancelled"),
            tr("The installation was cancelled while extracting files. "
               "If this was prior to a FOMOD setup, this warning may be ignored. "
               "However, if this was during installation, the mod will likely be "
               "missing "
               "files."),
            QMessageBox::Ok);
        refresh();
      }
    }
  }

  return {-1, nullptr};
}

ModInfo::Ptr OrganizerCore::installDownload(int index, int priority)
{
  ScopedDisableDirWatcher scopedDirwatcher(&m_DownloadManager);

  try {
    QString fileName        = m_DownloadManager.getFilePath(index);
    QString gameName        = m_DownloadManager.getGameName(index);
    int modID               = m_DownloadManager.getModID(index);
    int fileID              = m_DownloadManager.getFileInfo(index)->fileID;
    ModInfo::Ptr currentMod = nullptr;
    GuessedValue<QString> modName;

    // see if there already are mods with the specified mod id
    if (modID > 0) {
      std::vector<ModInfo::Ptr> modInfo = ModInfo::getByModID(gameName, modID);
      for (auto iter = modInfo.begin(); iter != modInfo.end(); ++iter) {
        std::vector<ModInfo::EFlag> flags = (*iter)->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) ==
            flags.end()) {
          modName.update((*iter)->name(), GUESS_PRESET);
          currentMod = *iter;
          (*iter)->saveMeta();
        }
      }
    }

    const auto [modIndex, modInfo] =
        doInstall(fileName, modName, currentMod, priority, false);

    if (modInfo != nullptr) {
      modInfo->addInstalledFile(modID, fileID);
      m_DownloadManager.markInstalled(index);
      if (settings().interface().hideDownloadsAfterInstallation()) {
        m_DownloadManager.removeDownload(index, false);
      }
    }

    return modInfo;
  } catch (const std::exception& e) {
    reportError(QString(e.what()));
  }

  return nullptr;
}

ModInfo::Ptr OrganizerCore::installArchive(const QString& archivePath, int priority,
                                           bool reinstallation, ModInfo::Ptr currentMod,
                                           const QString& initModName)
{
  GuessedValue<QString> modName;
  if (!initModName.isEmpty()) {
    modName.update(initModName, GUESS_USER);
  }
  const auto [modIndex, modInfo] =
      doInstall(archivePath, modName, currentMod, priority, reinstallation);
  if (m_CurrentProfile == nullptr) {
    return nullptr;
  }

  if (modInfo != nullptr) {
    auto dlIdx = m_DownloadManager.indexByName(QFileInfo(archivePath).fileName());
    if (dlIdx != -1) {
      int modId  = m_DownloadManager.getModID(dlIdx);
      int fileId = m_DownloadManager.getFileInfo(dlIdx)->fileID;
      modInfo->addInstalledFile(modId, fileId);
    }
    m_DownloadManager.markInstalled(archivePath);
  }
  return modInfo;
}

QString OrganizerCore::resolvePath(const QString& fileName) const
{
  if (m_DirectoryStructure == nullptr) {
    return QString();
  }
  const FileEntryPtr file =
      m_DirectoryStructure->searchFile(ToWString(fileName), nullptr);
  if (file.get() != nullptr) {
    return ToQString(file->getFullPath());
  } else {
    return QString();
  }
}

QStringList OrganizerCore::listDirectories(const QString& directoryName) const
{
  QStringList result;
  DirectoryEntry* dir = m_DirectoryStructure;
  if (!directoryName.isEmpty())
    dir = dir->findSubDirectoryRecursive(ToWString(directoryName));
  if (dir != nullptr) {
    for (const auto& d : dir->getSubDirectories()) {
      result.append(ToQString(d->getName()));
    }
  }
  return result;
}

QStringList
OrganizerCore::findFiles(const QString& path,
                         const std::function<bool(const QString&)>& filter) const
{
  QStringList result;
  DirectoryEntry* dir = m_DirectoryStructure;
  if (!path.isEmpty() && path != ".")
    dir = dir->findSubDirectoryRecursive(ToWString(path));
  if (dir != nullptr) {
    std::vector<FileEntryPtr> files = dir->getFiles();
    for (FileEntryPtr& file : files) {
      QString fullPath = ToQString(file->getFullPath());
      if (filter(ToQString(file->getName()))) {
        result.append(fullPath);
      }
    }
  }
  return result;
}

QStringList OrganizerCore::getFileOrigins(const QString& fileName) const
{
  QStringList result;
  const FileEntryPtr file =
      m_DirectoryStructure->searchFile(ToWString(fileName), nullptr);

  if (file.get() != nullptr) {
    result.append(
        ToQString(m_DirectoryStructure->getOriginByID(file->getOrigin()).getName()));
    for (const auto& i : file->getAlternatives()) {
      result.append(
          ToQString(m_DirectoryStructure->getOriginByID(i.originID()).getName()));
    }
  }
  return result;
}

QList<MOBase::IOrganizer::FileInfo> OrganizerCore::findFileInfos(
    const QString& path,
    const std::function<bool(const MOBase::IOrganizer::FileInfo&)>& filter) const
{
  QList<IOrganizer::FileInfo> result;
  DirectoryEntry* dir = m_DirectoryStructure;
  if (!path.isEmpty() && path != ".")
    dir = dir->findSubDirectoryRecursive(ToWString(path));
  if (dir != nullptr) {
    std::vector<FileEntryPtr> files = dir->getFiles();
    for (FileEntryPtr file : files) {
      IOrganizer::FileInfo info;
      info.filePath    = ToQString(file->getFullPath());
      bool fromArchive = false;
      info.origins.append(ToQString(
          m_DirectoryStructure->getOriginByID(file->getOrigin(fromArchive)).getName()));
      info.archive = fromArchive ? ToQString(file->getArchive().name()) : "";
      for (const auto& idx : file->getAlternatives()) {
        info.origins.append(
            ToQString(m_DirectoryStructure->getOriginByID(idx.originID()).getName()));
      }

      if (filter(info)) {
        result.append(info);
      }
    }
  }
  return result;
}

DownloadManager* OrganizerCore::downloadManager()
{
  return &m_DownloadManager;
}

PluginList* OrganizerCore::pluginList()
{
  return &m_PluginList;
}

ModList* OrganizerCore::modList()
{
  return &m_ModList;
}

bool OrganizerCore::previewFileWithAlternatives(QWidget* parent, QString fileName,
                                                int selectedOrigin)
{
  fileName = QDir::fromNativeSeparators(fileName);

  // what we have is an absolute path to the file in its actual location (for the
  // primary origin) what we want is the path relative to the virtual data directory

  // we need to look in the virtual directory for the file to make sure the info is up
  // to date.

  // check if the file comes from the actual data folder instead of a mod
  QDir gameDirectory   = managedGame()->dataDirectory().absolutePath();
  QString relativePath = gameDirectory.relativeFilePath(fileName);
  QDir dirRelativePath = gameDirectory.relativeFilePath(fileName);

  // if the file is on a different drive the dirRelativePath will actually be an
  // absolute path so we make sure that is not the case
  if (!dirRelativePath.isAbsolute() && !relativePath.startsWith("..")) {
    fileName = relativePath;
  } else {
    // crude: we search for the next slash after the base mod directory to skip
    // everything up to the data-relative directory
    int offset = settings().paths().mods().size() + 1;
    offset     = fileName.indexOf("/", offset);
    fileName   = fileName.mid(offset + 1);
  }

  const FileEntryPtr file =
      directoryStructure()->searchFile(ToWString(fileName), nullptr);

  if (file.get() == nullptr) {
    reportError(tr("file not found: %1").arg(qUtf8Printable(fileName)));
    return false;
  }

  // set up preview dialog
  PreviewDialog preview(fileName, parent);

  auto addFunc = [&](int originId, std::wstring archiveName = L"") {
    FilesOrigin& origin = directoryStructure()->getOriginByID(originId);
    QString filePath =
        QDir::fromNativeSeparators(ToQString(origin.getPath())) + "/" + fileName;
    if (QFile::exists(filePath)) {
      // it's very possible the file doesn't exist, because it's inside an archive. we
      // don't support that
      QWidget* wid = m_PluginManager->previewGenerator().genPreview(filePath);
      if (wid == nullptr) {
        reportError(tr("failed to generate preview for %1").arg(filePath));
      } else {
        preview.addVariant(ToQString(origin.getName()), wid);
      }
    } else if (archiveName != L"") {
      auto archiveFile = directoryStructure()->searchFile(archiveName);
      if (archiveFile.get() != nullptr) {
        try {
          libbsarch::bs_archive archiveLoader;
          archiveLoader.load_from_disk(archiveFile->getFullPath());
          libbsarch::memory_blob fileData =
              archiveLoader.extract_to_memory(fileName.toStdWString());
          QByteArray convertedFileData((char*)(fileData.data), fileData.size);
          QWidget* wid = m_PluginContainer->previewGenerator().genArchivePreview(
              convertedFileData, filePath);
          if (wid == nullptr) {
            reportError(tr("failed to generate preview for %1").arg(filePath));
          } else {
            preview.addVariant(ToQString(origin.getName()), wid);
          }
        } catch (std::exception& e) {
        }
      }
    }
  };

  if (selectedOrigin == -1) {
    // don't bother with the vector of origins, just add them as they come
    addFunc(file->getOrigin(), file->isFromArchive() ? file->getArchive().name() : L"");
    for (const auto& alt : file->getAlternatives()) {
      addFunc(alt.originID(), alt.isFromArchive() ? alt.archive().name() : L"");
    }
  } else {
    std::vector<int> origins;

    // start with the primary origin
    origins.push_back(file->getOrigin());

    // add other origins, push to front if it's the selected one
    for (const auto& alt : file->getAlternatives()) {
      if (alt.originID() == selectedOrigin) {
        origins.insert(origins.begin(), alt.originID());
      } else {
        origins.push_back(alt.originID());
      }
    }

    // can't be empty; either the primary origin was the selected one, or it
    // was one of the alternatives, which got inserted in front

    if (origins[0] != selectedOrigin) {
      // sanity check, this shouldn't happen unless the caller passed an
      // incorrect id

      log::warn("selected preview origin {} not found in list of alternatives",
                selectedOrigin);
    }

    for (int id : origins) {
      addFunc(id);
    }
  }

  if (preview.numVariants() > 0) {
    preview.exec();
    return true;
  } else {
    QMessageBox::information(parent, tr("Sorry"),
                             tr("Sorry, can't preview anything. This function "
                                "currently does not support extracting from bsas."));

    return false;
  }
}

bool OrganizerCore::previewFile(QWidget* parent, const QString& originName,
                                const QString& path)
{
  if (!QFile::exists(path)) {
    reportError(tr("File '%1' not found.").arg(path));
    return false;
  }

  PreviewDialog preview(path, parent);

  QWidget* wid = m_PluginManager->previewGenerator().genPreview(path);
  if (wid == nullptr) {
    reportError(tr("Failed to generate preview for %1").arg(path));
    return false;
  }

  preview.addVariant(originName, wid);
  preview.exec();

  return true;
}

boost::signals2::connection OrganizerCore::onAboutToRun(
    const std::function<bool(const QString&, const QDir&, const QString&)>& func)
{
  return m_AboutToRun.connect(func);
}

boost::signals2::connection OrganizerCore::onFinishedRun(
    const std::function<void(const QString&, unsigned int)>& func)
{
  return m_FinishedRun.connect(func);
}

boost::signals2::connection
OrganizerCore::onUserInterfaceInitialized(std::function<void(QMainWindow*)> const& func)
{
  return m_UserInterfaceInitialized.connect(func);
}

boost::signals2::connection
OrganizerCore::onProfileCreated(std::function<void(MOBase::IProfile*)> const& func)
{
  return m_ProfileCreated.connect(func);
}

boost::signals2::connection OrganizerCore::onProfileRenamed(
    std::function<void(MOBase::IProfile*, QString const&, QString const&)> const& func)
{
  return m_ProfileRenamed.connect(func);
}

boost::signals2::connection
OrganizerCore::onProfileRemoved(std::function<void(QString const&)> const& func)
{
  return m_ProfileRemoved.connect(func);
}

boost::signals2::connection
OrganizerCore::onProfileChanged(std::function<void(IProfile*, IProfile*)> const& func)
{
  return m_ProfileChanged.connect(func);
}

boost::signals2::connection OrganizerCore::onPluginSettingChanged(
    std::function<void(QString const&, const QString& key, const QVariant&,
                       const QVariant&)> const& func)
{
  return m_PluginSettingChanged.connect(func);
}

boost::signals2::connection
OrganizerCore::onPluginEnabled(std::function<void(const IPlugin*)> const& func)
{
  return m_PluginEnabled.connect(func);
}

boost::signals2::connection
OrganizerCore::onPluginDisabled(std::function<void(const IPlugin*)> const& func)
{
  return m_PluginDisabled.connect(func);
}

boost::signals2::connection
OrganizerCore::onNextRefresh(std::function<void()> const& func,
                             RefreshCallbackGroup group, RefreshCallbackMode mode)
{
  if (m_DirectoryUpdate || mode == RefreshCallbackMode::FORCE_WAIT_FOR_REFRESH) {
    return m_OnNextRefreshCallbacks.connect(static_cast<int>(group), func);
  } else {
    func();
    return {};
  }
}

void OrganizerCore::refresh(bool saveChanges)
{
  // don't lose changes!
  if (saveChanges) {
    m_CurrentProfile->writeModlistNow(true);
  }

  updateModInfoFromDisc();
  m_CurrentProfile->refreshModStatus();

  m_ModList.notifyChange(-1);

  refreshDirectoryStructure();

  emit refreshTriggered();
}

void OrganizerCore::refreshESPList(bool force)
{
  onNextRefresh(
      [this, force] {
        TimeThis tt("OrganizerCore::refreshESPList()");

        m_CurrentProfile->writeModlist();

        // clear list
        try {
          m_PluginList.refresh(m_CurrentProfile->name(), *m_DirectoryStructure,
                               m_CurrentProfile->getLockedOrderFileName(), force);
        } catch (const std::exception& e) {
          reportError(tr("Failed to refresh list of esps: %1").arg(e.what()));
        }
      },
      RefreshCallbackGroup::CORE, RefreshCallbackMode::RUN_NOW_IF_POSSIBLE);
}

void OrganizerCore::refreshBSAList()
{
  TimeThis tt("OrganizerCore::refreshBSAList()");

  auto archives = gameFeatures().gameFeature<DataArchives>();

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
      m_UserInterface->archivesWriter().write();
    }

    m_ArchivesInit = true;
  }
}

void OrganizerCore::refreshLists()
{
  if ((m_CurrentProfile != nullptr) && m_DirectoryStructure->isPopulated()) {
    refreshESPList(true);
    refreshBSAList();
  }  // no point in refreshing lists if no files have been added to the directory
     // tree
}

void OrganizerCore::updateModActiveState(int index, bool active)
{
  QList<unsigned int> modsToUpdate;
  modsToUpdate.append(index);
  updateModsActiveState(modsToUpdate, active);
}

void OrganizerCore::updateModsActiveState(const QList<unsigned int>& modIndices,
                                          bool active)
{
  int enabled = 0;
  for (auto index : modIndices) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
    QDir dir(modInfo->absolutePath());
    for (const QString& esm : dir.entryList(QStringList() << "*.esm", QDir::Files)) {
      const FileEntryPtr file = m_DirectoryStructure->findFile(ToWString(esm));
      if (file.get() == nullptr) {
        log::warn("failed to activate {}", esm);
        continue;
      }

      if (active != m_PluginList.isEnabled(esm) && file->getAlternatives().empty()) {
        m_PluginList.blockSignals(true);
        m_PluginList.enableESP(esm, active);
        m_PluginList.blockSignals(false);
      }
    }

    for (const QString& esl : dir.entryList(QStringList() << "*.esl", QDir::Files)) {
      const FileEntryPtr file = m_DirectoryStructure->findFile(ToWString(esl));
      if (file.get() == nullptr) {
        log::warn("failed to activate {}", esl);
        continue;
      }

      if (active != m_PluginList.isEnabled(esl) && file->getAlternatives().empty()) {
        m_PluginList.blockSignals(true);
        m_PluginList.enableESP(esl, active);
        m_PluginList.blockSignals(false);
        ++enabled;
      }
    }
    QStringList esps = dir.entryList(QStringList() << "*.esp", QDir::Files);
    for (const QString& esp : esps) {
      const FileEntryPtr file = m_DirectoryStructure->findFile(ToWString(esp));
      if (file.get() == nullptr) {
        log::warn("failed to activate {}", esp);
        continue;
      }

      if (active != m_PluginList.isEnabled(esp) && file->getAlternatives().empty()) {
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

void OrganizerCore::updateModsInDirectoryStructure(
    QMap<unsigned int, ModInfo::Ptr> modInfo)
{
  std::vector<DirectoryRefresher::EntryInfo> entries;

  for (auto idx : modInfo.keys()) {
    entries.push_back({modInfo[idx]->name(),
                       modInfo[idx]->absolutePath(),
                       modInfo[idx]->stealFiles(),
                       {},
                       m_CurrentProfile->getModPriority(idx)});
  }

  m_DirectoryRefresher->addMultipleModsFilesToStructure(m_DirectoryStructure, entries);

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
  m_DirectoryRefresher->setMods(m_CurrentProfile->getActiveMods(),
                                std::set<QString>(archives.begin(), archives.end()));

  // finally also add files from bsas to the directory structure
  for (auto idx : modInfo.keys()) {
    m_DirectoryRefresher->addModBSAToStructure(
        m_DirectoryStructure, modInfo[idx]->name(),
        m_CurrentProfile->getModPriority(idx), modInfo[idx]->absolutePath(),
        modInfo[idx]->archives());
  }
}

void OrganizerCore::loggedInAction(QWidget* parent, std::function<void()> f)
{
  if (NexusInterface::instance().getAccessManager()->validated()) {
    f();
  } else if (!m_Settings.network().offlineMode()) {
    QString apiKey;
    if (GlobalSettings::nexusApiKey(apiKey)) {
      doAfterLogin([f] {
        f();
      });
      NexusInterface::instance().getAccessManager()->apiCheck(apiKey);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus"), parent);
    }
  }
}

void OrganizerCore::requestDownload(const QUrl& url, QNetworkReply* reply)
{
  if (!m_PluginManager) {
    return;
  }
  for (IPluginModPage* modPage : m_PluginManager->plugins<MOBase::IPluginModPage>()) {
    if (m_PluginManager->isEnabled(modPage)) {
      ModRepositoryFileInfo* fileInfo = new ModRepositoryFileInfo();
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
    int modID        = 0;
    int fileID       = 0;
    QRegularExpression nameExp("www\\.nexusmods\\.com/(\\a+)/");
    auto match = nameExp.match(url.toString());
    if (match.hasMatch()) {
      gameName = match.captured(1);
    }
    QRegularExpression modExp("mods/(\\d+)");
    match = modExp.match(url.toString());
    if (match.hasMatch()) {
      modID = match.captured(1).toInt();
    }
    QRegularExpression fileExp("fid=(\\d+)");
    match = fileExp.match(url.toString());
    if (match.hasMatch()) {
      fileID = match.captured(1).toInt();
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
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      m_DownloadManager.addDownload(reply, new ModRepositoryFileInfo());
    }
  }
}

PluginManager& OrganizerCore::pluginManager() const
{
  return *m_PluginManager;
}

GameFeatures& OrganizerCore::gameFeatures() const
{
  return pluginManager().gameFeatures();
}

IPluginGame const* OrganizerCore::managedGame() const
{
  return m_GamePlugin;
}

IOrganizer const* OrganizerCore::managedGameOrganizer() const
{
  return m_PluginManager->details(m_GamePlugin).proxy();
}

std::vector<QString> OrganizerCore::enabledArchives()
{
  std::vector<QString> result;
  if (settings().archiveParsing()) {
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
  const auto archives      = enabledArchives();

  m_DirectoryRefresher->setMods(activeModList,
                                std::set<QString>(archives.begin(), archives.end()));

  // runs refresh() in a thread
  QTimer::singleShot(0, m_DirectoryRefresher.get(), &DirectoryRefresher::refresh);
}

void OrganizerCore::onDirectoryRefreshed()
{
  log::debug("directory refreshed, finishing up");
  TimeThis tt("OrganizerCore::onDirectoryRefreshed()");

  DirectoryEntry* newStructure = m_DirectoryRefresher->stealDirectoryStructure();
  Q_ASSERT(newStructure != m_DirectoryStructure);

  if (newStructure == nullptr) {
    // TODO: don't know why this happens, this slot seems to get called twice
    // with only one emit
    return;
  }

  std::swap(m_DirectoryStructure, newStructure);
  m_VirtualFileTree.invalidate();

  if (m_StructureDeleter.joinable()) {
    m_StructureDeleter.join();
  }

  m_StructureDeleter = MOShared::startSafeThread([=] {
    log::debug("structure deleter thread start");
    delete newStructure;
    log::debug("structure deleter thread done");
  });

  log::debug("clearing caches");
  for (int i = 0; i < m_ModList.rowCount(); ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
    modInfo->clearCaches();
  }

  // needs to be done before post refresh tasks
  m_DirectoryUpdate = false;

  log::debug("running post refresh tasks");
  m_OnNextRefreshCallbacks();
  m_OnNextRefreshCallbacks.disconnect_all_slots();

  if (m_CurrentProfile != nullptr) {
    log::debug("refreshing lists");
    refreshLists();
  }

  emit directoryStructureReady();

  log::debug("refresh done");
}

void OrganizerCore::clearCaches(std::vector<unsigned int> const& indices) const
{
  const auto insert = [](auto& dest, const auto& from) {
    dest.insert(from.begin(), from.end());
  };
  std::set<unsigned int> allIndices;
  for (const auto index : indices) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);

    if (m_CurrentProfile->modEnabled(index)) {
      // if the mod is enabled, we need to first clear its cache so that
      // getModOverwrite(), ..., returns the newly conflicting mods (in case
      // the mod just got enabled)
      modInfo->clearCaches();
      insert(allIndices, modInfo->getModOverwrite());
      insert(allIndices, modInfo->getModOverwritten());
      insert(allIndices, modInfo->getModArchiveOverwrite());
      insert(allIndices, modInfo->getModArchiveOverwritten());
      insert(allIndices, modInfo->getModArchiveLooseOverwrite());
      insert(allIndices, modInfo->getModArchiveLooseOverwritten());
    } else {
      // if the mod is disabled, we need to first fetch the conflicting
      // mods, and then clear the cache
      insert(allIndices, modInfo->getModOverwrite());
      insert(allIndices, modInfo->getModOverwritten());
      insert(allIndices, modInfo->getModArchiveOverwrite());
      insert(allIndices, modInfo->getModArchiveOverwritten());
      insert(allIndices, modInfo->getModArchiveLooseOverwrite());
      insert(allIndices, modInfo->getModArchiveLooseOverwritten());
      modInfo->clearCaches();
    }
  }

  for (auto& index : allIndices) {
    ModInfo::getByIndex(index)->clearCaches();
  }
}

void OrganizerCore::modPrioritiesChanged(const QModelIndexList& indices)
{
  for (unsigned int i = 0; i < currentProfile()->numMods(); ++i) {
    int priority = currentProfile()->getModPriority(i);
    if (currentProfile()->modEnabled(i)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      // priorities in the directory structure are one higher because data is 0
      directoryStructure()
          ->getOriginByName(MOBase::ToWString(modInfo->internalName()))
          .setPriority(priority + 1);
    }
  }
  refreshBSAList();
  currentProfile()->writeModlist();
  directoryStructure()->getFileRegister()->sortOrigins();

  std::vector<unsigned int> vindices;

  for (auto& idx : indices) {
    vindices.push_back(idx.data(ModList::IndexRole).toInt());
  }

  clearCaches(vindices);
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
        FilesOrigin& origin =
            m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
        origin.enable(false);
      }
      if (m_UserInterface != nullptr) {
        m_UserInterface->archivesWriter().write();
      }
    }

    for (unsigned int i = 0; i < m_CurrentProfile->numMods(); ++i) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      int priority         = m_CurrentProfile->getModPriority(i);
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        // priorities in the directory structure are one higher because data is
        // 0
        m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()))
            .setPriority(priority + 1);
      }
    }
    m_DirectoryStructure->getFileRegister()->sortOrigins();

    refreshLists();
    clearCaches({index});
    m_ModList.notifyModStateChanged({index});

  } catch (const std::exception& e) {
    reportError(tr("failed to update mod list: %1").arg(e.what()));
  }
}

void OrganizerCore::modStatusChanged(QList<unsigned int> index)
{
  try {
    QMap<unsigned int, ModInfo::Ptr> modsToEnable;
    QMap<unsigned int, ModInfo::Ptr> modsToDisable;
    std::vector<unsigned int> vindices;
    for (auto idx : index) {
      if (m_CurrentProfile->modEnabled(idx)) {
        modsToEnable[idx] = ModInfo::getByIndex(idx);
      } else {
        modsToDisable[idx] = ModInfo::getByIndex(idx);
      }
      vindices.push_back(idx);
    }
    if (!modsToEnable.isEmpty()) {
      updateModsInDirectoryStructure(modsToEnable);
    }
    if (!modsToDisable.isEmpty()) {
      updateModsActiveState(modsToDisable.keys(), false);
      for (auto idx : modsToDisable.keys()) {
        if (m_DirectoryStructure->originExists(ToWString(modsToDisable[idx]->name()))) {
          FilesOrigin& origin = m_DirectoryStructure->getOriginByName(
              ToWString(modsToDisable[idx]->name()));
          origin.enable(false);
        }
      }
      if (m_UserInterface != nullptr) {
        m_UserInterface->archivesWriter().write();
      }
    }

    for (unsigned int i = 0; i < m_CurrentProfile->numMods(); ++i) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      int priority         = m_CurrentProfile->getModPriority(i);
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        // priorities in the directory structure are one higher because data is
        // 0
        m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()))
            .setPriority(priority + 1);
      }
    }
    m_DirectoryStructure->getFileRegister()->sortOrigins();

    refreshLists();
    clearCaches(vindices);
    m_ModList.notifyModStateChanged(index);

  } catch (const std::exception& e) {
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
  NexusInterface::instance().loginCompleted();
}

void OrganizerCore::loginSuccessfulUpdate(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), qApp->activeWindow());
  }
  m_Updater.startUpdate();
}

void OrganizerCore::loginFailed(const QString& message)
{
  qCritical().nospace().noquote() << "Nexus API validation failed: " << message;

  if (QMessageBox::question(qApp->activeWindow(), tr("Login failed"),
                            tr("Login failed, try again?")) == QMessageBox::Yes) {
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
  NexusInterface::instance().loginCompleted();
}

void OrganizerCore::loginFailedUpdate(const QString& message)
{
  MessageDialog::showMessage(
      tr("login failed: %1. You need to log-in with Nexus to update MO.").arg(message),
      qApp->activeWindow());
}

void OrganizerCore::syncOverwrite()
{
  ModInfo::Ptr modInfo = ModInfo::getOverwrite();
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
  if (auto extender = gameFeatures().gameFeature<ScriptExtender>()) {
    QString hookdll =
        QDir::toNativeSeparators(managedGame()->dataDirectory().absoluteFilePath(
            extender->PluginPath() + "/hook.dll"));
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
    // This warning will now be shown every time the problems are checked, which is a
    // bit of a "log spam". But since this is a sever error which will most likely make
    // the game crash/freeze/etc. and is very hard to diagnose,  this "log spam" will
    // make it easier for the user to notice the warning.
    log::warn("hook.dll found in game folder: {}", hookdll);
    problems.push_back(PROBLEM_MO1SCRIPTEXTENDERWORKAROUND);
  }
  return problems;
}

QString OrganizerCore::shortDescription(unsigned int key) const
{
  switch (key) {
  case PROBLEM_MO1SCRIPTEXTENDERWORKAROUND: {
    return tr(
        "MO1 \"Script Extender\" load mechanism has left hook.dll in your game folder");
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
    return tr("<a href=\"%1\">hook.dll</a> has been found in your game folder (right "
              "click to copy the full path). "
              "This is most likely a leftover of setting the ModOrganizer 1 load "
              "mechanism to \"Script Extender\", "
              "in which case you must remove this file either by changing the load "
              "mechanism in ModOrganizer 1 or "
              "manually removing the file, otherwise the game is likely to crash and "
              "burn.")
        .arg(oldMO1HookDll());
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

void OrganizerCore::startGuidedFix(unsigned int) const {}

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
  } catch (const std::exception& e) {
    reportError(tr("failed to save load order: %1").arg(e.what()));
  }

  return true;
}

void OrganizerCore::savePluginList()
{
  onNextRefresh(
      [this]() {
        m_PluginList.saveTo(m_CurrentProfile->getLockedOrderFileName());
        m_PluginList.saveLoadOrder(*m_DirectoryStructure);
      },
      RefreshCallbackGroup::CORE, RefreshCallbackMode::RUN_NOW_IF_POSSIBLE);
}

void OrganizerCore::saveCurrentProfile()
{
  if (m_CurrentProfile == nullptr) {
    return;
  }

  m_CurrentProfile->writeModlist();
  m_CurrentProfile->createTweakedIniFile();
  saveCurrentLists();
  storeSettings();
}

ProcessRunner OrganizerCore::processRunner()
{
  return ProcessRunner(*this, m_UserInterface);
}

bool OrganizerCore::beforeRun(
    const QFileInfo& binary, const QDir& cwd, const QString& arguments,
    const QString& profileName, const QString& customOverwrite,
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

  if (!m_AboutToRun(binary.absoluteFilePath(), cwd, arguments)) {
    log::debug("start of \"{}\" cancelled by plugin", binary.absoluteFilePath());
    return false;
  }

  try {
    m_USVFS.updateMapping(fileMapping(profileName, customOverwrite));
    m_USVFS.updateForcedLibraries(forcedLibraries);
  } catch (const UsvfsConnectorException& e) {
    log::debug("{}", e.what());
    return false;
  } catch (const std::exception& e) {
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
  // need to remove our stored load order because it may be outdated if a
  // foreign tool changed the file time. After removing that file,
  // refreshESPList will use the file time as the order
  if (managedGame()->loadOrderMechanism() ==
      IPluginGame::LoadOrderMechanism::FileTime) {
    log::debug("removing loadorder.txt");
    QFile::remove(m_CurrentProfile->getLoadOrderFileName());
  }

  refreshDirectoryStructure();

  refreshESPList(true);
  savePluginList();
  cycleDiagnostics();

  // These callbacks should not fiddle with directory structure and ESPs.
  m_FinishedRun(binary.absoluteFilePath(), exitCode);
}

ProcessRunner::Results OrganizerCore::waitForAllUSVFSProcesses(UILocker::Reasons reason)
{
  return processRunner().waitForAllUSVFSProcessesWithLock(reason);
}

std::vector<Mapping> OrganizerCore::fileMapping(const QString& profileName,
                                                const QString& customOverwrite)
{
  // need to wait until directory structure is ready
  if (m_DirectoryUpdate) {
    QEventLoop loop;
    connect(this, &OrganizerCore::directoryStructureReady, &loop, &QEventLoop::quit,
            Qt::ConnectionType::QueuedConnection);
    loop.exec();
  }

  IPluginGame* game = qApp->property("managed_game").value<IPluginGame*>();
  Profile profile(QDir(m_Settings.paths().profiles() + "/" + profileName), game,
                  gameFeatures());

  MappingType result;

  QStringList dataPaths;
  dataPaths.append(QDir::toNativeSeparators(game->dataDirectory().absolutePath()));

  for (auto directory : game->secondaryDataDirectories()) {
    dataPaths.append(directory.absolutePath());
  }

  bool overwriteActive = false;

  for (const auto& mod : profile.getActiveMods()) {
    if (std::get<0>(mod).compare("overwrite", Qt::CaseInsensitive) == 0) {
      continue;
    }

    unsigned int modIndex = ModInfo::getIndex(std::get<0>(mod));
    ModInfo::Ptr modPtr   = ModInfo::getByIndex(modIndex);

    bool createTarget = customOverwrite == std::get<0>(mod);

    overwriteActive |= createTarget;

    if (modPtr->isRegular()) {
      for (auto dataPath : dataPaths) {
        result.insert(result.end(), {QDir::toNativeSeparators(std::get<1>(mod)),
                                     dataPath, true, createTarget});
      }
    }
  }

  if (!overwriteActive && !customOverwrite.isEmpty()) {
    throw MyException(
        tr("The designated write target \"%1\" is not enabled.").arg(customOverwrite));
  }

  if (m_CurrentProfile->localSavesEnabled()) {
    auto localSaves = gameFeatures().gameFeature<LocalSavegames>();
    if (localSaves != nullptr) {
      MappingType saveMap =
          localSaves->mappings(currentProfile()->absolutePath() + "/saves");
      result.reserve(result.size() + saveMap.size());
      result.insert(result.end(), saveMap.begin(), saveMap.end());
    } else {
      log::warn("local save games not supported by this game plugin");
    }
  }

  for (auto dataPath : dataPaths) {
    result.insert(result.end(),
                  {QDir::toNativeSeparators(m_Settings.paths().overwrite()), dataPath,
                   true, customOverwrite.isEmpty()});
  }

  // ini bakery
  {
    const auto iniBakeryMapping = m_IniBakery->mappings();
    result.reserve(result.size() + iniBakeryMapping.size());
    result.insert(result.end(), iniBakeryMapping.begin(), iniBakeryMapping.end());
  }

  for (MOBase::IPluginFileMapper* mapper :
       m_PluginManager->plugins<MOBase::IPluginFileMapper>()) {
    IPlugin* plugin = dynamic_cast<IPlugin*>(mapper);
    if (m_PluginManager->isEnabled(plugin)) {
      MappingType pluginMap = mapper->mappings();
      result.reserve(result.size() + pluginMap.size());
      result.insert(result.end(), pluginMap.begin(), pluginMap.end());
    }
  }

  return result;
}
