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
#include <windows.h>
#include <tchar.h>
#include <QFileInfo>
#include <QDir>


/**
 * @brief a dirty little trick so we can issue a clean restart from startBinary
 * @note unused
 */
/*class ExitProxy : public QObject {
  Q_OBJECT
public:
  static ExitProxy *instance();
  void emitExit();
signals:
  void exit();
private:
  ExitProxy() {}
private:
  static ExitProxy *s_Instance;
};*/


/**
 * @brief spawn a binary with Mod Organizer injected
 *
 * @param binary the binary to spawn
 * @param arguments arguments to pass to the binary
 * @param profileName name of the active profile
 * @param currentDirectory the directory to use as the working directory to run in
 * @param logLevel log level to be used by the hook library. Ignored if hooked is false
 * @param hooked if set, the binary is started with mo injected
 * @param stdout if not equal to INVALID_HANDLE_VALUE, this is used as stdout for the process
 * @param stderr if not equal to INVALID_HANDLE_VALUE, this is used as stderr for the process
 * @return the process handle
 * @todo is the profile name even used any more?
 * @todo is the hooked parameter used?
 **/
HANDLE startBinary(const QFileInfo &binary, const QString &arguments, const QString &profileName, int logLevel,
                   const QDir &currentDirectory, bool hooked,
                   HANDLE stdOut = INVALID_HANDLE_VALUE, HANDLE stdErr = INVALID_HANDLE_VALUE);

#endif // SPAWN_H

