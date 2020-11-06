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
#include "shared/util.h"
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

Instance::Instance(QDir dir, bool portable, QString profileName) :
  m_dir(std::move(dir)), m_portable(portable), m_plugin(nullptr),
  m_profile(std::move(profileName))
{
}

QString Instance::name() const
{
  if (isPortable())
    return QObject::tr("Portable");
  else
    return m_dir.dirName();
}

QString Instance::gameName() const
{
  return m_gameName;
}

QString Instance::gameDirectory() const
{
  return m_gameDir;
}

QDir Instance::directory() const
{
  return m_dir;
}

MOBase::IPluginGame* Instance::gamePlugin() const
{
  return m_plugin;
}

QString Instance::profileName() const
{
  return m_profile;
}

QString Instance::iniPath() const
{
  return InstanceManager::iniPath(m_dir);
}

bool Instance::isPortable() const
{
  return m_portable;
}

Instance::SetupResults Instance::setup(PluginContainer& plugins)
{
  Settings s(iniPath());

  if (s.iniStatus() != QSettings::NoError) {
    log::error("can't read ini {}", iniPath());
    return SetupResults::BadIni;
  }

  if (m_gameName.isEmpty()) {
    if (auto v=s.game().name())
      m_gameName = *v;
  }

  if (m_gameDir.isEmpty()) {
    if (auto v=s.game().directory())
      m_gameDir = *v;
  }

  const auto r = getGamePlugin(plugins);
  if (r != SetupResults::Ok) {
    return r;
  }

  if (m_gameVariant.isEmpty()) {
    if (auto v=s.game().edition()) {
      m_gameVariant = *v;
    }
  }

  if (m_gameVariant.isEmpty() && m_plugin->gameVariants().size() > 1) {
    return SetupResults::MissingVariant;
  } else {
    m_plugin->setGameVariant(m_gameVariant);
  }

  getProfile(s);

  s.game().setName(m_gameName);
  s.game().setDirectory(m_gameDir);
  s.game().setSelectedProfileName(m_profile);

  if (!m_gameVariant.isEmpty())
    s.game().setEdition(m_gameVariant);

  m_plugin->setGamePath(m_gameDir);

  return SetupResults::Ok;
}

void Instance::setGame(const QString& name, const QString& dir)
{
  m_gameName = name;
  m_gameDir = dir;
}

void Instance::setVariant(const QString& name)
{
  m_gameVariant = name;
}

Instance::SetupResults Instance::getGamePlugin(PluginContainer& plugins)
{
  if (!m_gameName.isEmpty() && !m_gameDir.isEmpty())
  {
    // normal case: both the name and dir are in the ini

    // find the plugin by name
    for (IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (m_gameName.compare(game->gameName(), Qt::CaseInsensitive) == 0) {
        // plugin found, check if the game directory is valid

        if (!game->looksValid(m_gameDir)) {
          // the directory from the ini is not valid anymore
          log::warn(
            "game plugin {} says dir {} from ini {} is not valid",
            game->gameName(), m_gameDir, iniPath());

          // note that some plugins return true for isInstalled() if a path
          // is found in the registry, but without actually checking if it's
          // valid

          if (game->isInstalled() && game->looksValid(game->gameDirectory())) {
            // bad game directory but the plugin reports there's a valid one
            // somewhere; take it instead
            log::warn(
              "game plugin {} found a game at {}, taking it",
              game->gameName(), game->gameDirectory().absolutePath());

            m_gameDir = game->gameDirectory().absolutePath();
          } else {
            // game seems to be gone completely
            log::warn("game plugin {} found no game installation at all", game->gameName());
            return SetupResults::GameGone;
          }
        }

        m_plugin = game;
        return SetupResults::Ok;
      }
    }

    log::warn("game plugin {} not found", m_gameName);
    return SetupResults::PluginGone;
  }
  else if (m_gameName.isEmpty() && !m_gameDir.isEmpty())
  {
    // the name is missing, but there's a directory; find a plugin that can
    // handle it

    log::warn(
      "game name is missing from ini {} but dir {} is available",
      iniPath(), m_gameDir);

    for (IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (game->looksValid(m_gameDir)) {
        // take it
        log::warn("found plugin {} that can use dir {}", game->gameName(), m_gameDir);

        m_plugin = game;
        m_gameName = game->gameName();

        return SetupResults::Ok;
      }
    }

    log::error("no plugins can use dir {}", m_gameDir);
    return SetupResults::GameGone;
  }
  else if (!m_gameName.isEmpty() && m_gameDir.isEmpty())
  {
    // dir is missing, find a plugin with the correct name and use the install
    // dir it detected

    log::warn(
      "game dir is missing from ini {} but name {} is available",
      iniPath(), m_gameName);

    for (IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (m_gameName.compare(game->gameName(), Qt::CaseInsensitive) == 0) {
        // plugin found, use its detected installation dir

        if (game->isInstalled()) {
          log::warn(
            "found plugin {} that matches name in ini {}, using auto detected "
            "game dir {}",
            game->gameName(), iniPath(), game->gameDirectory().absolutePath());

          m_plugin = game;
          m_gameDir = game->gameDirectory().absolutePath();

          return SetupResults::Ok;
        } else {
          log::warn(
            "found plugin {} that matches name in ini {}, but no game install "
            "detected by plugin",
            game->gameName(), iniPath());

          return SetupResults::GameGone;
        }
      }
    }

    // plugin seems to be gone
    log::error("no plugin matches name {}", m_gameName);
    return SetupResults::PluginGone;
  }
  else
  {
    // can't do anything with these two missing
    log::error("both game name and dir are missing from ini {}", iniPath());
    return SetupResults::IniMissingGame;
  }
}


