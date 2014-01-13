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

#include "logbuffer.h"
#include "report.h"
#include <QMutexLocker>
#include <QFile>
#include <Windows.h>

QScopedPointer<LogBuffer> LogBuffer::s_Instance;
QMutex LogBuffer::s_Mutex;


LogBuffer::LogBuffer(int messageCount, QtMsgType minMsgType, const QString &outputFileName)
  : QObject(NULL), m_OutFileName(outputFileName), m_ShutDown(false),
    m_MinMsgType(minMsgType), m_NumMessages(0)
{
  m_Messages.resize(messageCount);
}

LogBuffer::~LogBuffer()
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  qInstallMessageHandler(0);
#else
  qInstallMsgHandler(0);
#endif
//  if (!m_ShutDown) {
    write();
//  }
}


void LogBuffer::logMessage(QtMsgType type, const QString &message)
{
  if (type >= m_MinMsgType) {
    m_Messages.at(m_NumMessages % m_Messages.size()) = message;
    ++m_NumMessages;
    if (type >= QtCriticalMsg) {
      write();
    }
  }
}


void LogBuffer::write() const
{
  if (m_NumMessages == 0) {
    return;
  }

  DWORD lastError = ::GetLastError();

  QFile file(m_OutFileName);
  if (!file.open(QIODevice::WriteOnly)) {
    reportError(tr("failed to write log to %1: %2").arg(m_OutFileName).arg(file.errorString()));
    return;
  }

  unsigned int i = (m_NumMessages > m_Messages.size()) ? m_NumMessages - m_Messages.size()
                                                       : 0U;
  for (; i < m_NumMessages; ++i) {
    file.write(m_Messages.at(i % m_Messages.size()).toUtf8());
    file.write("\r\n");
  }
  ::SetLastError(lastError);
}


void LogBuffer::init(int messageCount, QtMsgType minMsgType, const QString &outputFileName)
{
  QMutexLocker guard(&s_Mutex);

  if (!s_Instance.isNull()) {
    s_Instance.reset();
  }
  s_Instance.reset(new LogBuffer(messageCount, minMsgType, outputFileName));
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  qInstallMessageHandler(LogBuffer::log);
#else
  qInstallMsgHandler(LogBuffer::log);
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)

void LogBuffer::log(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
  QMutexLocker guard(&s_Mutex);
  if (!s_Instance.isNull()) {
    s_Instance->logMessage(type, message);
  }
  fprintf(stdout, "(%s:%u) %s\n", context.file, context.line, qPrintable(message));
  fflush(stdout);
}

#else


char LogBuffer::msgTypeID(QtMsgType type)
{
  switch (type) {
    case QtDebugMsg: return 'D';
    case QtWarningMsg: return 'W';
    case QtCriticalMsg: return 'C';
    case QtFatalMsg: return 'F';
  }
}

#include <QDateTime>

void LogBuffer::log(QtMsgType type, const char *message)
{
  QMutexLocker guard(&s_Mutex);
  if (!s_Instance.isNull()) {
    s_Instance->logMessage(type, message);
  }
  fprintf(stdout, "%s [%c] %s\n", qPrintable(QTime::currentTime().toString()), msgTypeID(type), message);
  fflush(stdout);
}

#endif

void LogBuffer::writeNow()
{
  QMutexLocker guard(&s_Mutex);
  if (!s_Instance.isNull()) {
    s_Instance->write();
  }
}


void LogBuffer::cleanQuit()
{
  QMutexLocker guard(&s_Mutex);
  if (!s_Instance.isNull()) {
    s_Instance->m_ShutDown = true;
  }
}

void log(const char *format, ...)
{
  va_list argList;
  va_start(argList, format);

  static const int BUFFERSIZE = 1000;

  char buffer[BUFFERSIZE + 1];
  buffer[BUFFERSIZE] = '\0';

  vsnprintf(buffer, BUFFERSIZE, format, argList);

  qCritical("%s", buffer);

  va_end(argList);
}

