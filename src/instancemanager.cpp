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
#include "createinstancedialog.h"
#include "createinstancedialogpages.h"
#include "extensionmanager.h"
#include "filesystemutilities.h"
#include "instancemanagerdialog.h"
#include "nexusinterface.h"
#include "pluginmanager.h"
#include "selectiondialog.h"
#include "settings.h"
#include "shared/appconfig.h"
#include "shared/util.h"
#include <iplugingame.h>
#include <log.h>
#include <report.h>
#include <utility.h>

#include <QCoreApplication>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <cstdint>

using namespace MOBase;

Instance::Instance(QString dir, bool portable, QString profileName)
    : m_dir(std::move(dir)), m_portable(portable), m_plugin(nullptr),
      m_profile(std::move(profileName))
{}

QString Instance::displayName() const
{
  if (isPortable())
    return QObject::tr("Portable");
  else
    return QDir(m_dir).dirName();
}

QString Instance::gameName() const
{
  return m_gameName;
}

QString Instance::gameDirectory() const
{
  return m_gameDir;
}

QString Instance::directory() const
{
  return m_dir;
}

QString Instance::baseDirectory() const
{
  return m_baseDir;
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
  return InstanceManager::singleton().iniPath(m_dir);
}

bool Instance::isPortable() const
{
  return m_portable;
}

bool Instance::isActive() const
{
  auto& m = InstanceManager::singleton();

  if (auto i = m.currentInstance()) {
    if (m_portable) {
      return i->isPortable();
    } else {
      return (i->displayName() == displayName());
    }
  }

  return false;
}

bool Instance::readFromIni()
{
  Settings s(iniPath());

  if (s.iniStatus() != QSettings::NoError) {
    log::error("can't read ini {}", iniPath());
    return false;
  }

  // game name and directory are from ini unless overridden by setGame()
  if (m_gameName.isEmpty()) {
    if (auto v = s.game().name())
      m_gameName = *v;
  }

  if (m_gameDir.isEmpty()) {
    if (auto v = s.game().directory())
      m_gameDir = *v;
  }

  if (m_gameVariant.isEmpty()) {
    if (auto v = s.game().edition()) {
      m_gameVariant = *v;
    }
  }

  if (m_baseDir.isEmpty()) {
    m_baseDir = s.paths().base();
  }

  // figuring out profile from ini if it's missing
  getProfile(s);

  return true;
}

Instance::SetupResults Instance::setup(PluginManager& plugins)
{
  // read initial values from the ini
  if (!readFromIni()) {
    return SetupResults::BadIni;
  }

  // getting game plugin
  const auto r = getGamePlugin(plugins);
  if (r != SetupResults::Okay) {
    return r;
  }

  // error if the variant missing and required by the plugin
  if (m_gameVariant.isEmpty() && m_plugin->gameVariants().size() > 1) {
    return SetupResults::MissingVariant;
  } else {
    m_plugin->setGameVariant(m_gameVariant);
  }

  // update the ini in case anything was missing
  updateIni();

  // the game directory may be different than what the plugin detected, the user
  // can change it in the settings and might have multiple versions of the game
  // installed
  m_plugin->setGamePath(m_gameDir);

  return SetupResults::Okay;
}

void Instance::updateIni()
{
  Settings s(iniPath());

  if (s.iniStatus() != QSettings::NoError) {
    log::error("can't open ini {}", iniPath());
    return;
  }

  // updating the settings since some of these values might have been missing
  s.game().setName(m_gameName);
  s.game().setDirectory(m_gameDir);
  s.game().setSelectedProfileName(m_profile);

  if (!m_gameVariant.isEmpty()) {
    // don't write a variant to the ini if the plugin doesn't require one
    s.game().setEdition(m_gameVariant);
  }
}

void Instance::setGame(const QString& name, const QString& dir)
{
  m_gameName = name;
  m_gameDir  = dir;
}

void Instance::setVariant(const QString& name)
{
  m_gameVariant = name;
}

