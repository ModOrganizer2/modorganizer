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
 * allow ephemeral instances to send messages to the primary (visible) one.
 * This way, other instances can start a download in the primary one
 **/
class SingleInstance : public QObject
{

  Q_OBJECT

public:
  enum Flag
  {
    NoFlags      = 0x00,


    // when set, this will be treated as the primary instance even if
    // another instance is running. This is used after an update since the
    // other instance is assumed to be in the process of quitting
    //
    // todo: this makes no sense. The second instance after an update needs
    // to delete the files from before the update so the first instance
    // needs to quit first anyway
    ForcePrimary = 0x01,

    // if another instance is running, run this one disconnected from the
    // shared memory
    AllowMultiple = 0x02
  };

	using Flags = QFlags<Flag>;


  explicit SingleInstance(Flags flags, QObject *parent = 0);

  /**
   * @return true if this instance's job is to forward data to the primary
   *              instance through shared memory
   **/
  bool ephemeral() const
  {
    return m_Ephemeral;
  }

  // returns true if this is not the primary instance, but was allowed because
  // of the AllowMultiple flag
  //
  bool secondary() const
  {
    return !m_Ephemeral && !m_OwnsSM;
  }

  /**
   * send a message to the primary instance. This can be used to transmit download urls
   *
   * @param message message to send
   **/
  void sendMessage(const QString &message);

signals:

  /**
   * @brief emitted when an ephemeral instance has sent a message (to us)
   *
   * @param message the message we received
   **/
  void messageSent(const QString &message);

public slots:

private slots:

  void receiveMessage();

private:
  bool m_Ephemeral;
  bool m_OwnsSM;
  QSharedMemory m_SharedMem;
  QLocalServer m_Server;

};

Q_DECLARE_OPERATORS_FOR_FLAGS(SingleInstance::Flags);

#endif // SINGLEINSTANCE_H
