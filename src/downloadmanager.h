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

#include "serverinfo.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFileSystemWatcher>
#include <QHash>
#include <QMap>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QSettings>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/signals2.hpp>
#include <idownloadmanager.h>
#include <modrepositoryfileinfo.h>
#include <optional>
#include <set>
using namespace boost::accumulators;

namespace MOBase
{
class IPluginGame;
}

class NexusInterface;
class PluginContainer;
class OrganizerCore;

/**
 * @brief QFileSystemWatcher with a nestable RAII suspension scope.
 *
 * Forwards directoryChanged() only while no Guard is alive. Use a Guard to
 * bracket filesystem writes that would otherwise trigger a spurious refresh.
 */
class DirWatcherManager : public QObject
{
  Q_OBJECT

public:
  explicit DirWatcherManager(QObject* parent = nullptr);

  /// Set the directory being watched (replaces any previous path).
  void setPath(const QString& path);

  /// True while one or more Guards are alive.
  bool isSuspended() const;

  /**
   * @brief RAII suspension guard. Nests safely; the only way to suspend
   * forwarding.
   */
  class [[nodiscard]] Guard
  {
  public:
    explicit Guard(DirWatcherManager& manager);
    ~Guard();
    Guard(const Guard&)            = delete;
    Guard& operator=(const Guard&) = delete;
    Guard(Guard&&)                 = delete;
    Guard& operator=(Guard&&)      = delete;

  private:
    DirWatcherManager& m_manager;
  };

  /// Returns a suspension Guard bound to the caller's scope.
  [[nodiscard]] Guard scopedGuard();

signals:
  /// Emitted when the watched directory changes and no Guard is active.
  void directoryChanged();

private slots:
  void onDirectoryChanged(const QString&);

private:
  QFileSystemWatcher m_watcher;
  int m_suspendDepth = 0;
};

/*!
 * \brief manages downloading of files and provides progress information for gui
 *elements
 **/
class DownloadManager : public QObject
{
  Q_OBJECT

public:
  /**
   * @brief RAII full-reset guard. Use when the row count changes; drops
   * view selection/scroll state. Nests safely: inner guards coalesce into
   * the outermost scope so only one reset is emitted.
   */
  class [[nodiscard]] ModelResetGuard
  {
  public:
    explicit ModelResetGuard(DownloadManager& manager);
    ~ModelResetGuard();
    ModelResetGuard(const ModelResetGuard&)            = delete;
    ModelResetGuard& operator=(const ModelResetGuard&) = delete;
    ModelResetGuard(ModelResetGuard&&)                 = delete;
    ModelResetGuard& operator=(ModelResetGuard&&)      = delete;

  private:
    DownloadManager& m_manager;
  };

  enum DownloadState
  {
    STATE_STARTED = 0,
    STATE_DOWNLOADING,
    STATE_CANCELING,
    STATE_PAUSING,
    STATE_CANCELED,
    STATE_PAUSED,
    STATE_ERROR,
    STATE_FETCHINGMODINFO,
    STATE_FETCHINGFILEINFO,
    STATE_FETCHINGMODINFO_MD5,
    STATE_NOFETCH,
    STATE_READY,
    STATE_INSTALLED,
    STATE_UNINSTALLED
  };

  /**
   * @brief A download that has been requested but has not yet produced a
   * DownloadInfo.
   *
   * Created when the user initiates an NXM download; drained either when the
   * Nexus API returns the actual download URL (at which point a DownloadInfo
   * is created using reservedID as its download id so that external references
   * handed out before the download existed remain valid) or when the request
   * is cancelled or fails.
   */
  struct PendingDownload
  {
    QString gameName;
    int modID;
    int fileID;
    unsigned int reservedID;
  };

private:
  struct DownloadInfo
  {
    ~DownloadInfo() { delete m_FileInfo; }
    accumulator_set<qint64, stats<tag::rolling_mean>> m_DownloadAcc;
    accumulator_set<qint64, stats<tag::rolling_mean>> m_DownloadTimeAcc;
    qint64 m_DownloadLast;
    qint64 m_DownloadTimeLast;
    unsigned int m_DownloadID;
    QString m_FileName;
    QFile m_Output;
    QNetworkReply* m_Reply;
    QElapsedTimer m_StartTime;
    qint64 m_PreResumeSize;
    std::pair<int, QString> m_Progress;
    bool m_HasData;
    DownloadState m_State;
    int m_CurrentUrl;
    QStringList m_Urls;
    qint64 m_ResumePos;
    qint64 m_TotalSize;
    QDateTime m_Created;  // used as a cache in DownloadManager::getFileTime, may not be
                          // valid elsewhere
    QByteArray m_Hash;
    QStringList m_GamesToQuery;
    QString m_RemoteFileName;

