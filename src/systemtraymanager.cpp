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

#include "systemtraymanager.h"

#include <QAction>
#include <QDockWidget>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>

SystemTrayManager::SystemTrayManager(QMainWindow* parent, QDockWidget* logDock)
    : m_Parent(parent), m_LogDock(logDock),
      m_SystemTrayIcon(new QSystemTrayIcon(QIcon(":/MO/gui/app_icon"), m_Parent))
{
  m_SystemTrayIcon->setToolTip(tr("Mod Organizer"));

  connect(m_SystemTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
          SLOT(on_systemTrayIcon_activated(QSystemTrayIcon::ActivationReason)));

  auto* exitAction = new QAction(tr("Exit"), m_SystemTrayIcon);
  connect(exitAction, &QAction::triggered, m_Parent, &QMainWindow::close);

  auto* trayMenu = new QMenu(m_Parent);
  trayMenu->addAction(exitAction);

  m_SystemTrayIcon->setContextMenu(trayMenu);
}

void SystemTrayManager::minimizeToSystemTray()
{
  m_SystemTrayIcon->show();
  m_Parent->hide();

  if (m_LogDock->isFloating() && m_LogDock->isVisible()) {
    m_LogDock->hide();
  }
}

void SystemTrayManager::restoreFromSystemTray()
{
  m_SystemTrayIcon->hide();

  m_Parent->showNormal();
  m_Parent->raise();
  m_Parent->activateWindow();

  if (m_LogDock->isFloating() && m_LogDock->isHidden()) {
    m_LogDock->show();
  }
}

void SystemTrayManager::on_systemTrayIcon_activated(
    QSystemTrayIcon::ActivationReason reason)
{
  if (m_Parent->isHidden() && reason == QSystemTrayIcon::Trigger) {
    // left click
    restoreFromSystemTray();
  }
}
