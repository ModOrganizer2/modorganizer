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

#ifndef NEXUSINTERFACE_H
#define NEXUSINTERFACE_H

#include "apiuseraccount.h"
#include "pluginmanager.h"

#include <imodrepositorybridge.h>
#include <utility.h>
#include <versioninfo.h>

#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QQueue>
#include <QTimer>
#include <QVariant>

#include <list>
#include <set>

namespace MOBase
{
class IPluginGame;
}

class NexusInterface;
class NXMAccessManager;
class Settings;

/**
 * @brief convenience class to make nxm requests easier
 * usually, all objects that started a nxm request will be signaled if one finished.
 * Therefore, the objects need to store the id of the requests they started and then
 *filter the result. NexusBridge does this automatically. Users connect to the signals
 *of NexusBridge they intend to handle and only receive the signals the caused
 **/
class NexusBridge : public MOBase::IModRepositoryBridge
{

  Q_OBJECT

public:
  NexusBridge(const QString& subModule = "");

  /**
   * @brief request description for a mod
   *
   * @param modID id of the mod caller is interested in
   * @param userData user data to be returned with the result
   * @param url the url to request from
   **/
  virtual void requestDescription(QString gameName, int modID, QVariant userData);

  /**
   * @brief request a list of the files belonging to a mod
   *
   * @param modID id of the mod caller is interested in
   * @param userData user data to be returned with the result
   **/
  virtual void requestFiles(QString gameName, int modID, QVariant userData);

  /**
   * @brief request info about a single file of a mod
   *
   * @param modID id of the mod caller is interested in
   * @param fileID id of the file the caller is interested in
   * @param userData user data to be returned with the result
   **/
  virtual void requestFileInfo(QString gameName, int modID, int fileID,
                               QVariant userData);

  /**
   * @brief request the download url of a file
   *
   * @param modID id of the mod caller is interested in
   * @param fileID id of the file the caller is interested in
   * @param userData user data to be returned with the result
   **/
  virtual void requestDownloadURL(QString gameName, int modID, int fileID,
                                  QVariant userData);

  /**
   * @brief requestToggleEndorsement
   * @param modID id of the mod caller is interested in
   * @param userData user data to be returned with the result
   */
  virtual void requestToggleEndorsement(QString gameName, int modID, QString modVersion,
                                        bool endorse, QVariant userData);

  /**
   * @brief requestToggleTracking
   * @param modID id of the mod caller is interested in
   * @param userData user data to be returned with the result
   */
  virtual void requestToggleTracking(QString gameName, int modID, bool track,
                                     QVariant userData);

  /**
   * @brief requestGameInfo
   * @param userData user data to be returned with the result
   */
  virtual void requestGameInfo(QString gameName, QVariant userData);

public slots:

  void nxmDescriptionAvailable(QString gameName, int modID, QVariant userData,
                               QVariant resultData, int requestID);
  void nxmFilesAvailable(QString gameName, int modID, QVariant userData,
                         QVariant resultData, int requestID);
  void nxmFileInfoAvailable(QString gameName, int modID, int fileID, QVariant userData,
                            QVariant resultData, int requestID);
  void nxmDownloadURLsAvailable(QString gameName, int modID, int fileID,
                                QVariant userData, QVariant resultData, int requestID);
  void nxmEndorsementsAvailable(QVariant userData, QVariant resultData, int requestID);
  void nxmEndorsementToggled(QString gameName, int modID, QVariant userData,
                             QVariant resultData, int requestID);
  void nxmTrackedModsAvailable(QVariant userData, QVariant resultData, int requestID);
  void nxmTrackingToggled(QString gameName, int modID, QVariant userData, bool tracked,
                          int requestID);
  void nxmGameInfoAvailable(QString gameName, QVariant userData, QVariant resultData,
                            int requestID);
  void nxmRequestFailed(QString gameName, int modID, int fileID, QVariant userData,
                        int requestID, int errorCode, const QString& errorMessage);

private:
  NexusInterface* m_Interface;
  QString m_SubModule;
  std::set<int> m_RequestIDs;
};

