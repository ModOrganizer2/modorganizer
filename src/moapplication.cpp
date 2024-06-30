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
#include "commandline.h"
#include "instancemanager.h"
#include "loglist.h"
#include "mainwindow.h"
#include "messagedialog.h"
#include "multiprocess.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "organizercore.h"
#include "sanitychecks.h"
#include "settings.h"
#include "shared/appconfig.h"
#include "shared/util.h"
#include "thread_utils.h"
#include "tutorialmanager.h"
#include <QDebug>
#include <QFile>
#include <QPainter>
#include <QProxyStyle>
#include <QSSLSocket>
#include <QStringList>
#include <QStyleFactory>
#include <QStyleOption>
#include <iplugingame.h>
#include <log.h>
#include <report.h>
#include <scopeguard.h>
#include <utility.h>

// see addDllsToPath() below
#pragma comment(linker, "/manifestDependency:\""                                       \
                        "name='dlls' "                                                 \
                        "processorArchitecture='x86' "                                 \
                        "version='1.0.0.0' "                                           \
                        "type='win32' \"")

using namespace MOBase;
using namespace MOShared;

// style proxy that changes the appearance of drop indicators
//
class ProxyStyle : public QProxyStyle
{
public:
  ProxyStyle(QStyle* baseStyle = 0) : QProxyStyle(baseStyle) {}

