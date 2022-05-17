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

#ifndef LOGHIGHLIGHTER_H
#define LOGHIGHLIGHTER_H

#include <QSyntaxHighlighter>

/**
 * @brief Syntax highlighter to make log files from mo.dll more readable.
 * @note this is currently not used!
 **/
class LogHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit LogHighlighter(QObject *parent = 0);

signals:

public slots:

protected:

  virtual void highlightBlock(const QString &text);

};

#endif // LOGHIGHLIGHTER_H