/**
 * @brief Makes asynchronous requests to the nexus API
 *
 * This class can be used to make asynchronous requests to the Nexus API.
 * Currently, responses are sent to all receivers that have sent a request of the
 *relevant type, so the recipient has to filter the response by the id returned when
 *making the request
 **/
class NexusInterface : public QObject
{
  Q_OBJECT

public:
  enum UpdatePeriod
  {
    NONE,
    DAY,
    WEEK,
    MONTH
  };

  // Nexus file category IDs (MAIN, OLD etc), not to be confused with mod categories
  // (Armors, Texture etc).
  enum FileStatus
  {
    MAIN          = 1,
    UPDATE        = 2,
    OPTIONAL_FILE = 3,  // actual string version is "OPTIONAL", but that is already
                        // defined as a macro in minwindef.h
    OLD_VERSION     = 4,
    MISCELLANEOUS   = 5,
    REMOVED         = 6,
    ARCHIVED        = 7,
    ARCHIVED_HIDDEN = 1000  // Archived files can be hidden by authors so if they aren't
                            // listed we can assume they were hidden.
  };

public:
  static APILimits defaultAPILimits();
  static APILimits parseLimits(const QNetworkReply* reply);
  static APILimits parseLimits(const QList<QNetworkReply::RawHeaderPair>& headers);

  NexusInterface(Settings* s);
  ~NexusInterface();

  static NexusInterface& instance();

  /**
   * @return the access manager object used to connect to nexus
   **/
  NXMAccessManager* getAccessManager();

  /**
   * @brief cleanup this interface. this is destructive, afterwards it can't be used
   * again
   */
  void cleanup();

  /**
   * @brief clear webcache and cookies associated with this access manager
   */
  void clearCache();

  /**
   * @brief request description for a mod
   *
   * @param gameName the game short name to support multiple game sources
   * @param modID id of the mod caller is interested in (assumed to be for the current
   *game)
   * @param receiver the object to receive the result asynchronously via a signal
   *(nxmDescriptionAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   **/
  int requestDescription(QString gameName, int modID, QObject* receiver,
                         QVariant userData, const QString& subModule)
  {
    return requestDescription(gameName, modID, receiver, userData, subModule,
                              getGame(gameName));
  }

  /**
   * @brief request description for a mod
   *
   * @param gameName the game short name to support multiple game sources
   * @param modID id of the mod caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal
   *(nxmDescriptionAvailable)
   * @param userData user data to be returned with the result
   * @param game Game with which the mod is associated
   * @return int an id to identify the request
   **/
  int requestDescription(QString gameName, int modID, QObject* receiver,
                         QVariant userData, const QString& subModule,
                         MOBase::IPluginGame const* game);

  /**
   * @brief request description for a mod
   *
   * @param gameName the game short name to support multiple game sources
   * @param modID id of the mod caller is interested in (assumed to be for the current
   *game)
   * @param receiver the object to receive the result asynchronously via a signal
   *(nxmModInfoAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   **/
  int requestModInfo(QString gameName, int modID, QObject* receiver, QVariant userData,
                     const QString& subModule)
  {
    return requestModInfo(gameName, modID, receiver, userData, subModule,
                          getGame(gameName));
  }

  /**
   * @brief request mod info
   *
   * @param gameName the game short name to support multiple game sources
   * @param modID id of the mod caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal
   *(nxmModInfoAvailable)
   * @param userData user data to be returned with the result
   * @param game Game with which the mod is associated
   * @return int an id to identify the request
   **/
  int requestModInfo(QString gameName, int modID, QObject* receiver, QVariant userData,
                     const QString& subModule, MOBase::IPluginGame const* game);

  int requestUpdateInfo(QString gameName, NexusInterface::UpdatePeriod period,
                        QObject* receiver, QVariant userData, const QString& subModule)
  {
    return requestUpdateInfo(gameName, period, receiver, userData, subModule,
                             getGame(gameName));
  }

  int requestUpdateInfo(QString gameName, NexusInterface::UpdatePeriod period,
                        QObject* receiver, QVariant userData, const QString& subModule,
                        const MOBase::IPluginGame* game);

