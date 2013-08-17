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
#include <idownloadmanager.h>
#include <set>
#include <QObject>
#include <QUrl>
#include <QQueue>
#include <QFile>
#include <QNetworkReply>
#include <QTime>
#include <QVector>
#include <QMap>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QSettings>


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
class DownloadManager : public MOBase::IDownloadManager
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
    STATE_ERROR,
    STATE_FETCHINGMODINFO,
    STATE_FETCHINGFILEINFO,
    STATE_READY,
    STATE_INSTALLED,
    STATE_UNINSTALLED
  };

private:

  struct DownloadInfo {
    unsigned int m_DownloadID;
    QString m_FileName;
    QFile m_Output;
    QNetworkReply *m_Reply;
    QTime m_StartTime;
    int m_Progress;
    int m_ModID;
    int m_FileID;
    DownloadState m_State;
    int m_CurrentUrl;
    QStringList m_Urls;
    qint64 m_ResumePos;
    qint64 m_TotalSize;
    int m_Tries;
    bool m_ReQueried;

    NexusInfo m_NexusInfo;

    static DownloadInfo *createNew(const NexusInfo &nexusInfo, int modID, int fileID, const QStringList &URLs);
    static DownloadInfo *createFromMeta(const QString &filePath);

    /**
     * @brief rename the file
     * this will change the file name as well as the display name. It will automatically
     * append .unfinished to the name if this file is still being downloaded
     * @param newName the new name to setName
     * @param renameFile if true, the file is assumed to exist and renamed. If the file does not
     *                   yet exist, set this to false
     **/
    void setName(QString newName, bool renameFile);

    unsigned int downloadID() { return m_DownloadID; }

    bool isPausedState();

    QString currentURL();
  private:
    static unsigned int s_NextDownloadID;
  private:
    DownloadInfo() : m_TotalSize(0), m_ReQueried(false) {}
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
   * @brief setPreferredServers set the list of preferred servers
   */
  void setPreferredServers(const std::map<QString, int> &preferredServers);

  /**
   * @brief set the list of supported extensions
   * @param extensions list of supported extensions
   */
  void setSupportedExtensions(const QStringList &extensions);

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
  bool addDownload(QNetworkReply *reply, const QStringList &URLs, const QString &fileName, int modID, int fileID = 0, const NexusInfo &nexusInfo = NexusInfo());

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
   * @brief mark a download as uninstalled
   *
   * @param index index of the file to mark uninstalled
   */
  void markUninstalled(int index);

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
  static bool ServerByPreference(const std::map<QString, int> &preferredServers, const QVariant &LHS, const QVariant &RHS);


  virtual int startDownloadURLs(const QStringList &urls);
  virtual int startDownloadNexusFile(int modID, int fileID);
  virtual QString downloadPath(int id);

  /**
   * @brief retrieve a download index from the filename
   * @param fileName file to look up
   * @return index of that download or -1 if it wasn't found
   */
  int indexByName(const QString &fileName) const;

  void pauseAll();
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

  /**
   * @brief emitted whenever the state of a download changes
   * @param row the row that changed
   * @param state the new state
   */
  void stateChanged(int row, DownloadManager::DownloadState state);

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
  QString getDownloadFileName(const QString &baseName) const;

  void startDownload(QNetworkReply *reply, DownloadInfo *newDownload, bool resume);
  void resumeDownloadInt(int index);

  /**
   * @brief start a download from a url
   *
   * @param url the url to download from
   * @param modID the nexus mod id this download belongs to
   * @param fileID the nexus file id this download belongs to, if known. Defaults to 0.
   * @param nexusInfo information previously retrieved from the nexus network
   * @return true if the download was started, false if it wasn't. The latter currently only happens if there is a duplicate and the user decides not to download again
   **/
  bool addDownload(const QStringList &URLs, int modID, int fileID = 0, const NexusInfo &nexusInfo = NexusInfo());

  // important: the caller has to lock the list-mutex, otherwise the DownloadInfo-pointer might get invalidated at any time
  DownloadInfo *findDownload(QObject *reply, int *index = NULL) const;

  void removeFile(int index, bool deleteFile);

  void refreshAlphabeticalTranslation();

  bool ByName(int LHS, int RHS);

  QString getFileNameFromNetworkReply(QNetworkReply *reply);

  void setState(DownloadInfo *info, DownloadManager::DownloadState state);

  DownloadInfo *downloadInfoByID(unsigned int id);

private:

  static const int AUTOMATIC_RETRIES = 3;

private:

  NexusInterface *m_NexusInterface;
  QVector<DownloadInfo*> m_ActiveDownloads;

  QString m_OutputDirectory;
  std::map<QString, int> m_PreferredServers;
  QStringList m_SupportedExtensions;
  std::set<int> m_RequestIDs;
  QVector<int> m_AlphabeticalTranslation;

  QFileSystemWatcher m_DirWatcher;

  std::map<QString, int> m_DownloadFails;

};

#endif // DOWNLOADMANAGER_H
