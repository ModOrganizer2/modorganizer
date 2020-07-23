/*
Copyright (C) 2016 Sebastian Herbord. All rights reserved.

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


#include "instancemanager.h"
#include "selectiondialog.h"
#include "settings.h"
#include "shared/appconfig.h"
#include "plugincontainer.h"
#include <report.h>
#include <iplugingame.h>
#include <utility.h>
#include <log.h>

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QInputDialog>
#include <QMessageBox>
#include <cstdint>

using namespace MOBase;

static const char COMPANY_NAME[]     = "Tannin";
static const char APPLICATION_NAME[] = "Mod Organizer";
static const char INSTANCE_KEY[]     = "CurrentInstance";


InstanceManager::InstanceManager()
  : m_AppSettings(COMPANY_NAME, APPLICATION_NAME)
{
}

InstanceManager &InstanceManager::instance()
{
  static InstanceManager s_Instance;
  return s_Instance;
}

void InstanceManager::overrideInstance(const QString& instanceName)
{
  m_overrideInstanceName = instanceName;
  m_overrideInstance = true;
}

void InstanceManager::overrideProfile(const QString& profileName)
{
  m_overrideProfileName = profileName;
  m_overrideProfile = true;
}

QString InstanceManager::currentInstance() const
{
  if (m_overrideInstance)
    return m_overrideInstanceName;
  else
    return m_AppSettings.value(INSTANCE_KEY, "").toString();
}

void InstanceManager::clearCurrentInstance()
{
  setCurrentInstance("");
  m_Reset = true;
  m_overrideInstance = false;
}

void InstanceManager::setCurrentInstance(const QString &name)
{
  m_AppSettings.setValue(INSTANCE_KEY, name);
}

bool InstanceManager::deleteLocalInstance(const QString &instanceId) const
{
  bool result = true;
  QString instancePath = QDir::fromNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/" + instanceId);

  if (QMessageBox::warning(nullptr, QObject::tr("Deleting folder"),
                           QObject::tr("I'm about to delete the following folder: \"%1\". Proceed?").arg(instancePath), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::No ){
    return false;
  }

  if (!MOBase::shellDelete(QStringList(instancePath),true))
  {
    log::warn(
      "Failed to shell-delete \"{}\" (errorcode {}), trying regular delete",
      instancePath, ::GetLastError());

    if (!MOBase::removeDir(instancePath))
    {
      log::warn("regular delete failed too");
      result = false;
    }
  }

  return result;
}

QString InstanceManager::manageInstances(const QStringList &instanceList) const
{
	SelectionDialog selection(
		QString("<h3>%1</h3><br>%2")
		.arg(QObject::tr("Choose Instance to Delete"))
		.arg(QObject::tr("Be Careful! Deleting an Instance will remove all your files for that Instance (mods, downloads, profiles, configuration, ...). Custom paths outside of the instance folder for downloads, mods, etc. will be left untoched.")),
		nullptr);
	for (const QString &instance : instanceList)
	{
		selection.addChoice(QIcon(":/MO/gui/multiply_red"), instance, "", instance);
	}

	if (selection.exec() == QDialog::Rejected) {
		return(chooseInstance(instances()));
	}
	else {
		QString choice = selection.getChoiceData().toString();
		{
			if (QMessageBox::warning(nullptr, QObject::tr("Are you sure?"),
				QObject::tr("Are you really sure you want to delete the Instance \"%1\" with all its files?").arg(choice), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
			{
				if (!deleteLocalInstance(choice))
				{
					QMessageBox::warning(nullptr, QObject::tr("Failed to delete Instance"),
						QObject::tr("Could not delete Instance \"%1\". \nIf the folder was still in use, restart MO and try again.").arg(choice), QMessageBox::Ok);
				}
			}
		}
	}
	return(manageInstances(instances()));
}

QString InstanceManager::queryInstanceName(const QStringList &instanceList) const
{
  QString instanceId;
  QString dialogText;
  while (instanceId.isEmpty()) {
    QInputDialog dialog;

	  dialog.setWindowTitle(QObject::tr("Enter a Name for the new Instance"));
    dialog.setLabelText(QObject::tr("Enter a new name or select one from the suggested list: \n"
                                    "(This is just a name for the Instance and can be whatever you wish,\n"
                                    " the actual game selection will happen on the next screen regardless of chosen name)"));
    // would be neat if we could take the names from the game plugins but
    // the required initialization order requires the ini file to be
    // available *before* we load plugins
    dialog.setComboBoxItems({ "NewName", "Fallout 4", "SkyrimSE", "Skyrim", "SkyrimVR", "Fallout 3",
                              "Fallout NV", "TTW", "FO4VR", "Oblivion", "Morrowind", "Enderal" });
    dialog.setComboBoxEditable(true);

    if (dialog.exec() == QDialog::Rejected) {
      throw MOBase::MyException(QObject::tr("Canceled"));
    }
    dialogText = dialog.textValue();
    instanceId = sanitizeInstanceName(dialogText);
    if (instanceId != dialogText) {
      if (QMessageBox::question( nullptr,
                                QObject::tr("Invalid instance name"),
                                QObject::tr("The instance name \"%1\" is invalid.  Use the name \"%2\" instead?").arg(dialogText,instanceId),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
        instanceId="";
        continue;
        }
    }

    bool alreadyExists=false;
    for (const QString &instance : instanceList) {
      if(instanceId==instance)
        alreadyExists=true;
    }
    if(alreadyExists)
    {
      QMessageBox msgBox;
      msgBox.setText( QObject::tr("The instance \"%1\" already exists.").arg(instanceId) );
      msgBox.setInformativeText(QObject::tr("Please choose a different instance name, like: \"%1 1\" .").arg(instanceId));
      msgBox.exec();
      instanceId="";
    }
  }
  return instanceId;
}


QString InstanceManager::chooseInstance(const QStringList &instanceList) const
{
  if (portableInstallIsLocked()) {
    return QString();
  }

  enum class Special : uint8_t {
    NewInstance,
    Portable,
    Manage
  };

  SelectionDialog selection(
      QString("<h3>%1</h3><br>%2")
          .arg(QObject::tr("Choose Instance"))
          .arg(QObject::tr(
              "Each Instance is a full set of MO data files (mods, "
              "downloads, profiles, configuration, ...). You can use multiple "
			  "instances for different games. Instances are stored in Appdata and can be accessed by all MO installations. "
			  "If your MO folder is writable, you can also store a single instance locally (called "
              "a Portable install, and all the MO data files will be inside the installation folder).")),
      nullptr);
  selection.disableCancel();
  for (const QString &instance : instanceList) {
    selection.addChoice(instance, "", instance);
  }

  selection.addChoice(QIcon(":/MO/gui/add"), QObject::tr("New"),
                      QObject::tr("Create a new instance."),
                      static_cast<uint8_t>(Special::NewInstance));

  if (QFileInfo(qApp->applicationDirPath()).isWritable()) {
    selection.addChoice(QIcon(":/MO/gui/package"), QObject::tr("Portable"),
                        QObject::tr("Use MO folder for data."),
                        static_cast<uint8_t>(Special::Portable));
  }

  selection.addChoice(QIcon(":/MO/gui/remove"), QObject::tr("Manage Instances"),
                        QObject::tr("Delete an Instance."),
                        static_cast<uint8_t>(Special::Manage));

  selection.setWindowFlags(selection.windowFlags() | Qt::WindowStaysOnTopHint);

  if (selection.exec() == QDialog::Rejected) {
    log::debug("rejected");
    throw MOBase::MyException(QObject::tr("Canceled"));
  }

  QVariant choice = selection.getChoiceData();

  if (choice.type() == QVariant::String) {
    return choice.toString();
  } else {
    switch (static_cast<Special>(choice.value<uint8_t>())) {
      case Special::NewInstance: return queryInstanceName(instanceList);
      case Special::Portable: return QString();
      case Special::Manage: {

        return(manageInstances(instances()));
      }
      default: throw std::runtime_error("invalid selection");
    }
  }
}


QString InstanceManager::instancePath() const
{
  return QDir::fromNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation));
}


QStringList InstanceManager::instances() const
{
  const std::set<QString> ignore = {
    "cache", "qtwebengine",
  };

  const auto dirs = QDir(instancePath())
    .entryList(QDir::Dirs | QDir::NoDotAndDotDot);

  QStringList list;

  for (auto&& d : dirs) {
    if (!ignore.contains(QFileInfo(d).fileName().toLower())) {
      list.push_back(d);
    }
  }

  return list;
}


bool InstanceManager::isPortablePath(const QString& dataPath)
{
  return (dataPath == qApp->applicationDirPath());
}

bool InstanceManager::portableInstall() const
{
  return QFile::exists(qApp->applicationDirPath() + "/" +
                       QString::fromStdWString(AppConfig::iniFileName()));
}


bool InstanceManager::portableInstallIsLocked() const
{
  return QFile::exists(qApp->applicationDirPath() + "/" +
                       QString::fromStdWString(AppConfig::portableLockFileName()));
}


bool InstanceManager::allowedToChangeInstance() const
{
  return !portableInstallIsLocked();
}


void InstanceManager::createDataPath(const QString &dataPath) const
{
  if (!QDir(dataPath).exists()) {
    if (!QDir().mkpath(dataPath)) {
      throw MOBase::MyException(
            QObject::tr("failed to create %1").arg(dataPath));
    } else {
      QMessageBox::information(
          nullptr, QObject::tr("Data directory created"),
          QObject::tr("New data directory created at %1. If you don't want to "
                      "store a lot of data there, reconfigure the storage "
                      "directories via settings.").arg(dataPath));
    }
  }
}


QString InstanceManager::determineDataPath()
{
  QString instanceId = currentInstance();
  if (portableInstallIsLocked())
  {
    instanceId.clear();
  }
  if (instanceId.isEmpty() && !m_Reset && (m_overrideInstance || portableInstall()))
  {
    // startup, apparently using portable mode before
    return qApp->applicationDirPath();
  }

  QString dataPath = QDir::fromNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation)
        + "/" + instanceId);


  if (!m_overrideInstance && (instanceId.isEmpty() || !QFileInfo::exists(dataPath))) {
    instanceId = chooseInstance(instances());
    setCurrentInstance(instanceId);
    if (!instanceId.isEmpty()) {
      dataPath = QDir::fromNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation)
        + "/" + instanceId);
    }
  }

  if (instanceId.isEmpty()) {
    return qApp->applicationDirPath();
  } else {
    createDataPath(dataPath);

    return dataPath;
  }
}

QString InstanceManager::determineProfile(const Settings &settings)
{
  auto selectedProfileName = settings.game().selectedProfileName();

  if (m_overrideProfile) {
    log::debug("profile overwritten on command line");
    selectedProfileName = m_overrideProfileName;
  }

  if (!selectedProfileName) {
    log::debug("no configured profile");
    selectedProfileName = "Default";
  }

  return *selectedProfileName;
}

bool InstanceManager::determineGameEdition(
  Settings& settings, IPluginGame* game)
{
  QString edition;

  if (auto v=settings.game().edition()) {
    edition = *v;
  } else {
    QStringList editions = game->gameVariants();
    if (editions.size() < 2) {
      edition = "";
      return true;
    }

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
      return false;
    }

    edition = selection.getChoiceString();
    settings.game().setEdition(edition);
  }

  game->setGameVariant(edition);

  return true;
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

MOBase::IPluginGame* InstanceManager::determineCurrentGame(
  const QString& moPath, Settings& settings, const PluginContainer &plugins)
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


QString InstanceManager::sanitizeInstanceName(const QString &name) const
{
  QString new_name = name;

  // Restrict the allowed characters
  new_name = new_name.remove(QRegExp("[^A-Za-z0-9 _=+;!@#$%^'\\-\\.\\[\\]\\{\\}\\(\\)]"));

  // Don't end in spaces and periods
  new_name = new_name.remove(QRegExp("\\.*$"));
  new_name = new_name.remove(QRegExp(" *$"));

  // Recurse until stuff stops changing
  if (new_name != name) {
    return sanitizeInstanceName(new_name);
  }
  return new_name;
}
