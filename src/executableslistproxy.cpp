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

#include <generator>
#include <stdexcept>

#include <QFileInfo>
#include <QString>

ExecutablesListProxy::ExecutablesListProxy(ExecutablesList* executablesList)
    : m_Proxied(executablesList)
{}

std::generator<const MOBase::IExecutable&> ExecutablesListProxy::executables() const
{
  for (const auto& exe : *m_Proxied) {
    co_yield exe;
  }
}

const MOBase::IExecutable* ExecutablesListProxy::getByTitle(const QString& title) const
{
  try {
    return &m_Proxied->get(title);
  } catch (const std::runtime_error&) {
    return nullptr;
  }
}

const MOBase::IExecutable*
ExecutablesListProxy::getByBinary(const QFileInfo& info) const
{
  try {
    return &m_Proxied->getByBinary(info);
  } catch (const std::runtime_error&) {
    return nullptr;
  }
}

bool ExecutablesListProxy::contains(const QString& title) const
{
  return m_Proxied->titleExists(title);
}
