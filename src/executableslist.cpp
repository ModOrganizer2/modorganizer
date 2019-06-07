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
#include <QCoreApplication>


#include <algorithm>


using namespace MOBase;

ExecutablesList::iterator ExecutablesList::begin()
{
  return m_Executables.begin();
}

ExecutablesList::const_iterator ExecutablesList::begin() const
{
  return m_Executables.begin();
}

ExecutablesList::iterator ExecutablesList::end()
{
  return m_Executables.end();
}

ExecutablesList::const_iterator ExecutablesList::end() const
{
  return m_Executables.end();
}

std::size_t ExecutablesList::size() const
{
  return m_Executables.size();
}

bool ExecutablesList::empty() const
{
  return m_Executables.empty();
}

void ExecutablesList::load(const MOBase::IPluginGame* game, QSettings& settings)
{
  addFromPlugin(game);

  qDebug("setting up configured executables");

  int numCustomExecutables = settings.beginReadArray("customExecutables");
  for (int i = 0; i < numCustomExecutables; ++i) {
    settings.setArrayIndex(i);

    Executable::Flags flags;
    if (settings.value("custom", true).toBool())
      flags |= Executable::CustomExecutable;
    if (settings.value("toolbar", false).toBool())
      flags |= Executable::ShowInToolbar;
    if (settings.value("ownicon", false).toBool())
      flags |= Executable::UseApplicationIcon;

    addExecutable(
      settings.value("title").toString(), settings.value("binary").toString(),
      settings.value("arguments").toString(),
      settings.value("workingDirectory", "").toString(),
      settings.value("steamAppID", "").toString(), flags);
  }

  settings.endArray();
}

void ExecutablesList::store(QSettings& settings)
{
  settings.remove("customExecutables");
  settings.beginWriteArray("customExecutables");

  int count = 0;

  for (const auto& item : *this) {
    settings.setArrayIndex(count++);

    settings.setValue("title", item.title());
    settings.setValue("custom", item.isCustom());
    settings.setValue("toolbar", item.isShownOnToolbar());
    settings.setValue("ownicon", item.usesOwnIcon());

    if (item.isCustom()) {
      settings.setValue("binary", item.binaryInfo().absoluteFilePath());
      settings.setValue("arguments", item.arguments());
      settings.setValue("workingDirectory", item.workingDirectory());
      settings.setValue("steamAppID", item.steamAppID());
    }
  }

  settings.endArray();
}

void ExecutablesList::addFromPlugin(IPluginGame const *game)
{
  Q_ASSERT(game != nullptr);

  for (const ExecutableInfo &info : game->executables()) {
    if (info.isValid()) {
      addExecutableInternal(info.title(),
                            info.binary().absoluteFilePath(),
                            info.arguments().join(" "),
                            info.workingDirectory().absolutePath(),
                            info.steamAppID());
    }
  }

  ExecutableInfo explorerpp = ExecutableInfo("Explore Virtual Folder", QFileInfo(QCoreApplication::applicationDirPath() + "/explorer++/Explorer++.exe" ))
      .withArgument(QString("\"%1\"").arg(QDir::toNativeSeparators(game->dataDirectory().absolutePath())));

  if (explorerpp.isValid()) {
    addExecutableInternal(explorerpp.title(),
      explorerpp.binary().absoluteFilePath(),
      explorerpp.arguments().join(" "),
      explorerpp.workingDirectory().absolutePath(),
      explorerpp.steamAppID());
  }
}

const Executable &ExecutablesList::get(const QString &title) const
{
  for (const auto& exe : m_Executables) {
    if (exe.title() == title) {
      return exe;
    }
  }

  throw std::runtime_error(QString("executable not found: %1").arg(title).toLocal8Bit().constData());
}

Executable &ExecutablesList::get(const QString &title)
{
  return const_cast<Executable&>(std::as_const(*this).get(title));
}

Executable &ExecutablesList::getByBinary(const QFileInfo &info)
{
  for (Executable &exe : m_Executables) {
    if (exe.binaryInfo() == info) {
      return exe;
    }
  }
  throw std::runtime_error("invalid info");
}

