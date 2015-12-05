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

#include <utility.h>
#include <versioninfo.h>
#include <imodrepositorybridge.h>

#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QQueue>
#include <QVariant>
#include <QTimer>

#include <list>
#include <set>

namespace MOBase { class IPluginGame; }

class NexusInterface;
class NXMAccessManager;

/**
 * @brief convenience class to make nxm requests easier
 * usually, all objects that started a nxm request will be signaled if one finished.
 * Therefore, the objects need to store the id of the requests they started and then filter
 * the result.
 * NexusBridge does this automatically. Users connect to the signals of NexusBridge they intend
 * to handle and only receive the signals the caused
 **/
class NexusBridge : public MOBase::IModRepositoryBridge
{

  Q_OBJECT

public:

  NexusBridge(const QString &subModule = "");

  /**
   * @brief request description for a mod
   *
   * @param modID id of the mod caller is interested in
   * @param userData user data to be returned with the result
   * @param url the url to request from
   **/
  virtual void requestDescription(int modID, QVariant userData);

  /**
   * @brief request a list of the files belonging to a mod
   *
   * @param modID id of the mod caller is interested in
   * @param userData user data to be returned with the result
   **/
  virtual void requestFiles(int modID, QVariant userData);

  /**
   * @brief request info about a single file of a mod
   *
   * @param modID id of the mod caller is interested in
   * @param fileID id of the file the caller is interested in
   * @param userData user data to be returned with the result
   **/
  virtual void requestFileInfo(int modID, int fileID, QVariant userData);

  /**
   * @brief request the download url of a file
   *
   * @param modID id of the mod caller is interested in
   * @param fileID id of the file the caller is interested in
   * @param userData user data to be returned with the result
   **/
  virtual void requestDownloadURL(int modID, int fileID, QVariant userData);

  /**
   * @brief requestToggleEndorsement
   * @param modID id of the mod caller is interested in
   * @param userData user data to be returned with the result
   */
  virtual void requestToggleEndorsement(int modID, bool endorse, QVariant userData);

public slots:

  void nxmDescriptionAvailable(int modID, QVariant userData, QVariant resultData, int requestID);
  void nxmFilesAvailable(int modID, QVariant userData, QVariant resultData, int requestID);
  void nxmFileInfoAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID);
  void nxmDownloadURLsAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID);
  void nxmEndorsementToggled(int modID, QVariant userData, QVariant resultData, int requestID);
  void nxmRequestFailed(int modID, int fileID, QVariant userData, int requestID, const QString &errorMessage);

private:

  NexusInterface *m_Interface;
  QString m_SubModule;
  std::set<int> m_RequestIDs;

};


/**
 * @brief Makes asynchronous requests to the nexus API
 *
 * This class can be used to make asynchronous requests to the Nexus API.
 * Currently, responses are sent to all receivers that have sent a request of the relevant type, so the
 * recipient has to filter the response by the id returned when making the request
 **/
class NexusInterface : public QObject
{
  Q_OBJECT

public:

  ~NexusInterface();

  static NexusInterface *instance();

  /**
   * @return the access manager object used to connect to nexus
   **/
  NXMAccessManager *getAccessManager();

  /**
   * @brief cleanup this interface. this is destructive, afterwards it can't be used again
   */
  void cleanup();

  /**
   * @brief request description for a mod
   *
   * @param modID id of the mod caller is interested in (assumed to be for the current game)
   * @param receiver the object to receive the result asynchronously via a signal (nxmDescriptionAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   **/
  int requestDescription(int modID, QObject *receiver, QVariant userData, const QString &subModule)
  {
    return requestDescription(modID, receiver, userData, subModule, m_Game);
  }

  /**
   * @brief request description for a mod
   *
   * @param modID id of the mod caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal (nxmDescriptionAvailable)
   * @param userData user data to be returned with the result
   * @param game Game with which the mod is associated
   * @return int an id to identify the request
   **/
  int requestDescription(int modID, QObject *receiver, QVariant userData, const QString &subModule,
                         MOBase::IPluginGame const *game);

