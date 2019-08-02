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


#ifdef LEAK_CHECK_WITH_VLD
#include <wchar.h>
#include <vld.h>
#endif // LEAK_CHECK_WITH_VLD

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>

#include <appconfig.h>
#include <utility.h>
#include <scopeguard.h>
#include "mainwindow.h"
#include <report.h>
#include "modlist.h"
#include "profile.h"
#include "spawn.h"
#include "executableslist.h"
#include "singleinstance.h"
#include "utility.h"
#include "helper.h"
#include "loglist.h"
#include "selectiondialog.h"
#include "moapplication.h"
#include "tutorialmanager.h"
#include "nxmaccessmanager.h"
#include "instancemanager.h"
#include "moshortcut.h"
#include "organizercore.h"
#include "env.h"
#include "envmodule.h"

#include <eh.h>
#include <windows_error.h>
#include <usvfs.h>
#include <log.h>

#include <QApplication>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QWhatsThis>
#include <QToolBar>
#include <QFileDialog>
#include <QDesktopServices>
#include <QMessageBox>
#include <QSharedMemory>
#include <QBuffer>
#include <QSplashScreen>
#include <QDirIterator>
#include <QDesktopServices>
#include <QLibraryInfo>
#include <QSslSocket>
#include <QtPlatformHeaders/QWindowsWindowFunctions>

#include <boost/scoped_array.hpp>

#include <ShellAPI.h>

#include <cstdarg>
#include <iostream>
#include <sstream>
#include <stdexcept>


#pragma comment(linker, "/manifestDependency:\"name='dlls' processorArchitecture='x86' version='1.0.0.0' type='win32' \"")


using namespace MOBase;
using namespace MOShared;

bool createAndMakeWritable(const std::wstring &subPath) {
  QString const dataPath = qApp->property("dataPath").toString();
  QString fullPath = dataPath + "/" + QString::fromStdWString(subPath);

  if (!QDir(fullPath).exists() && !QDir().mkdir(fullPath)) {
    QMessageBox::critical(nullptr, QObject::tr("Error"),
                          QObject::tr("Failed to create \"%1\". Your user "
                                      "account probably lacks permission.")
                              .arg(fullPath));
    return false;
  } else {
    return true;
  }
}

bool bootstrap()
{
  // remove the temporary backup directory in case we're restarting after an update
  QString backupDirectory = qApp->applicationDirPath() + "/update_backup";
  if (QDir(backupDirectory).exists()) {
    shellDelete(QStringList(backupDirectory));
  }

  // cycle logfile
  removeOldFiles(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::logPath()),
                 "usvfs*.log", 5, QDir::Name);

  if (!createAndMakeWritable(AppConfig::logPath())) {
    return false;
  }

  return true;
}

LPTOP_LEVEL_EXCEPTION_FILTER prevUnhandledExceptionFilter = nullptr;

static LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *exceptionPtrs)
{
  const std::wstring& dumpPath = OrganizerCore::crashDumpsPath();
  int dumpRes =
    CreateMiniDump(exceptionPtrs, OrganizerCore::getGlobalCrashDumpsType(), dumpPath.c_str());
  if (!dumpRes)
    log::error("ModOrganizer has crashed, crash dump created.");
  else
    log::error("ModOrganizer has crashed, CreateMiniDump failed ({}, error {}).", dumpRes, GetLastError());

  if (prevUnhandledExceptionFilter)
    return prevUnhandledExceptionFilter(exceptionPtrs);
  else
    return EXCEPTION_CONTINUE_SEARCH;
}

