#include "multiprocess.h"
#include "utility.h"
#include <QLocalSocket>
#include <QThread>
#include <log.h>
#include <report.h>

static const char s_Key[]  = "mo-43d1a3ad-eeb0-4818-97c9-eda5216c29b5";
static const int s_Timeout = 5000;
static const int s_DeliverTimeout = 30000;

using MOBase::reportError;

MessageReceiver::MessageReceiver(QString key) : m_Key(std::move(key)), m_Server(nullptr)
{}

void MessageReceiver::start()
{
  m_Server = new QLocalServer(this);
  connect(m_Server, &QLocalServer::newConnection, this,
          &MessageReceiver::onNewConnection);
  // has to be called before listen
  m_Server->setSocketOptions(QLocalServer::WorldAccessOption);

  if (!m_Server->listen(s_Key)) {
    MOBase::log::error("failed to listen for secondary processes: {}",
                       m_Server->errorString().toStdString());
  }
}

void MessageReceiver::onNewConnection()
{
  while (QLocalSocket* socket = m_Server->nextPendingConnection()) {
    connect(socket, &QLocalSocket::readyRead, this,
            [this, socket]() { readFrom(socket); });
    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);

    readFrom(socket);
  }
}

void MessageReceiver::readFrom(QLocalSocket* socket)
{
  const QByteArray data = socket->readAll();
  if (data.isEmpty()) {
    return;
  }

  emit messageReceived(QString::fromUtf8(data));

  socket->disconnectFromServer();
}

MOMultiProcess::MOMultiProcess(bool allowMultiple, QObject* parent)
    : QObject(parent), m_Ephemeral(false), m_OwnsSM(false), m_ServerThread(nullptr),
      m_Receiver(nullptr)
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
    m_ServerThread = new QThread(this);
    m_ServerThread->setObjectName("mo-ipc-server");

    m_Receiver = new MessageReceiver(s_Key);
    m_Receiver->moveToThread(m_ServerThread);

    connect(m_ServerThread, &QThread::started, m_Receiver, &MessageReceiver::start);
    connect(m_ServerThread, &QThread::finished, m_Receiver, &QObject::deleteLater);

    connect(m_Receiver, &MessageReceiver::messageReceived, this,
            &MOMultiProcess::messageSent, Qt::QueuedConnection);

    m_ServerThread->start();
  }
}

MOMultiProcess::~MOMultiProcess()
{
  if (m_ServerThread) {
    m_ServerThread->quit();
    m_ServerThread->wait();
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
    socket.connectToServer(s_Key, QIODevice::ReadWrite);
    connected = socket.waitForConnected(s_Timeout);
  }

  if (!connected) {
    reportError(
        tr("failed to connect to running process: %1").arg(socket.errorString()));
    return;
  }

  socket.write(message.toUtf8());
  if (!socket.waitForBytesWritten(s_Timeout)) {
    if (socket.bytesToWrite()) {
      reportError(tr("failed to communicate with running process: %1")
                      .arg(socket.errorString()));
    }
  }

  socket.waitForDisconnected(s_DeliverTimeout);
}
