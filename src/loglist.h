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

#include "copyeventfilter.h"
#include "shared/appconfig.h"
#include <QTreeView>
#include <deque>
#include <log.h>

class OrganizerCore;

class LogModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  static void create();
  static LogModel& instance();

  void add(MOBase::log::Entry e);
  void clear();

  const std::deque<MOBase::log::Entry>& entries() const;

  QString formattedMessage(const QModelIndex& index) const;

protected:
  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  QVariant headerData(int section, Qt::Orientation ori,
                      int role = Qt::DisplayRole) const override;

private:
  std::deque<MOBase::log::Entry> m_entries;

  LogModel();
  void onEntryAdded(MOBase::log::Entry e);
};

class LogList : public QTreeView
{
  Q_OBJECT;

public:
  LogList(QWidget* parent = nullptr);

  void setCore(OrganizerCore& core);

  void copyToClipboard();
  void clear();
  void openLogsFolder();

  QMenu* createMenu(QWidget* parent = nullptr);

private:
  OrganizerCore* m_core;
  QTimer m_timer;
  CopyEventFilter m_copyFilter;

  void onContextMenu(const QPoint& pos);
  void onNewEntry();
};

void logToStdout(bool b);
void initLogging();
bool setLogDirectory(const QString& dir);

#endif  // LOGBUFFER_H
