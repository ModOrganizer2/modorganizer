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

#include "nxmurl.h"
#include <utility.h>
#include <QRegExp>
#include <QStringList>

NXMUrl::NXMUrl(const QString &url)
{
  QRegExp exp("nxm://([a-z]+)/mods/(\\d+)/files/(\\d+)", Qt::CaseInsensitive);
  exp.indexIn(url);
  if (exp.captureCount() != 3) {
    throw MOBase::MyException(tr("invalid nxm-link: %1").arg(url));
  }
  m_ModId = exp.cap(2).toInt();
  m_FileId = exp.cap(3).toInt();
}
