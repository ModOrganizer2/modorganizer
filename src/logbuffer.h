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

#ifndef LOGBUFFER_H
#define LOGBUFFER_H

#include <QObject>
#include <QMutex>
#include <QScopedPointer>
#include <vector>


class LogBuffer : public QObject
{
  Q_OBJECT

public:

  static void init(int messageCount, QtMsgType minMsgType, const QString &outputFileName);
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  static void log(QtMsgType type, const QMessageLogContext &context, const QString &message);
#else
  static void log(QtMsgType type, const char *message);
#endif

  static void writeNow();
  static void cleanQuit();

public:

  virtual ~LogBuffer();

  void logMessage(QtMsgType type, const QString &message);

signals:

public slots:

private:

  explicit LogBuffer(int messageCount, QtMsgType minMsgType, const QString &outputFileName);
  LogBuffer(const LogBuffer &reference); // not implemented
  LogBuffer &operator=(const LogBuffer &reference); // not implemented

  void write() const;

private:

  static QScopedPointer<LogBuffer> s_Instance;
  static QMutex s_Mutex;

  QString m_OutFileName;
  bool m_ShutDown;
  QtMsgType m_MinMsgType;
  unsigned int m_NumMessages;
  std::vector<QString> m_Messages;
  
};

#endif // LOGBUFFER_H
