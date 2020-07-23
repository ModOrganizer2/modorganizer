/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mainwindow.h"
#include "singleinstance.h"
#include "loglist.h"
#include "selectiondialog.h"
#include "moapplication.h"
#include "tutorialmanager.h"
#include "nxmaccessmanager.h"
#include "instancemanager.h"
#include "organizercore.h"
#include "env.h"
#include "envmodule.h"
#include "commandline.h"

#include "shared/util.h"
#include "shared/appconfig.h"

#include <report.h>
#include <usvfs.h>
#include <log.h>
#include <utility.h>

// see addDllsToPath() below
#pragma comment(linker, "/manifestDependency:\"" \
    "name='dlls' " \
    "processorArchitecture='x86' " \
    "version='1.0.0.0' " \
    "type='win32' \"")


using namespace MOBase;
using namespace MOShared;

void sanityChecks(const env::Environment& env);
int checkIncompatibleModule(const env::Module& m);
int checkPathsForSanity(MOBase::IPluginGame& game, const Settings& s);

void purgeOldFiles()
{
  // remove the temporary backup directory in case we're restarting after an
  // update
  QString backupDirectory = qApp->applicationDirPath() + "/update_backup";
  if (QDir(backupDirectory).exists()) {
    shellDelete(QStringList(backupDirectory));
  }

  // cycle log file
  removeOldFiles(
    qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::logPath()),
    "usvfs*.log", 5, QDir::Name);
}

thread_local LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = nullptr;
thread_local std::terminate_handler g_prevTerminateHandler = nullptr;

LONG WINAPI onUnhandledException(_EXCEPTION_POINTERS* ptrs)
{
  const std::wstring& dumpPath = OrganizerCore::crashDumpsPath();

  const int r = CreateMiniDump(
    ptrs, OrganizerCore::getGlobalCrashDumpsType(), dumpPath.c_str());

  if (r == 0) {
    log::error("ModOrganizer has crashed, crash dump created.");
  } else {
    log::error(
      "ModOrganizer has crashed, CreateMiniDump failed ({}, error {}).",
      r, GetLastError());
  }

  if (g_prevExceptionFilter && ptrs)
    return g_prevExceptionFilter(ptrs);
  else
    return EXCEPTION_CONTINUE_SEARCH;
}