    int m_Tries;
    bool m_ReQueried;
    bool m_AskIfNotFound;

    quint32 m_TaskProgressId;

    MOBase::ModRepositoryFileInfo* m_FileInfo{nullptr};

    bool m_Hidden;

    /**
     * @brief Issue a new download id.
     *
     * The only supported way to obtain one; ids are monotonically increasing
     * within a session and never reused.
     */
    static unsigned int newDownloadID();

    /**
     * @brief Create a new DownloadInfo for a fresh download.
     *
     * When reservedID is provided it is used as the download id. Callers that
     * need to hand out an id before the DownloadInfo exists (e.g. the NXM flow
     * reserves an id when the request is queued, long before the Nexus API
     * returns the actual URL) should reserve via newDownloadID() and pass it
     * here. Otherwise a fresh id is drawn internally.
     */
    static DownloadInfo* createNew(const MOBase::ModRepositoryFileInfo* fileInfo,
                                   const QStringList& URLs,
                                   std::optional<unsigned int> reservedID = {});
    static DownloadInfo* createFromMeta(const QString& filePath, bool showHidden,
                                        const QString outputDirectory,
                                        std::optional<uint64_t> fileSize = {});

    /**
     * @brief rename the file
     * this will change the file name as well as the display name. It will automatically
     * append .unfinished to the name if this file is still being downloaded
     * @param newName the new name to setName
     * @param renameFile if true, the file is assumed to exist and renamed. If the file
     *does not yet exist, set this to false
     **/
    void setName(QString newName, bool renameFile);

    unsigned int downloadID() { return m_DownloadID; }

    bool isPausedState();

    QString currentURL();

  private:
    static unsigned int s_NextDownloadID;

  private:
    DownloadInfo()
        : m_TotalSize(0), m_ReQueried(false), m_Hidden(false), m_HasData(false),
          m_AskIfNotFound(true), m_DownloadTimeLast(0), m_DownloadLast(0),
          m_DownloadAcc(tag::rolling_window::window_size = 200),
          m_DownloadTimeAcc(tag::rolling_window::window_size = 200)
    {}
  };

  friend class DownloadManagerProxy;

  using SignalDownloadCallback = boost::signals2::signal<void(int)>;

public:
  /**
   * @brief constructor
   *
   * @param nexusInterface interface to use to retrieve information from the relevant
   *nexus page
   * @param parent parent object
   **/
  explicit DownloadManager(NexusInterface* nexusInterface, QObject* parent);

  ~DownloadManager();

  void setParentWidget(QWidget* w);

  /**
   * @brief determine if a download is currently in progress
   *
   * @return true if there is currently a download in progress
   **/
  bool downloadsInProgress();

  /**
   * @brief determine if a download is currently in progress, does not count paused
   *ones.
   *
   * @return true if there is currently a download in progress (that is not paused
   *already).
   **/
  bool downloadsInProgressNoPause();

  /**
   * @brief set the output directory to write to
   *
   * @param outputDirectory the new output directory
   **/
  void setOutputDirectory(const QString& outputDirectory, const bool refresh = true);

  /**
   * @return current download directory
   **/
  QString getOutputDirectory() const { return m_OutputDirectory; }

  /**
   * @brief sets whether hidden files are to be shown after all
   */
  void setShowHidden(bool showHidden);

  void setPluginContainer(PluginContainer* pluginContainer);

  /**
   * @brief download from an already open network connection
   *
   * @param reply the network reply to download from
   * @param fileInfo information about the file, like mod id, file id, version, ...
   * @return true if the download was started, false if it wasn't. The latter currently
   *only happens if there is a duplicate and the user decides not to download again
   **/
  bool addDownload(QNetworkReply* reply, const MOBase::ModRepositoryFileInfo* fileInfo);

