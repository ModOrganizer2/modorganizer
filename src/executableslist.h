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

#ifndef EXECUTABLESLIST_H
#define EXECUTABLESLIST_H

#include "executableinfo.h"

#include <optional>
#include <vector>

#include <QFileInfo>
#include <QMetaType>

namespace MOBase
{
class IPluginGame;
class ExecutableInfo;
}  // namespace MOBase
class Settings;

/*!
 * @brief Information about an executable
 **/
class Executable
{
public:
  enum Flag
  {
    ShowInToolbar      = 0x02,
    UseApplicationIcon = 0x04,
    Hide               = 0x08
  };

  Q_DECLARE_FLAGS(Flags, Flag);

  Executable(QString title = {});

  /**
   * @brief Executable from plugin
   */
  Executable(const MOBase::ExecutableInfo& info, Flags flags);

  const QString& title() const;
  const QFileInfo& binaryInfo() const;
  const QString& arguments() const;
  const QString& steamAppID() const;
  const QString& workingDirectory() const;
  Flags flags() const;

  Executable& title(const QString& s);
  Executable& binaryInfo(const QFileInfo& fi);
  Executable& arguments(const QString& s);
  Executable& steamAppID(const QString& s);
  Executable& workingDirectory(const QString& s);
  Executable& flags(Flags f);

  bool isShownOnToolbar() const;
  void setShownOnToolbar(bool state);
  bool usesOwnIcon() const;
  bool hide() const;

  void mergeFrom(const Executable& other);

private:
  QString m_title;
  QFileInfo m_binaryInfo;
  QString m_arguments;
  QString m_steamAppID;
  QString m_workingDirectory;
  Flags m_flags;
};

/*!
 * @brief List of executables configured to by started from MO
 **/
class ExecutablesList
{
public:
  using vector_type    = std::vector<Executable>;
  using iterator       = vector_type::iterator;
  using const_iterator = vector_type::const_iterator;

  /**
   * standard container interface
   */
  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;
  std::size_t size() const;
  bool empty() const;

  /**
   * @brief initializes the list from the settings and the given plugin
   **/
  void load(const MOBase::IPluginGame* game, const Settings& settings);

  /**
   * @brief re-adds all the executables from the plugin and renames existing
   *        executables that are in the way
   **/
  void resetFromPlugin(MOBase::IPluginGame const* game);

  /**
   * @brief writes the current list to the settings
   */
  void store(Settings& settings);

  /**
   * @brief get an executable by name
   *
   * @param title the title of the executable to look up
   * @return the executable
   * @exception runtime_error will throw an exception if the executable is not found
   **/
  const Executable& get(const QString& title) const;

  /**
   * @brief find an executable by its name
   *
   * @param title the title of the executable to look up
   * @return the executable
   * @exception runtime_error will throw an exception if the name is not correct
   **/
  Executable& get(const QString& title);

  /**
   * @brief find an executable by a fileinfo structure
   * @param info the info object to search for
   * @return the executable
   * @exception runtime_error will throw an exception if the name is not correct
   */
  Executable& getByBinary(const QFileInfo& info);

  /**
   * @brief returns an iterator for the given executable by title, or end()
   */
  iterator find(const QString& title, bool caseSensitive = true);
  const_iterator find(const QString& title, bool caseSensitive = true) const;

  /**
   * @brief determine if an executable exists
   * @param title the title of the executable to look up
   * @return true if the executable exists, false otherwise
   **/
  bool titleExists(const QString& title) const;

  /**
   * @brief add a new executable to the list
   * @param executable
   */
  void setExecutable(const Executable& executable);

  /**
   * @brief remove the executable with the specified file name. This needs to
   *        be an absolute file path
   *
   * @param title title of the executable to remove
   * @note if the executable name is invalid, nothing happens. There is no way
   *       to determine if this was successful
   **/
  void remove(const QString& title);

  /**
   * returns a title that starts with the given prefix and does not clash with
   * an existing executable, may fail
   */
  std::optional<QString> makeNonConflictingTitle(const QString& prefix);

private:
  enum SetFlags
  {
    // executables having the same name as existing ones are ignored
    IgnoreExisting = 1,

    // executables having the same name are merged
    MergeExisting,

    // an existing executable with the same name is renamed
    MoveExisting
  };

  std::vector<Executable> m_Executables;

  /**
   * @brief add the executables preconfigured for this game
   **/
  void addFromPlugin(MOBase::IPluginGame const* game, SetFlags flags);

  /**
   * @brief add a new executable to the list
   * @param executable
   */
  void setExecutable(const Executable& exe, SetFlags flags);

  /**
   * returns the executables provided by the game plugin
   **/
  std::vector<Executable> getPluginExecutables(MOBase::IPluginGame const* game) const;

  /**
   * called when MO is still using the old custom executables from 2.2.0
   **/
  void upgradeFromCustom(const MOBase::IPluginGame* game);

  // logs all executables
  //
  void dump() const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Executable::Flags)

#endif  // EXECUTABLESLIST_H
