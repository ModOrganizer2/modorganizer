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

#ifndef MOAPPLICATION_H
#define MOAPPLICATION_H

#include "env.h"
#include <QApplication>
#include <QFileSystemWatcher>

#include "thememanager.h"
#include "translationmanager.h"

class Instance;
class MOMultiProcess;
class NexusInterface;
class OrganizerCore;
class PluginContainer;
class Settings;
class ThemeManager;

namespace MOBase
{
class IPluginGame;
}

class MOApplication : public QApplication
{
  Q_OBJECT

public:
  MOApplication(int& argc, char** argv);

  // called from main() only once for stuff that persists across "restarts"
  //
  void firstTimeSetup(MOMultiProcess& multiProcess);

  // called from main() each time MO "restarts", loads settings, plugins,
  // OrganizerCore and the current instance
  //
  int setup(MOMultiProcess& multiProcess, bool forceSelect);

  // shows splash, starts an api check, shows the main window and blocks until
  // MO exits
  //
  int run(MOMultiProcess& multiProcess);

  // called from main() when MO "restarts", must clean up everything so setup()
  // starts fresh
  //
  void resetForRestart();

  // undefined if setup() wasn't called
  //
  OrganizerCore& core();

  // wraps QApplication::notify() in a catch, reports errors and ignores them
  //
  bool notify(QObject* receiver, QEvent* event) override;

private:
  std::unique_ptr<env::ModuleNotification> m_modules;

  std::unique_ptr<Instance> m_instance;
  std::unique_ptr<Settings> m_settings;
  std::unique_ptr<NexusInterface> m_nexus;
  std::unique_ptr<PluginContainer> m_plugins;
  std::unique_ptr<ThemeManager> m_themes;
  std::unique_ptr<TranslationManager> m_translations;
  std::unique_ptr<OrganizerCore> m_core;

  void externalMessage(const QString& message);
  std::unique_ptr<Instance> getCurrentInstance(bool forceSelect);
  std::optional<int> setupInstanceLoop(Instance& currentInstance, PluginContainer& pc);
  void purgeOldFiles();
};

class MOSplash
{
public:
  MOSplash(const Settings& settings, const QString& dataPath,
           const MOBase::IPluginGame* game);

  void close();

private:
  std::unique_ptr<QSplashScreen> ss_;

  QString getSplashPath(const Settings& settings, const QString& dataPath,
                        const MOBase::IPluginGame* game) const;
};

#endif  // MOAPPLICATION_H
