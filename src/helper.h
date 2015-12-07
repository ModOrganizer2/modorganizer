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

#ifndef HELPER_H
#define HELPER_H


#include <string>


/**
 * @brief Convenience functions to work with the external helper program.
 * 
 * The mo_helper program is used to make changes on the system that require administrative
 * rights, so that ModOrganizer itself can run without special privileges
 **/
namespace Helper {

/**
 * @brief initialise the specified directory for use with mod organizer.
 * 
 * This will create all required sub-directories and give the user running ModOrganizer
 * write-access
 *
 * @param moDirectory absolute path to the ModOrganizer base directory
 * @return true on success
 **/
bool init(const std::wstring &moDirectory);

/**
 * @brief sets the last modified time for all .bsa-files in the target directory well into the past
 * @param moDirectory absolute path to the modOrganizer base directory
 * @param dataPath the path taht contains the .bsa-files, usually the data directory of the game
 **/
bool backdateBSAs(const std::wstring &moDirectory, const std::wstring &dataPath);

}


#endif // HELPER_H
