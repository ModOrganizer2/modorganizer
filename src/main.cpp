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
#include "instancemanagerdialog.h"
#include "createinstancedialog.h"
#include "createinstancedialogpages.h"
#include "organizercore.h"
#include "env.h"
#include "envmodule.h"
#include "commandline.h"
#include "shared/util.h"
#include "shared/appconfig.h"
#include "shared/error_report.h"
#include <imoinfo.h>
#include <report.h>
#include <usvfs.h>
#include <log.h>
#include <utility.h>


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

std::optional<int> handleCommandLine(
  const cl::CommandLine& cl, OrganizerCore& organizer)
{
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

  return {};
}

int runApplication(
  MOApplication &application, const cl::CommandLine& cl,
  SingleInstance &instance, const QString &dataPath,
  Instance& currentInstance)
{
  TimeThis tt("runApplication() to exec()");

  log::info(
    "starting Mod Organizer version {} revision {} in {}, usvfs: {}",
    createVersionInfo().displayString(3), GITID,
    QCoreApplication::applicationDirPath(), MOShared::getUsvfsVersionString());

  log::info("data path: {}", dataPath);

  log::info("working directory: {}", QDir::currentPath());

  if (!instance.secondary()) {
    purgeOldFiles();
  }

  QWindowsWindowFunctions::setWindowActivationBehavior(
    QWindowsWindowFunctions::AlwaysActivateWindow);

  try
  {
    Settings settings(
      dataPath + "/" + QString::fromStdWString(AppConfig::iniFileName()),
      true);

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

    log::debug("initializing nexus interface");
    NexusInterface ni(&settings);

    log::debug("initializing core");
    OrganizerCore organizer(settings);
    if (!organizer.bootstrap()) {
      reportError("failed to set up data paths");
      InstanceManager::singleton().clearCurrentInstance();
      return 1;
    }

    log::debug("initializing plugins");
    pluginContainer = std::make_unique<PluginContainer>(&organizer);
    pluginContainer->loadPlugins();

    for (;;)
    {
      const auto setupResult = setupInstance(currentInstance, *pluginContainer);

      if (setupResult == SetupInstanceResults::Ok) {
        break;
      } else if (setupResult == SetupInstanceResults::TryAgain) {
        continue;
      } else if (setupResult == SetupInstanceResults::SelectAnother) {
        InstanceManager::singleton().clearCurrentInstance();
        return RestartExitCode;
      } else {
        return 1;
      }
    }

    if (currentInstance.isPortable()) {
      log::debug("this is a portable instance");
    }

    checkPathsForSanity(*currentInstance.gamePlugin(), settings);

    organizer.setManagedGame(currentInstance.gamePlugin());
    organizer.createDefaultProfile();

    log::info(
      "using game plugin '{}' ('{}', variant {}, steam id '{}') at {}",
      currentInstance.gamePlugin()->gameName(),
      currentInstance.gamePlugin()->gameShortName(),
      (settings.game().edition().value_or("").isEmpty() ?
        "(none)" : *settings.game().edition()),
      currentInstance.gamePlugin()->steamAPPId(),
      currentInstance.gamePlugin()->gameDirectory().absolutePath());


    CategoryFactory::instance().loadCategories();
    organizer.updateExecutablesList();
    organizer.updateModInfoFromDisc();

    organizer.setCurrentProfile(currentInstance.profileName());

    if (auto r=handleCommandLine(cl, organizer)) {
      return *r;
    }

    MOSplash splash(settings, dataPath, currentInstance.gamePlugin());

    QString apiKey;
    if (GlobalSettings::nexusApiKey(apiKey)) {
      ni.getAccessManager()->apiCheck(apiKey);
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

      ni.getAccessManager()->setTopLevelWidget(&mainWindow);

      QObject::connect(&mainWindow, SIGNAL(styleChanged(QString)), &application,
                       SLOT(setStyleFile(QString)));
      QObject::connect(&instance, SIGNAL(messageSent(QString)), &organizer,
                       SLOT(externalMessage(QString)));

      log::debug("displaying main window");
      mainWindow.show();
      mainWindow.activateWindow();

      splash.close();

      tt.stop();

      res = application.exec();
      mainWindow.close();

      ni.getAccessManager()->setTopLevelWidget(nullptr);
    }

    settings.geometry().resetIfNeeded();
    return res;
  }
  catch (const std::exception &e)
  {
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

int doOneRun(
  cl::CommandLine& cl, MOApplication& application, SingleInstance& instance)
{
  TimeThis tt("doOneRun() to runApplication()");

  // resets things when MO is "restarted"
  resetForRestart(cl);

  auto& m = InstanceManager::singleton();
  auto currentInstance = m.currentInstance();

  if (!currentInstance)
  {
    currentInstance = selectInstance();
    if (!currentInstance) {
      return 1;
    }
  }
  else
  {
    if (!QDir(currentInstance->directory()).exists()) {
      // the previously used instance doesn't exist anymore

      if (m.hasAnyInstances()) {
        MOShared::criticalOnTop(QObject::tr(
          "Instance at '%1' not found. Select another instance.")
          .arg(currentInstance->directory()));
      } else {
        MOShared::criticalOnTop(QObject::tr(
          "Instance at '%1' not found. You must create a new instance")
            .arg(currentInstance->directory()));
      }

      currentInstance = selectInstance();
      if (!currentInstance) {
        return 1;
      }
    }
  }

  const QString dataPath = currentInstance->directory();
  application.setProperty("dataPath", dataPath);

  setExceptionHandlers();

  if (!setLogDirectory(dataPath)) {
    reportError("Failed to create log folder");
    InstanceManager::singleton().clearCurrentInstance();
    return 1;
  }

  log::debug("command line: '{}'", QString::fromWCharArray(GetCommandLineW()));

  tt.stop();

  return runApplication(application, cl, instance, dataPath, *currentInstance);
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

  SingleInstance instance(cl.multiple());
  if (instance.ephemeral()) {
    return forwardToPrimary(instance, cl);
  }

  tt.stop();

  if (cl.instance())
    InstanceManager::singleton().overrideInstance(*cl.instance());

  if (cl.profile()) {
    InstanceManager::singleton().overrideProfile(*cl.profile());
  }

  // makes plugin data path available to plugins, see
  // IOrganizer::getPluginDataPath()
  MOBase::details::setPluginDataPath(OrganizerCore::pluginDataPath());

  for (;;)
  {
    const auto r = doOneRun(cl, application, instance);
    if (r == RestartExitCode) {
      continue;
    }

    return r;
  }
}
