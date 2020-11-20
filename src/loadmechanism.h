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

/**
 * @brief manages the various load mechanisms supported by Mod Organizer
 **/
class LoadMechanism
{
public:
  enum EMechanism {
    LOAD_MODORGANIZER = 0,
  };

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
   * @return true
   **/
  bool isDirectLoadingSupported() const;

private:
  EMechanism m_SelectedMechanism;
};


QString toString(LoadMechanism::EMechanism e);

#endif // LOADMECHANISM_H