  void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget) const override
  {
    if (element == QStyle::PE_IndicatorItemViewItemDrop) {

      // 0. Fix a bug that made the drop indicator sometimes appear on top
      // of the mod list when selecting a mod.
      if (option->rect.height() == 0 && option->rect.bottomRight() == QPoint(-1, -1)) {
        return;
      }

      // 1. full-width drop indicator
      QRect rect(option->rect);
      if (auto* view = qobject_cast<const QTreeView*>(widget)) {
        rect.setLeft(view->indentation());
        rect.setRight(widget->width());
      }

      // 2. stylish drop indicator
      painter->setRenderHint(QPainter::Antialiasing, true);

      QColor col(option->palette.windowText().color());
      QPen pen(col);
      pen.setWidth(2);
      col.setAlpha(50);

      painter->setPen(pen);
      painter->setBrush(QBrush(col));
      if (rect.height() == 0) {
        QPoint tri[3] = {rect.topLeft(), rect.topLeft() + QPoint(-5, 5),
                         rect.topLeft() + QPoint(-5, -5)};
        painter->drawPolygon(tri, 3);
        painter->drawLine(rect.topLeft(), rect.topRight());
      } else {
        painter->drawRoundedRect(rect, 5, 5);
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
  const auto dllsPath =
      QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/dlls");

  QCoreApplication::setLibraryPaths(QStringList(dllsPath) +
                                    QCoreApplication::libraryPaths());

  env::prependToPath(dllsPath);
}

MOApplication::MOApplication(int& argc, char** argv) : QApplication(argc, argv)
{
  TimeThis tt("MOApplication()");

  qputenv("QML_DISABLE_DISK_CACHE", "true");

  connect(&m_styleWatcher, &QFileSystemWatcher::fileChanged, [&](auto&& file) {
    log::debug("style file '{}' changed, reloading", file);
    updateStyle(file);
  });

  m_defaultStyle = "windowsvista";
  updateStyle(m_defaultStyle);
  addDllsToPath();
}

OrganizerCore& MOApplication::core()
{
  return *m_core;
}

void MOApplication::firstTimeSetup(MOMultiProcess& multiProcess)
{
  connect(
      &multiProcess, &MOMultiProcess::messageSent, this,
      [this](auto&& s) {
        externalMessage(s);
      },
      Qt::QueuedConnection);
}

int MOApplication::setup(MOMultiProcess& multiProcess, bool forceSelect)
{
  TimeThis tt("MOApplication setup()");

  // makes plugin data path available to plugins, see
  // IOrganizer::getPluginDataPath()
  MOBase::details::setPluginDataPath(OrganizerCore::pluginDataPath());

  // figuring out the current instance
  m_instance = getCurrentInstance(forceSelect);
  if (!m_instance) {
    return 1;
  }

  // first time the data path is available, set the global property and log
  // directory, then log a bunch of debug stuff
  const QString dataPath = m_instance->directory();
  setProperty("dataPath", dataPath);

  if (!setLogDirectory(dataPath)) {
    reportError(tr("Failed to create log folder."));
    InstanceManager::singleton().clearCurrentInstance();
    return 1;
  }

  log::debug("command line: '{}'", QString::fromWCharArray(GetCommandLineW()));

  log::info("starting Mod Organizer version {} revision {} in {}, usvfs: {}",
            createVersionInfo().string(), GITID, QCoreApplication::applicationDirPath(),
            MOShared::getUsvfsVersionString());

  if (multiProcess.secondary()) {
    log::debug("another instance of MO is running but --multiple was given");
  }

  log::info("data path: {}", m_instance->directory());
  log::info("working directory: {}", QDir::currentPath());

  tt.start("MOApplication::doOneRun() settings");

  // deleting old files, only for the main instance
  if (!multiProcess.secondary()) {
    purgeOldFiles();
  }

  // loading settings
  m_settings.reset(new Settings(m_instance->iniPath(), true));
  log::getDefault().setLevel(m_settings->diagnostics().logLevel());
  log::debug("using ini at '{}'", m_settings->filename());

  OrganizerCore::setGlobalCoreDumpType(m_settings->diagnostics().coreDumpType());

  tt.start("MOApplication::doOneRun() log and checks");

  // logging and checking
  env::Environment env;
  env.dump(*m_settings);
  m_settings->dump();
  sanity::checkEnvironment(env);

  m_modules = std::move(env.onModuleLoaded(qApp, [](auto&& m) {
    if (m.interesting()) {
      log::debug("loaded module {}", m.toString());
    }

    sanity::checkIncompatibleModule(m);
  }));

  auto sslBuildVersion = QSslSocket::sslLibraryBuildVersionString();
  auto sslVersion      = QSslSocket::sslLibraryVersionString();
  log::debug("SSL Build Version: {}, SSL Runtime Version {}", sslBuildVersion,
             sslVersion);

  // nexus interface
  tt.start("MOApplication::doOneRun() NexusInterface");
  log::debug("initializing nexus interface");
  m_nexus.reset(new NexusInterface(m_settings.get()));

  // organizer core
  tt.start("MOApplication::doOneRun() OrganizerCore");
  log::debug("initializing core");

  m_core.reset(new OrganizerCore(*m_settings));
  if (!m_core->bootstrap()) {
    reportError(tr("Failed to set up data paths."));
    InstanceManager::singleton().clearCurrentInstance();
    return 1;
  }

  // plugins
  tt.start("MOApplication::doOneRun() plugins");
  log::debug("initializing plugins");

  m_plugins = std::make_unique<PluginContainer>(m_core.get());
  m_plugins->loadPlugins();

  // instance
  if (auto r = setupInstanceLoop(*m_instance, *m_plugins)) {
    return *r;
  }

  if (m_instance->isPortable()) {
    log::debug("this is a portable instance");
  }

  tt.start("MOApplication::doOneRun() OrganizerCore setup");

  sanity::checkPaths(*m_instance->gamePlugin(), *m_settings);

  // setting up organizer core
  m_core->setManagedGame(m_instance->gamePlugin());
  m_core->createDefaultProfile();

  log::info("using game plugin '{}' ('{}', variant {}, steam id '{}') at {}",
            m_instance->gamePlugin()->gameName(),
            m_instance->gamePlugin()->gameShortName(),
            (m_settings->game().edition().value_or("").isEmpty()
                 ? "(none)"
                 : *m_settings->game().edition()),
            m_instance->gamePlugin()->steamAPPId(),
            m_instance->gamePlugin()->gameDirectory().absolutePath());

  CategoryFactory::instance().loadCategories();
  m_core->updateExecutablesList();
  m_core->updateModInfoFromDisc();
  m_core->setCurrentProfile(m_instance->profileName());

  return 0;
}

int MOApplication::run(MOMultiProcess& multiProcess)
{
  // checking command line
  TimeThis tt("MOApplication::run()");

  // show splash
  tt.start("MOApplication::doOneRun() splash");

  MOSplash splash(*m_settings, m_instance->directory(), m_instance->gamePlugin());

  tt.start("MOApplication::doOneRun() finishing");

  // start an api check
  QString apiKey;
  if (GlobalSettings::nexusApiKey(apiKey)) {
    m_nexus->getAccessManager()->apiCheck(apiKey);
  }

  // tutorials
  log::debug("initializing tutorials");
  TutorialManager::init(qApp->applicationDirPath() + "/" +
                            QString::fromStdWString(AppConfig::tutorialsPath()) + "/",
                        m_core.get());

  // styling
  if (!setStyleFile(m_settings->interface().styleName().value_or(""))) {
    // disable invalid stylesheet
    m_settings->interface().setStyleName("");
  }

  int res = 1;

  {
    tt.start("MOApplication::doOneRun() MainWindow setup");
    MainWindow mainWindow(*m_settings, *m_core, *m_plugins);

    // the nexus interface can show dialogs, make sure they're parented to the
    // main window
    m_nexus->getAccessManager()->setTopLevelWidget(&mainWindow);

    connect(
        &mainWindow, &MainWindow::styleChanged, this,
        [this](auto&& file) {
          setStyleFile(file);
        },
        Qt::QueuedConnection);

    log::debug("displaying main window");
    mainWindow.show();
    mainWindow.activateWindow();
    splash.close();

    tt.stop();

    res = exec();
    mainWindow.close();

    // main window is about to be destroyed
    m_nexus->getAccessManager()->setTopLevelWidget(nullptr);
  }

  // reset geometry if the flag was set from the settings dialog
  m_settings->geometry().resetIfNeeded();

  return res;
}

void MOApplication::externalMessage(const QString& message)
{
  log::debug("received external message '{}'", message);

  MOShortcut moshortcut(message);

  if (moshortcut.isValid()) {
    if (moshortcut.hasExecutable()) {
      try {
        m_core->processRunner()
            .setFromShortcut(moshortcut)
            .setWaitForCompletion(ProcessRunner::TriggerRefresh)
            .run();
      } catch (std::exception&) {
        // user was already warned
      }
    }
  } else if (isNxmLink(message)) {
    MessageDialog::showMessage(tr("Download started"), qApp->activeWindow(), false);
    m_core->downloadRequestedNXM(message);
  } else {
    cl::CommandLine cl;

    if (auto r = cl.process(message.toStdWString())) {
      log::debug("while processing external message, command line wants to "
                 "exit; ignoring");

      return;
    }

    if (auto i = cl.instance()) {
      const auto ci = InstanceManager::singleton().currentInstance();

      if (*i != ci->displayName()) {
        reportError(
            tr("This shortcut or command line is for instance '%1', but the current "
               "instance is '%2'.")
                .arg(*i)
                .arg(ci->displayName()));

        return;
      }
    }

    if (auto p = cl.profile()) {
      if (*p != m_core->profileName()) {
        reportError(
            tr("This shortcut or command line is for profile '%1', but the current "
               "profile is '%2'.")
                .arg(*p)
                .arg(m_core->profileName()));

        return;
      }
    }

    cl.runPostOrganizer(*m_core);
  }
}

std::unique_ptr<Instance> MOApplication::getCurrentInstance(bool forceSelect)
{
  auto& m              = InstanceManager::singleton();
  auto currentInstance = m.currentInstance();

  if (forceSelect || !currentInstance) {
    // clear any overrides that might have been given on the command line
    m.clearOverrides();
    currentInstance = selectInstance();
  } else {
    if (!QDir(currentInstance->directory()).exists()) {
      // the previously used instance doesn't exist anymore

      // clear any overrides that might have been given on the command line
      m.clearOverrides();

      if (m.hasAnyInstances()) {
        reportError(QObject::tr("Instance at '%1' not found. Select another instance.")
                        .arg(currentInstance->directory()));
      } else {
        reportError(
            QObject::tr("Instance at '%1' not found. You must create a new instance")
                .arg(currentInstance->directory()));
      }

      currentInstance = selectInstance();
    }
  }

  return currentInstance;
}

std::optional<int> MOApplication::setupInstanceLoop(Instance& currentInstance,
                                                    PluginContainer& pc)
{
  for (;;) {
    const auto setupResult = setupInstance(currentInstance, pc);

    if (setupResult == SetupInstanceResults::Okay) {
      return {};
    } else if (setupResult == SetupInstanceResults::TryAgain) {
      continue;
    } else if (setupResult == SetupInstanceResults::SelectAnother) {
      InstanceManager::singleton().clearCurrentInstance();
      return ReselectExitCode;
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
  removeOldFiles(qApp->property("dataPath").toString() + "/" +
                     QString::fromStdWString(AppConfig::logPath()),
                 "usvfs*.log", 5, QDir::Name);
}

void MOApplication::resetForRestart()
{
  LogModel::instance().clear();
  ResetExitFlag();

  // make sure the log file isn't locked in case MO was restarted and
  // the previous instance gets deleted
  log::getDefault().setFile({});

  // clear instance and profile overrides
  InstanceManager::singleton().clearOverrides();

  m_core     = {};
  m_plugins  = {};
  m_nexus    = {};
  m_settings = {};
  m_instance = {};
}

bool MOApplication::setStyleFile(const QString& styleName)
{
  // remove all files from watch
  QStringList currentWatch = m_styleWatcher.files();
  if (currentWatch.count() != 0) {
    m_styleWatcher.removePaths(currentWatch);
  }
  // set new stylesheet or clear it
  if (styleName.length() != 0) {
    QString styleSheetName = applicationDirPath() + "/" +
                             MOBase::ToQString(AppConfig::stylesheetsPath()) + "/" +
                             styleName;
    if (QFile::exists(styleSheetName)) {
      m_styleWatcher.addPath(styleSheetName);
      updateStyle(styleSheetName);
    } else {
      updateStyle(styleName);
    }
  } else {
    setStyle(new ProxyStyle(QStyleFactory::create(m_defaultStyle)));
    setStyleSheet("");
  }
  return true;
}

bool MOApplication::notify(QObject* receiver, QEvent* event)
{
  try {
    return QApplication::notify(receiver, event);
  } catch (const std::exception& e) {
    log::error("uncaught exception in handler (object {}, eventtype {}): {}",
               receiver->objectName(), event->type(), e.what());
    reportError(tr("an error occurred: %1").arg(e.what()));
    return false;
  } catch (...) {
    log::error("uncaught non-std exception in handler (object {}, eventtype {})",
               receiver->objectName(), event->type());
    reportError(tr("an error occurred"));
    return false;
  }
}

namespace
{
QStringList extractTopStyleSheetComments(QFile& stylesheet)
{
  if (!stylesheet.open(QFile::ReadOnly)) {
    log::error("failed to open stylesheet file {}", stylesheet.fileName());
    return {};
  }
  ON_BLOCK_EXIT([&stylesheet]() {
    stylesheet.close();
  });

  QStringList topComments;

  while (true) {
    const auto byteLine = stylesheet.readLine();
    if (byteLine.isNull()) {
      break;
    }

    const auto line = QString(byteLine).trimmed();

    // skip empty lines
    if (line.isEmpty()) {
      continue;
    }

    // only handle single line comments
    if (!line.startsWith("/*")) {
      break;
    }

    topComments.push_back(line.mid(2, line.size() - 4).trimmed());
  }

  return topComments;
}

QString extractBaseStyleFromStyleSheet(QFile& stylesheet, const QString& defaultStyle)
{
  // read the first line of the files that are either empty or comments
  //
  const auto topLines = extractTopStyleSheetComments(stylesheet);

  const auto factoryStyles = QStyleFactory::keys();

  QString style = defaultStyle;

  for (const auto& line : topLines) {
    if (!line.startsWith("mo2-base-style")) {
      continue;
    }

    const auto parts = line.split(":");
    if (parts.size() != 2) {
      log::warn("found invalid top-comment for mo2 in {}: {}", stylesheet.fileName(),
                line);
      continue;
    }

    const auto tmpStyle = parts[1].trimmed();
    const auto index    = factoryStyles.indexOf(tmpStyle, 0, Qt::CaseInsensitive);
    if (index == -1) {
      log::warn("base style '{}' from style '{}' not found", tmpStyle,
                stylesheet.fileName(), line);
      continue;
    }

    style = factoryStyles[index];
    log::info("found base style '{}' for style '{}'", style, stylesheet.fileName());
    break;
  }

  return style;
}

}  // namespace

void MOApplication::updateStyle(const QString& fileName)
{
  if (QStyleFactory::keys().contains(fileName)) {
    setStyleSheet("");
    setStyle(new ProxyStyle(QStyleFactory::create(fileName)));
  } else {
    QFile stylesheet(fileName);
    if (stylesheet.exists()) {
      setStyle(new ProxyStyle(QStyleFactory::create(
          extractBaseStyleFromStyleSheet(stylesheet, m_defaultStyle))));
      setStyleSheet(QString("file:///%1").arg(fileName));
    } else {
      log::warn("invalid stylesheet: {}", fileName);
    }
  }
}

MOSplash::MOSplash(const Settings& settings, const QString& dataPath,
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

QString MOSplash::getSplashPath(const Settings& settings, const QString& dataPath,
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
