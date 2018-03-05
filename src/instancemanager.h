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


class InstanceManager {

public:

  static InstanceManager &instance();

  QString determineDataPath();
  void clearCurrentInstance();

  void overrideInstance(const QString& instanceName);

  QString currentInstance() const;

private:

  InstanceManager();

  QString instancePath() const;

  QStringList instances() const;

  bool deleteLocalInstance(const QString &instanceId) const;

  QString manageInstances(const QStringList &instanceList) const;

  void setCurrentInstance(const QString &name);

  QString queryInstanceName(const QStringList &instanceList) const;
  QString chooseInstance(const QStringList &instanceList) const;

  void createDataPath(const QString &dataPath) const;
  bool portableInstall() const;

private:

  QSettings m_AppSettings;
  bool m_Reset {false};
  bool m_overrideInstance{false};
  QString m_overrideInstanceName;
};
