/*
Copyright (C) 2014 Sebastian Herbord. All rights reserved.

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


#ifndef SAFEWRITEFILE_H
#define SAFEWRITEFILE_H


#include <utility.h>
#include <QTemporaryFile>
#include <QString>

/**
 * @brief a wrapper for QFile that ensures the file is only actually (over-)written if writing was successful
 */
class SafeWriteFile {
public:
  SafeWriteFile(const QString &fileName);

  QFile *operator->();

  void commit();

  bool commitIfDifferent(uint &hash);

  uint hash();

private:
  QString m_FileName;
  QTemporaryFile m_TempFile;
};


#endif // SAFEWRITEFILE_H