  /**
   * @brief download from an already open network connection
   *
   * @param reply the network reply to download from
   * @param fileName the name to use for the file. This may be overridden by the name in
   *the fileInfo-structure or if the http stream specifies a name
   * @param fileInfo information previously retrieved from the nexus network
   * @return true if the download was started, false if it wasn't. The latter currently
   *only happens if there is a duplicate and the user decides not to download again
   **/
  bool addDownload(QNetworkReply* reply, const QStringList& URLs,
                   const QString& fileName, QString gameName, int modID, int fileID = 0,
                   const MOBase::ModRepositoryFileInfo* fileInfo =
                       new MOBase::ModRepositoryFileInfo(),
                   std::optional<unsigned int> reservedID = {});

  /**
   * @brief start a download using a nxm-link
   *
   * Starts a download using a nxm-link. The download manager will first query the
   * nexus page for file information. The returned id identifies the eventual
   * download; it is reserved immediately so external references remain valid even
   * before the Nexus API responds.
   * @param url a nxm link looking like this: nxm://skyrim/mods/1234/files/4711
   * @return the reserved download id
   * @todo the game name encoded into the link is currently ignored, all downloads are
   *incorrectly assumed to be for the identified game
   **/
  unsigned int addNXMDownload(const QString& url);

  /**
   * @brief retrieve the total number of downloads, both finished and unfinished
   *including downloads from previous sessions
   *
   * @return total number of downloads
   **/
  int numTotalDownloads() const;

  /**
   * @brief retrieve number of pending downloads (nexus downloads for which we don't
   * know the name and url yet)
   * @return  number of pending downloads
   */
  int numPendingDownloads() const;

  /**
   * @brief retrieve the info of a pending download
   * @param index index of the pending download (index in the range [0,
   * numPendingDownloads()[)
   * @return the PendingDownload entry at the given index
   */
  PendingDownload getPendingDownload(int index);

  /**
   * @brief retrieve the full path to the download specified by index
   *
   * @param index the index to look up
   * @return absolute path of the file
   **/
  QString getFilePath(int index) const;

  /**
   * @brief retrieve a descriptive name of the download specified by index
   *
   * @param index index of the file to look up
   * @return display name of the file
   **/
  QString getDisplayName(int index) const;

  /**
   * @brief retrieve the filename of the download specified by index
   *
   * @param index index of the file to look up
   * @return name of the file
   **/
  QString getFileName(int index) const;

  /**
   * @brief retrieve the file size of the download specified by index
   *
   * @param index index of the file to look up
   * @return size of the file (total size during download)
   */
  qint64 getFileSize(int index) const;

  /**
   * @brief retrieve the creation time of the download specified by index
   * @param index index of the file to look up
   * @return size of the file (total size during download)
   */
  QDateTime getFileTime(int index) const;

  /**
   * @brief retrieve the current progress of the download specified by index
   *
   * @param index index of the file to look up
   * @return progress of the download in percent (integer)
   **/
  std::pair<int, QString> getProgress(int index) const;

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
   * @brief retrieve the displayable game name of the download specified by the index
   *
   * @param index index of the file to look up
   * @return the displayable game name
   **/
  QString getDisplayGameName(int index) const;

  /**
   * @brief retrieve the game name of the downlaod specified by the index
   *
   * @param index index of the file to look up
   * @return the game name
   **/
  QString getGameName(int index) const;

  /**
   * @brief determine if the specified file is supposed to be hidden
   * @param index index of the file to look up
   * @return true if the specified file is supposed to be hidden
   */
  bool isHidden(int index) const;

  /**
   * @brief retrieve all nexus info of the download specified by index
   *
   * @param index index of the file to look up
   * @return the nexus mod information
   **/
  const MOBase::ModRepositoryFileInfo* getFileInfo(int index) const;

  /**
   * @brief mark a download as installed
   *
   * @param index index of the file to mark installed
   */
  void markInstalled(int index);

  void markInstalled(QString download);

  /**
   * @brief mark a download as uninstalled
   *
   * @param index index of the file to mark uninstalled
   */
  void markUninstalled(int index);

  void markUninstalled(QString download);

  /**
   * @brief refreshes the list of downloads
   */
  void refreshList();