// Parses the first parseArgCount arguments of the current process command line and returns
// them in parsedArgs, the rest of the command line is returned untouched.
LPCWSTR UntouchedCommandLineArguments(int parseArgCount, std::vector<std::wstring>& parsedArgs)
{
  LPCWSTR cmd = GetCommandLineW();
  LPCWSTR arg = nullptr; // to skip executable name
  for (; parseArgCount >= 0 && *cmd; ++cmd)
  {
    if (*cmd == '"') {
      int escaped = 0;
      for (++cmd; *cmd && (*cmd != '"' || escaped % 2 != 0); ++cmd)
        escaped = *cmd == '\\' ? escaped + 1 : 0;
    }
    if (*cmd == ' ') {
      if (arg)
        if (cmd-1 > arg && *arg == '"' && *(cmd-1) == '"')
          parsedArgs.push_back(std::wstring(arg+1, cmd-1));
        else
          parsedArgs.push_back(std::wstring(arg, cmd));
      arg = cmd + 1;
      --parseArgCount;
    }
  }
  return cmd;
}

static int SpawnWaitProcess(LPCWSTR workingDirectory, LPCWSTR commandLine) {
  PROCESS_INFORMATION pi{ 0 };
  STARTUPINFO si{ 0 };
  si.cb = sizeof(si);
  std::wstring commandLineCopy = commandLine;

  if (!CreateProcessW(NULL, &commandLineCopy[0], NULL, NULL, FALSE, 0, NULL, workingDirectory, &si, &pi)) {
    // A bit of a problem where to log the error message here, at least this way you can get the message
    // using a either DebugView or a live debugger:
    std::wostringstream ost;
    ost << L"CreateProcess failed: " << commandLine << ", " << GetLastError();
    OutputDebugStringW(ost.str().c_str());
    return -1;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exitCode = (DWORD)-1;
  ::GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return static_cast<int>(exitCode);
}

static DWORD WaitForProcess() {

}

static bool HaveWriteAccess(const std::wstring &path)
{
  bool writable = false;

  const static SECURITY_INFORMATION requestedFileInformation = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;

  DWORD length = 0;
  if (!::GetFileSecurityW(path.c_str(), requestedFileInformation, nullptr, 0UL, &length)
      && (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
    std::string tempBuffer;
    tempBuffer.reserve(length);
    PSECURITY_DESCRIPTOR security = (PSECURITY_DESCRIPTOR)tempBuffer.data();
    if (security
        && ::GetFileSecurity(path.c_str(), requestedFileInformation, security, length, &length)) {
      HANDLE token = nullptr;
      const static DWORD tokenDesiredAccess = TOKEN_IMPERSONATE | TOKEN_QUERY | TOKEN_DUPLICATE | STANDARD_RIGHTS_READ;
      if (!::OpenThreadToken(::GetCurrentThread(), tokenDesiredAccess, TRUE, &token)) {
        if (!::OpenProcessToken(::GetCurrentProcess(), tokenDesiredAccess, &token)) {
          throw std::runtime_error("Unable to get any thread or process token");
        }
      }

      HANDLE impersonatedToken = nullptr;
      if (::DuplicateToken(token, SecurityImpersonation, &impersonatedToken)) {
        GENERIC_MAPPING mapping = { 0xFFFFFFFF };
        mapping.GenericRead = FILE_GENERIC_READ;
        mapping.GenericWrite = FILE_GENERIC_WRITE;
        mapping.GenericExecute = FILE_GENERIC_EXECUTE;
        mapping.GenericAll = FILE_ALL_ACCESS;

        DWORD genericAccessRights = FILE_GENERIC_WRITE;
        ::MapGenericMask(&genericAccessRights, &mapping);

        PRIVILEGE_SET privileges = { 0 };
        DWORD grantedAccess = 0;
        DWORD privilegesLength = sizeof(privileges);
        BOOL result = 0;
        if (::AccessCheck(security, impersonatedToken, genericAccessRights, &mapping, &privileges, &privilegesLength, &grantedAccess, &result)) {
          writable = result != 0;
        }
        ::CloseHandle(impersonatedToken);
      }

      ::CloseHandle(token);
    }
  }
  return writable;
}


QString determineProfile(QStringList &arguments, const QSettings &settings)
{
  QString selectedProfileName = QString::fromUtf8(settings.value("selected_profile", "").toByteArray());
  { // see if there is a profile on the command line
    int profileIndex = arguments.indexOf("-p", 1);
    if ((profileIndex != -1) && (profileIndex < arguments.size() - 1)) {
      log::debug("profile overwritten on command line");
      selectedProfileName = arguments.at(profileIndex + 1);
    }
    arguments.removeAt(profileIndex);
    arguments.removeAt(profileIndex);
  }
  if (selectedProfileName.isEmpty()) {
    log::debug("no configured profile");
    selectedProfileName = "Default";
  } else {
    log::debug("configured profile: {}", selectedProfileName);
  }

  return selectedProfileName;
}

MOBase::IPluginGame *selectGame(QSettings &settings, QDir const &gamePath, MOBase::IPluginGame *game)
{
  settings.setValue("gameName", game->gameName());
  //Sadly, hookdll needs gamePath in order to run. So following code block is
  //commented out
  /*if (gamePath == game->gameDirectory()) {
    settings.remove("gamePath");
  } else*/ {
    QString gameDir = gamePath.absolutePath();
    game->setGamePath(gameDir);
    settings.setValue("gamePath", QDir::toNativeSeparators(gameDir).toUtf8().constData());
  }
  return game; //Woot
}


MOBase::IPluginGame *determineCurrentGame(QString const &moPath, QSettings &settings, PluginContainer const &plugins)
{
  //Determine what game we are running where. Be very paranoid in case the
  //user has done something odd.

  //If the game name has been set up, try to use that.
  QString gameName = settings.value("gameName", "").toString();
  bool gameConfigured = !gameName.isEmpty();
  if (gameConfigured) {
    MOBase::IPluginGame *game = plugins.managedGame(gameName);
    if (game == nullptr) {
      reportError(QObject::tr("Plugin to handle %1 no longer installed").arg(gameName));
      return nullptr;
    }
    QString gamePath = QString::fromUtf8(settings.value("gamePath", "").toByteArray());
    if (gamePath == "") {
      gamePath = game->gameDirectory().absolutePath();
    }
    QDir gameDir(gamePath);
    QFileInfo directoryInfo(gameDir.path());
    if (directoryInfo.isSymLink()) {
      reportError(QObject::tr("The configured path to the game directory (%1) appears to be a symbolic (or other) link. "
        "This setup is incompatible with MO2's VFS and will not run correctly.").arg(gamePath));
    }
    if (game->looksValid(gameDir)) {
      return selectGame(settings, gameDir, game);
    }
  }

  //If we've made it this far and the instance is already configured for a game, something has gone wrong.
  //Tell the user about it.
  if (gameConfigured) {
    QString gamePath = QString::fromUtf8(settings.value("gamePath", "").toByteArray());
    reportError(QObject::tr("Could not use configuration settings for game \"%1\", path \"%2\".").
                                                   arg(gameName).arg(gamePath));
  }

  SelectionDialog selection(gameConfigured ? QObject::tr("Please select the installation of %1 to manage").arg(gameName)
                                           : QObject::tr("Please select the game to manage"), nullptr, QSize(32, 32));

  for (IPluginGame *game : plugins.plugins<IPluginGame>()) {
    //If a game is already configured, skip any plugins that are not for that game
    if (gameConfigured && gameName.compare(game->gameName(), Qt::CaseInsensitive) != 0)
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

    gamePath = QFileDialog::getExistingDirectory(nullptr, gameConfigured ? QObject::tr("Please select the installation of %1 to manage").arg(gameName)
                                                                                 : QObject::tr("Please select the game to manage"),
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
        if (gameConfigured && gameName.compare(game->gameName(), Qt::CaseInsensitive) != 0)
          continue;

        //Only try plugins that look valid for this directory
        if (game->looksValid(gameDir)) {
          possibleGames.append(game);
        }
      }
      if (possibleGames.count() > 1) {
        SelectionDialog browseSelection(gameConfigured ? QObject::tr("Please select the installation of %1 to manage").arg(gameName)
                                                       : QObject::tr("Please select the game to manage"),
                                        nullptr, QSize(32, 32));
        for (IPluginGame *game : possibleGames) {
          browseSelection.addChoice(game->gameIcon(), game->gameName(), gamePath, QVariant::fromValue(game));
        }
        if (browseSelection.exec() == QDialog::Accepted) {
          return selectGame(settings, gameDir, browseSelection.getChoiceData().value<IPluginGame *>());
        } else {
          reportError(gameConfigured ? QObject::tr("Canceled finding %1 in \"%2\".").arg(gameName).arg(gamePath)
                                     : QObject::tr("Canceled finding game in \"%1\".").arg(gamePath));
        }
      } else if(possibleGames.count() == 1) {
        return selectGame(settings, gameDir, possibleGames[0]);
      } else {
        if (gameConfigured) {
          reportError(QObject::tr("%1 not identified in \"%2\". The directory is required to contain the game binary.").arg(gameName).arg(gamePath));
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


// extend path to include dll directory so plugins don't need a manifest
// (using AddDllDirectory would be an alternative to this but it seems fairly
// complicated esp.
//  since it isn't easily accessible on Windows < 8
//  SetDllDirectory replaces other search directories and this seems to
//  propagate to child processes)
void setupPath()
{
  static const int BUFSIZE = 4096;

  QCoreApplication::setLibraryPaths(QStringList(QCoreApplication::applicationDirPath() + "/dlls") + QCoreApplication::libraryPaths());

  boost::scoped_array<TCHAR> oldPath(new TCHAR[BUFSIZE]);
  DWORD offset = ::GetEnvironmentVariable(TEXT("PATH"), oldPath.get(), BUFSIZE);
  if (offset > BUFSIZE) {
    oldPath.reset(new TCHAR[offset]);
    ::GetEnvironmentVariable(TEXT("PATH"), oldPath.get(), offset);
  }

  std::wstring newPath(ToWString(QDir::toNativeSeparators(
    QCoreApplication::applicationDirPath())) + L"\\dlls");
  newPath += L";";
  newPath += oldPath.get();

  ::SetEnvironmentVariableW(L"PATH", newPath.c_str());
}

void preloadDll(const QString& filename)
{
  if (GetModuleHandleW(filename.toStdWString().c_str())) {
    // already loaded, this can happen when "restarting" MO by switching
    // instances, for example
    return;
  }

  const auto appPath = QDir::toNativeSeparators(
    QCoreApplication::applicationDirPath());

  const auto dllPath = appPath + "\\" + filename;

  if (!QFile::exists(dllPath)) {
    log::warn("{} not found", dllPath);
    return;
  }

  if (!LoadLibraryW(dllPath.toStdWString().c_str())) {
    const auto e = GetLastError();
    log::warn("failed to load {}: {}", dllPath, formatSystemMessage(e));
  }
}

void preloadSsl()
{
#if Q_PROCESSOR_WORDSIZE == 8
  preloadDll("libcrypto-1_1-x64.dll");
  preloadDll("libssl-1_1-x64.dll");
#elif Q_PROCESSOR_WORDSIZE == 4
  preloadDll("libcrypto-1_1.dll");
  preloadDll("libssl-1_1.dll");
#endif
}

static QString getVersionDisplayString()
{
  return createVersionInfo().displayString(3);
}

void dumpSettings(QSettings& settings)
{
  static const QStringList ignore({
    "username", "password", "nexus_api_key"
  });

  log::debug("settings:");

  settings.beginGroup("Settings");

  for (auto k : settings.allKeys()) {
    if (ignore.contains(k, Qt::CaseInsensitive)) {
      continue;
    }

    log::debug("  . {}={}", k, settings.value(k).toString());
  }

  settings.endGroup();
}

void checkMissingFiles()
{
  // files that are likely to be eaten
  static const QStringList files({
    "helper.exe", "nxmhandler.exe",
    "usvfs_proxy_x64.exe", "usvfs_proxy_x86.exe",
    "usvfs_x64.dll", "usvfs_x86.dll"
  });

  const auto dir = QCoreApplication::applicationDirPath();

  for (const auto& name : files) {
    const QFileInfo file(dir + QDir::separator() + name);
    if (!file.exists()) {
      log::warn(
        "'{}' seems to be missing, an antivirus may have deleted it",
        file.absoluteFilePath());
    }
  }
}

void checkNahimic(const env::Environment& e)
{
  for (auto&& m : e.loadedModules()) {
    const QFileInfo file(m.path());

    if (file.fileName().compare("NahimicOSD.dll", Qt::CaseInsensitive) == 0) {
      log::warn(
        "NahimicOSD.dll is loaded. Nahimic is known to cause issues with "
        "Mod Organizer, such as freezing or blank windows. Consider "
        "uninstalling it.");

      break;
    }
  }
}

void sanityChecks(const env::Environment& e)
{
  checkMissingFiles();
  checkNahimic(e);
}


int runApplication(MOApplication &application, SingleInstance &instance,
                   const QString &splashPath)
{
  log::info(
    "starting Mod Organizer version {} revision {} in {}",
    getVersionDisplayString(), GITID, QCoreApplication::applicationDirPath());

  preloadSsl();
  if (!QSslSocket::supportsSsl()) {
    log::warn("no ssl support");
  }

  QString dataPath = application.property("dataPath").toString();
  log::info("data path: {}", dataPath);

  if (!bootstrap()) {
    reportError("failed to set up data paths");
    return 1;
  }

  QWindowsWindowFunctions::setWindowActivationBehavior(
    QWindowsWindowFunctions::AlwaysActivateWindow);

  QStringList arguments = application.arguments();

  try {
    log::info("working directory: {}", QDir::currentPath());

    QSettings initSettings(
      dataPath + "/" + QString::fromStdWString(AppConfig::iniFileName()),
      QSettings::IniFormat);

    Settings settings(initSettings);
    log::getDefault().setLevel(settings.logLevel());

    // global crashDumpType sits in OrganizerCore to make a bit less ugly to
    // update it when the settings are changed during runtime
    OrganizerCore::setGlobalCrashDumpsType(settings.crashDumpsType());

    env::Environment env;

    env.dump();
    dumpSettings(initSettings);
    sanityChecks(env);

    log::debug("initializing core");
    OrganizerCore organizer(settings);
    if (!organizer.bootstrap()) {
      reportError("failed to set up data paths");
      return 1;
    }

    log::debug("initializing plugins");
    PluginContainer pluginContainer(&organizer);
    pluginContainer.loadPlugins();

    MOBase::IPluginGame *game = determineCurrentGame(
        application.applicationDirPath(), initSettings, pluginContainer);
    if (game == nullptr) {
      InstanceManager &instance = InstanceManager::instance();
      QString instanceName = instance.currentInstance();
      if (instanceName.compare("Portable", Qt::CaseInsensitive) != 0) {
        instance.clearCurrentInstance();
        return INT_MAX;
      }
      return 1;
    }
    if (splashPath.startsWith(':')) {
      // currently using MO splash, see if the plugin contains one
      QString pluginSplash
          = QString(":/%1/splash").arg(game->gameShortName());
      QImage image(pluginSplash);
      if (!image.isNull()) {
        image.save(dataPath + "/splash.png");
      }
    }

    organizer.setManagedGame(game);
    organizer.createDefaultProfile();

    if (!initSettings.contains("game_edition")) {
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
          initSettings.setValue("game_edition", selection.getChoiceString());
        }
      }
    }
    game->setGameVariant(initSettings.value("game_edition").toString());

    log::info("managing game at {}", game->gameDirectory().absolutePath());

    organizer.updateExecutablesList(initSettings);

    QString selectedProfileName = determineProfile(arguments, initSettings);
    organizer.setCurrentProfile(selectedProfileName);

    // if we have a command line parameter, it is either a nxm link or
    // a binary to start
	if (arguments.size() > 1) {
		if (MOShortcut shortcut{ arguments.at(1) }) {
			if (shortcut.hasExecutable()) {
				try {
					organizer.runShortcut(shortcut);
					return 0;
				}
				catch (const std::exception &e) {
					reportError(
						QObject::tr("failed to start shortcut: %1").arg(e.what()));
					return 1;
				}
			}
		}
		else if (OrganizerCore::isNxmLink(arguments.at(1))) {
			log::debug("starting download from command line: {}", arguments.at(1));
			organizer.externalMessage(arguments.at(1));
		}
		else {
			QString exeName = arguments.at(1);
			log::debug("starting {} from command line", exeName);
			arguments.removeFirst(); // remove application name (ModOrganizer.exe)
			arguments.removeFirst(); // remove binary name
			// pass the remaining parameters to the binary
			try {
				organizer.startApplication(exeName, arguments, QString(), QString());
				return 0;
			}
			catch (const std::exception &e) {
				reportError(
					QObject::tr("failed to start application: %1").arg(e.what()));
				return 1;
			}
		}
	}

    QPixmap pixmap(splashPath);
    QSplashScreen splash(pixmap);

    if (initSettings.contains("window_monitor")) {
      const int monitor = initSettings.value("window_monitor").toInt();

      if (monitor != -1 && QGuiApplication::screens().size() > monitor) {
        QGuiApplication::screens().at(monitor)->geometry().center();
        const QPoint center = QGuiApplication::screens().at(monitor)->geometry().center();
        splash.move(center - splash.rect().center());
      } else {
        const QPoint center = QGuiApplication::primaryScreen()->geometry().center();
        splash.move(center - splash.rect().center());
      }
    }

    splash.show();
    splash.activateWindow();

    QString apiKey;
    if (organizer.settings().getNexusApiKey(apiKey)) {
      NexusInterface::instance(&pluginContainer)->getAccessManager()->apiCheck(apiKey);
    }

    log::debug("initializing tutorials");
    TutorialManager::init(
        qApp->applicationDirPath() + "/"
            + QString::fromStdWString(AppConfig::tutorialsPath()) + "/",
        &organizer);

    if (!application.setStyleFile(initSettings.value("Settings/style", "").toString())) {
      // disable invalid stylesheet
      initSettings.setValue("Settings/style", "");
    }

    int res = 1;
    { // scope to control lifetime of mainwindow
      // set up main window and its data structures
      MainWindow mainWindow(initSettings, organizer, pluginContainer);

      NexusInterface::instance(&pluginContainer)
        ->getAccessManager()->setTopLevelWidget(&mainWindow);

      QObject::connect(&mainWindow, SIGNAL(styleChanged(QString)), &application,
                       SLOT(setStyleFile(QString)));
      QObject::connect(&instance, SIGNAL(messageSent(QString)), &organizer,
                       SLOT(externalMessage(QString)));

      mainWindow.processUpdates();

      // this must be before readSettings(), see DockFixer in mainwindow.cpp
      splash.finish(&mainWindow);

      mainWindow.readSettings();

      log::debug("displaying main window");
      mainWindow.show();
      mainWindow.activateWindow();

      splash.finish(&mainWindow);

      const auto ret = application.exec();

      NexusInterface::instance(&pluginContainer)
        ->getAccessManager()->setTopLevelWidget(nullptr);

      return ret;
    }
  } catch (const std::exception &e) {
    reportError(e.what());
    return 1;
  }
}

int doCoreDump(env::CoreDumpTypes type)
{
  env::Console c;

  // dump
  const auto b = env::coredumpOther(type);
  if (!b) {
    std::wcerr << L"\n>>>> a minidump file was not written\n\n";
  }

  std::wcerr << L"Press enter to continue...";
  std::wcin.get();

  return (b ? 0 : 1);
}

log::Levels convertQtLevel(QtMsgType t)
{
  switch (t)
  {
    case QtDebugMsg:
      return log::Debug;

    case QtWarningMsg:
      return log::Warning;

    case QtCriticalMsg:  // fall-through
    case QtFatalMsg:
      return log::Error;

    case QtInfoMsg:  // fall-through
    default:
      return log::Info;
  }
}

void qtLogCallback(
  QtMsgType type, const QMessageLogContext& context, const QString& message)
{
  std::string_view file = "";

  if (type != QtDebugMsg) {
    if (context.file) {
      file = context.file;

      const auto lastSep = file.find_last_of("/\\");
      if (lastSep != std::string_view::npos) {
        file = {context.file + lastSep + 1};
      }
    }
  }

  if (file.empty()) {
    log::log(
      convertQtLevel(type), "{}",
      message.toStdString());
  } else {
    log::log(
      convertQtLevel(type), "[{}:{}] {}",
      file, context.line, message.toStdString());
  }
}

void initLogging()
{
  LogModel::create();

  log::createDefault(MOBase::log::Debug, "%^[%m-%d %H:%M:%S.%e %L] %v%$");

  log::getDefault().setCallback(
    [](log::Entry e){ LogModel::instance().add(e); });

  qInstallMessageHandler(qtLogCallback);
}


int main(int argc, char *argv[])
{
  // handle --crashdump first
  for (int i=1; i<argc; ++i) {
    if (std::strcmp(argv[i], "--crashdump") == 0) {
      return doCoreDump(env::CoreDumpTypes::Mini);
    } else if (std::strcmp(argv[i], "--crashdump-data") == 0) {
      return doCoreDump(env::CoreDumpTypes::Data);
    } else if (std::strcmp(argv[i], "--crashdump-full") == 0) {
      return doCoreDump(env::CoreDumpTypes::Full);
    }
  }

  initLogging();

  //Make sure the configured temp folder exists
  QDir tempDir = QDir::temp();
  if (!tempDir.exists())
    tempDir.root().mkpath(tempDir.canonicalPath());

  //Should allow for better scaling of ui with higher resolution displays
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

  if (argc >= 4) {
    std::vector<std::wstring> arg;
    auto args = UntouchedCommandLineArguments(2, arg);
    if (arg[0] == L"launch")
      return SpawnWaitProcess(arg[1].c_str(), args);
  }

  MOApplication application(argc, argv);
  QStringList arguments = application.arguments();

  setupPath();

  bool forcePrimary = false;
  if (arguments.contains("update")) {
    arguments.removeAll("update");
    forcePrimary = true;
  }

  MOShortcut moshortcut{ arguments.size() > 1 ? arguments.at(1) : "" };

  SingleInstance instance(forcePrimary);
  if (!instance.primaryInstance()) {
    if (moshortcut ||
        arguments.size() > 1 && OrganizerCore::isNxmLink(arguments.at(1)))
    {
      log::debug("not primary instance, sending shortcut/download message");
      instance.sendMessage(arguments.at(1));
      return 0;
    } else if (arguments.size() == 1) {
      QMessageBox::information(
          nullptr, QObject::tr("Mod Organizer"),
          QObject::tr("An instance of Mod Organizer is already running"));
      return 0;
    }
  } // we continue for the primary instance OR if MO was called with parameters

  do {
    QString dataPath;

    try {
      InstanceManager& instanceManager = InstanceManager::instance();
      if (moshortcut && moshortcut.hasInstance())
        instanceManager.overrideInstance(moshortcut.instance());
      dataPath = instanceManager.determineDataPath();
    } catch (const std::exception &e) {
      if (strcmp(e.what(),"Canceled"))
        QMessageBox::critical(nullptr, QObject::tr("Failed to set up instance"), e.what());
      return 1;
    }
    application.setProperty("dataPath", dataPath);

    // initialize dump collection only after "dataPath" since the crashes are stored under it
    prevUnhandledExceptionFilter = SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);

    const auto logFile =
      qApp->property("dataPath").toString() + "/logs/mo_interface.log";

    log::getDefault().setFile(MOBase::log::File::rotating(
      logFile.toStdWString(), 5*1024*1024, 5));

    QString splash = dataPath + "/splash.png";
    if (!QFile::exists(dataPath + "/splash.png")) {
      splash = ":/MO/gui/splash";
    }

    int result = runApplication(application, instance, splash);
    if (result != INT_MAX) {
      return result;
    }
    argc = 1;
    moshortcut = MOShortcut("");
  } while (true);
}
