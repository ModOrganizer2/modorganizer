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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>
#include <cstdarg>
#include <inject.h>
#include <appconfig.h>
#include <utility.h>
#include <stdexcept>
#include "mainwindow.h"
#include "report.h"
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
#include <iostream>
#include <QMessageBox>
#include <QSharedMemory>
#include <QBuffer>
#include <QSplashScreen>
#include <QDirIterator>
#include <QDesktopServices>
#include <ShellAPI.h>
#include <eh.h>
#include <windows_error.h>
#include <boost/scoped_array.hpp>


#pragma comment(linker, "/manifestDependency:\"name='dlls' processorArchitecture='x86' version='1.0.0.0' type='win32' \"")


using namespace MOBase;
using namespace MOShared;


// set up required folders (for a first install or after an update or to fix a broken installation)
bool bootstrap()
{
  GameInfo &gameInfo = GameInfo::instance();

  // remove the temporary backup directory in case we're restarting after an update
  QString moDirectory = QDir::fromNativeSeparators(ToQString(gameInfo.getOrganizerDirectory()));
  QString backupDirectory = moDirectory.mid(0).append("/update_backup");
  if (QDir(backupDirectory).exists()) {
    shellDelete(QStringList(backupDirectory));
  }

  // cycle logfile
  removeOldFiles(ToQString(GameInfo::instance().getLogDir()), "ModOrganizer*.log", 5, QDir::Name);

  // create organizer directories
  QString dirNames[] = {
      QDir::fromNativeSeparators(ToQString(gameInfo.getProfilesDir())),
      QDir::fromNativeSeparators(ToQString(gameInfo.getModsDir())),
      QDir::fromNativeSeparators(ToQString(gameInfo.getDownloadDir())),
      QDir::fromNativeSeparators(ToQString(gameInfo.getOverwriteDir())),
      QDir::fromNativeSeparators(ToQString(gameInfo.getLogDir())),
      QDir::fromNativeSeparators(ToQString(gameInfo.getTutorialDir()))
    };
  static const int NUM_DIRECTORIES = sizeof(dirNames) / sizeof(QString);

  // optimistic run: try to simply create the directories:
  for (int i = 0; i < NUM_DIRECTORIES; ++i) {
    if (!QDir(dirNames[i]).exists()) {
      QDir().mkdir(dirNames[i]);
    }
  }

  // verify all directories exist and are writable,
  // otherwise invoke the helper to create them and make them writable
  for (int i = 0; i < NUM_DIRECTORIES; ++i) {
    QFileInfo fileInfo(dirNames[i]);
    if (!fileInfo.exists() || !fileInfo.isWritable()) {
      if (QMessageBox::question(NULL, QObject::tr("Permissions required"),
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
      // should have created them all so we can break the loop
      break;
    }
  }

  // verify the hook-dll exists
  QString dllName = qApp->applicationDirPath() + "/" + ToQString(AppConfig::hookDLLName());

  if (::GetModuleHandleW(ToWString(dllName).c_str()) != NULL) {
    throw std::runtime_error("hook.dll already loaded! You can't start Mod Organizer from within itself (not even indirectly)");
  }

  HMODULE dllMod = ::LoadLibraryW(ToWString(dllName).c_str());
  if (dllMod == NULL) {
    throw windows_error("hook.dll is missing or invalid");
  }
  ::FreeLibrary(dllMod);

  return true;
}


void cleanupDir()
{
  // files from previous versions of MO that are no longer
  // required (in that location)
  QString fileNames[] = {
    "NCC/GamebryoBase.dll",
    "plugins/helloWorld.dll",
    "plugins/testnexus.py"
  };

  static const int NUM_FILES = sizeof(fileNames) / sizeof(QString);

  qDebug("cleaning up unused files");

  for (int i = 0; i < NUM_FILES; ++i) {
    if (QFile::remove(QApplication::applicationDirPath().append("/").append(fileNames[i]))) {
      qDebug("%s removed in cleanup",
             QApplication::applicationDirPath().append("/").append(fileNames[i]).toUtf8().constData());
    }
  }
}

bool isNxmLink(const QString &link)
{
  return link.left(6).toLower() == "nxm://";
}

LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *exceptionPtrs)
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

      if (QMessageBox::question(NULL, QObject::tr("Woops"),
                            QObject::tr("ModOrganizer has crashed! Should a diagnostic file be created? If you send me this file "
                               "(%1) to sherb@gmx.net, the bug is a lot more likely to be fixed. "
                               "Please include a short description of what you were doing when the crash happened").arg(qApp->applicationFilePath().append(".dmp")),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

        std::wstring dumpName = ToWString(qApp->applicationFilePath().append(".dmp"));

        HANDLE dumpFile = ::CreateFile(dumpName.c_str(),
                                       GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (dumpFile != INVALID_HANDLE_VALUE) {
          _MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
          exceptionInfo.ThreadId = ::GetCurrentThreadId();
          exceptionInfo.ExceptionPointers = exceptionPtrs;
          exceptionInfo.ClientPointers = NULL;

          BOOL success = funcDump(::GetCurrentProcess(), ::GetCurrentProcessId(), dumpFile, MiniDumpNormal, &exceptionInfo, NULL, NULL);

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

  QMessageBox::critical(NULL, QObject::tr("Woops"),
                        QObject::tr("ModOrganizer has crashed! Unfortunately I was not able to write a diagnostic file: %1").arg(errorBuffer));
  return result;
}

void registerMetaTypes()
{
  registerExecutable();
}

bool HaveWriteAccess(const std::wstring &path)
{
  bool writable = false;

  const static SECURITY_INFORMATION requestedFileInformation = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;

  DWORD length = 0;
  if (!::GetFileSecurityW(path.c_str(), requestedFileInformation, NULL, NULL, &length)
      && (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
    std::string tempBuffer;
    tempBuffer.reserve(length);
    PSECURITY_DESCRIPTOR security = (PSECURITY_DESCRIPTOR)tempBuffer.data();
    if (security
        && ::GetFileSecurity(path.c_str(), requestedFileInformation, security, length, &length)) {
      HANDLE token = NULL;
      const static DWORD tokenDesiredAccess = TOKEN_IMPERSONATE | TOKEN_QUERY | TOKEN_DUPLICATE | STANDARD_RIGHTS_READ;
      if (!::OpenThreadToken(::GetCurrentThread(), tokenDesiredAccess, TRUE, &token)) {
        if (!::OpenProcessToken(::GetCurrentProcess(), tokenDesiredAccess, &token)) {
          throw std::runtime_error("Unable to get any thread or process token");
        }
      }

      HANDLE impersonatedToken = NULL;
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


int main(int argc, char *argv[])
{
  MOApplication application(argc, argv);

  qDebug("application name: %s", qPrintable(application.applicationName()));

  QString instanceID;
  QFile instanceFile(application.applicationDirPath() + "/INSTANCE");
  if (instanceFile.open(QIODevice::ReadOnly)) {
    instanceID = instanceFile.readAll().trimmed();
  }

  QString dataPath = instanceID.isEmpty() ? application.applicationDirPath()
                                          : QDir::fromNativeSeparators(
#if QT_VERSION >= 0x050000
                                              QStandardPaths::writableLocation(QStandardPaths::DataLocation)
#else
                                              QDesktopServices::storageLocation(QDesktopServices::DataLocation)
#endif
                                              ) + "/" + instanceID;
  application.setProperty("dataPath", dataPath);

#if QT_VERSION >= 0x050000
  qDebug("ssl support: %d", QSslSocket::supportsSsl());
#endif

  qDebug("data path: %s", qPrintable(dataPath));
  if (!QDir(dataPath).exists()) {
    if (!QDir().mkpath(dataPath)) {
      qCritical("failed to create %s", qPrintable(dataPath));
      return 1;
    }
  }

  application.setLibraryPaths(QStringList() << (application.applicationDirPath() + "/dlls"));

  SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);

  if (!HaveWriteAccess(ToWString(application.applicationDirPath()))) {
    QStringList arguments = application.arguments();
    arguments.pop_front();
    ::ShellExecuteW( NULL
                   , L"runas"
                   , ToWString(QString("\"%1\"").arg(QCoreApplication::applicationFilePath())).c_str()
                   , ToWString(arguments.join(" ")).c_str()
                   , ToWString(QDir::currentPath()).c_str(), SW_SHOWNORMAL);
    return 1;
  }
  LogBuffer::init(100, QtDebugMsg, qApp->property("dataPath").toString() + "/logs/mo_interface.log");

  qDebug("Working directory: %s", qPrintable(QDir::toNativeSeparators(QDir::currentPath())));
  qDebug("MO at: %s", qPrintable(QDir::toNativeSeparators(application.applicationDirPath())));
  QPixmap pixmap(":/MO/gui/splash");
  QSplashScreen splash(pixmap);
  splash.show();

  { // extend path to include dll directory so plugins don't need a manifest
    static const int BUFSIZE = 4096;

    boost::scoped_array<TCHAR> oldPath(new TCHAR[BUFSIZE]);
    DWORD offset = ::GetEnvironmentVariable(TEXT("PATH"), oldPath.get(), BUFSIZE);
    if (offset > BUFSIZE) {
      oldPath.reset(new TCHAR[offset]);
      ::GetEnvironmentVariable(TEXT("PATH"), oldPath.get(), offset);
    }

    std::tstring newPath(oldPath.get());
    newPath += TEXT(";");
    newPath += ToWString(QDir::toNativeSeparators(QCoreApplication::applicationDirPath())).c_str();
    newPath += TEXT("\\dlls");

    ::SetEnvironmentVariable(TEXT("PATH"), newPath.c_str());
  }

  registerMetaTypes();

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
        QMessageBox::information(NULL, QObject::tr("Mod Organizer"), QObject::tr("An instance of Mod Organizer is already running"));
        return 0;
      }
    } // we continue for the primary instance OR if MO has been called with parameters


    QSettings settings(dataPath + "/ModOrganizer.ini", QSettings::IniFormat);

    QString gamePath = QString::fromUtf8(settings.value("gamePath", "").toByteArray());

    bool done = false;
    while (!done) {
      if (!GameInfo::init(ToWString(application.applicationDirPath()), ToWString(dataPath), ToWString(QDir::toNativeSeparators(gamePath)))) {
        if (!gamePath.isEmpty()) {
          reportError(QObject::tr("No game identified in \"%1\". The directory is required to contain "
                                  "the game binary and its launcher.").arg(gamePath));
        }
        SelectionDialog selection(QObject::tr("Please select the game to manage"), NULL);

        { // add options
          QString skyrimPath    = ToQString(SkyrimInfo::getRegPathStatic());
          QString falloutNVPath = ToQString(FalloutNVInfo::getRegPathStatic());
          QString fallout3Path  = ToQString(Fallout3Info::getRegPathStatic());
          QString oblivionPath  = ToQString(OblivionInfo::getRegPathStatic());
          if (skyrimPath.length() != 0) {
            selection.addChoice(QString("Skyrim"), skyrimPath, skyrimPath);
          }
          if (falloutNVPath.length() != 0) {
            selection.addChoice(QString("Fallout NV"), falloutNVPath, falloutNVPath);
          }
          if (fallout3Path.length() != 0) {
            selection.addChoice(QString("Fallout 3"), fallout3Path, fallout3Path);
          }
          if (oblivionPath.length() != 0) {
            selection.addChoice(QString("Oblivion"), oblivionPath, oblivionPath);
          }

          selection.addChoice(QString("Browse..."), QString(), QString());
        }

        if (selection.exec() == QDialog::Rejected) {
          gamePath = "";
          done = true;
        } else {
          gamePath = QDir::cleanPath(selection.getChoiceData().toString());
          if (gamePath.isEmpty()) {
            gamePath = QFileDialog::getExistingDirectory(NULL, QObject::tr("Please select the game to manage"), QString(),
                                                         QFileDialog::ShowDirsOnly);
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

    if (!settings.contains("game_edition")) {
      std::vector<std::wstring> editions = GameInfo::instance().getSteamVariants();
      if (editions.size() > 1) {
        SelectionDialog selection(QObject::tr("Please select the game edition you have (MO can't start the game correctly if this is set incorrectly!)"), NULL);
        int index = 0;
        for (auto iter = editions.begin(); iter != editions.end(); ++iter) {
          selection.addChoice(ToQString(*iter), "", index++);
        }
        if (selection.exec() == QDialog::Rejected) {
          return -1;
        } else {
          settings.setValue("game_edition", selection.getChoiceData().toInt());
        }
      }
    }
#pragma message("edition isn't used?")
    qDebug("managing game at %s", qPrintable(QDir::toNativeSeparators(gamePath)));

    ExecutablesList executablesList;

    executablesList.init();

    if (!bootstrap()) { // requires gameinfo to be initialised!
      return -1;
    }

    cleanupDir();

    qDebug("setting up configured executables");

    int numCustomExecutables = settings.beginReadArray("customExecutables");
    for (int i = 0; i < numCustomExecutables; ++i) {
      settings.setArrayIndex(i);
      CloseMOStyle closeMO = settings.value("closeOnStart").toBool() ? DEFAULT_CLOSE : DEFAULT_STAY;
      executablesList.addExecutable(settings.value("title").toString(),
                                    settings.value("binary").toString(),
                                    settings.value("arguments").toString(),
                                    settings.value("workingDirectory", "").toString(),
                                    closeMO,
                                    settings.value("steamAppID", "").toString(),
                                    settings.value("custom", true).toBool(),
                                    settings.value("toolbar", false).toBool());
    }

    settings.endArray();

    qDebug("initializing tutorials");
    TutorialManager::init(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getTutorialDir())).append("/"));

    if (!application.setStyleFile(settings.value("Settings/style", "").toString())) {
      // disable invalid stylesheet
      settings.setValue("Settings/style", "");
    }

    int res = 1;
    { // scope to control lifetime of mainwindow
      // set up main window and its data structures
      MainWindow mainWindow(argv[0], settings);
      QObject::connect(&mainWindow, SIGNAL(styleChanged(QString)), &application, SLOT(setStyleFile(QString)));
      QObject::connect(&instance, SIGNAL(messageSent(QString)), &mainWindow, SLOT(externalMessage(QString)));

      mainWindow.setExecutablesList(executablesList);
      mainWindow.readSettings();

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
      } else {
        qDebug("configured profile: %s", qPrintable(selectedProfileName));
      }

      // if we have a command line parameter, it is either a nxm link or
      // a binary to start
      if ((arguments.size() > 1) && (!isNxmLink(arguments.at(1)))) {
        QString exeName = arguments.at(1);
        qDebug("starting %s from command line", qPrintable(exeName));
        arguments.removeFirst(); // remove application name (ModOrganizer.exe)
        arguments.removeFirst(); // remove binary name
        // pass the remaining parameters to the binary
        try {
          mainWindow.startApplication(exeName, arguments, QString(), selectedProfileName);
        } catch (const std::exception &e) {
          reportError(QObject::tr("failed to start application: %1").arg(e.what()));
        }

        return 0;
      }

      mainWindow.createFirstProfile();

      if (selectedProfileName.length() != 0) {
        if (!mainWindow.setCurrentProfile(selectedProfileName)) {
          mainWindow.setCurrentProfile(1);
          qWarning("failed to set profile: %s",
                   selectedProfileName.toUtf8().constData());
        }
      } else {
        mainWindow.setCurrentProfile(1);
      }

      qDebug("displaying main window");
      mainWindow.show();

      if ((arguments.size() > 1) &&
          (isNxmLink(arguments.at(1)))) {
        qDebug("starting download from command line: %s", qPrintable(arguments.at(1)));
        mainWindow.externalMessage(arguments.at(1));
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
