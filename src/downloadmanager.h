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

#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include "nexusinterface.h"
#include <set>
#include <QObject>
#include <QUrl>
#include <QQueue>
#include <QFile>
#include <QNetworkReply>
#include <QTime>
#include <QVector>
#include <QMap>
#include <QFileSystemWatcher>


struct NexusInfo {
  NexusInfo() : m_Category(0), m_FileCategory(0), m_Set(false) {}
  int m_Category;
  int m_FileCategory;
  QString m_Name;
  QString m_ModName;
  QString m_Version;
  QString m_NewestVersion;
  QString m_FileName;
  bool m_Set;
};
Q_DECLARE_METATYPE(NexusInfo)


/*!
 * \brief manages downloading of files and provides progress information for gui elements
 **/
class DownloadManager : public QObject
{
  Q_OBJECT

public:

  enum DownloadState {
    STATE_STARTED = 0,
    STATE_DOWNLOADING,
    STATE_CANCELING,
    STATE_PAUSING,
    STATE_CANCELED,
    STATE_PAUSED,
    STATE_FETCHINGMODINFO,
    STATE_FETCHINGFILEINFO,
    STATE_READY,
    STATE_INSTALLED
  };

private:

  struct DownloadInfo {
    QString m_FileName;
    QFile m_Output;
    QNetworkReply *m_Reply;
    QTime m_StartTime;
    int m_Progress;
    int m_ModID;
    int m_FileID;
    NexusInfo m_NexusInfo;
    DownloadState m_State;
    QString m_Url;
    qint64 m_ResumePos;

    /**
     * @brief rename the file
     * this will change the file name as well as the display name. It will automatically
     * append .unfinished to the name if this file is still being downloaded
     * @param newName the new name to setName
     * @param renameFile if true, the file is assumed to exist and renamed. If the file does not
     *                   yet exist, set this to false
     **/
    void setName(QString newName, bool renameFile);
  };

public:

  /**
   * @brief constructor
   *
   * @param nexusInterface interface to use to retrieve information from the relevant nexus page
   * @param parent parent object
   **/
  explicit DownloadManager(NexusInterface *nexusInterface, QObject *parent);

  ~DownloadManager();

  /**
   * @brief determine if a download is currently in progress
   *
   * @return true if there is currently a download in progress
   **/
  bool downloadsInProgress();

  /**
   * @brief set the output directory to write to
   *
   * @param outputDirectory the new output directory
   **/
  void setOutputDirectory(const QString &outputDirectory);

  /**
   * @return current download directory
   **/
  QString getOutputDirectory() const { return m_OutputDirectory; }

  /**
   * @brief start a download from a url
   *
   * @param url the url to download from
   * @param modID the nexus mod id this download belongs to
   * @param fileID the nexus file id this download belongs to, if known. Defaults to 0.
   * @param nexusInfo information previously retrieved from the nexus network
   * @return true if the download was started, false if it wasn't. The latter currently only happens if there is a duplicate and the user decides not to download again
   **/
  bool addDownload(const QUrl &url, int modID, int fileID = 0, const NexusInfo &nexusInfo = NexusInfo());

  /**
   * @brief download from an already open network connection
   *
   * @param reply the network reply to download from
   * @param fileName the name to use for the file. This may be overridden by the name in the nexusInfo-structure or if the http stream specifies a name
   * @param modID the nexus mod id this download belongs to
   * @param fileID the nexus file id this download belongs to, if known. Defaults to 0.
   * @param nexusInfo information previously retrieved from the nexus network
   * @return true if the download was started, false if it wasn't. The latter currently only happens if there is a duplicate and the user decides not to download again
   **/
  bool addDownload(QNetworkReply *reply, const QString &fileName, int modID, int fileID = 0, const NexusInfo &nexusInfo = NexusInfo());

  /**
   * @brief start a download using a nxm-link
   *
   * starts a download using a nxm-link. The download manager will first query the nexus
   * page for file information.
   * @param url a nxm link looking like this: nxm://skyrim/mods/1234/files/4711
   * @todo the game name encoded into the link is currently ignored, all downloads are incorrectly assumed to be for the identified game
   **/
  void addNXMDownload(const QString &url);

  /**
   * @brief retrieve the total number of downloads, both finished and unfinished including downloads from previous sessions
   *
   * @return total number of downloads
   **/
  int numTotalDownloads() const;

