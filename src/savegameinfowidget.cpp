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
#include "utility.h"
#include <QGraphicsDropShadowEffect>


SaveGameInfoWidget::SaveGameInfoWidget(QWidget *parent)
  : QWidget(parent), ui(new Ui::SaveGameInfoWidget)
{
  ui->setupUi(this);
  this->setWindowFlags(Qt::ToolTip | Qt::BypassGraphicsProxyWidget);
  setWindowOpacity(style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, 0, this) / qreal(255.0));
//  installEventFilter(this);
}

SaveGameInfoWidget::~SaveGameInfoWidget()
{
  delete ui;
}


void SaveGameInfoWidget::setSave(const SaveGame *saveGame)
{
  ui->saveNumLabel->setText(QString("%1").arg(saveGame->saveNumber()));
  ui->characterLabel->setText(saveGame->pcName());
  ui->locationLabel->setText(saveGame->pcLocation());
  ui->levelLabel->setText(QString("%1").arg(saveGame->pcLevel()));
  ui->dateLabel->setText(MOBase::ToString(saveGame->creationTime()));
  ui->screenshotLabel->setPixmap(QPixmap::fromImage(saveGame->screenshot()));
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
