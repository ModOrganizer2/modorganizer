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

#include <vector>

#include <QFileInfo>
#include <QMetaType>

namespace MOBase { class IPluginGame; }

/*!
 * @brief Information about an executable
 **/
struct Executable {
  QString m_Title;
  QFileInfo m_BinaryInfo;
  QString m_Arguments;
  QString m_SteamAppID;
  QString m_WorkingDirectory;

  enum Flag {
    CustomExecutable = 0x01,
    ShowInToolbar = 0x02,
    UseApplicationIcon = 0x04,

    AllFlags = 0xff //I know, I know
  };

  Q_DECLARE_FLAGS(Flags, Flag)

  Flags m_Flags;

  bool isCustom() const { return m_Flags.testFlag(CustomExecutable); }

  bool isShownOnToolbar() const { return m_Flags.testFlag(ShowInToolbar); }

  void showOnToolbar(bool state);

  bool usesOwnIcon() const { return  m_Flags.testFlag(UseApplicationIcon); }
};


/*!
 * @brief List of executables configured to by started from MO
 **/
class ExecutablesList {
public:

  /**
   * @brief constructor
   *
   **/
  ExecutablesList();

  ~ExecutablesList();

  /**
   * @brief initialise the list with the executables preconfigured for this game
   **/
  void init(MOBase::IPluginGame const *game);

  /**
   * @brief find an executable by its name
   *
   * @param title the title of the executable to look up
   * @return the executable
   * @exception runtime_error will throw an exception if the name is not correct
   **/
  const Executable &find(const QString &title) const;

  /**
   * @brief find an executable by its name
   *
   * @param title the title of the executable to look up
   * @return the executable
   * @exception runtime_error will throw an exception if the name is not correct
   **/
  Executable &find(const QString &title);

  /**
   * @brief find an executable by a fileinfo structure
   * @param info the info object to search for
   * @return the executable
   * @exception runtime_error will throw an exception if the name is not correct
   */
  Executable &findByBinary(const QFileInfo &info);

  /**
   * @brief determine if an executable exists
   * @param title the title of the executable to look up
   * @return true if the executable exists, false otherwise
   **/
  bool titleExists(const QString &title) const;

  /**
   * @brief add a new executable to the list
   * @param executable
   */
  void addExecutable(const Executable &executable);

  /**
   * @brief add a new executable to the list
   *
   * @param title name displayed in the UI
   * @param executableName the actual filename to execute
   * @param arguments arguments to pass to the executable
   **/
  void addExecutable(const QString &title,
                     const QString &executableName,
                     const QString &arguments,
                     const QString &workingDirectory,
                     const QString &steamAppID,
                     Executable::Flags flags)
  {
    updateExecutable(title, executableName, arguments, workingDirectory,
                     steamAppID, Executable::AllFlags, flags);
  }

  /**
   * @brief Update an executable to the list
   *
   * @param title name displayed in the UI
   * @param executableName the actual filename to execute
   * @param arguments arguments to pass to the executable
   * @param closeMO if true, MO will be closed when the binary is started
   **/
  void updateExecutable(const QString &title,
                        const QString &executableName,
                        const QString &arguments,
                        const QString &workingDirectory,
                        const QString &steamAppID,
                        Executable::Flags mask,
                        Executable::Flags flags);

  /**
   * @brief remove the executable with the specified file name. This needs to be an absolute file path
   *
   * @param title title of the executable to remove
   * @note if the executable name is invalid, nothing happens. There is no way to determine if this was successful
   **/
  void remove(const QString &title);

  /**
   * @brief retrieve begin and end iterators of the configured executables
   *
   * @param begin iterator to the first executable
   * @param end iterator one past the last executable
   **/
  void getExecutables(std::vector<Executable>::const_iterator &begin, std::vector<Executable>::const_iterator &end) const;

  /**
   * @brief retrieve begin and end iterators of the configured executables
   *
   * @param begin iterator to the first executable
   * @param end iterator one past the last executable
   **/
  void getExecutables(std::vector<Executable>::iterator &begin, std::vector<Executable>::iterator &end);

  /**
   * @brief get the number of executables (custom or otherwise)
   **/
  size_t size() const {
    return m_Executables.size();
  }

private:

  std::vector<Executable>::iterator findExe(const QString &title);

  void addExecutableInternal(const QString &title, const QString &executableName, const QString &arguments,
                             const QString &workingDirectory,
                             const QString &steamAppID);

private:

  std::vector<Executable> m_Executables;

};

Q_DECLARE_OPERATORS_FOR_FLAGS(Executable::Flags)

#endif // EXECUTABLESLIST_H
