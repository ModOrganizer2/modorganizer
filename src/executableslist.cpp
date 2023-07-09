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
#include "settings.h"
#include "utility.h"
#include <log.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

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

void ExecutablesList::load(const MOBase::IPluginGame* game, const Settings& s)
{
  log::debug("loading executables");

  m_Executables.clear();

  // whether the executable list in the .ini is still using the old custom
  // executables from 2.2.0, see upgradeFromCustom()
  bool needsUpgrade = false;

  for (auto& map : s.executables()) {
    Executable::Flags flags;

    if (map["toolbar"].toBool())
      flags |= Executable::ShowInToolbar;

    if (map["ownicon"].toBool())
      flags |= Executable::UseApplicationIcon;

    if (map["hide"].toBool())
      flags |= Executable::Hide;

    if (map.contains("custom")) {
      // the "custom" setting only exists in older versions
      needsUpgrade = true;
    }

    setExecutable(Executable()
                      .title(map["title"].toString())
                      .binaryInfo(QFileInfo(map["binary"].toString()))
                      .arguments(map["arguments"].toString())
                      .steamAppID(map["steamAppID"].toString())
                      .workingDirectory(map["workingDirectory"].toString())
                      .flags(flags));
  }

  addFromPlugin(game, IgnoreExisting);

  if (needsUpgrade)
    upgradeFromCustom(game);

  dump();
}

void ExecutablesList::store(Settings& s)
{
  std::vector<std::map<QString, QVariant>> v;

  for (const auto& item : *this) {
    std::map<QString, QVariant> map;

    map["title"]            = item.title();
    map["toolbar"]          = item.isShownOnToolbar();
    map["ownicon"]          = item.usesOwnIcon();
    map["hide"]             = item.hide();
    map["binary"]           = item.binaryInfo().filePath();
    map["arguments"]        = item.arguments();
    map["workingDirectory"] = item.workingDirectory();
    map["steamAppID"]       = item.steamAppID();

    v.push_back(std::move(map));
  }

  s.setExecutables(v);
}

std::vector<Executable>
ExecutablesList::getPluginExecutables(MOBase::IPluginGame const* game) const
{
  Q_ASSERT(game != nullptr);

  std::vector<Executable> v;

  for (const ExecutableInfo& info : game->executables()) {
    if (!info.isValid()) {
      continue;
    }

    v.push_back({info, Executable::UseApplicationIcon});
  }

  const QFileInfo eppBin(QCoreApplication::applicationDirPath() +
                         "/explorer++/Explorer++.exe");

  if (eppBin.exists()) {
    const auto args = QString("\"%1\"").arg(
        QDir::toNativeSeparators(game->dataDirectory().absolutePath()));

    const auto exe = Executable()
                         .title("Explore Virtual Folder")
                         .binaryInfo(eppBin)
                         .arguments(args)
                         .workingDirectory(eppBin.absolutePath())
                         .flags(Executable::UseApplicationIcon);

    v.push_back(exe);
  }

  return v;
}

void ExecutablesList::resetFromPlugin(MOBase::IPluginGame const* game)
{
  log::debug("resetting plugin executables");

  Q_ASSERT(game != nullptr);

  for (const auto& exe : getPluginExecutables(game)) {
    setExecutable(exe, MoveExisting);
  }
}

void ExecutablesList::addFromPlugin(IPluginGame const* game, SetFlags flags)
{
  Q_ASSERT(game != nullptr);

  for (const auto& exe : getPluginExecutables(game)) {
    setExecutable(exe, flags);
  }
}

const Executable& ExecutablesList::get(const QString& title) const
{
  for (const auto& exe : m_Executables) {
    if (exe.title() == title) {
      return exe;
    }
  }

  throw std::runtime_error(
      QString("executable not found: %1").arg(title).toLocal8Bit().constData());
}

Executable& ExecutablesList::get(const QString& title)
{
  return const_cast<Executable&>(std::as_const(*this).get(title));
}

Executable& ExecutablesList::getByBinary(const QFileInfo& info)
{
  for (Executable& exe : m_Executables) {
    if (exe.binaryInfo() == info) {
      return exe;
    }
  }
  throw std::runtime_error("invalid info");
}

ExecutablesList::iterator ExecutablesList::find(const QString& title, bool ci)
{
  const auto cif = ci ? Qt::CaseInsensitive : Qt::CaseSensitive;

  return std::find_if(begin(), end(), [&](auto&& e) {
    return (e.title().compare(title, cif) == 0);
  });
}

ExecutablesList::const_iterator ExecutablesList::find(const QString& title,
                                                      bool ci) const
{
  const auto cif = ci ? Qt::CaseInsensitive : Qt::CaseSensitive;

  return std::find_if(begin(), end(), [&](auto&& e) {
    return (e.title().compare(title, cif) == 0);
  });
}

