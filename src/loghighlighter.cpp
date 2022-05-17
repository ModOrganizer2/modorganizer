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

#include "loghighlighter.h"

LogHighlighter::LogHighlighter(QObject* parent) : QSyntaxHighlighter(parent) {}

void LogHighlighter::highlightBlock(const QString& text)
{
  int spacePos = text.indexOf(" ");
  if (spacePos != -1) {
    QString type = text.mid(0, spacePos);
    if (type == "DEBUG") {
      setFormat(0, text.length(), Qt::gray);
    } else if (type == "INFO") {
      setFormat(0, text.length(), Qt::darkGreen);
    } else if (type == "ERROR") {
      setFormat(0, text.length(), Qt::red);
    }
  }

  int markPos = text.indexOf("injecting to");
  if (markPos != -1) {
    setFormat(markPos + 12, text.length(), Qt::blue);
  }

  markPos = text.indexOf("using profile");
  if (markPos != -1) {
    setFormat(markPos + 13, text.length(), Qt::blue);
  }
}