ExecutablesList::iterator ExecutablesList::find(const QString &title)
{
  return std::find_if(begin(), end(), [&](auto&& e) { return e.title() == title; });
}

ExecutablesList::const_iterator ExecutablesList::find(const QString &title) const
{
  return std::find_if(begin(), end(), [&](auto&& e) { return e.title() == title; });
}

bool ExecutablesList::titleExists(const QString &title) const
{
  auto test = [&] (const Executable &exe) { return exe.title() == title; };
  return std::find_if(m_Executables.begin(), m_Executables.end(), test) != m_Executables.end();
}

void ExecutablesList::addExecutable(const Executable &executable)
{
  auto existingExe = find(executable.title());
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
  QDir dir(workingDirectory);
  auto existingExe = find(title);
  flags &= mask;

  if (existingExe != m_Executables.end()) {
    existingExe->setTitle(title);

    auto newFlags = existingExe->flags();
    newFlags &= ~mask;
    newFlags |= flags;

    existingExe->setFlags(newFlags);

    // for pre-configured executables don't overwrite settings we didn't store
    if (flags & Executable::CustomExecutable) {
      if (file.exists()) {
        // don't overwrite a valid binary with an invalid one
        existingExe->setBinaryInfo(file);
      }

      if (dir.exists()) {
        // don't overwrite a valid working directory with an invalid one
        existingExe->setWorkingDirectory(workingDirectory);
      }
      existingExe->setArguments(arguments);
      existingExe->setSteamAppID(steamAppID);
    }
  } else {
    m_Executables.push_back({
      title, file, arguments, workingDirectory, steamAppID,
      Executable::CustomExecutable | flags});
  }
}


void ExecutablesList::remove(const QString &title)
{
  for (std::vector<Executable>::iterator iter = m_Executables.begin(); iter != m_Executables.end(); ++iter) {
    if (iter->isCustom() && (iter->title() == title)) {
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
    m_Executables.push_back({
      title, file, arguments, steamAppID,
      workingDirectory, Executable::UseApplicationIcon});
  }
}


Executable::Executable(
  QString title, QFileInfo binaryInfo, QString arguments,
  QString steamAppID, QString workingDirectory, Flags flags) :
    m_title(std::move(title)),
    m_binaryInfo(std::move(binaryInfo)),
    m_arguments(std::move(arguments)),
    m_steamAppID(std::move(steamAppID)),
    m_workingDirectory(std::move(workingDirectory)),
    m_flags(flags)
{
}

const QString& Executable::title() const
{
  return m_title;
}

void Executable::setTitle(const QString& s)
{
  m_title = s;
}

const QFileInfo& Executable::binaryInfo() const
{
  return m_binaryInfo;
}

void Executable::setBinaryInfo(const QFileInfo& fi)
{
  m_binaryInfo = fi;
}

const QString& Executable::arguments() const
{
  return m_arguments;
}

void Executable::setArguments(const QString& s)
{
  m_arguments = s;
}

const QString& Executable::steamAppID() const
{
  return m_steamAppID;
}

void Executable::setSteamAppID(const QString& s)
{
  m_steamAppID = s;
}

const QString& Executable::workingDirectory() const
{
  return m_workingDirectory;
}

void Executable::setWorkingDirectory(const QString& s)
{
  m_workingDirectory = s;
}

Executable::Flags Executable::flags() const
{
  return m_flags;
}

void Executable::setFlags(Flags f)
{
  m_flags = f;
}

bool Executable::isCustom() const
{
  return m_flags.testFlag(CustomExecutable);
}

bool Executable::isShownOnToolbar() const
{
  return m_flags.testFlag(ShowInToolbar);
}

void Executable::setShownOnToolbar(bool state)
{
  if (state) {
    m_flags |= ShowInToolbar;
  } else {
    m_flags &= ~ShowInToolbar;
  }
}

bool Executable::usesOwnIcon() const
{
  return m_flags.testFlag(UseApplicationIcon);
}
