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
#include <scopeguard.h>
#include <report.h>
#include <QMutexLocker>
#include <QFile>
#include <QIcon>
#include <QDateTime>
#include <Windows.h>

using MOBase::reportError;

QScopedPointer<LogBuffer> LogBuffer::s_Instance;
QMutex LogBuffer::s_Mutex;

LogBuffer::LogBuffer(int messageCount, QtMsgType minMsgType,
                     const QString &outputFileName)
  : QAbstractItemModel(nullptr)
  , m_OutFileName(outputFileName)
  , m_ShutDown(false)
  , m_MinMsgType(minMsgType)
  , m_NumMessages(0)
{
  m_Messages.resize(messageCount);
}

LogBuffer::~LogBuffer()
{
  qInstallMessageHandler(0);
  write();
}

void LogBuffer::logMessage(QtMsgType type, const QString &message)
{
  if (type >= m_MinMsgType) {
    Message msg = {type, QTime::currentTime(), message};
    if (m_NumMessages < m_Messages.size()) {
      beginInsertRows(QModelIndex(), m_NumMessages, m_NumMessages + 1);
    }
    m_Messages.at(m_NumMessages % m_Messages.size()) = msg;
    if (m_NumMessages < m_Messages.size()) {
      endInsertRows();
    } else {
      emit dataChanged(createIndex(0, 0), createIndex(m_Messages.size(), 0));
    }
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
    reportError(tr("failed to write log to %1: %2")
                    .arg(m_OutFileName)
                    .arg(file.errorString()));
    return;
  }

  unsigned int i = (m_NumMessages > m_Messages.size())
                       ? m_NumMessages - m_Messages.size()
                       : 0U;
  for (; i < m_NumMessages; ++i) {
    file.write(m_Messages.at(i % m_Messages.size()).toString().toUtf8());
    file.write("\r\n");
  }
  ::SetLastError(lastError);
}

void LogBuffer::init(int messageCount, QtMsgType minMsgType,
                     const QString &outputFileName)
{
  QMutexLocker guard(&s_Mutex);

  s_Instance.reset(new LogBuffer(messageCount, minMsgType, outputFileName));
  qInstallMessageHandler(LogBuffer::log);
}

char LogBuffer::msgTypeID(QtMsgType type)
{
  switch (type) {
    case QtDebugMsg:
      return 'D';
    case QtWarningMsg:
      return 'W';
    case QtCriticalMsg:
      return 'C';
    case QtFatalMsg:
      return 'F';
    default:
      return '?';
  }
}

void LogBuffer::log(QtMsgType type, const QMessageLogContext &context,
                    const QString &message)
{
  // QMutexLocker doesn't support timeout...
  if (!s_Mutex.tryLock(100)) {
    fprintf(stderr, "failed to log: %s", qPrintable(message));
    return;
  }
  ON_BLOCK_EXIT([]() { s_Mutex.unlock(); });

  if (!s_Instance.isNull()) {
    s_Instance->logMessage(type, message);
  }

  if (type == QtDebugMsg) {
    fprintf(stdout, "%s [%c] %s\n", qPrintable(QTime::currentTime().toString()),
            msgTypeID(type), qPrintable(message));
  } else {
    if (context.line != 0) {
      fprintf(stdout, "%s [%c] (%s:%u) %s\n",
              qPrintable(QTime::currentTime().toString()), msgTypeID(type),
              context.file, context.line, qPrintable(message));
    } else {
      fprintf(stdout, "%s [%c] %s\n",
              qPrintable(QTime::currentTime().toString()), msgTypeID(type),
              qPrintable(message));
    }
  }
  fflush(stdout);
}

QModelIndex LogBuffer::index(int row, int column, const QModelIndex &) const
{
  return createIndex(row, column, row);
}

QModelIndex LogBuffer::parent(const QModelIndex &) const
{
  return QModelIndex();
}

int LogBuffer::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid())
    return 0;
  else
    return std::min(m_NumMessages, m_Messages.size());
}

int LogBuffer::columnCount(const QModelIndex &) const
{
  return 2;
}

QVariant LogBuffer::data(const QModelIndex &index, int role) const
{
  unsigned offset = m_NumMessages < m_Messages.size()
                        ? 0
                        : m_NumMessages - m_Messages.size();
  unsigned int msgIndex = (offset + index.row() + 1) % m_Messages.size();
  switch (role) {
    case Qt::DisplayRole: {
      if (index.column() == 0) {
        return m_Messages[msgIndex].time;
      } else if (index.column() == 1) {
        const QString &msg = m_Messages[msgIndex].message;
        if (msg.length() < 200) {
          return msg;
        } else {
          return msg.mid(0, 200) + "...";
        }
      }
    } break;
    case Qt::DecorationRole: {
      if (index.column() == 1) {
        switch (m_Messages[msgIndex].type) {
          case QtDebugMsg:
            return QIcon(":/MO/gui/information");
          case QtWarningMsg:
            return QIcon(":/MO/gui/warning");
          case QtCriticalMsg:
            return QIcon(":/MO/gui/important");
          case QtFatalMsg:
            return QIcon(":/MO/gui/problem");
        }
      }
    } break;
    case Qt::UserRole: {
      if (index.column() == 1) {
        switch (m_Messages[msgIndex].type) {
          case QtDebugMsg:
            return "D";
          case QtWarningMsg:
            return "W";
          case QtCriticalMsg:
            return "C";
          case QtFatalMsg:
            return "F";
        }
      }
    } break;
  }
  return QVariant();
}

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

QString LogBuffer::Message::toString() const
{
  return QString("%1 [%2] %3")
      .arg(time.toString())
      .arg(msgTypeID(type))
      .arg(message);
}