void Instance::getProfile(const Settings& s)
{
  if (!m_profile.isEmpty()) {
    // there's already a profile set up, probably an override
    return;
  }

  if (auto name=s.game().selectedProfileName()) {
    // use last profile
    m_profile = *name;
    return;
  }

  // profile missing from ini, use the default
  m_profile = QString::fromStdWString(AppConfig::defaultProfileName());

  log::warn(
    "no profile found in ini {}, using default '{}'",
    iniPath(), m_profile);
}


InstanceManager::InstanceManager()
{
  GlobalSettings::updateRegistryKey();
}

InstanceManager &InstanceManager::singleton()
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

std::optional<Instance> InstanceManager::currentInstance() const
{
  const QString profile = m_overrideProfile ? m_overrideProfileName : "";

  if (portableInstallIsLocked()) {
    // force portable instance
    return Instance(QDir(portablePath()), true, profile);
  }

  QString name;

  if (m_overrideInstance)
    name = m_overrideInstanceName;
  else
    name = GlobalSettings::currentInstance();

  if (name.isEmpty()) {
    if (portableInstanceExists()) {
      // use portable
      return Instance(QDir(portablePath()), true, profile);
    } else {
      // no instance set
      return {};
    }
  }

  return Instance(QDir(instancePath(name)), false, profile);
}

void InstanceManager::clearCurrentInstance()
{
  setCurrentInstance("");
  m_overrideInstance = false;
}

void InstanceManager::setCurrentInstance(const QString &name)
{
  GlobalSettings::setCurrentInstance(name);
}

QString InstanceManager::instancePath(const QString& instanceName) const
{
  return QDir::fromNativeSeparators(instancesPath() + "/" + instanceName);
}

QString InstanceManager::instancesPath() const
{
  return QDir::fromNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation));
}

QString InstanceManager::iniPath(const QDir& instanceDir)
{
  return instanceDir.filePath(QString::fromStdWString(AppConfig::iniFileName()));
}

std::vector<QDir> InstanceManager::instancePaths() const
{
  const std::set<QString> ignore = {
    "cache", "qtwebengine",
  };

  const QDir root(instancesPath());
  const auto dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

  std::vector<QDir> list;

  for (auto&& d : dirs) {
    if (!ignore.contains(QFileInfo(d).fileName().toLower())) {
      list.push_back(root.filePath(d));
    }
  }

  return list;
}

QStringList InstanceManager::instanceNames() const
{
  QStringList list;

  for (auto&& d : instancePaths()) {
    list.push_back(d.dirName());
  }

  return list;
}


bool InstanceManager::isPortablePath(const QString& dataPath)
{
  return (dataPath == portablePath());
}

QString InstanceManager::portablePath()
{
  return qApp->applicationDirPath();
}

bool InstanceManager::portableInstanceExists() const
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

const MOBase::IPluginGame* InstanceManager::gamePluginForDirectory(
  const QDir& instanceDir, const PluginContainer& plugins) const
{
  const QString ini = iniPath(instanceDir);

  Settings s(ini);

  if (s.iniStatus() != QSettings::NoError)
  {
    log::error("failed to load settings from {}", ini);
    return nullptr;
  }

  const auto instanceGameName = s.game().name();

  if (instanceGameName && !instanceGameName->isEmpty())
  {
    for (const IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (instanceGameName->compare(game->gameName(), Qt::CaseInsensitive) == 0) {
        return game;
      }
    }

    log::error(
      "no plugin reports game name '{}' found in ini {}",
      *instanceGameName, ini);
  }
  else
  {
    log::error("no game name found in ini {}", ini);
  }


  log::error("falling back on looksValid check");

  const auto gameDir = s.game().directory();

  if (gameDir && !gameDir->isEmpty())
  {
    for (const IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (game->looksValid(*gameDir)) {
        return game;
      }
    }

    log::error(
      "no plugins appear to support game directory '{}' from ini {}",
      *gameDir, ini);
  }
  else
  {
    log::error("no game directory found in ini {}", ini);
  }

  return nullptr;
}

QString InstanceManager::makeUniqueName(const QString& instanceName) const
{
  const QString sanitized = sanitizeInstanceName(instanceName);

  QString name = sanitized;
  for (int i=2; i<100; ++i) {
    if (!instanceExists(name)) {
      return name;
    }

    name = QString("%1 (%2)").arg(sanitized).arg(i);
  }

  return {};
}

bool InstanceManager::instanceExists(const QString& instanceName) const
{
  const QDir root = instancesPath();
  return root.exists(instanceName);
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

bool InstanceManager::validInstanceName(const QString& instanceName) const
{
  if (instanceName.isEmpty()) {
    return false;
  }

  return (instanceName == sanitizeInstanceName(instanceName));
}
