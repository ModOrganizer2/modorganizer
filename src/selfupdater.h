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

#ifndef SELFUPDATER_H
#define SELFUPDATER_H

#include <map>

class Archive;
class NexusInterface;
class PluginManager;
namespace MOBase
{
class IPluginGame;
}

#include <QFile>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QtGlobal>  //for qint64

class QNetworkReply;
class QProgressDialog;
class Settings;

#include <uibase/versioning.h>

#include "github.h"

/**
 * @brief manages updates for Mod Organizer itself
 * This class is used to update the Mod Organizer
 * The process looks like this:
 * 1. call testForUpdate() to determine is available
 * 2. if the updateAvailable() signal is received, allow the user to start the update
 * 3. if the user start the update, call startUpdate()
 * 4. startUpdate() will first query a list of files, try to determine if there is an
 *    incremental update. If not, the user will have to confirm the download of a full
 *download. Once the correct file is selected, it is downloaded.
 * 5. before the downloaded file is extracted, existing files that are going to be
 *replaced are moved to "update_backup" on because files that are currently open can't
 *be replaced.
 * 6. the update is extracted and then deleted
 * 7. finally, a restart is requested via signal.
 * 8. at restart, Mod Organizer will remove the update_backup directory since none of
 *the files should now be open
 *
 * @todo use NexusBridge
 **/
class SelfUpdater : public QObject
{

  Q_OBJECT

public:
  /**
   * @brief constructor
   *
   * @param nexusInterface interface to query information from nexus
   * @param parent parent widget
   * @todo passing the nexus interface is unneccessary
   **/
  explicit SelfUpdater(NexusInterface* nexusInterface);

  virtual ~SelfUpdater();

  void setUserInterface(QWidget* widget);

  void setPluginManager(PluginManager* pluginManager);

  /**
   * @brief request information about the current version
   **/
  void testForUpdate(const Settings& settings);

  /**
   * @brief start the update process
   * @note this should not be called if there is no update available
   **/
  void startUpdate();

  /**
   * @return current version of Mod Organizer
   **/
  MOBase::Version getVersion() const { return m_MOVersion; }

signals:

  /**
   * @brief emitted if a restart of the client is necessary to complete the update
   **/
  void restart();

  /**
   * @brief emitted if an update is available
   **/
  void updateAvailable();

  /**
   * @brief emitted if a message of the day was received
   **/
  void motdAvailable(const QString& motd);

private:
  void openOutputFile(const QString& fileName);
  void download(const QString& downloadLink);
  void installUpdate();
  void report7ZipError(const QString& errorMessage);
  void showProgress();
  void closeProgress();

private slots:

  void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void downloadReadyRead();
  void downloadFinished();
  void downloadCancel();

private:
  QWidget* m_Parent;
  MOBase::Version m_MOVersion;
  NexusInterface* m_Interface;
  QFile m_UpdateFile;
  QNetworkReply* m_Reply;
  QProgressDialog* m_Progress{nullptr};
  bool m_Canceled;
  int m_Attempts;

  GitHub m_GitHub;

  // Map from version to release, in decreasing order (first element is the latest
  // release):
  using CandidatesMap = std::map<MOBase::Version, QJsonObject, std::greater<>>;
  CandidatesMap m_UpdateCandidates;
};

#endif  // SELFUPDATER_H
