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
#include <cstdarg>
#include <inject.h>
#include <appconfig.h>
#include <utility.h>
#include <scopeguard.h>
#include <stdexcept>
#include "mainwindow.h"
#include <report.h>
#include "modlist.h"
#include "profile.h"
#include "gameinfo.h"
#include "fallout3info.h"
#include "falloutnvinfo.h"
#include "oblivioninfo.h"
#include "skyriminfo.h"
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
#include <iostream>
#include <ShellAPI.h>
#include <eh.h>
#include <windows_error.h>
#include <boost/scoped_array.hpp>
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


#pragma comment(linker, "/manifestDependency:\"name='dlls' processorArchitecture='x86' version='1.0.0.0' type='win32' \"")


using namespace MOBase;
using namespace MOShared;

bool createAndMakeWritable(const std::wstring &subPath)
{
  QString fullPath = qApp->property("dataPath").toString() + "/" + QString::fromStdWString(subPath);

  if (!QDir(fullPath).exists()) {
    QDir().mkdir(fullPath);
  }

  QFileInfo fileInfo(fullPath);
  if (!fileInfo.exists() || !fileInfo.isWritable()) {
    if (QMessageBox::question(nullptr, QObject::tr("Permissions required"),
        QObject::tr("The current user account doesn't have the required access rights to run "
           "Mod Organizer. The neccessary changes can be made automatically (the MO directory "
           "will be made writable for the current user account). You will be asked to run "
           "\"helper.exe\" with administrative rights."),
           QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Yes) {
      if (!Helper::init(GameInfo::instance().getOrganizerDirectory())) {
        return false;
      }
    } else {
      return false;
    }
    // no matter which directory didn't exist/wasn't writable, the helper
    // should have created them all so we don't have to worry this message box would appear repeatedly
  }
  return true;
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
                 "ModOrganizer*.log", 5, QDir::Name);

  createAndMakeWritable(AppConfig::profilesPath());
  createAndMakeWritable(AppConfig::modsPath());
  createAndMakeWritable(AppConfig::downloadPath());
  createAndMakeWritable(AppConfig::overwritePath());
  createAndMakeWritable(AppConfig::logPath());

  // verify the hook-dll exists
  QString dllName = qApp->applicationDirPath() + "/" + ToQString(AppConfig::hookDLLName());

  if (::GetModuleHandleW(ToWString(dllName).c_str()) != nullptr) {
    throw std::runtime_error("hook.dll already loaded! You can't start Mod Organizer from within itself (not even indirectly)");
  }

  HMODULE dllMod = ::LoadLibraryW(ToWString(dllName).c_str());
  if (dllMod == nullptr) {
    throw windows_error("hook.dll is missing or invalid");
  }
  ::FreeLibrary(dllMod);

  return true;
}

void cleanupDir()
{
  // files from previous versions of MO that are no longer
  // required (in that location)
  QStringList fileNames {
    "imageformats/",
    "loot/resources/",
    "plugins/previewDDS.dll",
    "dlls/boost_python-vc100-mt-1_55.dll",
    "dlls/QtCore4.dll",
    "dlls/QtDeclarative4.dll",
    "dlls/QtGui4.dll",
    "dlls/QtNetwork4.dll",
    "dlls/QtOpenGL4.dll",
    "dlls/QtScript4.dll",
    "dlls/QtSql4.dll",
    "dlls/QtSvg4.dll",
    "dlls/QtWebKit4.dll",
    "dlls/QtXml4.dll",
    "dlls/QtXmlPatterns4.dll",
    "msvcp100.dll",
    "msvcr100.dll",
    "proxy.dll"
  };

  for (const QString &fileName : fileNames) {
    QString fullPath = qApp->applicationDirPath() + "/" + fileName;
    if (QFile::exists(fullPath)) {
      if (shellDelete(QStringList(fullPath), true)) {
        qDebug("removed obsolete file %s", qPrintable(fullPath));
      } else {
        qDebug("failed to remove obsolete %s", qPrintable(fullPath));
      }
    }
  }
}


bool isNxmLink(const QString &link)
{
  return link.startsWith("nxm://", Qt::CaseInsensitive);
}

static LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *exceptionPtrs)
{
  typedef BOOL (WINAPI *FuncMiniDumpWriteDump)(HANDLE process, DWORD pid, HANDLE file, MINIDUMP_TYPE dumpType,
                                               const PMINIDUMP_EXCEPTION_INFORMATION exceptionParam,
                                               const PMINIDUMP_USER_STREAM_INFORMATION userStreamParam,
                                               const PMINIDUMP_CALLBACK_INFORMATION callbackParam);
  LONG result = EXCEPTION_CONTINUE_SEARCH;

  HMODULE dbgDLL = ::LoadLibrary(L"dbghelp.dll");

  static const int errorLen = 200;
  char errorBuffer[errorLen + 1];
  memset(errorBuffer, '\0', errorLen + 1);

  if (dbgDLL) {
    FuncMiniDumpWriteDump funcDump = (FuncMiniDumpWriteDump)::GetProcAddress(dbgDLL, "MiniDumpWriteDump");
    if (funcDump) {
      if (QMessageBox::question(nullptr, QObject::tr("Woops"),
                                QObject::tr("ModOrganizer has crashed! "
                                            "Should a diagnostic file be created? "
                                            "If you send me this file (%1) to sherb@gmx.net, "
                                            "the bug is a lot more likely to be fixed. "
                                            "Please include a short description of what you were "
                                            "doing when the crash happened"
                                            ).arg(qApp->applicationFilePath().append(".dmp")),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

        std::wstring dumpName = ToWString(qApp->applicationFilePath().append(".dmp"));

        HANDLE dumpFile = ::CreateFile(dumpName.c_str(),
                                       GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (dumpFile != INVALID_HANDLE_VALUE) {
          _MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
          exceptionInfo.ThreadId = ::GetCurrentThreadId();
          exceptionInfo.ExceptionPointers = exceptionPtrs;
          exceptionInfo.ClientPointers = false;

          BOOL success = funcDump(::GetCurrentProcess(), ::GetCurrentProcessId(), dumpFile,
                                  MiniDumpNormal, &exceptionInfo, nullptr, nullptr);

          ::FlushFileBuffers(dumpFile);
          ::CloseHandle(dumpFile);
          if (success) {
            return EXCEPTION_EXECUTE_HANDLER;
          }
          _snprintf(errorBuffer, errorLen, "failed to save minidump to %ls (error %lu)",
                    dumpName.c_str(), ::GetLastError());
        } else {
          _snprintf(errorBuffer, errorLen, "failed to create %ls (error %lu)",
                    dumpName.c_str(), ::GetLastError());
        }
      } else {
        return result;
      }
    } else {
      _snprintf(errorBuffer, errorLen, "dbghelp.dll outdated");
    }
  } else {
    _snprintf(errorBuffer, errorLen, "dbghelp.dll not found");
  }

  QMessageBox::critical(nullptr, QObject::tr("Woops"),
                        QObject::tr("ModOrganizer has crashed! Unfortunately I was not able to write a diagnostic file: %1").arg(errorBuffer));
  return result;
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

int main(int argc, char *argv[])
{
  MOApplication application(argc, argv);

  qDebug("application name: %s", qPrintable(application.applicationName()));

  QString instanceID;
  QFile instanceFile(application.applicationDirPath() + "/INSTANCE");
  if (instanceFile.open(QIODevice::ReadOnly)) {
    instanceID = instanceFile.readAll().trimmed();
  }  

  QString dataPath =
      instanceID.isEmpty() ? application.applicationDirPath()
                           : QDir::fromNativeSeparators(
                               QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                               + "/" + instanceID
                               );
  application.setProperty("dataPath", dataPath);

  if (!QDir(dataPath).exists()) {
    if (!QDir().mkpath(dataPath)) {
      qCritical("failed to create %s", qPrintable(dataPath));
      return 1;
    }
  }

  SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);

  if (!HaveWriteAccess(ToWString(application.applicationDirPath()))) {
    QStringList arguments = application.arguments();
    arguments.pop_front();
    ::ShellExecuteW( nullptr
                   , L"runas"
                   , ToWString(QString("\"%1\"").arg(QCoreApplication::applicationFilePath())).c_str()
                   , ToWString(arguments.join(" ")).c_str()
                   , ToWString(QDir::currentPath()).c_str(), SW_SHOWNORMAL);
    return 1;
  }

  QPixmap pixmap(":/MO/gui/splash");
  QSplashScreen splash(pixmap);

  try {
    if (!bootstrap()) { // requires gameinfo to be initialised!
      return -1;
    }

    LogBuffer::init(100, QtDebugMsg, qApp->property("dataPath").toString() + "/logs/mo_interface.log");

#if QT_VERSION >= 0x050000 && !defined(QT_NO_SSL)
    qDebug("ssl support: %d", QSslSocket::supportsSsl());
#endif

    qDebug("Working directory: %s", qPrintable(QDir::toNativeSeparators(QDir::currentPath())));
    qDebug("MO at: %s", qPrintable(QDir::toNativeSeparators(application.applicationDirPath())));
    splash.show();

    cleanupDir();
  } catch (const std::exception &e) {
    reportError(e.what());
    return 1;
  }

  { // extend path to include dll directory so plugins don't need a manifest
    // (using AddDllDirectory would be an alternative to this but it seems fairly complicated esp.
    //  since it isn't easily accessible on Windows < 8
    //  SetDllDirectory replaces other search directories and this seems to propagate to child processes)
    static const int BUFSIZE = 4096;

    boost::scoped_array<TCHAR> oldPath(new TCHAR[BUFSIZE]);
    DWORD offset = ::GetEnvironmentVariable(TEXT("PATH"), oldPath.get(), BUFSIZE);
    if (offset > BUFSIZE) {
      oldPath.reset(new TCHAR[offset]);
      ::GetEnvironmentVariable(TEXT("PATH"), oldPath.get(), offset);
    }

    std::wstring newPath(oldPath.get());
    newPath += L";";
    newPath += ToWString(QDir::toNativeSeparators(QCoreApplication::applicationDirPath())).c_str();
    newPath += L"\\dlls";

    ::SetEnvironmentVariableW(L"PATH", newPath.c_str());
  }

  QStringList arguments = application.arguments();

  bool forcePrimary = false;
  if (arguments.contains("update")) {
    arguments.removeAll("update");
    forcePrimary = true;
  }

  try {
    SingleInstance instance(forcePrimary);
    if (!instance.primaryInstance()) {
      if ((arguments.size() == 2) && isNxmLink(arguments.at(1))) {
        qDebug("not primary instance, sending download message");
        instance.sendMessage(arguments.at(1));
        return 0;
      } else if (arguments.size() == 1) {
        QMessageBox::information(nullptr, QObject::tr("Mod Organizer"), QObject::tr("An instance of Mod Organizer is already running"));
        return 0;
      }
    } // we continue for the primary instance OR if MO has been called with parameters

    QSettings settings(dataPath + "/" + QString::fromStdWString(AppConfig::iniFileName()), QSettings::IniFormat);
    qDebug("initializing core");
    OrganizerCore organizer(settings);
    qDebug("initialize plugins");
    PluginContainer pluginContainer(&organizer);
    pluginContainer.loadPlugins();

    QString gamePath = QString::fromUtf8(settings.value("gamePath", "").toByteArray());
    bool done = false;
    while (!done) {
      if (!GameInfo::init(ToWString(application.applicationDirPath()), ToWString(dataPath), ToWString(QDir::toNativeSeparators(gamePath)))) {
        if (!gamePath.isEmpty()) {
          reportError(QObject::tr("No game identified in \"%1\". The directory is required to contain "
                                  "the game binary and its launcher.").arg(gamePath));
        }
        SelectionDialog selection(QObject::tr("Please select the game to manage"), nullptr, QSize(32, 32));

        for (const IPluginGame * const game : pluginContainer.plugins<IPluginGame>()) {
          if (game->isInstalled()) {
            QString path = game->gameDirectory().absolutePath();
            selection.addChoice(game->gameIcon(), game->gameName(), path, path);
          }
        }

        selection.addChoice(QString("Browse..."), QString(), QString());

        if (selection.exec() == QDialog::Rejected) {
          gamePath = "";
          done = true;
        } else {
          gamePath = QDir::cleanPath(selection.getChoiceData().toString());
          if (gamePath.isEmpty()) {
            gamePath = QFileDialog::getExistingDirectory(
                  nullptr, QObject::tr("Please select the game to manage"), QString(),
                  QFileDialog::ShowDirsOnly);
            qDebug() << "manually selected path " << gamePath;
          }
        }
      } else {
        done = true;
        gamePath = ToQString(GameInfo::instance().getGameDirectory());
      }
    }

    if (gamePath.isEmpty()) {
      // game not found and user canceled
      return -1;
    } else if (gamePath.length() != 0) {
      // user selected a folder and game was initialised with it
      qDebug("game path: %s", qPrintable(gamePath));
      settings.setValue("gamePath", gamePath.toUtf8().constData());
    }

    organizer.setManagedGame(ToQString(GameInfo::instance().getGameName()), gamePath);

    organizer.createDefaultProfile();

    if (pluginContainer.managedGame(ToQString(GameInfo::instance().getGameName())) == nullptr) {
      reportError(QObject::tr("Plugin to handle %1 not installed").arg(ToQString(GameInfo::instance().getGameName())));
      return 1;
    }

    IPluginGame *game = organizer.managedGame();

    if (!settings.contains("game_edition")) {
      QStringList editions = game->gameVariants();
      if (editions.size() > 1) {
        SelectionDialog selection(QObject::tr("Please select the game edition you have (MO can't start the game correctly if this is set incorrectly!)"), nullptr);
        int index = 0;
        for (const QString &edition : editions) {
          selection.addChoice(edition, "", index++);
        }
        if (selection.exec() == QDialog::Rejected) {
          return -1;
        } else {
          settings.setValue("game_edition", selection.getChoiceString());
        }
      }
    }
    game->setGameVariant(settings.value("game_edition").toString());


#pragma message("edition isn't used?")
    qDebug("managing game at %s", qPrintable(QDir::toNativeSeparators(gamePath)));

    organizer.updateExecutablesList(settings);

    QString selectedProfileName = determineProfile(arguments, settings);
    organizer.setCurrentProfile(selectedProfileName);

    // if we have a command line parameter, it is either a nxm link or
    // a binary to start
    if ((arguments.size() > 1)
        && !isNxmLink(arguments.at(1))) {
      QString exeName = arguments.at(1);
      qDebug("starting %s from command line", qPrintable(exeName));
      arguments.removeFirst(); // remove application name (ModOrganizer.exe)
      arguments.removeFirst(); // remove binary name
      // pass the remaining parameters to the binary
      try {
        organizer.startApplication(exeName, arguments, QString(), QString());
        return 0;
      } catch (const std::exception &e) {
        reportError(QObject::tr("failed to start application: %1").arg(e.what()));
        return 1;
      }
    }

    NexusInterface::instance()->getAccessManager()->startLoginCheck();

    qDebug("initializing tutorials");
    TutorialManager::init(qApp->applicationDirPath() + "/" + QString::fromStdWString(AppConfig::tutorialsPath()) + "/",
                          &organizer);

    if (!application.setStyleFile(settings.value("Settings/style", "").toString())) {
      // disable invalid stylesheet
      settings.setValue("Settings/style", "");
    }

    int res = 1;
    { // scope to control lifetime of mainwindow
      // set up main window and its data structures
      MainWindow mainWindow(argv[0], settings, organizer, pluginContainer);

      QObject::connect(&mainWindow, SIGNAL(styleChanged(QString)), &application, SLOT(setStyleFile(QString)));
      QObject::connect(&instance, SIGNAL(messageSent(QString)), &organizer, SLOT(externalMessage(QString)));

      mainWindow.readSettings();

      qDebug("displaying main window");
      mainWindow.show();

      if ((arguments.size() > 1)
          && isNxmLink(arguments.at(1))) {
        qDebug("starting download from command line: %s", qPrintable(arguments.at(1)));
        organizer.externalMessage(arguments.at(1));
      }
      splash.finish(&mainWindow);
      res = application.exec();
    }
    return res;
  } catch (const std::exception &e) {
    reportError(e.what());
    return 1;
  }
}
