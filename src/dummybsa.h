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

#ifndef DUMMYBSA_H
#define DUMMYBSA_H

#include <QString>
#include <QFile>

/**
 * @brief Class for creating a dummy bsa used for archive invalidation
 **/
class DummyBSA
{

public:

  /**
   * @brief constructor
   *
   **/
  DummyBSA();

  /**
   * @brief write to the specified file
   *
   * @param fileName name of the file to write to
   **/
  void write(const QString& fileName);

private:

  void writeHeader(QFile& file);
  void writeFolderRecord(QFile& file, const std::string& folderName);
  void writeFileRecord(QFile& file, const std::string& fileName);
  void writeFileRecordBlocks(QFile& file, const std::string& folderName);

private:

  unsigned long m_Version;
  std::string m_FolderName;
  std::string m_FileName;
  unsigned long m_TotalFileNameLength;

};


#endif // DUMMYBSA_H
