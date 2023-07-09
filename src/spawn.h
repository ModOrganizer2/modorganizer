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

#ifndef SPAWN_H
#define SPAWN_H

#define WIN32_LEAN_AND_MEAN
#include <QDir>
#include <QFileInfo>
#include <tchar.h>
#include <windows.h>

class Settings;

namespace spawn
{

/*
 * @param binary the binary to spawn
 * @param arguments arguments to pass to the binary
 * @param profileName name of the active profile
 * @param currentDirectory the directory to use as the working directory to run in
 * @param logLevel log level to be used by the hook library. Ignored if hooked is false
 * @param hooked if set, the binary is started with mo injected
 * @param stdout if not equal to INVALID_HANDLE_VALUE, this is used as stdout for the
 * process
 * @param stderr if not equal to INVALID_HANDLE_VALUE, this is used as stderr for the
 * process
 */
struct SpawnParameters
{
  QFileInfo binary;
  QString arguments;
  QDir currentDirectory;
  QString steamAppID;
  bool hooked   = false;
  HANDLE stdOut = INVALID_HANDLE_VALUE;
  HANDLE stdErr = INVALID_HANDLE_VALUE;
};

bool checkSteam(QWidget* parent, const SpawnParameters& sp, const QDir& gameDirectory,
                const QString& steamAppID, const Settings& settings);

bool checkBlacklist(QWidget* parent, const SpawnParameters& sp, Settings& settings);

/**
 * @brief spawn a binary with Mod Organizer injected
 * @return the process handle
 **/
HANDLE startBinary(QWidget* parent, const SpawnParameters& sp);

enum class FileExecutionTypes
{
  Executable = 1,
  Other
};

struct FileExecutionContext
{
  QFileInfo binary;
  QString arguments;
  FileExecutionTypes type;
};

QString findJavaInstallation(const QString& jarFile);

FileExecutionContext getFileExecutionContext(QWidget* parent, const QFileInfo& target);

FileExecutionTypes getFileExecutionType(const QFileInfo& target);

}  // namespace spawn

// convenience functions to work with the external helper program, which is used
// to make changes on the system that require administrative rights, so that
// ModOrganizer itself can run without special privileges
//
namespace helper
{

/**
 * @brief sets the last modified time for all .bsa-files in the target directory well
 *into the past
 * @param moPath absolute path to the modOrganizer base directory
 * @param dataPath the path taht contains the .bsa-files, usually the data directory of
 *the game
 **/
bool backdateBSAs(QWidget* parent, const std::wstring& moPath,
                  const std::wstring& dataPath);

/**
 * @brief waits for the current process to exit and restarts it as an administrator
 * @param moPath absolute path to the modOrganizer base directory
 * @param moFile file name of modOrganizer
 * @param workingDir current working directory
 **/
bool adminLaunch(QWidget* parent, const std::wstring& moPath,
                 const std::wstring& moFile, const std::wstring& workingDir);

}  // namespace helper

#endif  // SPAWN_H
