/*
Copyright (C) 2015 Sebastian Herbord. All rights reserved.

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


#ifndef USVFSCONNECTOR_H
#define USVFSCONNECTOR_H


#include <filemapping.h>
#include <QString>
#include <QThread>
#include <QFile>
#include <QDebug>
#include <usvfsparameters.h>


class LogWorker : public QThread {

  Q_OBJECT

public:

  LogWorker();
  ~LogWorker();

public slots:

  void process();
  void exit();

signals:

  void outputLog(const QString &message);
  void finished();

private:

  std::string m_Buffer;
  bool m_QuitRequested;
  QFile m_LogFile;

};


class UsvfsConnector : public QObject {

  Q_OBJECT

public:

  UsvfsConnector();
  ~UsvfsConnector();

  void updateMapping(const MappingType &mapping);
  void updateParams(int logLevel, int crashDumpsType);

private:

  LogWorker m_LogWorker;
  QThread m_WorkerThread;

};

CrashDumpsType crashDumpsType(int type);

#endif // USVFSCONNECTOR_H