  /**
   * @brief request nexus descriptions for multiple mods at once
   * @param modID id of the mod the caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal
   * (nxmDescriptionAvailable)
   * @param userData user data to be returned with the result
   * @param gameName the game with which the mods are associated
   * @return int an id to identify the request
   */
  int requestUpdates(const int& modID, QObject* receiver, QVariant userData,
                     QString gameName, const QString& subModule);

  /**
   * @brief request a list of the files belonging to a mod
   *
   * @param gameName the game short name to support multiple game sources
   * @param modID id of the mod caller is interested in (assumed to be for the current
   *game)
   * @param receiver the object to receive the result asynchronously via a signal
   *(nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   **/
  int requestFiles(QString gameName, int modID, QObject* receiver, QVariant userData,
                   const QString& subModule)
  {
    return requestFiles(gameName, modID, receiver, userData, subModule,
                        getGame(gameName));
  }

  /**
   * @brief request a list of the files belonging to a mod
   *
   * @param gameName the game short name to support multiple game sources
   * @param modID id of the mod caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal
   *(nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   **/
  int requestFiles(QString gameName, int modID, QObject* receiver, QVariant userData,
                   const QString& subModule, MOBase::IPluginGame const* game);

  /**
   * @brief request info about a single file of a mod
   *
   * @param gameName name of the game short name to request the download from
   * @param modID id of the mod caller is interested in (assumed to be for the current
   *game)
   * @param fileID id of the file the caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal
   *(nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   **/
  int requestFileInfo(QString gameName, int modID, int fileID, QObject* receiver,
                      QVariant userData, const QString& subModule);

  /**
   * @brief request the download url of a file
   *
   * @param gameName the game short name to support multiple game sources
   * @param modID id of the mod caller is interested in (assumed to be for the current
   *game)
   * @param fileID id of the file the caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal
   *(nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   **/
  int requestDownloadURL(QString gameName, int modID, int fileID, QObject* receiver,
                         QVariant userData, const QString& subModule)
  {
    return requestDownloadURL(gameName, modID, fileID, receiver, userData, subModule,
                              getGame(gameName));
  }

  /**
   * @brief request the download url of a file
   *
   * @param gameName the game short name to support multiple game sources
   * @param modID id of the mod caller is interested in
   * @param fileID id of the file the caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal
   *(nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   **/
  int requestDownloadURL(QString gameName, int modID, int fileID, QObject* receiver,
                         QVariant userData, const QString& subModule,
                         MOBase::IPluginGame const* game);

  int requestEndorsementInfo(QObject* receiver, QVariant userData,
                             const QString& subModule);

  /**
   * @param gameName the game short name to support multiple game sources
   * @brief toggle endorsement state of the mod
   * @param modID id of the mod (assumed to be for the current game)
   * @param endorse true if the mod should be endorsed, false for un-endorse
   * @param receiver the object to receive the result asynchronously via a signal
   * (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   */
  int requestToggleEndorsement(QString gameName, int modID, QString modVersion,
                               bool endorse, QObject* receiver, QVariant userData,
                               const QString& subModule)
  {
    return requestToggleEndorsement(gameName, modID, modVersion, endorse, receiver,
                                    userData, subModule, getGame(gameName));
  }

  /**
   * @param gameName the game short name to support multiple game sources
   * @brief toggle endorsement state of the mod
   * @param modID id of the mod
   * @param endorse true if the mod should be endorsed, false for un-endorse
   * @param receiver the object to receive the result asynchronously via a signal
   * (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   */
  int requestToggleEndorsement(QString gameName, int modID, QString modVersion,
                               bool endorse, QObject* receiver, QVariant userData,
                               const QString& subModule,
                               MOBase::IPluginGame const* game);

  int requestTrackingInfo(QObject* receiver, QVariant userData,
                          const QString& subModule);

  /**
   * @param gameName the game short name to support multiple game sources
   * @brief toggle tracking state of the mod
   * @param modID id of the mod
   * @param track true if the mod should be tracked, false for not tracked
   * @param receiver the object to receive the result asynchronously via a signal
   * (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   */
  int requestToggleTracking(QString gameName, int modID, bool track, QObject* receiver,
                            QVariant userData, const QString& subModule)
  {
    return requestToggleTracking(gameName, modID, track, receiver, userData, subModule,
                                 getGame(gameName));
  }

