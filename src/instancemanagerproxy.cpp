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

#include <QString>
#include <uibase/iinstance.h>

InstanceManagerProxy::InstanceManagerProxy(InstanceManager* instanceManager)
    : m_Proxied(instanceManager)
{}

void InstanceManagerProxy::overrideInstance(const QString& instanceName)
{
  m_Proxied->overrideInstance(instanceName);
}

void InstanceManagerProxy::overrideProfile(const QString& profileName)
{
  m_Proxied->overrideProfile(profileName);
}

void InstanceManagerProxy::clearOverrides()
{
  m_Proxied->clearOverrides();
}

void InstanceManagerProxy::clearCurrentInstance()
{
  m_Proxied->clearCurrentInstance();
}

std::shared_ptr<MOBase::IInstance> InstanceManagerProxy::currentInstance() const
{
  return m_Proxied->currentInstance();
}

void InstanceManagerProxy::setCurrentInstance(const QString& instanceName)
{
  m_Proxied->setCurrentInstance(instanceName);
}

bool InstanceManagerProxy::allowedToChangeInstance() const
{
  return m_Proxied->allowedToChangeInstance();
}

bool InstanceManagerProxy::portableInstanceExists() const
{
  return m_Proxied->portableInstanceExists();
}

QString InstanceManagerProxy::portablePath() const
{
  return m_Proxied->portablePath();
}

QString InstanceManagerProxy::globalInstancesRootPath() const
{
  return m_Proxied->globalInstancesRootPath();
}

std::vector<QString> InstanceManagerProxy::globalInstancePaths() const
{
  return m_Proxied->globalInstancePaths();
}

bool InstanceManagerProxy::globalInstanceExists(const QString& instanceName) const
{
  return m_Proxied->instanceExists(instanceName);
}

QString InstanceManagerProxy::globalInstancePath(const QString& instanceName) const
{
  return m_Proxied->instancePath(instanceName);
}

QString InstanceManagerProxy::iniPath(const QString& instanceDirectory) const
{
  return m_Proxied->iniPath(instanceDirectory);
}
