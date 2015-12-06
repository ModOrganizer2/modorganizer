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

#include "savegame.h"

#include "iplugingame.h"
#include "scriptextender.h"
#include "utility.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>

#include <limits>
#include <set>

using namespace MOBase;

SaveGame::SaveGame(QObject *parent, const QString &filename, const MOBase::IPluginGame *game)
  : QObject(parent)
  , m_FileName(filename)
  , m_Game(game)
{
}

SaveGame::~SaveGame()
{
}

QStringList SaveGame::attachedFiles() const
{
  QStringList result;
  ScriptExtender const *extender = m_Game->feature<ScriptExtender>();
  if (extender != nullptr) {
    for (QString const &ext : extender->saveGameAttachmentExtensions()) {
      QFileInfo fi(fileName());
      fi.setFile(fi.canonicalPath() + "/" + fi.completeBaseName() + "." + ext);
      if (fi.exists()) {
        result.append(fi.filePath());
      }
    }
  }

  return result;
}

QStringList SaveGame::saveFiles() const
{
  QStringList result = attachedFiles();
  result.append(fileName());
  return result;
}