bool ExecutablesList::titleExists(const QString& title) const
{
  auto test = [&](const Executable& exe) {
    return exe.title() == title;
  };
  return std::find_if(m_Executables.begin(), m_Executables.end(), test) !=
         m_Executables.end();
}

void ExecutablesList::setExecutable(const Executable& exe)
{
  setExecutable(exe, MergeExisting);
}

void ExecutablesList::setExecutable(const Executable& exe, SetFlags flags)
{
  auto itor = find(exe.title());

  if (itor != end()) {
    if (flags == IgnoreExisting) {
      return;
    }

    if (flags == MoveExisting) {
      const auto newTitle = makeNonConflictingTitle(exe.title());
      if (!newTitle) {
        log::error("executable '{}' was in the way but could not be renamed",
                   exe.title());

        return;
      }

      log::warn("executable '{}' was in the way and was renamed to '{}'", itor->title(),
                *newTitle);

      itor->title(*newTitle);
      itor = end();
    }
  }

  if (itor == m_Executables.end()) {
    m_Executables.push_back(exe);
  } else {
    itor->mergeFrom(exe);
  }
}

void ExecutablesList::remove(const QString& title)
{
  auto itor = find(title);
  if (itor != m_Executables.end()) {
    m_Executables.erase(itor);
  }
}

std::optional<QString> ExecutablesList::makeNonConflictingTitle(const QString& prefix)
{
  const int max = 100;

  QString title = prefix;

  for (int i = 1; i < max; ++i) {
    if (!titleExists(title)) {
      return title;
    }

    title = prefix + QString(" (%1)").arg(i);
  }

  log::error("ran out of executable titles for prefix '{}'", prefix);
  return {};
}

void ExecutablesList::upgradeFromCustom(MOBase::IPluginGame const* game)
{
  log::debug("upgrading executables list");

  Q_ASSERT(game != nullptr);

  // prior to 2.2.1, plugin executables were special in the sense that they
  // did not store certain settings, like the binary info and working directory;
  // those were filled in when MO started, but never saved
  //
  // this interferes with the new executables list, which is completely
  // customizable, because plugin executables are only added to the list when
  // they don't exist at all and are ignored otherwise, leaving some of the
  // fields completely blank
  //
  // when the "custom" setting is found in the .ini file (see load()), it is
  // assumed that the older scheme is still present; in that case, the plugin
  // executables force their binary info and working directory into the existing
  // executables one last time
  //
  // from that point on, plugin executables are ignored unless they're
  // completely missing from the list, allowing users to customize them as they
  // want

  for (const auto& exe : getPluginExecutables(game)) {
    auto itor = find(exe.title());
    if (itor == end()) {
      continue;
    }

    if (!itor->binaryInfo().exists()) {
      itor->binaryInfo(exe.binaryInfo());
    }

    if (itor->workingDirectory().isEmpty()) {
      itor->workingDirectory(exe.workingDirectory());
    }
  }
}

void ExecutablesList::dump() const
{
  for (const auto& e : m_Executables) {
    QStringList flags;

    if (e.flags() & Executable::ShowInToolbar) {
      flags.push_back("toolbar");
    }

    if (e.flags() & Executable::UseApplicationIcon) {
      flags.push_back("icon");
    }

    if (e.flags() & Executable::Hide) {
      flags.push_back("hide");
    }

    log::debug(" . executable '{}'\n"
               "    binary: {}\n"
               "    arguments: {}\n"
               "    steam ID: {}\n"
               "    directory: {}\n"
               "    flags: {} ({})",
               e.title(), e.binaryInfo().filePath(), e.arguments(), e.steamAppID(),
               e.workingDirectory(), flags.join("|"), e.flags());
  }
}

Executable::Executable(QString title) : m_title(title) {}

Executable::Executable(const MOBase::ExecutableInfo& info, Flags flags)
    : m_title(info.title()), m_binaryInfo(info.binary()),
      m_arguments(info.arguments().join(" ")), m_steamAppID(info.steamAppID()),
      m_workingDirectory(info.workingDirectory().path()), m_flags(flags)
{}

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

bool Executable::hide() const
{
  return m_flags.testFlag(Hide);
}

void Executable::mergeFrom(const Executable& other)
{
  // this happens after executables are loaded from settings and plugin
  // executables are being added, or when users are modifying executables

  m_title            = other.title();
  m_binaryInfo       = other.binaryInfo();
  m_arguments        = other.arguments();
  m_steamAppID       = other.steamAppID();
  m_workingDirectory = other.workingDirectory();
  m_flags            = other.flags();
}
