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

#include "savegameinfowidgetgamebryo.h"

#include "pluginlist.h"
#include "gamebryosavegame.h"

#include <QFont>
#include <QLabel>
#include <QLayout>
#include <QVBoxLayout>


SaveGameInfoWidgetGamebryo::SaveGameInfoWidgetGamebryo(MOBase::ISaveGame const *saveGame, PluginList *pluginList, QWidget *parent)
  : SaveGameInfoWidget(parent), m_PluginList(pluginList)
{
  QVBoxLayout *gameLayout = new QVBoxLayout();
  gameLayout->setMargin(0);
  gameLayout->setSpacing(2);
  getGameFrame()->setLayout(gameLayout);
  setSave(saveGame);
}


void SaveGameInfoWidgetGamebryo::setSave(MOBase::ISaveGame const *saveGame)
{
  SaveGameInfoWidget::setSave(saveGame);
  GamebryoSaveGame const *gamebryoSave = dynamic_cast<GamebryoSaveGame const *>(saveGame);
  QLayout *layout = getGameFrame()->layout();
  QLabel *header = new QLabel(tr("Missing ESPs"));
  QFont headerFont = header->font();
  QFont contentFont = headerFont;
  headerFont.setItalic(true);
  contentFont.setBold(true);
  contentFont.setPointSize(7);
  header->setFont(headerFont);
  layout->addWidget(header);
  int count = 0;
  for (QString const &pluginName : gamebryoSave->getPlugins()) {
    if (m_PluginList->isEnabled(pluginName)) {
      continue;
    }

    ++count;

    if (count > 10) {
      break;
    }

    QLabel *pluginLabel = new QLabel(pluginName);
    pluginLabel->setIndent(10);
    pluginLabel->setFont(contentFont);
    layout->addWidget(pluginLabel);
  }
  if (count > 10) {
    QLabel *dotDotLabel = new QLabel("...");
    dotDotLabel->setIndent(10);
    dotDotLabel->setFont(contentFont);
    layout->addWidget(dotDotLabel);
  }
  if (count == 0) {
    QLabel *dotDotLabel = new QLabel(tr("None"));
    dotDotLabel->setIndent(10);
    dotDotLabel->setFont(contentFont);
    layout->addWidget(dotDotLabel);
  }
}
