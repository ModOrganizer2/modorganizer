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
#include <gameinfo.h>
#include <QFileInfo>
#include <QDir>
#include "utility.h"
#include <algorithm>


using namespace MOBase;
using namespace MOShared;


QDataStream &operator<<(QDataStream &out, const Executable &obj)
{
  out << obj.m_Title << obj.m_BinaryInfo.absoluteFilePath() << obj.m_Arguments << obj.m_CloseMO
      << obj.m_SteamAppID << obj.m_WorkingDirectory << obj.m_Custom << obj.m_Toolbar;
  return out;
}

QDataStream &operator>>(QDataStream &in, Executable &obj)
{
  QString binaryTemp;
  int closeStyleTemp;
  in >> obj.m_Title >> binaryTemp >> obj.m_Arguments >> closeStyleTemp
     >> obj.m_SteamAppID >> obj.m_WorkingDirectory >> obj.m_Custom >> obj.m_Toolbar;

  obj.m_CloseMO = (CloseMOStyle)closeStyleTemp;
  obj.m_BinaryInfo.setFile(binaryTemp);
  return in;
}


void registerExecutable()
{
  qRegisterMetaType<Executable>("Executable");
  qRegisterMetaTypeStreamOperators<Executable>("Executable");
}


ExecutablesList::ExecutablesList()
{
}

ExecutablesList::~ExecutablesList()
{
}

void ExecutablesList::init()
{
  std::vector<ExecutableInfo> executables = GameInfo::instance().getExecutables();
  for (std::vector<ExecutableInfo>::const_iterator iter = executables.begin(); iter != executables.end(); ++iter) {
    addExecutableInternal(ToQString(iter->title),
                          QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory())).append("/").append(ToQString(iter->binary)),
                          ToQString(iter->arguments), ToQString(iter->workingDirectory),
                          iter->closeMO, ToQString(iter->steamAppID));
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
  for (std::vector<Executable>::const_iterator iter = m_Executables.begin(); iter != m_Executables.end(); ++iter) {
    if (iter->m_Title == title) {
      return *iter;
    }
  }
  throw std::runtime_error("invalid name");
}


Executable &ExecutablesList::find(const QString &title)
{
  for (std::vector<Executable>::iterator iter = m_Executables.begin(); iter != m_Executables.end(); ++iter) {
    if (QString::compare(iter->m_Title, title, Qt::CaseInsensitive) == 0) {
      return *iter;
    }
  }
  throw std::runtime_error("invalid name");
}


Executable &ExecutablesList::findByBinary(const QFileInfo &info)
{
  for (std::vector<Executable>::iterator iter = m_Executables.begin(); iter != m_Executables.end(); ++iter) {
    if (info == iter->m_BinaryInfo) {
      return *iter;
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


void ExecutablesList::position(const QString &title, bool toolbar, int pos)
{
  auto existingExe = findExe(title);
  if (existingExe != m_Executables.end()) {
    Executable temp = *existingExe;
    temp.m_Toolbar = toolbar;
    m_Executables.erase(existingExe);
    m_Executables.insert(m_Executables.begin() + pos, temp);
  }
}


void ExecutablesList::addExecutable(const QString &title, const QString &executableName, const QString &arguments,
                                    const QString &workingDirectory, CloseMOStyle closeMO, const QString &steamAppID,
                                    bool custom, bool toolbar, int pos)
{
  QFileInfo file(executableName);
  auto existingExe = findExe(title);

  if (existingExe != m_Executables.end()) {
    existingExe->m_Title = title;
    existingExe->m_CloseMO = closeMO;
    existingExe->m_BinaryInfo = file;
    existingExe->m_Arguments = arguments;
    existingExe->m_WorkingDirectory = workingDirectory;
    existingExe->m_SteamAppID = steamAppID;
    existingExe->m_Custom = custom;
    existingExe->m_Toolbar = toolbar;
    if (pos >= 0) {
      Executable temp = *existingExe;
      m_Executables.erase(existingExe);
      m_Executables.insert(m_Executables.begin() + pos, temp);
    }
  } else {
    Executable newExe;
    newExe.m_Title = title;
    newExe.m_CloseMO = closeMO;
    newExe.m_BinaryInfo = file;
    newExe.m_Arguments = arguments;
    newExe.m_WorkingDirectory = workingDirectory;
    newExe.m_SteamAppID = steamAppID;
    newExe.m_Custom = true;
    newExe.m_Toolbar = toolbar;
    if ((pos < 0) || (pos >= static_cast<int>(m_Executables.size()))) {
      m_Executables.push_back(newExe);
    } else {
      m_Executables.insert(m_Executables.begin() + pos, newExe);
    }
  }
}


void ExecutablesList::remove(const QString &title)
{
  for (std::vector<Executable>::iterator iter = m_Executables.begin(); iter != m_Executables.end(); ++iter) {
    if (iter->m_Custom && (iter->m_Title == title)) {
      m_Executables.erase(iter);
      break;
    }
  }
}


void ExecutablesList::addExecutableInternal(const QString &title, const QString &executableName,
                                            const QString &arguments, const QString &workingDirectory,
                                            CloseMOStyle closeMO, const QString &steamAppID)
{
  QFileInfo file(executableName);
  if (file.exists()) {
    Executable newExe;
    newExe.m_CloseMO = closeMO;
    newExe.m_BinaryInfo = file;
    newExe.m_Title = title;
    newExe.m_Arguments = arguments;
    newExe.m_WorkingDirectory = workingDirectory;
    newExe.m_SteamAppID = steamAppID;
    newExe.m_Custom = false;
    newExe.m_Toolbar = false;
    m_Executables.push_back(newExe);
  }
}
