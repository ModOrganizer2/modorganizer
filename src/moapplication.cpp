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

#include "moapplication.h"
#include "settings.h"
#include "env.h"
#include "commandline.h"
#include "instancemanager.h"
#include "organizercore.h"
#include "thread_utils.h"
#include "loglist.h"
#include "multiprocess.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "tutorialmanager.h"
#include "sanitychecks.h"
#include "mainwindow.h"
#include "shared/appconfig.h"
#include "shared/error_report.h"
#include "shared/util.h"

#include <iplugingame.h>
#include <report.h>
#include <utility.h>
#include <log.h>

// see addDllsToPath() below
#pragma comment(linker, "/manifestDependency:\"" \
    "name='dlls' " \
    "processorArchitecture='x86' " \
    "version='1.0.0.0' " \
    "type='win32' \"")

using namespace MOBase;
using namespace MOShared;

class ProxyStyle : public QProxyStyle {
public:
  ProxyStyle(QStyle *baseStyle = 0)
    : QProxyStyle(baseStyle)
  {
  }

  void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    if(element == QStyle::PE_IndicatorItemViewItemDrop) {
      painter->setRenderHint(QPainter::Antialiasing, true);

      QColor col(option->palette.windowText().color());
      QPen pen(col);
      pen.setWidth(2);
      col.setAlpha(50);
      QBrush brush(col);

      painter->setPen(pen);
      painter->setBrush(brush);
      if(option->rect.height() == 0) {
        QPoint tri[3] = {
          option->rect.topLeft(),
          option->rect.topLeft() + QPoint(-5,  5),
          option->rect.topLeft() + QPoint(-5, -5)
        };
        painter->drawPolygon(tri, 3);
        painter->drawLine(QPoint(option->rect.topLeft().x(), option->rect.topLeft().y()), option->rect.topRight());
      } else {
        painter->drawRoundedRect(option->rect, 5, 5);
      }
    } else {
      QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
  }
};


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


MOApplication::MOApplication(cl::CommandLine& cl, int& argc, char** argv)
  : QApplication(argc, argv), m_cl(cl)
{
  TimeThis tt("MOApplication()");

  connect(&m_StyleWatcher, &QFileSystemWatcher::fileChanged, [&](auto&& file){
    log::debug("style file '{}' changed, reloading", file);
    updateStyle(file);
  });

  m_DefaultStyle = style()->objectName();
  setStyle(new ProxyStyle(style()));
  addDllsToPath();
}

int MOApplication::run(MOMultiProcess& multiProcess)
{
  TimeThis tt("MOApplication run() to doOneRun()");

  auto& m = InstanceManager::singleton();

  if (m_cl.instance())
    m.overrideInstance(*m_cl.instance());

  if (m_cl.profile()) {
    m.overrideProfile(*m_cl.profile());
  }

  // makes plugin data path available to plugins, see
  // IOrganizer::getPluginDataPath()
  MOBase::details::setPluginDataPath(OrganizerCore::pluginDataPath());

  // MO runs in a loop because it can be restarted in several ways, such as
  // when switching instances or changing some settings
  for (;;)
  {
    try
    {
      tt.stop();

      const auto r = doOneRun(multiProcess);

      if (r == RestartExitCode) {
        // resets things when MO is "restarted"
        resetForRestart();
        continue;
      }

      return r;
    }
    catch (const std::exception &e)
    {
      reportError(e.what());
      return 1;
    }
  }
}

