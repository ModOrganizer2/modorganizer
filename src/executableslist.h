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


#include <vector>
#include <QFileInfo>
#include <QMetaType>
#include <gameinfo.h>


/*!
 * @brief Information about an executable
 **/
struct Executable {
  QString m_Title;
  QFileInfo m_BinaryInfo;
  QString m_Arguments;
  MOShared::CloseMOStyle m_CloseMO;
  QString m_SteamAppID;
  QString m_WorkingDirectory;

  bool m_Custom;
  bool m_Toolbar;
};
Q_DECLARE_METATYPE(Executable)


void registerExecutable();


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
  void init();

  /**
   * @brief retrieve an executable by index
   *
   * @param index index of the executable to look up
   * @return reference to the executable
   * @exception out_of_range will throw an exception if the index is invalid
   **/
  const Executable &get(int index) const { return m_Executables.at(index); }

  /**
   * @brief find an executable by its name
   *
   * @param title the title of the executable to look up
   * @return the executable
   * @exception runtime_error will throw an exception if the name is not correct
   **/
  const Executable &find(const QString &tilte) const;

  /**
   * @brief find an executable by its name
   *
   * @param title the title of the executable to look up
   * @return the executable
   * @exception runtime_error will throw an exception if the name is not correct
   **/
  Executable &find(const QString &tilte);

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
   * @param closeMO if true, MO will be closed when the binary is started
   **/
  void addExecutable(const QString &title, const QString &executableName, const QString &arguments, const QString &workingDirectory, MOShared::CloseMOStyle closeMO, const QString &steamAppID, bool custom, bool toolbar);

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

private:

  Executable *findExe(const QString &title);

  void addExecutableInternal(const QString &title, const QString &executableName, const QString &arguments, const QString &workingDirectory, MOShared::CloseMOStyle closeMO, const QString &steamAppID);

private:

  std::vector<Executable> m_Executables;

};

#endif // EXECUTABLESLIST_H
