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

#ifndef DOWNLOADLISTITEM_H
#define DOWNLOADLISTITEM_H

#include "downloadmanager.h"
#include <QtCore>

/**
 * @brief model of the list of active and pending downloads
 **/
struct DownloadListItem
{
  DownloadListItem()                                   = default;
  ~DownloadListItem()                                  = default;
  DownloadListItem(const DownloadListItem&)            = default;
  DownloadListItem& operator=(const DownloadListItem&) = default;

  int pendingIndex{-1};
  bool isPending{false};
  bool showInfoIncompleteWarning{false};
  QUuid moId;
  DownloadManager::DownloadState state;
  QString fileName;
  QString name;
  QString status;
  QString size;
  QDateTime fileTime;
  QString modName;
  QString version;
  QString modId;
  QString game;
  QString tooltip;
};

Q_DECLARE_METATYPE(DownloadListItem)

#endif  // DOWNLOADLISTITEM_H
