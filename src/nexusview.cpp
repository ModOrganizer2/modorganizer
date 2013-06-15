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

#include "nexusview.h"

#include <QEvent>
#include <QKeyEvent>
#include <QWebFrame>
#include <QWebElement>
#include <QNetworkDiskCache>
#include <QMenu>
#include <Shlwapi.h>
#include "utility.h"


using namespace MOBase;


NexusView::NexusView(QWidget *parent)
  : QWebView(parent), m_LastSeenModID(0)
{
  installEventFilter(this);
  connect(this, SIGNAL(urlChanged(QUrl)), this, SLOT(urlChanged(QUrl)));

  page()->settings()->setMaximumPagesInCache(10);
}


void NexusView::urlChanged(const QUrl &url)
{
  QRegExp modidExp(QString("http://([a-zA-Z0-9]*).nexusmods.com/downloads/file.php\\?id=(\\d+)"), Qt::CaseInsensitive);
  if (modidExp.indexIn(url.toString()) != -1) {
    m_LastSeenModID = modidExp.cap(2).toInt();
  }
}

void NexusView::openPageExternal()
{
  ::ShellExecuteW(NULL, L"open", ToWString(url().toString()).c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void NexusView::openLinkExternal()
{
  ::ShellExecuteW(NULL, L"open", ToWString(m_ContextURL.toString()).c_str(), NULL, NULL, SW_SHOWNORMAL);
}


QWebView *NexusView::createWindow(QWebPage::WebWindowType)
{
  NexusView *newView = new NexusView(parentWidget());
  emit initTab(newView);
  return newView;
}


bool NexusView::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::ShortcutOverride) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->matches(QKeySequence::Find)) {
      emit startFind();
    } else if (keyEvent->matches(QKeySequence::FindNext)) {
      emit findAgain();
    }
  } else if (event->type() == QEvent::MouseButtonPress) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::MidButton) {
      mouseEvent->ignore();
      return true;
    }
  } else if (event->type() == QEvent::MouseButtonRelease) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::MidButton) {
      QWebHitTestResult hitTest = page()->frameAt(mouseEvent->pos())->hitTestContent(mouseEvent->pos());
      if (hitTest.linkUrl().isValid()) {
        emit openUrlInNewTab(hitTest.linkUrl());
      }
      mouseEvent->ignore();

      return true;
    }
  }
  return QWebView::eventFilter(obj, event);
}


void NexusView::contextMenuEvent(QContextMenuEvent *event)
{
  if (!page()->swallowContextMenuEvent(event)) {
    QWebHitTestResult r = page()->mainFrame()->hitTestContent(event->pos());

    QMenu *menu = page()->createStandardContextMenu();
    QAction *openExternalAction = new QAction("Open in external browser", menu);
    if (r.linkUrl().isEmpty()) {
      connect(openExternalAction, SIGNAL(triggered()), this, SLOT(openPageExternal()));
    } else {
      m_ContextURL = r.linkUrl();
      connect(openExternalAction, SIGNAL(triggered()), this, SLOT(openLinkExternal()));
    }

    menu->addSeparator();
    menu->addAction(openExternalAction);
    menu->exec(mapToGlobal(event->pos()));
  }
}
