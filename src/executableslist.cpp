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

    setExecutable({
      settings.value("title").toString(),
      settings.value("binary").toString(),
      settings.value("arguments").toString(),
      settings.value("workingDirectory", "").toString(),
      settings.value("steamAppID", "").toString(),
      flags});
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
      setExecutable({
        info.title(),
        info.binary().absoluteFilePath(),
        info.arguments().join(" "),
        info.workingDirectory().absolutePath(),
        info.steamAppID(),
        Executable::UseApplicationIcon});
    }
  }

  ExecutableInfo explorerpp = ExecutableInfo("Explore Virtual Folder", QFileInfo(QCoreApplication::applicationDirPath() + "/explorer++/Explorer++.exe" ))
      .withArgument(QString("\"%1\"").arg(QDir::toNativeSeparators(game->dataDirectory().absolutePath())));

  if (explorerpp.isValid()) {
    setExecutable({
      explorerpp.title(),
      explorerpp.binary().absoluteFilePath(),
      explorerpp.arguments().join(" "),
      explorerpp.workingDirectory().absolutePath(),
      explorerpp.steamAppID(),
      Executable::UseApplicationIcon});
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

void ExecutablesList::setExecutable(const Executable &executable)
{
  auto itor = find(executable.title());

  if (itor == m_Executables.end()) {
    m_Executables.push_back(executable);
  } else {
    itor->mergeFrom(executable);
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

const QFileInfo& Executable::binaryInfo() const
{
  return m_binaryInfo;
}

const QString& Executable::arguments() const
{
  return m_arguments;
}

const QString& Executable::steamAppID() const
{
  return m_steamAppID;
}

const QString& Executable::workingDirectory() const
{
  return m_workingDirectory;
}

Executable::Flags Executable::flags() const
{
  return m_flags;
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

void Executable::mergeFrom(const Executable& other)
{
  if (!isCustom()) {
    // this happens when the user is trying to modify a plugin executable

    // only change some of the flags
    const auto allow = ShowInToolbar;

    m_flags |= (other.flags() & allow);
  } else {
    // this happens after executables are loaded from settings and plugin
    // executables are being added, or when users are modifying executables

    m_title = other.title();
    m_arguments = other.arguments();
    m_steamAppID = other.steamAppID();
    m_workingDirectory = other.workingDirectory();

    // don't overwrite a valid binary with an invalid one
    if (other.binaryInfo().exists()) {
      m_binaryInfo = other.binaryInfo();
    }

    if (!other.isCustom()) {
      // overwriting a custom executable with a plugin, merge all the flags
      m_flags |= other.flags();
    } else {
      // overwriting a custom with another custom, just replace the flags
      m_flags = other.flags();
    }
  }
}
