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

#include "singleinstance.h"
#include "utility.h"
#include <report.h>
#include <log.h>
#include <QLocalSocket>

static const char s_Key[] = "mo-43d1a3ad-eeb0-4818-97c9-eda5216c29b5";
static const int s_Timeout = 5000;

using MOBase::reportError;

SingleInstance::SingleInstance(bool allowMultiple, QObject *parent) :
  QObject(parent), m_Ephemeral(false), m_OwnsSM(false)
{
  m_SharedMem.setKey(s_Key);

  if (!m_SharedMem.create(1)) {
    if (m_SharedMem.error() == QSharedMemory::AlreadyExists) {
      if (!allowMultiple) {
        m_SharedMem.attach();
        m_Ephemeral = true;
      }
    }

    if ((m_SharedMem.error() != QSharedMemory::NoError) &&
        (m_SharedMem.error() != QSharedMemory::AlreadyExists)) {
      throw MOBase::MyException(tr("SHM error: %1").arg(m_SharedMem.errorString()));
    }
  } else {
    m_OwnsSM = true;
  }

  if (m_OwnsSM) {
    connect(&m_Server, SIGNAL(newConnection()), this, SLOT(receiveMessage()), Qt::QueuedConnection);
    // has to be called before listen
    m_Server.setSocketOptions(QLocalServer::WorldAccessOption);
    m_Server.listen(s_Key);
  }
}


void SingleInstance::sendMessage(const QString &message)
{
  if (m_OwnsSM) {
    // nobody there to receive the message
    return;
  }
  QLocalSocket socket(this);

  bool connected = false;
  for(int i = 0; i < 2 && !connected; ++i) {
    if (i > 0) {
      Sleep(250);
    }

    // other instance may be just starting up
    socket.connectToServer(s_Key, QIODevice::WriteOnly);
    connected = socket.waitForConnected(s_Timeout);
  }

  if (!connected) {
    reportError(tr("failed to connect to running instance: %1").arg(socket.errorString()));
    return;
  }

  socket.write(message.toUtf8());
  if (!socket.waitForBytesWritten(s_Timeout)) {
    reportError(tr("failed to communicate with running instance: %1").arg(socket.errorString()));
    return;
  }

  socket.disconnectFromServer();
  socket.waitForDisconnected();
}


void SingleInstance::receiveMessage()
{
  QLocalSocket *socket = m_Server.nextPendingConnection();
  if (!socket) {
    return;
  }

  if (!socket->waitForReadyRead(s_Timeout)) {
    // check if there are bytes available; if so, it probably means the data was
    // already received by the time waitForReadyRead() was called and the
    // connection has been closed
    const auto av = socket->bytesAvailable();

    if (av <= 0) {
      MOBase::log::error(
        "failed to receive data from secondary instance: {}",
        socket->errorString());

      reportError(tr("failed to receive data from secondary instance: %1").arg(socket->errorString()));
      return;
    }
  }

  QString message = QString::fromUtf8(socket->readAll().constData());
  emit messageSent(message);
  socket->disconnectFromServer();
}
