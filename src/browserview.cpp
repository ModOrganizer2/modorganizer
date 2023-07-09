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

#include "browserview.h"

#include "utility.h"
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QNetworkDiskCache>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineSettings>
#include <Shlwapi.h>

BrowserView::BrowserView(QWidget* parent) : QWebEngineView(parent)
{
  installEventFilter(this);

  // page()->settings()->setMaximumPagesInCache(10);
}

QWebEngineView* BrowserView::createWindow(QWebEnginePage::WebWindowType)
{
  BrowserView* newView = new BrowserView(parentWidget());
  emit initTab(newView);
  return newView;
}

bool BrowserView::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::ShortcutOverride) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->matches(QKeySequence::Find)) {
      emit startFind();
    } else if (keyEvent->matches(QKeySequence::FindNext)) {
      emit findAgain();
    }
  } else if (event->type() == QEvent::MouseButtonPress) {
    QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::MouseButton::MiddleButton) {
      mouseEvent->ignore();
      return true;
    }
    //	TODO This is due to that QTWebEnginePage doesn't support QWebFrame anymore
    //   } else if (event->type() == QEvent::MouseButtonRelease) {
    //    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    //    if (mouseEvent->button() == Qt::MidButton) {
    //      QWebEngineContextMenuData hitTest =
    //      page()->hitTestContent(mouseEvent->pos()); if (hitTest.linkUrl().isValid())
    //      {
    //        emit openUrlInNewTab(hitTest.linkUrl());
    //      }
    //      mouseEvent->ignore();
    //
    //      return true;
    //    }
  }
  return QWebEngineView::eventFilter(obj, event);
}
