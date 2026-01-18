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

#include "executableslistproxy.h"

#include "executableslist.h"

#include <memory>
#include <vector>

#include <QFileInfo>
#include <QString>

ExecutablesListProxy::ExecutablesListProxy(ExecutablesList* executablesList)
    : m_Proxied(executablesList)
{}

std::vector<std::shared_ptr<const MOBase::IExecutable>>
ExecutablesListProxy::executables() const
{
  std::vector<std::shared_ptr<const MOBase::IExecutable>> executables;
  for (const auto& exe : *m_Proxied) {
    executables.emplace_back(std::make_shared<const Executable>(exe));
  }

  return executables;
}

std::shared_ptr<const MOBase::IExecutable>
ExecutablesListProxy::getByTitle(const QString& title) const
{
  try {
    const auto& exe = m_Proxied->get(title);
    return std::make_shared<const Executable>(exe);
  } catch (const std::runtime_error&) {
    return nullptr;
  }
}

std::shared_ptr<const MOBase::IExecutable>
ExecutablesListProxy::getByBinary(const QFileInfo& info) const
{
  try {
    const auto& exe = m_Proxied->getByBinary(info);
    return std::make_shared<const Executable>(exe);
  } catch (const std::runtime_error&) {
    return nullptr;
  }
}

bool ExecutablesListProxy::titleExists(const QString& title) const
{
  return m_Proxied->titleExists(title);
}