Instance::SetupResults Instance::getGamePlugin(PluginManager& plugins)
{
  if (!m_gameName.isEmpty() && !m_gameDir.isEmpty()) {
    // normal case: both the name and dir are in the ini

    // find the plugin by name
    for (IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (m_gameName.compare(game->gameName(), Qt::CaseInsensitive) == 0) {
        // plugin found, check if the game directory is valid

        if (!game->looksValid(m_gameDir)) {
          // the directory from the ini is not valid anymore
          log::warn("game plugin {} says dir {} from ini {} is not valid",
                    game->gameName(), m_gameDir, iniPath());

          // note that some plugins return true for isInstalled() if a path
          // is found in the registry, but without actually checking if it's
          // valid

          if (game->isInstalled() && game->looksValid(game->gameDirectory())) {
            // bad game directory but the plugin reports there's a valid one
            // somewhere; take it instead
            log::warn("game plugin {} found a game at {}, taking it", game->gameName(),
                      game->gameDirectory().absolutePath());

            m_gameDir = game->gameDirectory().absolutePath();
          } else {
            // game seems to be gone completely
            log::warn("game plugin {} found no game installation at all",
                      game->gameName());
            return SetupResults::GameGone;
          }
        }

        m_plugin = game;
        return SetupResults::Okay;
      }
    }

    log::warn("game plugin {} not found", m_gameName);
    return SetupResults::PluginGone;
  } else if (m_gameName.isEmpty() && !m_gameDir.isEmpty()) {
    // the name is missing, but there's a directory; find a plugin that can
    // handle it

    log::warn("game name is missing from ini {} but dir {} is available", iniPath(),
              m_gameDir);

    for (IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (game->looksValid(m_gameDir)) {
        // take it
        log::warn("found plugin {} that can use dir {}", game->gameName(), m_gameDir);

        m_plugin   = game;
        m_gameName = game->gameName();

        return SetupResults::Okay;
      }
    }

    log::error("no plugins can use dir {}", m_gameDir);
    return SetupResults::GameGone;
  } else if (!m_gameName.isEmpty() && m_gameDir.isEmpty()) {
    // dir is missing, find a plugin with the correct name and use the install
    // dir it detected

    log::warn("game dir is missing from ini {} but name {} is available", iniPath(),
              m_gameName);

    for (IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (m_gameName.compare(game->gameName(), Qt::CaseInsensitive) == 0) {
        // plugin found, use its detected installation dir

        if (game->isInstalled()) {
          log::warn("found plugin {} that matches name in ini {}, using auto detected "
                    "game dir {}",
                    game->gameName(), iniPath(), game->gameDirectory().absolutePath());

          m_plugin  = game;
          m_gameDir = game->gameDirectory().absolutePath();

          return SetupResults::Okay;
        } else {
          log::warn("found plugin {} that matches name in ini {}, but no game install "
                    "detected by plugin",
                    game->gameName(), iniPath());

          return SetupResults::GameGone;
        }
      }
    }

    // plugin seems to be gone
    log::error("no plugin matches name {}", m_gameName);
    return SetupResults::PluginGone;
  } else {
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

  if (auto name = s.game().selectedProfileName()) {
    // use last profile
    m_profile = *name;
    return;
  }

  // profile missing from ini, use the default
  m_profile = QString::fromStdWString(AppConfig::defaultProfileName());

  log::warn("no profile found in ini {}, using default '{}'", iniPath(), m_profile);
}

// returns a list of files and folders that must be deleted when deleting
// this instance
//
std::vector<Instance::Object> Instance::objectsForDeletion() const
{
  // native separators and ending slash
  auto prettyDir = [](auto s) {
    if (!s.endsWith("/") || !s.endsWith("\\")) {
      s += "/";
    }

    return QDir::toNativeSeparators(s);
  };

  // native separators
  auto prettyFile = [](auto s) {
    return QDir::toNativeSeparators(s);
  };

  // lowercase, native separators and ending slash
  auto canonicalDir = [](QString s) {
    s = s.toLower();

    if (!s.endsWith("/") || !s.endsWith("\\")) {
      s += "/";
    }

    return QDir::toNativeSeparators(s);
  };

  // lower and native separators
  auto canonicalFile = [](auto s) {
    return QDir::toNativeSeparators(s.toLower());
  };

  // whether the given directory is contained in the root
  auto dirInRoot = [&](const QString& root, const QString& dir) {
    return canonicalDir(dir).startsWith(canonicalDir(root));
  };

  // whether the given file is contained in the root
  auto fileInRoot = [&](const QString& root, const QString& file) {
    return canonicalFile(file).startsWith(canonicalDir(root));
  };

  Settings settings(iniPath());

  if (settings.iniStatus() != QSettings::NoError) {
    log::error("can't read ini {}", iniPath());
    return {};
  }

  const QString loc  = directory();
  const QString base = settings.paths().base();

  // directories that might contain the individual files and directories
  // set in the path settings
  std::vector<Object> roots;

  // a portable instance has its location in the installation directory,
  // don't delete that
  if (!isPortable()) {
    if (QDir(loc).exists()) {
      roots.push_back({loc, true});
    }
  }

  // the base directory is the location directory by default, don't add it
  // if it's the same
  if (canonicalDir(base) != canonicalDir(loc)) {
    if (QDir(base).exists()) {
      roots.push_back({base, false});
    }
  }

  // all the directories that are part of an instance; none of them are
  // mandatory for deletion
  const std::vector<Object> dirs = {
      settings.paths().downloads(),
      settings.paths().mods(),
      settings.paths().cache(),
      settings.paths().profiles(),
      settings.paths().overwrite(),
      QDir(m_dir).filePath(QString::fromStdWString(AppConfig::dumpsDir())),
      QDir(m_dir).filePath(QString::fromStdWString(AppConfig::logPath())),
  };

  // all the files that are part of an instance
  const std::vector<Object> files = {
      {iniPath(), true},  // the ini file must be deleted
  };

  // this will contain the root directories, plus all the individual
  // directories that are not inside these roots
  std::vector<Object> cleanDirs;

  for (const auto& f : dirs) {
    bool inRoots = false;

    for (const auto& root : roots) {
      if (dirInRoot(root.path, f.path)) {
        inRoots = true;
        break;
      }
    }

    if (!inRoots) {
      // not in roots, this is a path that was changed by the user; make
      // sure it exists
      if (QDir(f.path).exists()) {
        cleanDirs.push_back({prettyDir(f.path), f.mandatoryDelete});
      }
    }
  }

  // prepending the roots
  for (auto itor = roots.rbegin(); itor != roots.rend(); ++itor) {
    cleanDirs.insert(cleanDirs.begin(), {prettyDir(itor->path), itor->mandatoryDelete});
  }

  // this will contain the individual files that are not inside the roots;
  // not that this only contains the INI file for now, so most of this is
  // useless
  std::vector<Object> cleanFiles;

  for (const auto& f : files) {
    bool inRoots = false;

    for (const auto& root : roots) {
      if (fileInRoot(root.path, f.path)) {
        inRoots = true;
        break;
      }
    }

    if (!inRoots) {
      // not in roots, this is a path that was changed by the user; make
      // sure it exists
      if (QFileInfo(f.path).exists()) {
        cleanFiles.push_back({prettyFile(f.path), f.mandatoryDelete});
      }
    }
  }

  // contains all the directories and files to be deleted
  std::vector<Object> all;
  all.insert(all.end(), cleanDirs.begin(), cleanDirs.end());
  all.insert(all.end(), cleanFiles.begin(), cleanFiles.end());

  // mandatory on top
  std::stable_sort(all.begin(), all.end());

  return all;
}

InstanceManager::InstanceManager()
{
  GlobalSettings::updateRegistryKey();
}

InstanceManager& InstanceManager::singleton()
{
  static InstanceManager s_Instance;
  return s_Instance;
}

void InstanceManager::overrideInstance(const QString& instanceName)
{
  m_overrideInstanceName = instanceName;
}

void InstanceManager::overrideProfile(const QString& profileName)
{
  m_overrideProfileName = profileName;
}

void InstanceManager::clearOverrides()
{
  m_overrideInstanceName = {};
  m_overrideProfileName  = {};
}

std::unique_ptr<Instance> InstanceManager::currentInstance() const
{
  const QString profile = m_overrideProfileName ? *m_overrideProfileName : "";

  const QString name = m_overrideInstanceName ? *m_overrideInstanceName
                                              : GlobalSettings::currentInstance();

  if (!allowedToChangeInstance()) {
    // force portable instance
    return std::make_unique<Instance>(portablePath(), true, profile);
  }

  if (name.isEmpty()) {
    if (portableInstanceExists()) {
      // use portable
      return std::make_unique<Instance>(portablePath(), true, profile);
    } else {
      // no instance set
      return {};
    }
  }

  return std::make_unique<Instance>(instancePath(name), false, profile);
}

void InstanceManager::clearCurrentInstance()
{
  setCurrentInstance("");
  m_overrideInstanceName = {};
}

void InstanceManager::setCurrentInstance(const QString& name)
{
  GlobalSettings::setCurrentInstance(name);
}

QString InstanceManager::instancePath(const QString& instanceName) const
{
  return QDir::fromNativeSeparators(globalInstancesRootPath() + "/" + instanceName);
}

QString InstanceManager::globalInstancesRootPath() const
{
  return QDir::fromNativeSeparators(
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
}

QString InstanceManager::iniPath(const QString& instanceDir) const
{
  return QDir(instanceDir).filePath(QString::fromStdWString(AppConfig::iniFileName()));
}

std::vector<QString> InstanceManager::globalInstancePaths() const
{
  const QDir root(globalInstancesRootPath());
  const auto dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

  std::vector<QString> list;

  for (auto&& d : dirs) {
    const QFileInfo iniFile(QDir(root.filePath(d)), "ModOrganizer.ini");
    log::debug("Checking for INI at path '{}'", iniFile.absoluteFilePath());

    if (iniFile.exists()) {
      log::debug("Found INI at path '{}'", iniFile.absoluteFilePath());
      list.push_back(root.filePath(d));
    }
  }

  return list;
}

bool InstanceManager::hasAnyInstances() const
{
  return portableInstanceExists() || !globalInstancePaths().empty();
}

QString InstanceManager::portablePath() const
{
  return qApp->applicationDirPath();
}

bool InstanceManager::portableInstanceExists() const
{
  return QFile::exists(qApp->applicationDirPath() + "/" +
                       QString::fromStdWString(AppConfig::iniFileName()));
}

bool InstanceManager::allowedToChangeInstance() const
{
  const auto lockFile = qApp->applicationDirPath() + "/" +
                        QString::fromStdWString(AppConfig::portableLockFileName());

  return !QFile::exists(lockFile);
}

MOBase::IPluginGame*
InstanceManager::gamePluginForDirectory(const QString& instanceDir,
                                        PluginManager& plugins) const
{
  return const_cast<MOBase::IPluginGame*>(
      gamePluginForDirectory(instanceDir, const_cast<const PluginManager&>(plugins)));
}

const MOBase::IPluginGame*
InstanceManager::gamePluginForDirectory(const QString& instanceDir,
                                        const PluginManager& plugins) const
{
  const QString ini = iniPath(instanceDir);

  // reading ini
  Settings s(ini);

  if (s.iniStatus() != QSettings::NoError) {
    log::error("failed to load settings from {}", ini);
    return nullptr;
  }

  // using game name from ini, if available
  const auto instanceGameName = s.game().name();

  if (instanceGameName && !instanceGameName->isEmpty()) {
    for (const IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (instanceGameName->compare(game->gameName(), Qt::CaseInsensitive) == 0) {
        return game;
      }
    }

    log::error("no plugin has game name '{}' that was found in ini {}",
               *instanceGameName, ini);
  } else {
    log::error("no game name found in ini {}", ini);
  }

  // using game directory from ini, if available
  const auto gameDir = s.game().directory();

  if (gameDir && !gameDir->isEmpty()) {
    for (const IPluginGame* game : plugins.plugins<IPluginGame>()) {
      if (game->looksValid(*gameDir)) {
        return game;
      }
    }

    log::error("no plugin appears to support game directory '{}' from ini {}", *gameDir,
               ini);
  } else {
    log::error("no game directory found in ini {}", ini);
  }

  // looking for a plugin that can handle the directory
  log::debug("falling back on looksValid check");

  for (const IPluginGame* game : plugins.plugins<IPluginGame>()) {
    if (game->looksValid(instanceDir)) {
      return game;
    }
  }

  return nullptr;
}

QString InstanceManager::makeUniqueName(const QString& instanceName) const
{
  const QString sanitized = MOBase::sanitizeFileName(instanceName);

  // trying "name (N)"
  QString name = sanitized;
  for (int i = 2; i < 100; ++i) {
    if (!instanceExists(name)) {
      return name;
    }

    name = QString("%1 (%2)").arg(sanitized).arg(i);
  }

  return {};
}

bool InstanceManager::instanceExists(const QString& instanceName) const
{
  const QDir root = globalInstancesRootPath();
  return root.exists(instanceName);
}

std::unique_ptr<Instance> selectInstance()
{
  auto& m = InstanceManager::singleton();

  // since there is no instance currently active, load plugins with a null
  // OrganizerCore; see PluginManager::initPlugin()
  NexusInterface ni(nullptr);
  ExtensionManager ec(nullptr);
  ec.loadExtensions(QDir(QCoreApplication::applicationDirPath() + "/extensions")
                        .filesystemAbsolutePath());

  PluginManager pc(ec, nullptr);
  pc.loadPlugins();

  if (m.hasAnyInstances()) {
    // there is at least one instance available, show the instance manager
    // dialog
    InstanceManagerDialog dlg(pc);

    // the dialog normally restarts MO when an instance is selected, but this
    // is not necessary here since MO hasn't really started yet
    dlg.setRestartOnSelect(false);

    dlg.show();
    dlg.activateWindow();
    dlg.raise();

    if (dlg.exec() != QDialog::Accepted) {
      return {};
    }

  } else {
    // no instances configured, ask the user to create one
    CreateInstanceDialog dlg(pc, nullptr);

    if (dlg.exec() != QDialog::Accepted) {
      return {};
    }
  }

  // return the new instance or the selection
  return m.currentInstance();
}

// shows the game selection page of the create instance dialog so the user can
// pick which game is managed by this instance
//
// this is used below in setupInstance() when the game directory is gone or
// no plugins can recognize it
//
SetupInstanceResults selectGame(Instance& instance, PluginManager& pc)
{
  CreateInstanceDialog dlg(pc, nullptr);

  // only show the game page
  dlg.setSinglePage<cid::GamePage>(instance.displayName());

  dlg.show();
  dlg.activateWindow();
  dlg.raise();

  if (dlg.exec() != QDialog::Accepted) {
    // cancelled
    return SetupInstanceResults::SelectAnother;
  }

  // this info will be used instead of the ini, which should fix this
  // particular problem
  instance.setGame(dlg.creationInfo().game->gameName(),
                   dlg.creationInfo().gameLocation);

  return SetupInstanceResults::TryAgain;
}

// shows the game variant page of the create instance dialog so the user can
// pick which game variant is installed
//
// this is used below in setupInstance() when there is no variant in the ini
// but the game plugin requires one; this can happen when the ini is broken
// or when a new variant has become supported by the plugin for a game the
// user already has an instance for
//
SetupInstanceResults selectVariant(Instance& instance, PluginManager& pc)
{
  CreateInstanceDialog dlg(pc, nullptr);

  // the variant page uses the game page to know which game was selected, so
  // set it manually
  dlg.getPage<cid::GamePage>()->select(instance.gamePlugin(), instance.gameDirectory());

  // only show the variant page
  dlg.setSinglePage<cid::VariantsPage>(instance.displayName());

  dlg.show();
  dlg.activateWindow();
  dlg.raise();

  if (dlg.exec() != QDialog::Accepted) {
    return SetupInstanceResults::SelectAnother;
  }

  // this info will be used instead of the ini, which should fix this
  // particular problem
  instance.setVariant(dlg.creationInfo().gameVariant);

  return SetupInstanceResults::TryAgain;
}

SetupInstanceResults setupInstance(Instance& instance, PluginManager& pc)
{
  // set up the instance
  const auto setupResult = instance.setup(pc);

  switch (setupResult) {
  case Instance::SetupResults::Okay: {
    // all good
    return SetupInstanceResults::Okay;
  }

  case Instance::SetupResults::BadIni: {
    // unreadable ini, there's not much that can be done, select another
    // instance

    reportError(QObject::tr("Cannot open instance '%1', failed to read INI file %2.")
                    .arg(instance.displayName())
                    .arg(instance.iniPath()));

    return SetupInstanceResults::SelectAnother;
  }

  case Instance::SetupResults::IniMissingGame: {
    // both the game name and directory are missing from the ini; although
    // this shouldn't happen, setup() is able to handle when either is
    // missing, but not both
    //
    // ask the user for the game managed by this instance

    reportError(
        QObject::tr(
            "Cannot open instance '%1', the managed game was not found in the INI "
            "file %2. Select the game managed by this instance.")
            .arg(instance.displayName())
            .arg(instance.iniPath()));

    return selectGame(instance, pc);
  }

  case Instance::SetupResults::PluginGone: {
    // there is no plugin that can handle the game name/directory from the
    // ini, so this instance is unusable

    reportError(
        QObject::tr("Cannot open instance '%1', the game plugin '%2' doesn't exist. It "
                    "may have been deleted by an antivirus. Select another instance.")
            .arg(instance.displayName())
            .arg(instance.gameName()));

    return SetupInstanceResults::SelectAnother;
  }

  case Instance::SetupResults::GameGone: {
    // the game directory doesn't exist or the plugin doesn't recognize it;
    // ask the user for the game managed by this instance

    reportError(
        QObject::tr(
            "Cannot open instance '%1', the game directory '%2' doesn't exist or "
            "the game plugin '%3' doesn't recognize it. Select the game managed "
            "by this instance.")
            .arg(instance.displayName())
            .arg(instance.gameDirectory())
            .arg(instance.gameName()));

    return selectGame(instance, pc);
  }

  case Instance::SetupResults::MissingVariant: {
    // the game variant is missing from the ini, ask the user for it
    return selectVariant(instance, pc);
  }

  default: {
    // shouldn't happen
    return SetupInstanceResults::Exit;
  }
  }
}
