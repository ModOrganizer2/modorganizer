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

#include "loadmechanism.h"
#include "utility.h"
#include "util.h"
#include <gameinfo.h>
#include <appconfig.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QMessageBox>
#include <QCryptographicHash>


using namespace MOBase;
using namespace MOShared;


LoadMechanism::LoadMechanism()
  : m_SelectedMechanism(LOAD_MODORGANIZER)
{
}

void LoadMechanism::writeHintFile(const QDir &targetDirectory)
{
  QString hintFilePath = targetDirectory.absoluteFilePath("mo_path.txt");
  QFile hintFile(hintFilePath);
  if (hintFile.exists()) {
    hintFile.remove();
  }
  if (!hintFile.open(QIODevice::WriteOnly)) {
    throw MyException(QObject::tr("failed to open %1: %2").arg(hintFilePath).arg(hintFile.errorString()));
  }
  hintFile.write(ToString(GameInfo::instance().getOrganizerDirectory(), true).c_str());
  hintFile.close();
}


void LoadMechanism::removeHintFile(QDir &targetDirectory)
{
  targetDirectory.remove("mo_path.txt");
}


bool LoadMechanism::isDirectLoadingSupported()
{
  if (GameInfo::instance().getType() == GameInfo::TYPE_OBLIVION) {
    // oblivion can be loaded directly if it's not the steam variant
    QString gamePath = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory()));
    QDir gameDir(gamePath);
    // test if this is the steam version of oblivion
    if (QFile(gameDir.absoluteFilePath("steam_api.dll")).exists()) {
      return false;
    }
  }

  // all other games work afaik
  return true;
}


bool LoadMechanism::isScriptExtenderSupported()
{
  QString targetPath = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory()));
  if ((QFile(targetPath.mid(0).append("/").append(ToQString(GameInfo::instance().getSEName())).append("_loader.exe")).exists()) ||
      (QFile(targetPath.mid(0).append("/").append(ToQString(GameInfo::instance().getSEName())).append("_steam_loader.dll")).exists())) {
    return true;
  } else {
    return false;
  }
}


bool LoadMechanism::isProxyDLLSupported()
{
  QString targetPath = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory()));
  targetPath.append("/").append(ToQString(AppConfig::proxyDLLTarget()));
  return QFile(targetPath).exists();
}


bool LoadMechanism::hashIdentical(const QString &fileNameLHS, const QString &fileNameRHS)
{
  QFile fileLHS(fileNameLHS);
  if (!fileLHS.open(QIODevice::ReadOnly)) {
    throw MyException(QObject::tr("%1 not found").arg(fileNameLHS));
  }
  QByteArray dataLHS = fileLHS.readAll();
  QByteArray hashLHS = QCryptographicHash::hash(dataLHS, QCryptographicHash::Md5);

  fileLHS.close();

  QFile fileRHS(fileNameRHS);
  if (!fileRHS.open(QIODevice::ReadOnly)) {
    throw MyException(QObject::tr("%1 not found").arg(fileNameRHS));
  }
  QByteArray dataRHS = fileRHS.readAll();
  QByteArray hashRHS = QCryptographicHash::hash(dataRHS, QCryptographicHash::Md5);

  fileRHS.close();

  return hashLHS == hashRHS;
}


void LoadMechanism::deactivateScriptExtender()
{
  try {
    QString gameDirectory = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory()));

    QString pluginsDirPath = gameDirectory;
    pluginsDirPath.append("/data/").append(ToQString(GameInfo::instance().getSEName())).append("/plugins");

    QDir pluginsDir(pluginsDirPath);

    QString hookDLLName = ToQString(AppConfig::hookDLLName());
    if (QFile(pluginsDir.absoluteFilePath(hookDLLName)).exists()) {
      // remove dll from SE plugins directory
      if (!pluginsDir.remove(hookDLLName)) {
        throw MyException(QObject::tr("Failed to delete %1").arg(pluginsDir.absoluteFilePath(hookDLLName)));
      }
    }

    removeHintFile(pluginsDir);
  } catch (const std::exception &e) {
    QMessageBox::critical(nullptr, QObject::tr("Failed to deactivate script extender loading"), e.what());
  }
}


