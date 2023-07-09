#include "multiprocess.h"
#include "utility.h"
#include <QLocalSocket>
#include <log.h>
#include <report.h>

static const char s_Key[]  = "mo-43d1a3ad-eeb0-4818-97c9-eda5216c29b5";
static const int s_Timeout = 5000;

using MOBase::reportError;

MOMultiProcess::MOMultiProcess(bool allowMultiple, QObject* parent)
    : QObject(parent), m_Ephemeral(false), m_OwnsSM(false)
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
    connect(&m_Server, SIGNAL(newConnection()), this, SLOT(receiveMessage()),
            Qt::QueuedConnection);
    // has to be called before listen
    m_Server.setSocketOptions(QLocalServer::WorldAccessOption);
    m_Server.listen(s_Key);
  }
}

void MOMultiProcess::sendMessage(const QString& message)
{
  if (m_OwnsSM) {
    // nobody there to receive the message
    return;
  }
  QLocalSocket socket(this);

  bool connected = false;
  for (int i = 0; i < 2 && !connected; ++i) {
    if (i > 0) {
      Sleep(250);
    }

    // other process may be just starting up
    socket.connectToServer(s_Key, QIODevice::WriteOnly);
    connected = socket.waitForConnected(s_Timeout);
  }

  if (!connected) {
    reportError(
        tr("failed to connect to running process: %1").arg(socket.errorString()));
    return;
  }

  socket.write(message.toUtf8());
  if (!socket.waitForBytesWritten(s_Timeout)) {
    reportError(
        tr("failed to communicate with running process: %1").arg(socket.errorString()));
    return;
  }

  socket.disconnectFromServer();
  socket.waitForDisconnected();
}

void MOMultiProcess::receiveMessage()
{
  QLocalSocket* socket = m_Server.nextPendingConnection();
  if (!socket) {
    return;
  }

  if (!socket->waitForReadyRead(s_Timeout)) {
    // check if there are bytes available; if so, it probably means the data was
    // already received by the time waitForReadyRead() was called and the
    // connection has been closed
    const auto av = socket->bytesAvailable();

    if (av <= 0) {
      MOBase::log::error("failed to receive data from secondary process: {}",
                         socket->errorString());

      reportError(tr("failed to receive data from secondary process: %1")
                      .arg(socket->errorString()));
      return;
    }
  }

  QString message = QString::fromUtf8(socket->readAll().constData());
  emit messageSent(message);
  socket->disconnectFromServer();
}