void onTerminate() noexcept
{
  __try
  {
    // force an exception to get a valid stack trace for this thread
    *(int*)0 = 42;
  }
  __except
    (
      onUnhandledException(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER
      )
  {
  }

  if (g_prevTerminateHandler) {
    g_prevTerminateHandler();
  } else {
    std::abort();
  }
}

void setExceptionHandlers()
{
  g_prevExceptionFilter = SetUnhandledExceptionFilter(onUnhandledException);
  g_prevTerminateHandler = std::set_terminate(onTerminate);
}

QString determineProfile(const cl::CommandLine& cl, const Settings &settings)
{
  auto selectedProfileName = settings.game().selectedProfileName();

  if (cl.profile()) {
    log::debug("profile overwritten on command line");
    selectedProfileName = *cl.profile();
  }

  if (!selectedProfileName) {
    log::debug("no configured profile");
    selectedProfileName = "Default";
  }

  return *selectedProfileName;
}

MOBase::IPluginGame *selectGame(
  Settings &settings, QDir const &gamePath, MOBase::IPluginGame *game)
{
  settings.game().setName(game->gameName());

  QString gameDir = gamePath.absolutePath();
  game->setGamePath(gameDir);

  settings.game().setDirectory(gameDir);

  return game;
}


MOBase::IPluginGame *determineCurrentGame(
  QString const &moPath, Settings &settings, PluginContainer const &plugins)
{
  //Determine what game we are running where. Be very paranoid in case the
  //user has done something odd.

  //If the game name has been set up, try to use that.
  const auto gameName = settings.game().name();
  const bool gameConfigured = (gameName.has_value() && *gameName != "");

  if (gameConfigured) {
    MOBase::IPluginGame *game = plugins.managedGame(*gameName);
    if (game == nullptr) {
      reportError(
        QObject::tr("Plugin to handle %1 no longer installed. An antivirus might have deleted files.")
        .arg(*gameName));

      return nullptr;
    }

    auto gamePath = settings.game().directory();
    if (!gamePath || *gamePath == "") {
      gamePath = game->gameDirectory().absolutePath();
    }

    QDir gameDir(*gamePath);
    QFileInfo directoryInfo(gameDir.path());

    if (directoryInfo.isSymLink()) {
      reportError(QObject::tr("The configured path to the game directory (%1) appears to be a symbolic (or other) link. "
        "This setup is incompatible with MO2's VFS and will not run correctly.").arg(*gamePath));
    }

    if (game->looksValid(gameDir)) {
      return selectGame(settings, gameDir, game);
    }
  }

  //If we've made it this far and the instance is already configured for a game, something has gone wrong.
  //Tell the user about it.
  if (gameConfigured) {
    const auto gamePath = settings.game().directory();

    reportError(
      QObject::tr("Could not use configuration settings for game \"%1\", path \"%2\".")
      .arg(*gameName).arg(gamePath ? *gamePath : ""));
  }

  SelectionDialog selection(gameConfigured ?
    QObject::tr("Please select the installation of %1 to manage").arg(*gameName) :
    QObject::tr("Please select the game to manage"), nullptr, QSize(32, 32));

  for (IPluginGame *game : plugins.plugins<IPluginGame>()) {
    //If a game is already configured, skip any plugins that are not for that game
    if (gameConfigured && gameName->compare(game->gameName(), Qt::CaseInsensitive) != 0)
      continue;

    //Only add games that are installed
    if (game->isInstalled()) {
      QString path = game->gameDirectory().absolutePath();
      selection.addChoice(game->gameIcon(), game->gameName(), path, QVariant::fromValue(game));
    }
  }

  selection.addChoice(QString("Browse..."), QString(), QVariant::fromValue(static_cast<IPluginGame *>(nullptr)));

  while (selection.exec() != QDialog::Rejected) {
    IPluginGame * game = selection.getChoiceData().value<IPluginGame *>();
    QString gamePath = selection.getChoiceDescription();
    QFileInfo directoryInfo(gamePath);
    if (directoryInfo.isSymLink()) {
      reportError(QObject::tr("The configured path to the game directory (%1) appears to be a symbolic (or other) link. "
        "This setup is incompatible with MO2's VFS and will not run correctly.").arg(gamePath));
    }
    if (game != nullptr) {
      return selectGame(settings, game->gameDirectory(), game);
    }

    gamePath = QFileDialog::getExistingDirectory(nullptr, gameConfigured ?
        QObject::tr("Please select the installation of %1 to manage").arg(*gameName) :
        QObject::tr("Please select the game to manage"),
      QString(), QFileDialog::ShowDirsOnly);

    if (!gamePath.isEmpty()) {
      QDir gameDir(gamePath);
      QFileInfo directoryInfo(gamePath);
      if (directoryInfo.isSymLink()) {
        reportError(QObject::tr("The configured path to the game directory (%1) appears to be a symbolic (or other) link. "
          "This setup is incompatible with MO2's VFS and will not run correctly.").arg(gamePath));
      }
      QList<IPluginGame *> possibleGames;
      for (IPluginGame * const game : plugins.plugins<IPluginGame>()) {
        //If a game is already configured, skip any plugins that are not for that game
        if (gameConfigured && gameName->compare(game->gameName(), Qt::CaseInsensitive) != 0)
          continue;

        //Only try plugins that look valid for this directory
        if (game->looksValid(gameDir)) {
          possibleGames.append(game);
        }
      }

      if (possibleGames.count() > 1) {
        SelectionDialog browseSelection(gameConfigured ?
            QObject::tr("Please select the installation of %1 to manage").arg(*gameName) :
            QObject::tr("Please select the game to manage"),
          nullptr, QSize(32, 32));

        for (IPluginGame *game : possibleGames) {
          browseSelection.addChoice(game->gameIcon(), game->gameName(), gamePath, QVariant::fromValue(game));
        }

        if (browseSelection.exec() == QDialog::Accepted) {
          return selectGame(settings, gameDir, browseSelection.getChoiceData().value<IPluginGame *>());
        } else {
          reportError(gameConfigured ?
            QObject::tr("Canceled finding %1 in \"%2\".").arg(*gameName).arg(gamePath) :
            QObject::tr("Canceled finding game in \"%1\".").arg(gamePath));
        }
      } else if(possibleGames.count() == 1) {
        return selectGame(settings, gameDir, possibleGames[0]);
      } else {
        if (gameConfigured) {
          reportError(
            QObject::tr("%1 not identified in \"%2\". The directory is required to contain the game binary.")
            .arg(*gameName).arg(gamePath));
        } else {
          QString supportedGames;

          for (IPluginGame * const game : plugins.plugins<IPluginGame>()) {
            supportedGames += "<li>" + game->gameName() + "</li>";
          }

          QString text = QObject::tr(
            "No game identified in \"%1\". The directory is required to "
            "contain the game binary.<br><br>"
            "<b>These are the games supported by Mod Organizer:</b>"
            "<ul>%2</ul>")
            .arg(gamePath)
            .arg(supportedGames);

          reportError(text);
        }
      }
    }
  }

  return nullptr;
}


// This adds the `dlls` directory to the path so the dlls can be found. How
// MO is able to find dlls in there is a bit convoluted:
//
// Dependencies on DLLs can be baked into an executable by passing a
// `manifestdependency` option to the linker. This can be done on the command
// line or with a pragma. Typically, the dependency will not be a hardcoded
// filename, but an assembly name, such as Microsoft.Windows.Common-Controls.
//
// When Windows loads the exe, it will look for this assembly in a variety of
// places, such as in the WinSxS folder, but also in the program's folder. It
// will look for `assemblyname.dll` or `assemblyname/assemblyname.dll` and try
// to load that.
//
// If these files don't exist, then the loader gets creative and looks for
// `assemblyname.manifest` and `assemblyname/assemblyname.manifest`. A manifest
// file is just an XML file that can contain a list of DLLs to load for this
// assembly.
//
// In MO's case, there's a `pragma` at the beginning of this file which adds
// `dlls` as an "assembly" dependency. This is a bit of a hack to just force
// the loader to eventually find `dlls/dlls.manifest`, which contains the list
// of all the DLLs MO requires to load.
//
// This file was handwritten in `modorganizer/src/dlls.manifest.qt5` and
// is copied and renamed in CMakeLists.txt into `bin/dlls/dlls.manifest`. Note
// that the useless and incorrect .qt5 extension is removed.
//
void addDllsToPath()
{
  const auto dllsPath = QDir::toNativeSeparators(
    QCoreApplication::applicationDirPath() + "/dlls");

  QCoreApplication::setLibraryPaths(
    QStringList(dllsPath) + QCoreApplication::libraryPaths());

  env::prependToPath(dllsPath);
}

QString getSplashPath(
  const Settings& settings, const QString& dataPath,
  const MOBase::IPluginGame* game)
{
  if (!settings.useSplash()) {
    return {};
  }

  const QString splashPath = dataPath + "/splash.png";
  if (QFile::exists(dataPath + "/splash.png")) {
    return splashPath;
  }

  // currently using MO splash, see if the plugin contains one
  QString pluginSplash = QString(":/%1/splash").arg(game->gameShortName());
  QImage image(pluginSplash);

  if (image.isNull()) {
    return {};
  }

  image.save(splashPath);
  return splashPath;
}

std::unique_ptr<QSplashScreen> createSplash(
  const Settings& settings, const QString& dataPath,
  const MOBase::IPluginGame* game)
{
  const auto splashPath = getSplashPath(settings, dataPath, game);
  if (splashPath.isEmpty()) {
    return {};
  }

  QPixmap image(splashPath);
  if (image.isNull()) {
    log::error("failed to load splash from {}", splashPath);
    return {};
  }

  auto splash = std::make_unique<QSplashScreen>(image);
  settings.geometry().centerOnMainWindowMonitor(splash.get());

  splash->show();
  splash->activateWindow();

  return splash;
}

int runApplication(
  MOApplication &application, const cl::CommandLine& cl,
  SingleInstance &instance, const QString &dataPath)
{
  TimeThis tt("runApplication() to exec()");

  log::info(
    "starting Mod Organizer version {} revision {} in {}, usvfs: {}",
    createVersionInfo().displayString(3), GITID,
    QCoreApplication::applicationDirPath(), MOShared::getUsvfsVersionString());

  log::info("data path: {}", dataPath);

  if (InstanceManager::isPortablePath(dataPath)) {
    log::debug("this is a portable instance");
  }

  log::info("working directory: {}", QDir::currentPath());

  if (!instance.secondary()) {
    purgeOldFiles();
  }

  QWindowsWindowFunctions::setWindowActivationBehavior(
    QWindowsWindowFunctions::AlwaysActivateWindow);

  try
  {
    Settings settings(dataPath + "/" + QString::fromStdWString(AppConfig::iniFileName()));
    log::getDefault().setLevel(settings.diagnostics().logLevel());

    log::debug("using ini at '{}'", settings.filename());

    if (instance.secondary()) {
      log::debug("another instance of MO is running but --multiple was given");
    }

    // global crashDumpType sits in OrganizerCore to make a bit less ugly to
    // update it when the settings are changed during runtime
    OrganizerCore::setGlobalCrashDumpsType(settings.diagnostics().crashDumpsType());

    env::Environment env;
    env.dump(settings);
    settings.dump();
    sanityChecks(env);

    const auto moduleNotification = env.onModuleLoaded(qApp, [](auto&& m) {
      log::debug("loaded module {}", m.toString());
      checkIncompatibleModule(m);
    });

    // this must outlive `organizer`
    std::unique_ptr<PluginContainer> pluginContainer;

    log::debug("initializing core");
    OrganizerCore organizer(settings);
    if (!organizer.bootstrap()) {
      reportError("failed to set up data paths");
      InstanceManager::instance().clearCurrentInstance();
      return 1;
    }

    log::debug("initializing plugins");
    pluginContainer = std::make_unique<PluginContainer>(&organizer);
    pluginContainer->loadPlugins();

    MOBase::IPluginGame* game = determineCurrentGame(
        application.applicationDirPath(), settings, *pluginContainer);

    if (game == nullptr) {
      InstanceManager &instance = InstanceManager::instance();
      QString instanceName = instance.currentInstance();

      if (instanceName.compare("Portable", Qt::CaseInsensitive) != 0) {
        instance.clearCurrentInstance();
        return RestartExitCode;
      }

      return 1;
    }

    checkPathsForSanity(*game, settings);


    organizer.setManagedGame(game);
    organizer.createDefaultProfile();

    QString edition;

    if (auto v=settings.game().edition()) {
      edition = *v;
    } else {
      QStringList editions = game->gameVariants();
      if (editions.size() > 1) {
        SelectionDialog selection(
            QObject::tr("Please select the game edition you have (MO can't "
                        "start the game correctly if this is set "
                        "incorrectly!)"),
            nullptr);
        selection.setWindowFlag(Qt::WindowStaysOnTopHint, true);
        int index = 0;
        for (const QString &edition : editions) {
          selection.addChoice(edition, "", index++);
        }
        if (selection.exec() == QDialog::Rejected) {
          return 1;
        } else {
          edition = selection.getChoiceString();
          settings.game().setEdition(edition);
        }
      }
    }

    game->setGameVariant(edition);

    log::info(
      "using game plugin '{}' ('{}', steam id '{}') at {}",
      game->gameName(), game->gameShortName(), game->steamAPPId(),
      game->gameDirectory().absolutePath());

    CategoryFactory::instance().loadCategories();
    organizer.updateExecutablesList();
    organizer.updateModInfoFromDisc();

    QString selectedProfileName = determineProfile(cl, settings);
    organizer.setCurrentProfile(selectedProfileName);

    // if we have a command line parameter, it is either a nxm link or
    // a binary to start
    if (cl.shortcut().isValid()) {
      if (cl.shortcut().hasExecutable()) {
        try {
	      organizer.processRunner()
            .setFromShortcut(cl.shortcut())
            .setWaitForCompletion()
            .run();

	      return 0;
        }
        catch (const std::exception &e) {
	        reportError(
		        QObject::tr("failed to start shortcut: %1").arg(e.what()));
	        return 1;
        }
      }
    } else if (cl.nxmLink()) {
      log::debug("starting download from command line: {}", *cl.nxmLink());
      organizer.externalMessage(*cl.nxmLink());
    } else if (cl.executable()) {
      const QString exeName = *cl.executable();
      log::debug("starting {} from command line", exeName);

      try
      {
        // pass the remaining parameters to the binary
        organizer.processRunner()
          .setFromFileOrExecutable(exeName, cl.untouched())
          .setWaitForCompletion()
          .run();

	    return 0;
      }
      catch (const std::exception &e)
      {
	    reportError(
		    QObject::tr("failed to start application: %1").arg(e.what()));
	    return 1;
      }
    }

    auto splash = createSplash(settings, dataPath, game);

    QString apiKey;
    if (settings.nexus().apiKey(apiKey)) {
      NexusInterface::instance(pluginContainer.get())->getAccessManager()->apiCheck(apiKey);
    }

    log::debug("initializing tutorials");
    TutorialManager::init(
        qApp->applicationDirPath() + "/"
            + QString::fromStdWString(AppConfig::tutorialsPath()) + "/",
        &organizer);

    if (!application.setStyleFile(settings.interface().styleName().value_or(""))) {
      // disable invalid stylesheet
      settings.interface().setStyleName("");
    }

    int res = 1;

    { // scope to control lifetime of mainwindow
      // set up main window and its data structures
      MainWindow mainWindow(settings, organizer, *pluginContainer);

      NexusInterface::instance(pluginContainer.get())
        ->getAccessManager()->setTopLevelWidget(&mainWindow);

      QObject::connect(&mainWindow, SIGNAL(styleChanged(QString)), &application,
                       SLOT(setStyleFile(QString)));
      QObject::connect(&instance, SIGNAL(messageSent(QString)), &organizer,
                       SLOT(externalMessage(QString)));

      log::debug("displaying main window");
      mainWindow.show();
      mainWindow.activateWindow();

      if (splash) {
        // don't pass mainwindow as it just waits half a second for it
        // instead of proceding
        splash->finish(nullptr);
      }

      tt.stop();

      res = application.exec();
      mainWindow.close();

      NexusInterface::instance(pluginContainer.get())
        ->getAccessManager()->setTopLevelWidget(nullptr);
    }

    settings.geometry().resetIfNeeded();
    return res;
  } catch (const std::exception &e) {
    reportError(e.what());
  }

  return 1;
}

int forwardToPrimary(SingleInstance& instance, const cl::CommandLine& cl)
{
  if (cl.shortcut().isValid()) {
    instance.sendMessage(cl.shortcut().toString());
  } else if (cl.nxmLink()) {
    instance.sendMessage(*cl.nxmLink());
  } else {
    QMessageBox::information(
      nullptr, QObject::tr("Mod Organizer"),
      QObject::tr("An instance of Mod Organizer is already running"));
  }

  return 0;
}

void resetForRestart(cl::CommandLine& cl)
{
  LogModel::instance().clear();
  ResetExitFlag();

  // make sure the log file isn't locked in case MO was restarted and
  // the previous instance gets deleted
  log::getDefault().setFile({});

  // don't reprocess command line
  cl.clear();
}

QString determineDataPath(const cl::CommandLine& cl)
{
  try
  {
    InstanceManager& instanceManager = InstanceManager::instance();

    if (cl.instance())
      instanceManager.overrideInstance(*cl.instance());

    return instanceManager.determineDataPath();
  }
  catch (const std::exception &e)
  {
    if (strcmp(e.what(),"Canceled")) {
      QMessageBox::critical(nullptr, QObject::tr("Failed to set up instance"), e.what());
    }

    return {};
  }
}

int doOneRun(
  cl::CommandLine& cl, MOApplication& application, SingleInstance& instance)
{
  TimeThis tt("doOneRun() to runApplication()");

  // resets things when MO is "restarted"
  resetForRestart(cl);

  const QString dataPath = determineDataPath(cl);
  if (dataPath.isEmpty()) {
    return 1;
  }

  application.setProperty("dataPath", dataPath);
  setExceptionHandlers();

  if (!setLogDirectory(dataPath)) {
    reportError("Failed to create log folder");
    InstanceManager::instance().clearCurrentInstance();
    return 1;
  }

  log::debug("command line: '{}'", QString::fromWCharArray(GetCommandLineW()));

  tt.stop();

  return runApplication(application, cl, instance, dataPath);
}

int main(int argc, char *argv[])
{
  cl::CommandLine cl;

  if (auto r=cl.run(GetCommandLineW())) {
    return *r;
  }

  TimeThis tt("main() to doOneRun()");

  SetThisThreadName("main");

  initLogging();
  auto application = MOApplication::create(argc, argv);
  addDllsToPath();

  SingleInstance instance(cl.multiple());
  if (instance.ephemeral()) {
    return forwardToPrimary(instance, cl);
  }

  tt.stop();

  for (;;)
  {
    const auto r = doOneRun(cl, application, instance);
    if (r == RestartExitCode) {
      continue;
    }

    return r;
  }
}