int MOApplication::doOneRun(MOMultiProcess& multiProcess)
{
  TimeThis tt("MOApplication::doOneRun() instances");

  // figuring out the current instance
  auto currentInstance = getCurrentInstance();
  if (!currentInstance) {
    return 1;
  }

  // first time the data path is available, set the global property and log
  // directory, then log a bunch of debug stuff
  const QString dataPath = currentInstance->directory();
  setProperty("dataPath", dataPath);

  if (!setLogDirectory(dataPath)) {
    reportError("Failed to create log folder");
    InstanceManager::singleton().clearCurrentInstance();
    return 1;
  }

  log::debug("command line: '{}'", QString::fromWCharArray(GetCommandLineW()));

  log::info(
    "starting Mod Organizer version {} revision {} in {}, usvfs: {}",
    createVersionInfo().displayString(3), GITID,
    QCoreApplication::applicationDirPath(), MOShared::getUsvfsVersionString());

  if (multiProcess.secondary()) {
    log::debug("another instance of MO is running but --multiple was given");
  }

  log::info("data path: {}", currentInstance->directory());
  log::info("working directory: {}", QDir::currentPath());


  tt.start("MOApplication::doOneRun() settings");

  // deleting old files, only for the main instance
  if (!multiProcess.secondary()) {
    purgeOldFiles();
  }

  QWindowsWindowFunctions::setWindowActivationBehavior(
    QWindowsWindowFunctions::AlwaysActivateWindow);


  // loading settings
  Settings settings(currentInstance->iniPath(), true);
  log::getDefault().setLevel(settings.diagnostics().logLevel());
  log::debug("using ini at '{}'", settings.filename());

  OrganizerCore::setGlobalCoreDumpType(settings.diagnostics().coreDumpType());


  tt.start("MOApplication::doOneRun() log and checks");

  // logging and checking
  env::Environment env;
  env.dump(settings);
  settings.dump();
  sanity::checkEnvironment(env);

  const auto moduleNotification = env.onModuleLoaded(qApp, [](auto&& m) {
    log::debug("loaded module {}", m.toString());
    sanity::checkIncompatibleModule(m);
  });


  // this must outlive `organizer`
  std::unique_ptr<PluginContainer> pluginContainer;

  // nexus interface
  tt.start("MOApplication::doOneRun() NexusInterface");
  log::debug("initializing nexus interface");
  NexusInterface ni(&settings);

  // organizer core
  tt.start("MOApplication::doOneRun() OrganizerCore");
  log::debug("initializing core");

  OrganizerCore organizer(settings);
  if (!organizer.bootstrap()) {
    reportError("failed to set up data paths");
    InstanceManager::singleton().clearCurrentInstance();
    return 1;
  }

  // plugins
  tt.start("MOApplication::doOneRun() plugins");
  log::debug("initializing plugins");

  pluginContainer = std::make_unique<PluginContainer>(&organizer);
  pluginContainer->loadPlugins();

  // instance
  if (auto r=setupInstanceLoop(*currentInstance, *pluginContainer)) {
    return *r;
  }

  if (currentInstance->isPortable()) {
    log::debug("this is a portable instance");
  }

  tt.start("MOApplication::doOneRun() OrganizerCore setup");

  sanity::checkPaths(*currentInstance->gamePlugin(), settings);

  // setting up organizer core
  organizer.setManagedGame(currentInstance->gamePlugin());
  organizer.createDefaultProfile();

  log::info(
    "using game plugin '{}' ('{}', variant {}, steam id '{}') at {}",
    currentInstance->gamePlugin()->gameName(),
    currentInstance->gamePlugin()->gameShortName(),
    (settings.game().edition().value_or("").isEmpty() ?
      "(none)" : *settings.game().edition()),
    currentInstance->gamePlugin()->steamAPPId(),
    currentInstance->gamePlugin()->gameDirectory().absolutePath());

  CategoryFactory::instance().loadCategories();
  organizer.updateExecutablesList();
  organizer.updateModInfoFromDisc();
  organizer.setCurrentProfile(currentInstance->profileName());

  // checking command line
  tt.start("MOApplication::doOneRun() command line");
  if (auto r=m_cl.setupCore(organizer)) {
    return *r;
  }

  // show splash
  tt.start("MOApplication::doOneRun() splash");

  MOSplash splash(
    settings, currentInstance->directory(), currentInstance->gamePlugin());

  tt.start("MOApplication::doOneRun() finishing");

  // start an api check
  QString apiKey;
  if (GlobalSettings::nexusApiKey(apiKey)) {
    ni.getAccessManager()->apiCheck(apiKey);
  }

  // tutorials
  log::debug("initializing tutorials");
  TutorialManager::init(
      qApp->applicationDirPath() + "/"
          + QString::fromStdWString(AppConfig::tutorialsPath()) + "/",
      &organizer);

  // styling
  if (!setStyleFile(settings.interface().styleName().value_or(""))) {
    // disable invalid stylesheet
    settings.interface().setStyleName("");
  }


  int res = 1;

  {
    tt.start("MOApplication::doOneRun() MainWindow setup");
    MainWindow mainWindow(settings, organizer, *pluginContainer);

    // qt resets the thread name somewhere when creating the main window
    MOShared::SetThisThreadName("main");

    // the nexus interface can show dialogs, make sure they're parented to the
    // main window
    ni.getAccessManager()->setTopLevelWidget(&mainWindow);

    QObject::connect(&mainWindow, SIGNAL(styleChanged(QString)), this,
                      SLOT(setStyleFile(QString)));

    QObject::connect(&multiProcess, SIGNAL(messageSent(QString)), &organizer,
                      SLOT(externalMessage(QString)));


    log::debug("displaying main window");
    mainWindow.show();
    mainWindow.activateWindow();
    splash.close();

    tt.stop();

    res = exec();
    mainWindow.close();

    // main window is about to be destroyed
    ni.getAccessManager()->setTopLevelWidget(nullptr);
  }

  // reset geometry if the flag was set from the settings dialog
  settings.geometry().resetIfNeeded();

  return res;
}

std::optional<Instance> MOApplication::getCurrentInstance()
{
  auto& m = InstanceManager::singleton();
  auto currentInstance = m.currentInstance();

  if (!currentInstance)
  {
    // clear any overrides that might have been given on the command line
    m.clearOverrides();
    m_cl.clear();

    currentInstance = selectInstance();
  }
  else
  {
    if (!QDir(currentInstance->directory()).exists()) {
      // the previously used instance doesn't exist anymore

      // clear any overrides that might have been given on the command line
      m.clearOverrides();
      m_cl.clear();

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
    }
  }

  return currentInstance;
}