  /**
   * @brief Query infos for every download in the list
   */
  void queryDownloadListInfo();

  /**
   * @return the directory watcher for the downloads folder; call
   * scopedGuard() on it to suspend across filesystem writes.
   */
  DirWatcherManager& dirWatcher() { return m_DirWatcher; }

public:  // IDownloadManager interface:
  int startDownloadURLs(const QStringList& urls);
  int startDownloadNexusFile(const QString& gameName, int modID, int fileID);
  QString downloadPath(int id);

  boost::signals2::connection
  onDownloadComplete(const std::function<void(int)>& callback);
  boost::signals2::connection
  onDownloadPaused(const std::function<void(int)>& callback);
  boost::signals2::connection
  onDownloadFailed(const std::function<void(int)>& callback);
  boost::signals2::connection
  onDownloadRemoved(const std::function<void(int)>& callback);

  /**
   * @brief retrieve a download index from the filename
   * @param fileName file to look up
   * @return index of that download or -1 if it wasn't found
   */
  int indexByName(const QString& fileName) const;
  int indexByInfo(const DownloadInfo* info) const;

  void pauseAll();

  /**
   * @brief notify the UI that a single row's data changed. Preserves view
   * state; prefer over ModelResetGuard when the row count is unchanged.
   *
   * @param row the row that changed. This corresponds to the download index
   */
  void notifyRowChanged(int row);

Q_SIGNALS:

  /**
   * @brief emitted before the download list model is about to be reset
   *
   * Emitted by ModelResetGuard on construction. Views should call
   * beginResetModel() in response.
   */
  void aboutToResetModel();

  /**
   * @brief emitted after the download list model has been reset
   *
   * Emitted by ModelResetGuard on destruction. Views should call
   * endResetModel() in response.
   */
  void modelReset();

  /**
   * @brief signals that the specified download row's data has changed
   *
   * @param row the row that changed. This corresponds to the download index
   */
  void rowChanged(int row);

  /**
   * @brief signals the ui that a message should be displayed
   *
   * @param message the message to display
   **/
  void showMessage(const QString& message);

  /**
   * @brief emitted whenever the state of a download changes
   * @param row the row that changed
   * @param state the new state
   */
  void stateChanged(int row, DownloadManager::DownloadState state);

  /**
   * @brief emitted whenever a download completes successfully, reporting the download
   * speed for the server used
   */
  void downloadSpeed(const QString& serverName, int bytesPerSecond);

  /**
   * @brief emitted whenever a new download is added to the list
   */
  void downloadAdded();

public slots:

  /**
   * @brief removes the specified download
   *
   * @param index index of the download to remove
   * @param deleteFile if true, the file will also be deleted from disc, otherwise it is
   *only marked as hidden.
   **/
  void removeDownload(int index, bool deleteFile);

  /**
   * @brief restores the specified download to view (which was previously hidden
   * @param index index of the download to restore
   */
  void restoreDownload(int index);

  /**
   * @brief cancel the specified download. This will lead to the corresponding file to
   *be deleted
   *
   * @param index index of the download to cancel
   **/
  void cancelDownload(int index);

  void pauseDownload(int index);

  void resumeDownload(int index);

  void queryInfo(int index);

  void queryInfoMd5(int index, bool askIfNotFound = true);

  void visitOnNexus(int index);

  void visitUploaderProfile(int index);

  void openFile(int index);

  void openMetaFile(int index);

  void openInDownloadsFolder(int index);

  void nxmDescriptionAvailable(QString gameName, int modID, QVariant userData,
                               QVariant resultData, int requestID);

  void nxmFilesAvailable(QString gameName, int modID, QVariant userData,
                         QVariant resultData, int requestID);

  void nxmFileInfoAvailable(QString gameName, int modID, int fileID, QVariant userData,
                            QVariant resultData, int requestID);

  void nxmDownloadURLsAvailable(QString gameName, int modID, int fileID,
                                QVariant userData, QVariant resultData, int requestID);

  void nxmFileInfoFromMd5Available(QString gameName, QVariant userData,
                                   QVariant resultData, int requestID);

  void nxmRequestFailed(QString gameName, int modID, int fileID, QVariant userData,
                        int requestID, int errorCode, const QString& errorString);

  void managedGameChanged(MOBase::IPluginGame const* gamePlugin);

private slots:

