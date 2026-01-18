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

#ifndef EXECUTABLESLISTPROXY_H
#define EXECUTABLESLISTPROXY_H

#include "executableslist.h"

#include <memory>
#include <vector>

#include <QFileInfo>
#include <QString>

#include <uibase/iexecutable.h>
#include <uibase/iexecutableslist.h>

class ExecutablesListProxy : public MOBase::IExecutablesList
{
public:
  ExecutablesListProxy(ExecutablesList* executablesList);
  std::vector<std::shared_ptr<const MOBase::IExecutable>> executables() const override;
  std::shared_ptr<const MOBase::IExecutable>
  getByTitle(const QString& title) const override;
  std::shared_ptr<const MOBase::IExecutable>
  getByBinary(const QFileInfo& info) const override;
  bool titleExists(const QString& title) const override;

private:
  ExecutablesList* m_Proxied;
};

#endif  // EXECUTABLESLISTPROXY_H
