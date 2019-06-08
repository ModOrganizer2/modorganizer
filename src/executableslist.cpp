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
  qDebug("setting up configured executables");

  m_Executables.clear();

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

    setExecutable(Executable()
      .title(settings.value("title").toString())
      .binaryInfo(settings.value("binary").toString())
      .arguments(settings.value("arguments").toString())
      .steamAppID(settings.value("steamAppID", "").toString())
      .workingDirectory(settings.value("workingDirectory", "").toString())
      .flags(flags));
  }

  settings.endArray();

  addFromPlugin(game);
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
      setExecutable({info, Executable::UseApplicationIcon});
    }
  }

  const QFileInfo eppBin(QCoreApplication::applicationDirPath() + "/explorer++/Explorer++.exe");

  if (eppBin.exists()) {
    const auto args = QString("\"%1\"")
      .arg(QDir::toNativeSeparators(game->dataDirectory().absolutePath()));

    setExecutable(Executable()
      .title("Explore Virtual Folder")
      .binaryInfo(eppBin)
      .arguments(args)
      .workingDirectory(eppBin.absolutePath())
      .flags(Executable::UseApplicationIcon));
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


Executable::Executable(const MOBase::ExecutableInfo& info, Flags flags) :
  m_title(info.title()),
  m_binaryInfo(info.binary()),
  m_arguments(info.arguments().join(" ")),
  m_steamAppID(info.steamAppID()),
  m_workingDirectory(info.workingDirectory().absolutePath()),
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

Executable& Executable::title(const QString& s)
{
  m_title = s;
  return *this;
}

Executable& Executable::binaryInfo(const QFileInfo& fi)
{
  m_binaryInfo = fi;
  return *this;
}

Executable& Executable::arguments(const QString& s)
{
  m_arguments = s;
  return *this;
}

Executable& Executable::steamAppID(const QString& s)
{
  m_steamAppID = s;
  return *this;
}

Executable& Executable::workingDirectory(const QString& s)
{
  m_workingDirectory = s;
  return *this;
}

Executable& Executable::flags(Flags f)
{
  m_flags = f;
  return *this;
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
  // flags on plugin executables that the user is allowed to chnage
  const auto allow = ShowInToolbar;


  if (!isCustom() && !other.isCustom()) {
    // this happens when loading plugin executables in addFromPlugin(), replace
    // everything in case the plugin has changed

    // remember the flags though
    const auto flags = m_flags;

    // overwrite everything
    *this = other;

    // set the user flags
    m_flags |= (flags & allow);
  }
  else if (!isCustom()) {
    // this happens when the user is trying to modify a plugin executable
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
