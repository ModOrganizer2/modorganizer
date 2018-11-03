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
#include "logbuffer.h"
#include "selectiondialog.h"
#include "moapplication.h"
#include "tutorialmanager.h"
#include "nxmaccessmanager.h"
#include "instancemanager.h"
#include "moshortcut.h"
#include "organizercore.h"

#include <eh.h>
#include <windows_error.h>
#include <usvfs.h>

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
    qCritical("ModOrganizer has crashed, crash dump created.");
  else
    qCritical("ModOrganizer has crashed, CreateMiniDump failed (%d, error %lu).", dumpRes, GetLastError());

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
      qDebug("profile overwritten on command line");
      selectedProfileName = arguments.at(profileIndex + 1);
    }
    arguments.removeAt(profileIndex);
    arguments.removeAt(profileIndex);
  }
  if (selectedProfileName.isEmpty()) {
    qDebug("no configured profile");
    selectedProfileName = "Default";
  } else {
    qDebug("configured profile: %s", qPrintable(selectedProfileName));
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
  //If the game name has been set up, use that.
  QString gameName = settings.value("gameName", "").toString();
  if (!gameName.isEmpty()) {
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
    if (game->looksValid(gameDir)) {
      return selectGame(settings, gameDir, game);
    }
  }

  //gameName wasn't set, or otherwise can't be found. Try looking through all
  //the plugins using the gamePath
  QString gamePath = QString::fromUtf8(settings.value("gamePath", "").toByteArray());
  if (!gamePath.isEmpty()) {
    QDir gameDir(gamePath);
    //Look to see if one of the installed games binary file exists in the current
    //game directory.
    for (IPluginGame * const game : plugins.plugins<IPluginGame>()) {
      if (game->looksValid(gameDir)) {
        return selectGame(settings, gameDir, game);
      }
    }
  }

  //The following code would try to determine the right game to mange but it would usually find the wrong one
  //so it was commented out.
  /* 
  //OK, we are in a new setup or existing info is useless.
  //See if MO has been installed inside a game directory
  for (IPluginGame * const game : plugins.plugins<IPluginGame>()) {
    if (game->isInstalled() && moPath.startsWith(game->gameDirectory().absolutePath())) {
      //Found it.
      return selectGame(settings, game->gameDirectory(), game);
    }
  }

  //Try walking up the directory tree to see if MO has been installed inside a game
  {
    QDir gameDir(moPath);
    do {
      //Look to see if one of the installed games binary file exists in the current
      //directory.
      for (IPluginGame * const game : plugins.plugins<IPluginGame>()) {
        if (game->looksValid(gameDir)) {
          return selectGame(settings, gameDir, game);
        }
      }
      //OK, chop off the last directory and try again
    } while (gameDir.cdUp());
  }
  */

  //Then try a selection dialogue.
  if (!gamePath.isEmpty() || !gameName.isEmpty()) {
    reportError(QObject::tr("Could not use configuration settings for game \"%1\", path \"%2\".").
                                                   arg(gameName).arg(gamePath));
  }

  SelectionDialog selection(QObject::tr("Please select the game to manage"), nullptr, QSize(32, 32));

  for (IPluginGame *game : plugins.plugins<IPluginGame>()) {
    if (game->isInstalled()) {
      QString path = game->gameDirectory().absolutePath();
      selection.addChoice(game->gameIcon(), game->gameName(), path, QVariant::fromValue(game));
    }
  }

  selection.addChoice(QString("Browse..."), QString(), QVariant::fromValue(static_cast<IPluginGame *>(nullptr)));

  while (selection.exec() != QDialog::Rejected) {
    IPluginGame * game = selection.getChoiceData().value<IPluginGame *>();
    if (game != nullptr) {
      return selectGame(settings, game->gameDirectory(), game);
    }

    gamePath = QFileDialog::getExistingDirectory(
          nullptr, QObject::tr("Please select the game to manage"), QString(),
          QFileDialog::ShowDirsOnly);

    if (!gamePath.isEmpty()) {
      QDir gameDir(gamePath);
      for (IPluginGame * const game : plugins.plugins<IPluginGame>()) {
        if (game->looksValid(gameDir)) {
          return selectGame(settings, gameDir, game);
        }
      }
      reportError(QObject::tr("No game identified in \"%1\". The directory is required to contain "
                              "the game binary and its launcher.").arg(gamePath));
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

  qDebug("MO at: %s", qPrintable(QDir::toNativeSeparators(
                          QCoreApplication::applicationDirPath())));

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

static void preloadSsl()
{
  QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
  if (GetModuleHandleA("libeay32.dll"))
    qWarning("libeay32.dll already loaded?!");
  else {
    QString libeay32 = appPath + "\\libeay32.dll";
    if (!QFile::exists(libeay32))
      qWarning("libeay32.dll not found: %s", qPrintable(libeay32));
    else if (!LoadLibraryW(libeay32.toStdWString().c_str()))
      qWarning("failed to load: %s, %d", qPrintable(libeay32), GetLastError());
  }
  if (GetModuleHandleA("ssleay32.dll"))
    qWarning("ssleay32.dll already loaded?!");
  else {
    QString ssleay32 = appPath + "\\ssleay32.dll";
    if (!QFile::exists(ssleay32))
      qWarning("ssleay32.dll not found: %s", qPrintable(ssleay32));
    else if (!LoadLibraryW(ssleay32.toStdWString().c_str()))
      qWarning("failed to load: %s, %d", qPrintable(ssleay32), GetLastError());
  }
}

static QString getVersionDisplayString()
{
  return createVersionInfo().displayString();
}

int runApplication(MOApplication &application, SingleInstance &instance,
                   const QString &splashPath)
{

  qDebug("Starting Mod Organizer version %s revision %s", qPrintable(getVersionDisplayString()),
#if defined(HGID)
    HGID
#elif defined(GITID)
    GITID
#else
    "unknown"
#endif
  );

#if !defined(QT_NO_SSL)
  preloadSsl();
  qDebug("ssl support: %d", QSslSocket::supportsSsl());
#else
  qDebug("non-ssl build");
#endif

  QString dataPath = application.property("dataPath").toString();
  qDebug("data path: %s", qPrintable(dataPath));

  if (!bootstrap()) {
    reportError("failed to set up data paths");
    return 1;
  }

  QWindowsWindowFunctions::setWindowActivationBehavior(QWindowsWindowFunctions::AlwaysActivateWindow);

  QStringList arguments = application.arguments();

  try {
    qDebug("Working directory: %s", qPrintable(QDir::toNativeSeparators(QDir::currentPath())));

    QSettings settings(dataPath + "/"
                           + QString::fromStdWString(AppConfig::iniFileName()),
                       QSettings::IniFormat);

    // global crashDumpType sits in OrganizerCore to make a bit less ugly to update it when the settings are changed during runtime
    OrganizerCore::setGlobalCrashDumpsType(settings.value("Settings/crash_dumps_type", static_cast<int>(CrashDumpsType::Mini)).toInt());

    qDebug("Loaded settings:");
    settings.beginGroup("Settings");
    for (auto k : settings.allKeys())
      if (!k.contains("username") && !k.contains("password"))
        qDebug("  %s=%s", k.toUtf8().data(), settings.value(k).toString().toUtf8().data());
    settings.endGroup();


    qDebug("initializing core");
    OrganizerCore organizer(settings);
    if (!organizer.bootstrap()) {
      reportError("failed to set up data paths");
      return 1;
    }
    qDebug("initialize plugins");
    PluginContainer pluginContainer(&organizer);
    pluginContainer.loadPlugins();

    MOBase::IPluginGame *game = determineCurrentGame(
        application.applicationDirPath(), settings, pluginContainer);
    if (game == nullptr) {
      return 1;
    }
    if (splashPath.startsWith(':')) {
      // currently using MO splash, see if the plugin contains one
      QString pluginSplash
          = QString(":/%1/splash").arg(game->gameShortName());
      QImage image(pluginSplash);
      if (!image.isNull()) {
        image.save(dataPath + "/splash.png");
      } else {
        qDebug("no plugin splash");
      }
    }

    organizer.setManagedGame(game);
    organizer.createDefaultProfile();

    if (!settings.contains("game_edition")) {
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
          settings.setValue("game_edition", selection.getChoiceString());
        }
      }
    }
    game->setGameVariant(settings.value("game_edition").toString());

    qDebug("managing game at %s", qPrintable(QDir::toNativeSeparators(
                                      game->gameDirectory().absolutePath())));

    organizer.updateExecutablesList(settings);

    QString selectedProfileName = determineProfile(arguments, settings);
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
			qDebug("starting download from command line: %s",
				qPrintable(arguments.at(1)));
			organizer.externalMessage(arguments.at(1));
		}
		else {
			QString exeName = arguments.at(1);
			qDebug("starting %s from command line", qPrintable(exeName));
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
    splash.show();
    splash.activateWindow();

    NexusInterface::instance(&pluginContainer)->getAccessManager()->startLoginCheck();

    qDebug("initializing tutorials");
    TutorialManager::init(
        qApp->applicationDirPath() + "/"
            + QString::fromStdWString(AppConfig::tutorialsPath()) + "/",
        &organizer);

    if (!application.setStyleFile(settings.value("Settings/style", "").toString())) {
      // disable invalid stylesheet
      settings.setValue("Settings/style", "");
    }

    int res = 1;
    { // scope to control lifetime of mainwindow
      // set up main window and its data structures
      MainWindow mainWindow(settings, organizer, pluginContainer);

      QObject::connect(&mainWindow, SIGNAL(styleChanged(QString)), &application,
                       SLOT(setStyleFile(QString)));
      QObject::connect(&instance, SIGNAL(messageSent(QString)), &organizer,
                       SLOT(externalMessage(QString)));

      mainWindow.processUpdates();
      mainWindow.readSettings();

      qDebug("displaying main window");
      mainWindow.show();
      mainWindow.activateWindow();

      splash.finish(&mainWindow);
      return application.exec();
    }
  } catch (const std::exception &e) {
    reportError(e.what());
    return 1;
  }
}


int main(int argc, char *argv[])
{
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
      qDebug("not primary instance, sending shortcut/download message");
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

    LogBuffer::init(1000, QtDebugMsg, qApp->property("dataPath").toString() + "/logs/mo_interface.log");

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
