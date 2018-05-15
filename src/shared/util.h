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

#ifndef UTIL_H
#define UTIL_H


#include <string>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace MOShared {

/// Test if a file (or directory) by the specified name exists
bool FileExists(const std::string &filename);
bool FileExists(const std::wstring &filename);

bool FileExists(const std::wstring &searchPath, const std::wstring &filename);

std::string ToString(const std::wstring &source, bool utf8);
std::wstring ToWString(const std::string &source, bool utf8);

std::string &ToLower(std::string &text);
std::string ToLower(const std::string &text);

std::wstring &ToLower(std::wstring &text);
std::wstring ToLower(const std::wstring &text);

bool CaseInsensitiveEqual(const std::wstring &lhs, const std::wstring &rhs);

VS_FIXEDFILEINFO GetFileVersion(const std::wstring &fileName);
std::wstring GetFileVersionString(const std::wstring &fileName);

} // namespace MOShared

#endif // UTIL_H
