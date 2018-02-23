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

#ifndef LOADMECHANISM_H
#define LOADMECHANISM_H


#include <QDir>
#include <QString>


/**
 * @brief manages the various load mechanisms supported by Mod Organizer
 * the load mechanisms is the means by which the mo-dll is injected into the target
 * process. The default mode "mod organizer" requires the target process to be started
 * from inside mod organizer. In certain cases (oblivion steam edition) this is not
 * possible since the game can then only be started from steam.
 * "Script Extender" is an alternative load mechanism that uses a script extender (obse,
 * fose, nvse or skse) to load MO. This is reliable but prevents se plugins installed
 * through MO from working.
 * "Proxy DLL" replaces a dll belonging to the game by a proxy that will load MO and then
 * chain-load the original dll. This currently only works with steam-versions of games and
 * is intended as a last resort solution.
 **/
class LoadMechanism
{
public:

  enum EMechanism {
    LOAD_MODORGANIZER = 0,
    LOAD_SCRIPTEXTENDER,
    LOAD_PROXYDLL
  };

public:

  /**
   * @brief constructor
   *
   **/
  LoadMechanism();

  /**
   * activate the specified mechanism. This automatically disables any other mechanism active
   *
   * @param mechanism the mechanism to activate
   **/
  void activate(EMechanism mechanism);

  /**
   * @brief test whether the "Mod Organizer" load mechanism is supported for the current game
   *
   * @return true if the load mechanism is supported
   **/
  bool isDirectLoadingSupported();

  /**
   * @brief test whether the "Script Extender" load mechanism is supported for the current game
   *
   * @return true if the load mechanism is supported
   **/
  bool isScriptExtenderSupported();

  /**
   * @brief test whether the "Proxy DLL" load mechanism is supported for the current game
   *
   * @return true if the load mechanism is supported
   **/
  bool isProxyDLLSupported();

private:

  // write a hint file that is required for certain loading mechanisms for the dll to find
  // the mod organizer installation
  void writeHintFile(const QDir &targetDirectory);

  // remove the hint file if it exists. does nothing if the file doesn't exist
  void removeHintFile(QDir targetDirectory);

  // compare the two files by md5-hash, returns true if they are identical
  bool hashIdentical(const QString &fileNameLHS, const QString &fileNameRHS);

  // deactivate loading through script extender. does nothing if se-loading wasn't active
  void deactivateScriptExtender();

  // deactivate loading through proxy-dll. does nothing if se-loading wasn't active
  void deactivateProxyDLL();

  // activate loading through script extender. does nothing if already active. updates
  // the dll if necessary
  void activateScriptExtender();

  // activate loading through proxy-dll. does nothing if already active. updates
  // the dll if necessary
  void activateProxyDLL();

private:

  EMechanism m_SelectedMechanism;

};


#endif // LOADMECHANISM_H
