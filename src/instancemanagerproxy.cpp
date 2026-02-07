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

#include "instancemanagerproxy.h"

#include "instancemanager.h"

#include <memory>
#include <vector>

#include <QDir>
#include <QString>

#include <uibase/iinstance.h>

InstanceManagerProxy::InstanceManagerProxy(InstanceManager* instanceManager)
    : m_Proxied(instanceManager)
{}

std::shared_ptr<MOBase::IInstance> InstanceManagerProxy::currentInstance() const
{
  return m_Proxied->currentInstance();
}

std::vector<QDir> InstanceManagerProxy::globalInstancePaths() const
{
  const auto globalPaths = m_Proxied->globalInstancePaths();
  return std::vector<QDir>(globalPaths.begin(), globalPaths.end());
}

std::shared_ptr<const MOBase::IInstance>
InstanceManagerProxy::getGlobalInstance(const QString& instanceName) const
{
  return m_Proxied->getGlobalInstance(instanceName);
}
