#ifndef MODORGANIZER_MOMULTIPROCESS_INCLUDED
#define MODORGANIZER_MOMULTIPROCESS_INCLUDED

#include <QLocalServer>
#include <QObject>
#include <QSharedMemory>

/**
 * used to ensure only a single process of Mod Organizer is started and to
 * allow ephemeral processes to send messages to the primary (visible) one.
 * This way, other processes can start a download in the primary one
 **/
class MOMultiProcess : public QObject
{
  Q_OBJECT

public:
  // `allowMultiple`: if another process is running, run this one
  // disconnected from the shared memory
  explicit MOMultiProcess(bool allowMultiple, QObject* parent = 0);

  /**
   * @return true if this process's job is to forward data to the primary
   *              process through shared memory
   **/
  bool ephemeral() const { return m_Ephemeral; }

  // returns true if this is not the primary process, but was allowed because
  // of the AllowMultiple flag
  //
  bool secondary() const { return !m_Ephemeral && !m_OwnsSM; }

  /**
   * send a message to the primary process. This can be used to transmit download urls
   *
   * @param message message to send
   **/
  void sendMessage(const QString& message);

signals:

  /**
   * @brief emitted when an ephemeral process has sent a message (to us)
   *
   * @param message the message we received
   **/
  void messageSent(const QString& message);

public slots:

private slots:

  void receiveMessage();

private:
  bool m_Ephemeral;
  bool m_OwnsSM;
  QSharedMemory m_SharedMem;
  QLocalServer m_Server;
};

#endif  // MODORGANIZER_MOMULTIPROCESS_INCLUDED
