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

#include "report.h"
#include "utility.h"
#include <QMessageBox>
#include <QApplication>
#include <Windows.h>


using namespace MOBase;


void reportError(QString message)
{
  if (QApplication::topLevelWidgets().count() != 0) {
    QMessageBox messageBox(QMessageBox::Warning, QObject::tr("Error"), message, QMessageBox::Ok);
    messageBox.exec();
  } else {
    ::MessageBoxW(NULL, ToWString(message).c_str(), ToWString(QObject::tr("Error")).c_str(), MB_ICONERROR | MB_OK);
  }
}


std::tstring toTString(const QString& source)
{
#ifdef UNICODE
  wchar_t* temp = new wchar_t[source.size() + 1];
  source.toWCharArray(temp);
  temp[source.size()] = '\0';
  std::tstring result(temp);
  delete[] temp;
  return result;
#else // UNICODE
  return source.toAscii();
#endif // UNICODE
}