  /**
   * @brief retrieve the full path to the download specified by index
   *
   * @param index the index to look up
   * @return absolute path of the file
   **/
  QString getFilePath(int index) const;

  /**
   * @brief retrieve the filename of the download specified by index
   *
   * @param index index of the file to look up
   * @return name of the file
   **/
  QString getFileName(int index) const;

  /**
   * @brief retrieve the current progress of the download specified by index
   *
   * @param index index of the file to look up
   * @return progress of the download in percent (integer)
   **/
  int getProgress(int index) const;

  /**
   * @brief retrieve the current state of the download
   *
   * retrieve the current state of the download. A download usually goes through
   * the following states:
   *   started -> downloading -> fetching mod info -> fetching file info -> done
   * in case of downloads started via nxm-link, file information is fetched first
   * 
   * @param index index of the file to look up
   * @return the download state
   **/
  DownloadState getState(int index) const;

  /**
   * @param index index of the file to look up
   * @return true if the nexus information for this download is not complete
   **/
  bool isInfoIncomplete(int index) const;

  /**
   * @brief retrieve the nexus mod id of the download specified by index
   *
   * @param index index of the file to look up
   * @return the nexus mod id
   **/
  int getModID(int index) const;

  /**
   * @brief retrieve all nexus info of the download specified by index
   *
   * @param index index of the file to look up
   * @return the nexus mod information
   **/
  NexusInfo getNexusInfo(int index) const;

  /**
   * @brief mark a download as installed
   *
   * @param index index of the file to mark installed
   */
  void markInstalled(int index);

  /**
   * @brief refreshes the list of downloads
   */
  void refreshList();

  /**
   * @brief Sort function for download servers
   * @param LHS
   * @param RHS
   * @return
   */
  static bool ServerByPreference(const QVariant &LHS, const QVariant &RHS);

signals:

  void aboutToUpdate();

  /**
   * @brief signals that the specified download has changed
   *
   * @param row the row that changed. This corresponds to the download index
   **/
  void update(int row);

  /**
   * @brief signals the ui that a message should be displayed
   *
   * @param message the message to display
   **/
  void showMessage(const QString &message);

public slots:

  /**
   * @brief removes the specified download
   *
   * @param index index of the download to remove
   * @param deleteFile if true, the file will also be deleted from disc, otherwise it is only marked as hidden.
   **/
  void removeDownload(int index, bool deleteFile);

  /**
   * @brief cancel the specified download. This will lead to the corresponding file to be deleted
   *
   * @param index index of the download to cancel
   **/
  void cancelDownload(int index);

  void pauseDownload(int index);

  void resumeDownload(int index);

  void queryInfo(int index);

  void nxmDescriptionAvailable(int modID, QVariant userData, QVariant resultData, int requestID);

  void nxmFilesAvailable(int modID, QVariant userData, QVariant resultData, int requestID);

  void nxmFileInfoAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID);

  void nxmDownloadURLsAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID);

  void nxmRequestFailed(int modID, QVariant userData, int requestID, const QString &errorString);

private slots:

  void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void downloadReadyRead();
  void downloadFinished();
  void metaDataChanged();
  void directoryChanged(const QString &dirctory);

private:

  void createMetaFile(DownloadInfo *info);
//  QString getOutputPath(const QUrl &url, const QString &fileName) const;
  QString getDownloadFileName(const QString &baseName) const;

  void addDownload(QNetworkReply *reply, DownloadInfo *newDownload, bool resume);

  // important: the caller has to lock the list-mutex, otherwise the DownloadInfo-pointer might get invalidated at any time
  DownloadInfo *findDownload(QObject *reply, int *index = NULL) const;

  void removeFile(int index, bool deleteFile);

  void refreshAlphabeticalTranslation();

  bool ByName(int LHS, int RHS);

  QString getFileNameFromNetworkReply(QNetworkReply *reply);

private:

  NexusInterface *m_NexusInterface;
  QVector<DownloadInfo*> m_ActiveDownloads;

  QString m_OutputDirectory;
  std::set<int> m_RequestIDs;
  QVector<int> m_AlphabeticalTranslation;

  QFileSystemWatcher m_DirWatcher;

};

#endif // DOWNLOADMANAGER_H