  /**
   * @brief request nexus descriptions for multiple mods at once
   * @param modIDs a list of ids of mods the caller is interested in (assumed to be for the current game)
   * @param receiver the object to receive the result asynchronously via a signal (nxmDescriptionAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   */
  int requestUpdates(const std::vector<int> &modIDs, QObject *receiver, QVariant userData, const QString &subModule)
  {
    return requestUpdates(modIDs, receiver, userData, subModule, m_Game);
  }

  /**
   * @brief request nexus descriptions for multiple mods at once
   * @param modIDs a list of ids of mods the caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal (nxmDescriptionAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   */
  int requestUpdates(const std::vector<int> &modIDs, QObject *receiver, QVariant userData, const QString &subModule,
                     MOBase::IPluginGame const *game);

  /**
   * @brief request a list of the files belonging to a mod
   *
   * @param modID id of the mod caller is interested in (assumed to be for the current game)
   * @param receiver the object to receive the result asynchronously via a signal (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   **/
  int requestFiles(int modID, QObject *receiver, QVariant userData, const QString &subModule)
  {
    return requestFiles(modID, receiver, userData, subModule, m_Game);
  }


  /**
   * @brief request a list of the files belonging to a mod
   *
   * @param modID id of the mod caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   **/
  int requestFiles(int modID, QObject *receiver, QVariant userData, const QString &subModule,
                   MOBase::IPluginGame const *game);

  /**
   * @brief request info about a single file of a mod
   *
   * @param modID id of the mod caller is interested in (assumed to be for the current game)
   * @param fileID id of the file the caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   **/
  int requestFileInfo(int modID, int fileID, QObject *receiver, QVariant userData, const QString &subModule)
  {
    return requestFileInfo(modID, fileID, receiver, userData, subModule, m_Game);
  }

  /**
   * @brief request info about a single file of a mod
   *
   * @param modID id of the mod caller is interested in (assumed to be for the current game)
   * @param fileID id of the file the caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   **/
  int requestFileInfo(int modID, int fileID, QObject *receiver, QVariant userData, const QString &subModule,
                      MOBase::IPluginGame const *game);

  /**
   * @brief request the download url of a file
   *
   * @param modID id of the mod caller is interested in (assumed to be for the current game)
   * @param fileID id of the file the caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   **/
  int requestDownloadURL(int modID, int fileID, QObject *receiver, QVariant userData, const QString &subModule)
  {
    return requestDownloadURL(modID, fileID, receiver, userData, subModule, m_Game);
  }

  /**
   * @brief request the download url of a file
   *
   * @param modID id of the mod caller is interested in
   * @param fileID id of the file the caller is interested in
   * @param receiver the object to receive the result asynchronously via a signal (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   **/
  int requestDownloadURL(int modID, int fileID, QObject *receiver, QVariant userData, const QString &subModule, MOBase::IPluginGame const *game);

  /**
   * @brief toggle endorsement state of the mod
   * @param modID id of the mod (assumed to be for the current game)
   * @param endorse true if the mod should be endorsed, false for un-endorse
   * @param receiver the object to receive the result asynchronously via a signal (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @return int an id to identify the request
   */
  int requestToggleEndorsement(int modID, bool endorse, QObject *receiver, QVariant userData, const QString &subModule)
  {
    return requestToggleEndorsement(modID, endorse, receiver, userData, subModule, m_Game);
  }

  /**
   * @brief toggle endorsement state of the mod
   * @param modID id of the mod
   * @param endorse true if the mod should be endorsed, false for un-endorse
   * @param receiver the object to receive the result asynchronously via a signal (nxmFilesAvailable)
   * @param userData user data to be returned with the result
   * @param game the game with which the mods are associated
   * @return int an id to identify the request
   */
  int requestToggleEndorsement(int modID, bool endorse, QObject *receiver, QVariant userData, const QString &subModule,
                               MOBase::IPluginGame const *game);

  /**
   * @param directory the directory to store cache files
   **/
  void setCacheDirectory(const QString &directory);

  /**
   * MO has to send a "Nexus Client Vx.y.z" as part of the user agent to be allowed to use the API
   * @param nmmVersion the version of nmm to impersonate
   **/
  void setNMMVersion(const QString &nmmVersion);

