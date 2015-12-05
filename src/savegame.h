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

#ifndef SAVEGAME_H
#define SAVEGAME_H


#include <QString>
#include <QObject>
#include <QImage>
#include <QMetaType>
#include <QFile>
#include <QStringList>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace MOBase { class IPluginGame; }

/**
 * @brief represents a single save game
 **/
class SaveGame : public QObject {

Q_OBJECT

public:

  /**
   * @brief construct a save game and immediately read out information from the file
   *
   * @param filename absolute path of the save game file
   **/
  SaveGame(QObject *parent, const QString &filename, MOBase::IPluginGame const *game);

  virtual ~SaveGame();

  /**
   * @brief read out information from a savegame
   *
   * @param fileName absolute path of the save game file
   **/
  virtual void readFile(const QString) { }

  /**
   * @return filename of this savegame
   */
  const QString &fileName() const { return m_FileName; }

  /**
   * @return a list of additional files that belong to this savegame
   */
  virtual QStringList attachedFiles() const;

  /**
   * @return a list of all files that belong to this savegame
   */
  virtual QStringList saveFiles() const;

  /**
   * @return name of the player character
   **/
  const QString &pcName() const { return m_PCName; }

  /**
   * @return level of the player character
   **/
  unsigned short pcLevel() const { return m_PCLevel; }

  /**
   * @return location of the player character
   **/
  const QString &pcLocation() const { return m_PCLocation; }

  /**
   * @return index of the save game
   **/
  unsigned long saveNumber() const { return m_SaveNumber; }

  /**
   * @return creation time of the save game
   **/
  SYSTEMTIME creationTime() const { return m_CreationTime; }

  /**
   * @return screenshot in the savegame
   **/
  const QImage &screenshot() const { return m_Screenshot; }

protected:

  QString m_FileName;
  QString m_PCName;
  unsigned short m_PCLevel;
  QString m_PCLocation;
  unsigned long m_SaveNumber;
  SYSTEMTIME m_CreationTime;
  QImage m_Screenshot;

private:
  MOBase::IPluginGame const * const m_Game;
};


#endif // SAVEGAME_H
