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

#include "savegameinfowidget.h"
#include "ui_savegameinfowidget.h"

#include "isavegame.h"
#include "gamebryosavegame.h"

#include <QDate>
#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QPixmap>
#include <QString>
#include <QStyle>
#include <QTime>

#include <Qt>


SaveGameInfoWidget::SaveGameInfoWidget(QWidget *parent)
  : QWidget(parent)
  , ui(new Ui::SaveGameInfoWidget)
{
  ui->setupUi(this);
  this->setWindowFlags(Qt::ToolTip | Qt::BypassGraphicsProxyWidget);
  setWindowOpacity(style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, 0, this) / qreal(255.0));
  ui->gameFrame->setStyleSheet("background-color: transparent;");
//  installEventFilter(this);
}

SaveGameInfoWidget::~SaveGameInfoWidget()
{
  delete ui;
}


void SaveGameInfoWidget::setSave(MOBase::ISaveGame const *saveGame)
{
  GamebryoSaveGame const *game = dynamic_cast<GamebryoSaveGame const *>(saveGame);
  ui->saveNumLabel->setText(QString("%1").arg(game->getSaveNumber()));
  ui->characterLabel->setText(game->getPCName());
  ui->locationLabel->setText(game->getPCLocation());
  ui->levelLabel->setText(QString("%1").arg(game->getPCLevel()));
  //This somewhat contorted code is because on my system at least, the
  //old way of doing this appears to give short date and long time.
  QDateTime t = saveGame->getCreationTime();
  ui->dateLabel->setText(t.date().toString(Qt::DefaultLocaleShortDate) + " " +
                         t.time().toString(Qt::DefaultLocaleLongDate));
  ui->screenshotLabel->setPixmap(QPixmap::fromImage(game->getScreenshot()));
  if (ui->gameFrame->layout() != nullptr) {
    QLayoutItem *item = nullptr;
    while ((item = ui->gameFrame->layout()->takeAt(0)) != nullptr) {
      delete item->widget();
      delete item;
    }
    ui->gameFrame->layout()->setSizeConstraint(QLayout::SetFixedSize);
  }
}

QFrame *SaveGameInfoWidget::getGameFrame()
{
  return ui->gameFrame;
}