  /**
   * @param gameName the game short name to support multiple game sources
   * @brief toggle tracking state of the mod
   * @param modID id of the mod
   * @param track true if the mod should be tracked, false for not tracked
   * @param receiver the object to receive the result asynchronously via a signal
   * (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   */
  int requestToggleTracking(QString gameName, int modID, bool track, QObject* receiver,
                            QVariant userData, const QString& subModule,
                            MOBase::IPluginGame const* game);

  /**
   * @param gameName the game short name to support multiple game sources
   * @brief toggle tracking state of the mod
   * @param modID id of the mod
   * @param track true if the mod should be tracked, false for not tracked
   * @param receiver the object to receive the result asynchronously via a signal
   * (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   */
  int requestGameInfo(QString gameName, QObject* receiver, QVariant userData,
                      const QString& subModule)
  {
    return requestGameInfo(gameName, receiver, userData, subModule, getGame(gameName));
  }

  /**
   * @param gameName the game short name to support multiple game sources
   * @brief toggle tracking state of the mod
   * @param modID id of the mod
   * @param track true if the mod should be tracked, false for not tracked
   * @param receiver the object to receive the result asynchronously via a signal
   * (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   */
  int requestGameInfo(QString gameName, QObject* receiver, QVariant userData,
                      const QString& subModule, MOBase::IPluginGame const* game);

  /**
   *
   */
  int requestInfoFromMd5(QString gameName, QByteArray& hash, QObject* receiver,
                         QVariant userData, const QString& subModule)
  {
    return requestInfoFromMd5(gameName, hash, receiver, userData, subModule,
                              getGame(gameName));
  }

  /**
   *
   */
  int requestInfoFromMd5(QString gameName, QByteArray& hash, QObject* receiver,
                         QVariant userData, const QString& subModule,
                         MOBase::IPluginGame const* game);

  /**
   * @param directory the directory to store cache files
   **/
  void setCacheDirectory(const QString& directory);

  /**
   * @brief called when the log-in completes. This was, requests waiting for the log-in
   * can be run
   */
  void loginCompleted();

  std::vector<std::pair<QString, QString>>
  getGameChoices(const MOBase::IPluginGame* game);

  APIUserAccount getAPIUserAccount() const;
  APIStats getAPIStats() const;

public:
  /**
   * @brief guess the mod id from a filename as delivered by Nexus
   * @param fileName name of the file
   * @return the guessed mod id
   * @note this currently doesn't fit well with the remaining interface but this is the
   * best place for the function
   */
  static void interpretNexusFileName(const QString& fileName, QString& modName,
                                     int& modID, bool query);

  /**
   * @brief get the currently managed game
   */
  MOBase::IPluginGame const* managedGame() const;

  /**
   * @brief see if the passed URL is related to the current game
   *
   * Arguably, this should optionally take a gameplugin pointer
   */
  bool isURLGameRelated(QUrl const& url) const;

  /**
   * @brief Get the nexus page for the current game
   *
   * Arguably, this should optionally take a gameplugin pointer
   */
  QString getGameURL(QString gameName) const;

  /**
   * @brief Get the URL for the mod web page
   * @param modID
   */
  QString getModURL(int modID, QString gameName) const;

  /**
   * @brief Checks if the specified URL might correspond to a nexus mod
   * @param modID
   * @param url
   * @return
   */
  bool isModURL(int modID, QString const& url) const;

  void setPluginManager(PluginManager* pluginManager);

signals:

  void requestNXMDownload(const QString& url);

  void needLogin();