void LoadMechanism::deactivateProxyDLL()
{
  try {
    QString gameDirectory = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory()));

    QString targetPath = gameDirectory;
    targetPath.append("/").append(ToQString(AppConfig::proxyDLLTarget()));

    QFile targetDLL(targetPath);
    if (targetDLL.exists()) {
      QString origFile = gameDirectory.mid(0).append(ToQString(AppConfig::proxyDLLOrig()));
      // determine if a proxy-dll is installed
      // this is a very crude way of making this decision but it should be good enough
      if ((targetDLL.size() < 24576) && (QFile(origFile).exists())) {
        // remove proxy-dll
        if (!targetDLL.remove()) {
          throw MyException(QObject::tr("Failed to remove %1: %2").arg(targetPath).arg(targetDLL.errorString()));
        } else if (!QFile::rename(origFile, targetPath)) {
          throw MyException(QObject::tr("Failed to rename %1 to %2").arg(origFile, targetPath));
        }
      }
    }

    QDir dir(gameDirectory);
    removeHintFile(dir);
  } catch (const std::exception &e) {
    QMessageBox::critical(nullptr, QObject::tr("Failed to deactivate proxy-dll loading"), e.what());
  }
}


void LoadMechanism::activateScriptExtender()
{
  try {
    QString gameDirectory = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory()));

    QString pluginsDirPath = gameDirectory;
    pluginsDirPath.append("/data/").append(ToQString(GameInfo::instance().getSEName())).append("/plugins");

    QDir pluginsDir(pluginsDirPath);

    if (!pluginsDir.exists()) {
      pluginsDir.mkpath(".");
    }

    QString targetPath = pluginsDir.absoluteFilePath(ToQString(AppConfig::hookDLLName()));
    QString hookDLLPath = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getOrganizerDirectory())).append("/").append(ToQString(AppConfig::hookDLLName()));

    QFile dllFile(targetPath);

    if (dllFile.exists()) {
      // may be outdated
      if (!hashIdentical(targetPath, hookDLLPath)) {
        dllFile.remove();
      }
    }

    if (!dllFile.exists()) {
      // install dll to SE plugins
      if (!QFile::copy(hookDLLPath, targetPath)) {
        throw MyException(QObject::tr("Failed to copy %1 to %2").arg(hookDLLPath, targetPath));
      }
    }
    writeHintFile(pluginsDir);
  } catch (const std::exception &e) {
    QMessageBox::critical(nullptr, QObject::tr("Failed to set up script extender loading"), e.what());
  }
}


void LoadMechanism::activateProxyDLL()
{
  try {
    QString gameDirectory = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory()));

    QString targetPath = gameDirectory;
    targetPath.append("/").append(ToQString(AppConfig::proxyDLLTarget()));
    QFile targetDLL(targetPath);
    if (!targetDLL.exists()) {
      return;
    }

    QString sourcePath = QDir::fromNativeSeparators(
          ToQString(GameInfo::instance().getOrganizerDirectory())).append("/").append(ToQString(AppConfig::proxyDLLSource()));

    // this is a very crude way of making this decision but it should be good enough
    if (targetDLL.size() < 24576) {
      // determine if a proxy-dll is already installed and if so, if it's the right one
      if (!hashIdentical(targetPath, sourcePath)) {
        // wrong proxy dll, probably outdated. delete and install the new one
        if (!QFile::remove(targetPath)) {
          throw MyException(QObject::tr("Failed to delete old proxy-dll %1").arg(targetPath));
        }
        if (!QFile::copy(sourcePath, targetPath)) {
          throw MyException(QObject::tr("Failed to copy %1 to %2").arg(sourcePath).arg(targetPath));
        }
      } // otherwise the proxy-dll is already the right one
    } else {
      // no proxy dll installed yet. move the original and insert proxy-dll

      QString origFile = gameDirectory;
      origFile.append("/").append(ToQString(AppConfig::proxyDLLOrig()));

      if (QFile(origFile).exists()) {
        // orig-file exists. this may happen if the steam-api was updated or the user messed with the
        // dlls.
        if (!QFile::remove(origFile)) {
          throw MyException(QObject::tr("Failed to overwrite %1").arg(origFile));
        }
      }
      if (!QFile::rename(targetPath, origFile)) {
        throw MyException(QObject::tr("Failed to rename %1 to %2").arg(targetPath).arg(origFile));
      }
      if (!QFile::copy(sourcePath, targetPath)) {
        throw MyException(QObject::tr("Failed to copy %1 to %2").arg(sourcePath).arg(targetPath));
      }
    }
    writeHintFile(QDir(gameDirectory));
  } catch (const std::exception &e) {
    QMessageBox::critical(nullptr, QObject::tr("Failed to set up proxy-dll loading"), e.what());
  }
}


void LoadMechanism::activate(EMechanism mechanism)
{
  switch (mechanism) {
    case LOAD_MODORGANIZER: {
      deactivateProxyDLL();
      deactivateScriptExtender();
    } break;
    case LOAD_SCRIPTEXTENDER: {
      deactivateProxyDLL();
      activateScriptExtender();
    } break;
    case LOAD_PROXYDLL: {
      deactivateScriptExtender();
      activateProxyDLL();
    } break;
  }
}

