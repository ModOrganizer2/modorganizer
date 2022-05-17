/*
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

#include "lcdnumber.h"

#include <QTimer>
#include <QToolTip>

LCDNumber::LCDNumber(QWidget* parent) : QLCDNumber(parent) {}

void LCDNumber::mousePressEvent(QMouseEvent* event)
{
  m_toolTipPosition = mapToGlobal(event->pos());
  QTimer::singleShot(100, this, SLOT(showToolTip()));
}

void LCDNumber::showToolTip()
{
  QToolTip::showText(m_toolTipPosition, toolTip());
}