  void nxmDescriptionAvailable(QString gameName, int modID, QVariant userData,
                               QVariant resultData, int requestID);
  void nxmModInfoAvailable(QString gameName, int modID, QVariant userData,
                           QVariant resultData, int requestID);
  void nxmUpdateInfoAvailable(QString gameName, QVariant userData, QVariant resultData,
                              int requestID);
  void nxmUpdatesAvailable(QString gameName, int modID, QVariant userData,
                           QVariant resultData, int requestID);
  void nxmFilesAvailable(QString gameName, int modID, QVariant userData,
                         QVariant resultData, int requestID);
  void nxmFileInfoAvailable(QString gameName, int modID, int fileID, QVariant userData,
                            QVariant resultData, int requestID);
  void nxmFileInfoFromMd5Available(QString gameName, QVariant userData,
                                   QVariant resultData, int requestID);
  void nxmDownloadURLsAvailable(QString gameName, int modID, int fileID,
                                QVariant userData, QVariant resultData, int requestID);
  void nxmEndorsementsAvailable(QVariant userData, QVariant resultData, int requestID);
  void nxmEndorsementToggled(QString gameName, int modID, QVariant userData,
                             QVariant resultData, int requestID);
  void nxmTrackedModsAvailable(QVariant userData, QVariant resultData, int requestID);
  void nxmTrackingToggled(QString gameName, int modID, QVariant userData, bool tracked,
                          int requestID);
  void nxmGameInfoAvailable(QString gameName, QVariant userData, QVariant resultData,
                            int requestID);
  void nxmRequestFailed(QString gameName, int modID, int fileID, QVariant userData,
                        int requestID, int errorCode, const QString& errorString);
  void requestsChanged(const APIStats& stats, const APIUserAccount& user);

public slots:

  void setUserAccount(const APIUserAccount& user);

private slots:

  void requestFinished();
  void requestError(QNetworkReply::NetworkError error);
  void requestTimeout();

  void downloadRequestedNXM(const QString& url);

  void fakeFiles();

private:
  struct NXMRequestInfo
  {
    int m_ModID;
    QString m_ModVersion;
    std::vector<int> m_ModIDList;
    int m_FileID;
    QNetworkReply* m_Reply;
    enum Type
    {
      TYPE_DESCRIPTION,
      TYPE_MODINFO,
      TYPE_FILES,
      TYPE_FILEINFO,
      TYPE_DOWNLOADURL,
      TYPE_ENDORSEMENTS,
      TYPE_TOGGLEENDORSEMENT,
      TYPE_GETUPDATES,
      TYPE_CHECKUPDATES,
      TYPE_TOGGLETRACKING,
      TYPE_TRACKEDMODS,
      TYPE_FILEINFO_MD5,
      TYPE_GAMEINFO,
    } m_Type;
    UpdatePeriod m_UpdatePeriod;
    QVariant m_UserData;
    QTimer* m_Timeout;
    QString m_URL;
    QString m_SubModule;
    QString m_GameName;
    int m_NexusGameID;
    bool m_Reroute;
    int m_ID;
    int m_Endorse;
    int m_Track;
    QByteArray m_Hash;
    QMap<QNetworkReply::NetworkError, QList<int>> m_AllowedErrors;
    bool m_IgnoreGenericErrorHandler;

    NXMRequestInfo(int modID, Type type, QVariant userData, const QString& subModule,
                   MOBase::IPluginGame const* game);
    NXMRequestInfo(int modID, QString modVersion, Type type, QVariant userData,
                   const QString& subModule, MOBase::IPluginGame const* game);
    NXMRequestInfo(int modID, int fileID, Type type, QVariant userData,
                   const QString& subModule, MOBase::IPluginGame const* game);
    NXMRequestInfo(Type type, QVariant userData, const QString& subModule,
                   MOBase::IPluginGame const* game);
    NXMRequestInfo(Type type, QVariant userData, const QString& subModule);
    NXMRequestInfo(UpdatePeriod period, Type type, QVariant userData,
                   const QString& subModule, MOBase::IPluginGame const* game);
    NXMRequestInfo(QByteArray& hash, Type type, QVariant userData,
                   const QString& subModule, MOBase::IPluginGame const* game);

  private:
    static QAtomicInt s_NextID;
  };

  static const int MAX_ACTIVE_DOWNLOADS = 6;

private:
  void nextRequest();
  void requestFinished(std::list<NXMRequestInfo>::iterator iter);
  MOBase::IPluginGame* getGame(QString gameName) const;
  QString getOldModsURL(QString gameName) const;

private:
  QNetworkDiskCache* m_DiskCache;
  NXMAccessManager* m_AccessManager;
  std::list<NXMRequestInfo> m_ActiveRequest;
  QQueue<NXMRequestInfo> m_RequestQueue;
  PluginManager* m_PluginManager;
  APIUserAccount m_User;
};

#endif  // NEXUSINTERFACE_H
