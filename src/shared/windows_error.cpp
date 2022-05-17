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

#include "windows_error.h"
#include <sstream>

namespace MOShared {

std::string windows_error::constructMessage(const std::string& input, int inErrorCode)
{
  std::ostringstream finalMessage;
  finalMessage << input;

  LPSTR buffer = nullptr;

  DWORD errorCode = inErrorCode != -1 ? inErrorCode : ::GetLastError();

  // TODO: the message is not english?
  if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                     nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, nullptr) == 0) {
    finalMessage << " (errorcode " << errorCode << ")";
  } else {
    LPSTR lastChar = buffer + strlen(buffer) - 2;
    *lastChar = '\0';
    finalMessage << " (" << buffer << " [" << errorCode << "])";
    LocalFree(buffer); // allocated by FormatMessage
  }

  ::SetLastError(errorCode); // restore error code because FormatMessage might have modified it
  return finalMessage.str();
}

} // namespace MOShared
