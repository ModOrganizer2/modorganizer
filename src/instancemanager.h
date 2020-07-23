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


#pragma once


#include <QString>
#include <QSettings>

namespace MOBase { class IPluginGame; }

class Settings;
class PluginContainer;

class InstanceManager
{
public:
  static InstanceManager &instance();

  void overrideInstance(const QString& instanceName);
  void overrideProfile(const QString& profileName);

  QString determineDataPath();
  QString determineProfile(const Settings &settings);
  bool determineGameEdition(Settings& settings, MOBase::IPluginGame* game);
  MOBase::IPluginGame* determineCurrentGame(
    const QString& moPath, Settings& settings, const PluginContainer &plugins);

  void clearCurrentInstance();
  QString currentInstance() const;

  bool allowedToChangeInstance() const;
  static bool isPortablePath(const QString& dataPath);

  QString instancesPath() const;
  QStringList instanceNames() const;
  std::vector<QDir> instancePaths() const;

private:

  InstanceManager();

  QString instancePath(const QString& instanceName) const;

  bool deleteLocalInstance(const QString &instanceId) const;

  QString manageInstances(const QStringList &instanceList) const;

  QString sanitizeInstanceName(const QString &name) const;
  void setCurrentInstance(const QString &name);

  QString queryInstanceName(const QStringList &instanceList) const;
  QString chooseInstance(const QStringList &instanceList) const;

  void createDataPath(const QString &dataPath) const;
  bool portableInstall() const;
  bool portableInstallIsLocked() const;

  void updateRegistryKey();

private:

  QSettings m_AppSettings;
  bool m_Reset {false};
  bool m_overrideInstance{false};
  QString m_overrideInstanceName;
  bool m_overrideProfile{false};
  QString m_overrideProfileName;
};