  void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void downloadReadyRead();
  /**
   * @brief Slot wired to QNetworkReply::finished().
   *
   * Resolves the originating reply through sender() and then dispatches to
   * finishDownload. Use the public finishDownload directly for non-slot calls.
   */
  void onReplyFinished();

  /**
   * @brief Run the post-download bookkeeping for the given download.
   *
   * Writes any remaining data, transitions the download's state, and emits
   * the appropriate plugin signals.
   */
  void finishDownload(unsigned int id);
  void downloadError(QNetworkReply::NetworkError error);
  void metaDataChanged();
  void checkDownloadTimeout();

private:
  void createMetaFile(DownloadInfo* info);
  DownloadManager::DownloadInfo* getDownloadInfo(QString fileName);

public:
  /** Get a unique filename for a download.
   *
   * This allows you multiple versions of download files, useful if the file
   * comes from a web site with no version control
   *
   * @param basename: Name of the file
   *
   * @return Unique(ish) name
   */
  QString getDownloadFileName(const QString& baseName, bool rename = false) const;

private:
  /**
   * @brief Begin downloading into newDownload from reply.
   *
   * On the !resume path newDownload becomes owned by m_ActiveDownloads on
   * success; on failure (e.g. the output file cannot be opened) it is deleted
   * before returning. Returns whether the download actually started.
   */
  bool startDownload(QNetworkReply* reply, DownloadInfo* newDownload, bool resume);
  void resumeDownloadInt(int index);

  /**
   * @brief start a download from a url
   *
   * @param url the url to download from
   * @param fileInfo information previously retrieved from the mod page
   * @return true if the download was started, false if it wasn't. The latter currently
   *only happens if there is a duplicate and the user decides not to download again
   **/
  bool addDownload(const QStringList& URLs, QString gameName, int modID, int fileID,
                   const MOBase::ModRepositoryFileInfo* fileInfo,
                   std::optional<unsigned int> reservedID = {});

  // important: the caller has to lock the list-mutex, otherwise the
  // DownloadInfo-pointer might get invalidated at any time
  DownloadInfo* findDownload(QObject* reply, int* index = nullptr) const;

  void removeFile(int index, bool deleteFile);

  QString getFileNameFromNetworkReply(QNetworkReply* reply);

  void setState(DownloadInfo* info, DownloadManager::DownloadState state);

  DownloadInfo* downloadInfoByID(unsigned int id);

  void removePending(QString gameName, int modID, int fileID);

  /**
   * @brief Fire onDownloadFailed for a pending entry, if any matches.
   *
   * Used on Nexus API failures so callers holding a reserved id from
   * addNXMDownload do not wait indefinitely for a result. No-op if no pending
   * entry matches the (gameName, modID, fileID) triple.
   */
  void notifyPendingDownloadFailed(const QString& gameName, int modID, int fileID);

  static QString getFileTypeString(int fileType);

  void writeData(DownloadInfo* info);

  QString getValidGameShortName(const QString& gameNexusName) const;

private:
  static const int AUTOMATIC_RETRIES = 3;

private:
  NexusInterface* m_NexusInterface;

  OrganizerCore* m_OrganizerCore;
  QWidget* m_ParentWidget;

  QVector<PendingDownload> m_PendingDownloads;

  QVector<DownloadInfo*> m_ActiveDownloads;

  // Secondary index into m_ActiveDownloads keyed by m_DownloadID; kept in sync
  // with every m_ActiveDownloads mutation.
  QHash<unsigned int, DownloadInfo*> m_ByID;

  QString m_OutputDirectory;
  std::set<int> m_RequestIDs;

  DirWatcherManager m_DirWatcher;

  // nesting depth of active ModelResetGuard scopes; see its docs
  int m_modelResetDepth = 0;

  SignalDownloadCallback m_DownloadComplete;
  SignalDownloadCallback m_DownloadPaused;
  SignalDownloadCallback m_DownloadFailed;
  SignalDownloadCallback m_DownloadRemoved;

  std::map<QString, int> m_DownloadFails;

  bool m_ShowHidden;

  MOBase::IPluginGame const* m_ManagedGame;

  QTimer m_TimeoutTimer;
};

#endif  // DOWNLOADMANAGER_H
