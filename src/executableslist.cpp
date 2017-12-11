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

#include "executableslist.h"

#include "iplugingame.h"
#include "utility.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include <algorithm>


using namespace MOBase;


ExecutablesList::ExecutablesList()
{
}

ExecutablesList::~ExecutablesList()
{
}

void ExecutablesList::init(IPluginGame const *game)
{
  Q_ASSERT(game != nullptr);
  m_Executables.clear();
  for (const ExecutableInfo &info : game->executables()) {
    if (info.isValid()) {
      addExecutableInternal(info.title(),
                            info.binary().absoluteFilePath(),
                            info.arguments().join(" "),
                            info.workingDirectory().absolutePath(),
                            info.steamAppID());
    }
  }
}

void ExecutablesList::getExecutables(std::vector<Executable>::iterator &begin, std::vector<Executable>::iterator &end)
{
  begin = m_Executables.begin();
  end = m_Executables.end();
}

void ExecutablesList::getExecutables(std::vector<Executable>::const_iterator &begin,
                                     std::vector<Executable>::const_iterator &end) const
{
  begin = m_Executables.begin();
  end = m_Executables.end();
}

const Executable &ExecutablesList::find(const QString &title) const
{
  for (Executable const &exe : m_Executables) {
    if (exe.m_Title == title) {
      return exe;
    }
  }
  throw std::runtime_error(QString("invalid name %1").arg(title).toLocal8Bit().constData());
}


Executable &ExecutablesList::find(const QString &title)
{
  for (Executable &exe : m_Executables) {
    if (exe.m_Title == title) {
      return exe;
    }
  }
  throw std::runtime_error(QString("invalid name %1").arg(title).toLocal8Bit().constData());
}


Executable &ExecutablesList::findByBinary(const QFileInfo &info)
{
  for (Executable &exe : m_Executables) {
    if (info == exe.m_BinaryInfo) {
      return exe;
    }
  }
  throw std::runtime_error("invalid info");
}


std::vector<Executable>::iterator ExecutablesList::findExe(const QString &title)
{
  for (std::vector<Executable>::iterator iter = m_Executables.begin(); iter != m_Executables.end(); ++iter) {
    if (iter->m_Title == title) {
      return iter;
    }
  }
  return m_Executables.end();
}


bool ExecutablesList::titleExists(const QString &title) const
{
  auto test = [&] (const Executable &exe) { return exe.m_Title == title; };
  return std::find_if(m_Executables.begin(), m_Executables.end(), test) != m_Executables.end();
}


void ExecutablesList::addExecutable(const Executable &executable)
{
  auto existingExe = findExe(executable.m_Title);
  if (existingExe != m_Executables.end()) {
    *existingExe = executable;
  } else {
    m_Executables.push_back(executable);
  }
}


void ExecutablesList::updateExecutable(const QString &title,
                                       const QString &executableName,
                                       const QString &arguments,
                                       const QString &workingDirectory,
                                       const QString &steamAppID,
                                       Executable::Flags mask,
                                       Executable::Flags flags)
{
  QFileInfo file(executableName);
  auto existingExe = findExe(title);
  flags &= mask;

  if (existingExe != m_Executables.end()) {
    existingExe->m_Title = title;
    existingExe->m_Flags &= ~mask;
    existingExe->m_Flags |= flags;
    // for pre-configured executables don't overwrite settings we didn't store
    if (flags & Executable::CustomExecutable) {
      if (file.exists()) {
        // don't overwrite a valid binary with an invalid one
        existingExe->m_BinaryInfo = file;
      }
      existingExe->m_Arguments = arguments;
      existingExe->m_WorkingDirectory = workingDirectory;
      existingExe->m_SteamAppID = steamAppID;
    }
  } else {
    Executable newExe;
    newExe.m_Title = title;
    newExe.m_BinaryInfo = file;
    newExe.m_Arguments = arguments;
    newExe.m_WorkingDirectory = workingDirectory;
    newExe.m_SteamAppID = steamAppID;
    newExe.m_Flags = Executable::CustomExecutable | flags;
    m_Executables.push_back(newExe);
  }
}


void ExecutablesList::remove(const QString &title)
{
  for (std::vector<Executable>::iterator iter = m_Executables.begin(); iter != m_Executables.end(); ++iter) {
    if (iter->isCustom() && (iter->m_Title == title)) {
      m_Executables.erase(iter);
      break;
    }
  }
}


void ExecutablesList::addExecutableInternal(const QString &title, const QString &executableName,
                                            const QString &arguments, const QString &workingDirectory,
                                            const QString &steamAppID)
{
  QFileInfo file(executableName);
  if (file.exists()) {
    Executable newExe;
    newExe.m_BinaryInfo = file;
    newExe.m_Title = title;
    newExe.m_Arguments = arguments;
    newExe.m_WorkingDirectory = workingDirectory;
    newExe.m_SteamAppID = steamAppID;
    newExe.m_Flags = Executable::UseApplicationIcon;
    m_Executables.push_back(newExe);
  }
}


void Executable::showOnToolbar(bool state)
{
  if (state) {
    m_Flags |= ShowInToolbar;
  } else {
    m_Flags &= ~ShowInToolbar;
  }
}