std::optional<int> MOApplication::setupInstanceLoop(
  Instance& currentInstance, PluginContainer& pc)
{
  for (;;)
  {
    const auto setupResult = setupInstance(currentInstance, pc);

    if (setupResult == SetupInstanceResults::Okay) {
      return {};
    } else if (setupResult == SetupInstanceResults::TryAgain) {
      continue;
    } else if (setupResult == SetupInstanceResults::SelectAnother) {
      InstanceManager::singleton().clearCurrentInstance();
      return RestartExitCode;
    } else {
      return 1;
    }
  }
}

void MOApplication::purgeOldFiles()
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

void MOApplication::resetForRestart()
{
  LogModel::instance().clear();
  ResetExitFlag();

  // make sure the log file isn't locked in case MO was restarted and
  // the previous instance gets deleted
  log::getDefault().setFile({});

  // don't reprocess command line
  m_cl.clear();

  // clear instance and profile overrides
  InstanceManager::singleton().clearOverrides();
}

bool MOApplication::setStyleFile(const QString& styleName)
{
  // remove all files from watch
  QStringList currentWatch = m_StyleWatcher.files();
  if (currentWatch.count() != 0) {
    m_StyleWatcher.removePaths(currentWatch);
  }
  // set new stylesheet or clear it
  if (styleName.length() != 0) {
    QString styleSheetName = applicationDirPath() + "/" + MOBase::ToQString(AppConfig::stylesheetsPath()) + "/" + styleName;
    if (QFile::exists(styleSheetName)) {
      m_StyleWatcher.addPath(styleSheetName);
      updateStyle(styleSheetName);
    } else {
      updateStyle(styleName);
    }
  } else {
    setStyle(new ProxyStyle(QStyleFactory::create(m_DefaultStyle)));
    setStyleSheet("");
  }
  return true;
}

bool MOApplication::notify(QObject* receiver, QEvent* event)
{
  try {
    return QApplication::notify(receiver, event);
  } catch (const std::exception &e) {
    log::error(
      "uncaught exception in handler (object {}, eventtype {}): {}",
      receiver->objectName(), event->type(), e.what());
    reportError(tr("an error occurred: %1").arg(e.what()));
    return false;
  } catch (...) {
    log::error(
      "uncaught non-std exception in handler (object {}, eventtype {})",
      receiver->objectName(), event->type());
    reportError(tr("an error occurred"));
    return false;
  }
}

void MOApplication::updateStyle(const QString& fileName)
{
  if (QStyleFactory::keys().contains(fileName)) {
    setStyle(QStyleFactory::create(fileName));
    setStyleSheet("");
  } else {
    setStyle(new ProxyStyle(QStyleFactory::create(m_DefaultStyle)));
    if (QFile::exists(fileName)) {
      setStyleSheet(QString("file:///%1").arg(fileName));
    } else {
      log::warn("invalid stylesheet: {}", fileName);
    }
  }
}


MOSplash::MOSplash(
  const Settings& settings, const QString& dataPath,
  const MOBase::IPluginGame* game)
{
  const auto splashPath = getSplashPath(settings, dataPath, game);
  if (splashPath.isEmpty()) {
    return;
  }

  QPixmap image(splashPath);
  if (image.isNull()) {
    log::error("failed to load splash from {}", splashPath);
    return;
  }

  ss_.reset(new QSplashScreen(image));
  settings.geometry().centerOnMainWindowMonitor(ss_.get());

  ss_->show();
  ss_->activateWindow();
}

void MOSplash::close()
{
  if (ss_) {
    // don't pass mainwindow as it just waits half a second for it
    // instead of proceding
    ss_->finish(nullptr);
  }
}

QString MOSplash::getSplashPath(
  const Settings& settings, const QString& dataPath,
  const MOBase::IPluginGame* game) const
{
  if (!settings.useSplash()) {
    return {};
  }

  // try splash from instance directory
  const QString splashPath = dataPath + "/splash.png";
  if (QFile::exists(dataPath + "/splash.png")) {
    QImage image(splashPath);
    if (!image.isNull()) {
      return splashPath;
    }
  }

  // try splash from plugin
  QString pluginSplash = QString(":/%1/splash").arg(game->gameShortName());
  if (QFile::exists(pluginSplash)) {
    QImage image(pluginSplash);
    if (!image.isNull()) {
      image.save(splashPath);
      return pluginSplash;
    }
  }

  // try default splash from resource
  QString defaultSplash = ":/MO/gui/splash";
  if (QFile::exists(defaultSplash)) {
    QImage image(defaultSplash);
    if (!image.isNull()) {
      return defaultSplash;
    }
  }

  return splashPath;
}
