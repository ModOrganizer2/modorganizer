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

#ifndef INSTANCEMANAGERPROXY_H
#define INSTANCEMANAGERPROXY_H

#include "instancemanager.h"

#include <memory>
#include <vector>

#include <QString>

#include <uibase/iinstancemanager.h>

namespace MOBase
{
class IInstance;
}

class InstanceManagerProxy : public MOBase::IInstanceManager
{
public:
  InstanceManagerProxy(InstanceManager* instanceManager);
  void overrideInstance(const QString& instanceName) override;
  void overrideProfile(const QString& profileName) override;
  void clearOverrides() override;
  void clearCurrentInstance() override;
  std::shared_ptr<MOBase::IInstance> currentInstance() const override;
  void setCurrentInstance(const QString& instanceName) override;
  bool allowedToChangeInstance() const override;
  bool portableInstanceExists() const override;
  QString portablePath() const override;
  QString globalInstancesRootPath() const override;
  std::vector<QString> globalInstancePaths() const override;
  bool globalInstanceExists(const QString& instanceName) const override;
  QString globalInstancePath(const QString& instanceName) const override;
  QString iniPath(const QString& instanceDirectory) const override;

private:
  InstanceManager* m_Proxied;
};
#endif  // INSTANCEMANAGERPROXY_H