  /**
   * @brief called when the log-in completes. This was, requests waiting for the log-in can be run
   */
  void loginCompleted();

public:

  /**
   * @brief guess the mod id from a filename as delivered by Nexus
   * @param fileName name of the file
   * @return the guessed mod id
   * @note this currently doesn't fit well with the remaining interface but this is the best place for the function
   */
  static void interpretNexusFileName(const QString &fileName, QString &modName, int &modID, bool query);

  /**
   * @brief get the currently managed game
   */
  MOBase::IPluginGame const *managedGame() const;

  /**
   * @brief see if the passed URL is related to the current game
   *
   * Arguably, this should optionally take a gameplugin pointer
   */
  bool isURLGameRelated(QUrl const &url) const;

  /**
   * @brief Get the nexus page for the current game
   *
   * Arguably, this should optionally take a gameplugin pointer
   */
  QString getGameURL() const;

  /**
   * @brief Get the URL for the mod web page
   * @param modID
   */
  QString getModURL(int modID) const;

  /**
   * @brief Checks if the specified URL might correspond to a nexus mod
   * @param modID
   * @param url
   * @return
   */
  bool isModURL(int modID, QString const &url) const;

signals:

  void requestNXMDownload(const QString &url);

  void needLogin();

  void nxmDescriptionAvailable(int modID, QVariant userData, QVariant resultData, int requestID);
  void nxmUpdatesAvailable(const std::vector<int> &modIDs, QVariant userData, QVariant resultData, int requestID);
  void nxmFilesAvailable(int modID, QVariant userData, QVariant resultData, int requestID);
  void nxmFileInfoAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID);
  void nxmDownloadURLsAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID);
  void nxmEndorsementToggled(int modID, QVariant userData, QVariant resultData, int requestID);
  void nxmRequestFailed(int modID, int fileID, QVariant userData, int requestID, const QString &errorString);

public slots:
  void managedGameChanged(MOBase::IPluginGame const *game);

private slots:

  void requestFinished();
  void requestError(QNetworkReply::NetworkError error);
  void requestTimeout();

  void downloadRequestedNXM(const QString &url);

  void fakeFiles();

private:

  struct NXMRequestInfo {
    int m_ModID;
    std::vector<int> m_ModIDList;
    int m_FileID;
    QNetworkReply *m_Reply;
    enum Type {
      TYPE_DESCRIPTION,
      TYPE_FILES,
      TYPE_FILEINFO,
      TYPE_DOWNLOADURL,
      TYPE_TOGGLEENDORSEMENT,
      TYPE_GETUPDATES
    } m_Type;
    QVariant m_UserData;
    QTimer *m_Timeout;
    QString m_URL;
    QString m_SubModule;
    int m_NexusGameID;
    bool m_Reroute;
    int m_ID;
    int m_Endorse;

    NXMRequestInfo(int modID, Type type, QVariant userData, const QString &subModule, MOBase::IPluginGame const *game);
    NXMRequestInfo(std::vector<int> modIDList, Type type, QVariant userData, const QString &subModule, MOBase::IPluginGame const *game);
    NXMRequestInfo(int modID, int fileID, Type type, QVariant userData, const QString &subModule, MOBase::IPluginGame const *game);

  private:
    static QAtomicInt s_NextID;
  };

  static const int MAX_ACTIVE_DOWNLOADS = 2;

private:

  NexusInterface();
  void nextRequest();
  void requestFinished(std::list<NXMRequestInfo>::iterator iter);
  bool requiresLogin(const NXMRequestInfo &info);
  QString getOldModsURL() const;

private:

  QNetworkDiskCache *m_DiskCache;

  NXMAccessManager *m_AccessManager;

  std::list<NXMRequestInfo> m_ActiveRequest;
  QQueue<NXMRequestInfo> m_RequestQueue;

  MOBase::VersionInfo m_MOVersion;
  QString m_NMMVersion;

  MOBase::IPluginGame const *m_Game;

};

#endif // NEXUSINTERFACE_H
