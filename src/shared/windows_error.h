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

#ifndef WINDOWS_ERROR_H
#define WINDOWS_ERROR_H


#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace MOShared {

class windows_error : public std::runtime_error {
public:
  windows_error(const std::string& message, int errorcode = ::GetLastError())
    : runtime_error(constructMessage(message, errorcode)), m_ErrorCode(errorcode)
  {}
  int getErrorCode() const { return m_ErrorCode; }
private:
  std::string constructMessage(const std::string& input, int errorcode);
private:
  int m_ErrorCode;
};

} // namespace MOShared

#endif // WINDOWS_ERROR_H
