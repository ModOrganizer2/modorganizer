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

#ifndef SINGLEINSTANCE_H
#define SINGLEINSTANCE_H

#include <QObject>
#include <QSharedMemory>
#include <QLocalServer>


/**
 * used to ensure only a single instance of Mod Organizer is started and to
 * allow secondary instances to send messages to the primary (visible) one. This way,
 * secondary instances can start a download in the primary one
 **/
class SingleInstance : public QObject
{

  Q_OBJECT

public:

  /**
   * @brief constructor
   *
   * @param forcePrimary if true, this will be treated as the primary instance even
   *                     if another instance is running. This is used after an update since
   *                     the other instance is assumed to be in the process of quitting
   * @param parent parent object
   * @todo the forcePrimary parameter makes no sense. The second instance after an update
   *       needs to delete the files from before the update so the first instance needs to quit
   *       first anyway
   **/
  explicit SingleInstance(bool forcePrimary, QObject *parent = 0);

  /**
   * @return true if this is the primary instance (the one that gets to display a UI)
   **/
  bool primaryInstance() const { return m_PrimaryInstance; }

  /**
   * send a message to the primary instance. This can be used to transmit download urls
   *
   * @param message message to send
   **/
  void sendMessage(const QString &message);

signals:

  /**
   * @brief emitted when a secondary instance has sent a message (to us)
   *
   * @param message the message we received
   **/
  void messageSent(const QString &message);

public slots:

private slots:

  void receiveMessage();

private:

  bool m_PrimaryInstance;
  QSharedMemory m_SharedMem;
  QLocalServer m_Server;

};

#endif // SINGLEINSTANCE_H
