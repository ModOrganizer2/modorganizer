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

#include "downloadmanager.h"

#include "bbcode.h"
#include "envfs.h"
#include "filesystemutilities.h"
#include "iplugingame.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "nxmurl.h"
#include "organizercore.h"
#include "selectiondialog.h"
#include "shared/util.h"
#include "utility.h"
#include <nxmurl.h>
#include <report.h>
#include <taskprogressmanager.h>
#include <utility.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFileInfo>
#include <QHttp2Configuration>
#include <QInputDialog>
#include <QMessageBox>
#include <QTextDocument>
#include <QTimer>

#include <boost/bind/bind.hpp>
#include <regex>

using namespace MOBase;

// TODO limit number of downloads, also display download during nxm requests, store
// modid/fileid with downloads

static const char UNFINISHED[] = ".unfinished";

unsigned int DownloadManager::DownloadInfo::s_NextDownloadID = 1U;
int DownloadManager::m_DirWatcherDisabler                    = 0;

DownloadManager::DownloadInfo*
DownloadManager::DownloadInfo::createNew(const ModRepositoryFileInfo* fileInfo,
                                         const QStringList& URLs)
{
  DownloadInfo* info = new DownloadInfo;
  info->m_DownloadID = s_NextDownloadID++;
  info->m_StartTime.start();
  info->m_PreResumeSize  = 0LL;
  info->m_Progress       = std::make_pair<int, QString>(0, "0.0 B/s ");
  info->m_ResumePos      = 0;
  info->m_FileInfo       = new ModRepositoryFileInfo(*fileInfo);
  info->m_Urls           = URLs;
  info->m_CurrentUrl     = 0;
  info->m_Tries          = AUTOMATIC_RETRIES;
  info->m_State          = STATE_STARTED;
  info->m_TaskProgressId = TaskProgressManager::instance().getId();
  info->m_Reply          = nullptr;

  return info;
}

DownloadManager::DownloadInfo*
DownloadManager::DownloadInfo::createFromMeta(const QString& filePath, bool showHidden,
                                              const QString outputDirectory,
                                              std::optional<uint64_t> fileSize)
{
  DownloadInfo* info = new DownloadInfo;

  QString metaFileName = filePath + ".meta";
  QFileInfo metaFileInfo(metaFileName);
  if (QDir::fromNativeSeparators(metaFileInfo.path())
          .compare(QDir::fromNativeSeparators(outputDirectory), Qt::CaseInsensitive) !=
      0)
    return nullptr;
  QSettings metaFile(metaFileName, QSettings::IniFormat);
  if (!showHidden && metaFile.value("removed", false).toBool()) {
    return nullptr;
  } else {
    info->m_Hidden = metaFile.value("removed", false).toBool();
  }

  QString fileName = QFileInfo(filePath).fileName();

  if (fileName.endsWith(UNFINISHED)) {
    info->m_FileName =
        fileName.mid(0, fileName.length() - static_cast<int>(strlen(UNFINISHED)));
    info->m_State = STATE_PAUSED;
  } else {
    info->m_FileName = fileName;

    if (metaFile.value("paused", false).toBool()) {
      info->m_State = STATE_PAUSED;
    } else if (metaFile.value("uninstalled", false).toBool()) {
      info->m_State = STATE_UNINSTALLED;
    } else if (metaFile.value("installed", false).toBool()) {
      info->m_State = STATE_INSTALLED;
    } else {
      info->m_State = STATE_READY;
    }
  }

  info->m_DownloadID = s_NextDownloadID++;
  info->m_Output.setFileName(filePath);
  info->m_TotalSize      = fileSize ? *fileSize : QFileInfo(filePath).size();
  info->m_PreResumeSize  = info->m_TotalSize;
  info->m_CurrentUrl     = 0;
  info->m_Urls           = metaFile.value("url", "").toString().split(";");
  info->m_Tries          = 0;
  info->m_TaskProgressId = TaskProgressManager::instance().getId();
  QString gameName       = metaFile.value("gameName", "").toString();
  int modID              = metaFile.value("modID", 0).toInt();
  int fileID             = metaFile.value("fileID", 0).toInt();
  info->m_FileInfo       = new ModRepositoryFileInfo(gameName, modID, fileID);
  info->m_FileInfo->name = metaFile.value("name", "").toString();
  if (info->m_FileInfo->name == "0") {
    // bug in earlier version
    info->m_FileInfo->name = "";
  }
  info->m_FileInfo->modName     = metaFile.value("modName", "").toString();
  info->m_FileInfo->gameName    = gameName;
  info->m_FileInfo->modID       = modID;
  info->m_FileInfo->fileID      = fileID;
  info->m_FileInfo->description = metaFile.value("description").toString();
  info->m_FileInfo->version.parse(metaFile.value("version", "0").toString());
  info->m_FileInfo->newestVersion.parse(
      metaFile.value("newestVersion", "0").toString());
  info->m_FileInfo->categoryID   = metaFile.value("category", 0).toInt();
  info->m_FileInfo->fileCategory = metaFile.value("fileCategory", 0).toInt();
  info->m_FileInfo->repository   = metaFile.value("repository", "Nexus").toString();
  info->m_FileInfo->userData     = metaFile.value("userData").toMap();
  info->m_Reply                  = nullptr;

  return info;
}

ScopedDisableDirWatcher::ScopedDisableDirWatcher(DownloadManager* downloadManager)
{
  m_downloadManager = downloadManager;
  m_downloadManager->startDisableDirWatcher();
  log::debug("Scoped Disable DirWatcher: Started");
}

ScopedDisableDirWatcher::~ScopedDisableDirWatcher()
{
  m_downloadManager->endDisableDirWatcher();
  m_downloadManager = nullptr;
  log::debug("Scoped Disable DirWatcher: Stopped");
}

void DownloadManager::startDisableDirWatcher()
{
  DownloadManager::m_DirWatcherDisabler++;
}

void DownloadManager::endDisableDirWatcher()
{
  if (DownloadManager::m_DirWatcherDisabler > 0) {
    if (DownloadManager::m_DirWatcherDisabler == 1)
      QCoreApplication::processEvents();
    DownloadManager::m_DirWatcherDisabler--;
  } else {
    DownloadManager::m_DirWatcherDisabler = 0;
  }
}

void DownloadManager::DownloadInfo::setName(QString newName, bool renameFile)
{
  QString oldMetaFileName = QString("%1.meta").arg(m_FileName);
  m_FileName              = QFileInfo(newName).fileName();
  if ((m_State == DownloadManager::STATE_STARTED) ||
      (m_State == DownloadManager::STATE_DOWNLOADING) ||
      (m_State == DownloadManager::STATE_PAUSED)) {
    newName.append(UNFINISHED);
    oldMetaFileName = QString("%1%2.meta").arg(m_FileName).arg(UNFINISHED);
  }
  if (renameFile) {
    if ((newName != m_Output.fileName()) && !m_Output.rename(newName)) {
      reportError(tr("failed to rename \"%1\" to \"%2\"")
                      .arg(m_Output.fileName())
                      .arg(newName));
      return;
    }

    QFile metaFile(QFileInfo(newName).path() + "/" + oldMetaFileName);
    if (metaFile.exists())
      metaFile.rename(newName.mid(0).append(".meta"));
  }
  if (!m_Output.isOpen()) {
    // can't set file name if it's open
    m_Output.setFileName(newName);
  }
}

bool DownloadManager::DownloadInfo::isPausedState()
{
  return m_State == STATE_PAUSED || m_State == STATE_ERROR;
}

QString DownloadManager::DownloadInfo::currentURL()
{
  return m_Urls[m_CurrentUrl];
}

DownloadManager::DownloadManager(NexusInterface* nexusInterface, QObject* parent)
    : m_NexusInterface(nexusInterface), m_DirWatcher(), m_ShowHidden(false),
      m_ParentWidget(nullptr)
{
  m_OrganizerCore = dynamic_cast<OrganizerCore*>(parent);
  connect(&m_DirWatcher, SIGNAL(directoryChanged(QString)), this,
          SLOT(directoryChanged(QString)));
  m_TimeoutTimer.setSingleShot(false);
  // connect(&m_TimeoutTimer, SIGNAL(timeout()), this, SLOT(checkDownloadTimeout()));
  m_TimeoutTimer.start(5 * 1000);
}

DownloadManager::~DownloadManager()
{
  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin();
       iter != m_ActiveDownloads.end(); ++iter) {
    delete *iter;
  }
  m_ActiveDownloads.clear();
}

void DownloadManager::setParentWidget(QWidget* w)
{
  m_ParentWidget = w;
}

bool DownloadManager::downloadsInProgress()
{
  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin();
       iter != m_ActiveDownloads.end(); ++iter) {
    if ((*iter)->m_State < STATE_READY) {
      return true;
    }
  }
  return false;
}

bool DownloadManager::downloadsInProgressNoPause()
{
  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin();
       iter != m_ActiveDownloads.end(); ++iter) {
    if ((*iter)->m_State < STATE_READY && (*iter)->m_State != STATE_PAUSED) {
      return true;
    }
  }
  return false;
}

void DownloadManager::pauseAll()
{

  // first loop: pause all downloads
  for (int i = 0; i < m_ActiveDownloads.count(); ++i) {
    if (m_ActiveDownloads[i]->m_State < STATE_READY) {
      pauseDownload(i);
    }
  }

  ::Sleep(100);

  bool done       = false;
  QTime startTime = QTime::currentTime();
  // further loops: busy waiting for all downloads to complete. This could be neater...
  while (!done && (startTime.secsTo(QTime::currentTime()) < 5)) {
    QCoreApplication::processEvents();
    done = true;
    foreach (DownloadInfo* info, m_ActiveDownloads) {
      if ((info->m_State < STATE_CANCELED) ||
          (info->m_State == STATE_FETCHINGFILEINFO) ||
          (info->m_State == STATE_FETCHINGMODINFO) ||
          (info->m_State == STATE_FETCHINGMODINFO_MD5)) {
        done = false;
        break;
      }
    }
    if (!done) {
      ::Sleep(100);
    }
  }
}

void DownloadManager::setOutputDirectory(const QString& outputDirectory,
                                         const bool refresh)
{
  QStringList directories = m_DirWatcher.directories();
  if (directories.length() != 0) {
    m_DirWatcher.removePaths(directories);
  }
  m_OutputDirectory = QDir::fromNativeSeparators(outputDirectory);
  if (refresh) {
    refreshList();
  }
  m_DirWatcher.addPath(m_OutputDirectory);
}

void DownloadManager::setShowHidden(bool showHidden)
{
  m_ShowHidden = showHidden;
  refreshList();
}

void DownloadManager::setPluginManager(PluginManager* pluginManager)
{
  m_NexusInterface->setPluginManager(pluginManager);
}

void DownloadManager::refreshList()
{
  TimeThis tt("DownloadManager::refreshList()");

  try {
    // avoid triggering other refreshes
    ScopedDisableDirWatcher scopedDirWatcher(this);

    int downloadsBefore = m_ActiveDownloads.size();

    // remove finished downloads
    for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin();
         iter != m_ActiveDownloads.end();) {
      if (((*iter)->m_State == STATE_READY) || ((*iter)->m_State == STATE_INSTALLED) ||
          ((*iter)->m_State == STATE_UNINSTALLED)) {
        delete *iter;
        iter = m_ActiveDownloads.erase(iter);
      } else {
        ++iter;
      }
    }

    const QStringList supportedExtensions =
        m_OrganizerCore->installationManager()->getSupportedExtensions();
    std::vector<std::wstring> nameFilters;
    for (const auto& extension : supportedExtensions) {
      nameFilters.push_back(L"." + extension.toLower().toStdWString());
    }

    nameFilters.push_back(QString(UNFINISHED).toLower().toStdWString());

    QDir dir(QDir::fromNativeSeparators(m_OutputDirectory));

    // find orphaned meta files and delete them (sounds cruel but it's better for
    // everyone)
    QStringList orphans;
    QStringList metaFiles = dir.entryList(QStringList() << "*.meta");
    foreach (const QString& metaFile, metaFiles) {
      QString baseFile = metaFile.left(metaFile.length() - 5);
      if (!QFile::exists(dir.absoluteFilePath(baseFile))) {
        orphans.append(dir.absoluteFilePath(metaFile));
      }
    }
    if (orphans.size() > 0) {
      log::debug("{} orphaned meta files will be deleted", orphans.size());
      shellDelete(orphans, true);
    }

    std::set<std::wstring> seen;

    struct Context
    {
      DownloadManager& self;
      std::set<std::wstring>& seen;
      std::vector<std::wstring>& extensions;
    };

    Context cx = {*this, seen, nameFilters};

    for (auto&& d : m_ActiveDownloads) {
      cx.seen.insert(d->m_FileName.toLower().toStdWString());
      cx.seen.insert(
          QFileInfo(d->m_Output.fileName()).fileName().toLower().toStdWString());
    }

    env::forEachEntry(
        QDir::toNativeSeparators(m_OutputDirectory).toStdWString(), &cx, nullptr,
        nullptr, [](void* data, std::wstring_view f, FILETIME, uint64_t size) {
          auto& cx = *static_cast<Context*>(data);

          std::wstring lc = MOShared::ToLowerCopy(f);

          bool interestingExt = false;
          for (auto&& ext : cx.extensions) {
            if (lc.ends_with(ext)) {
              interestingExt = true;
              break;
            }
          }

          if (!interestingExt) {
            return;
          }

          if (cx.seen.contains(lc)) {
            return;
          }

          QString fileName = QDir::fromNativeSeparators(cx.self.m_OutputDirectory) +
                             "/" + QString::fromWCharArray(f.data(), f.size());

          DownloadInfo* info = DownloadInfo::createFromMeta(
              fileName, cx.self.m_ShowHidden, cx.self.m_OutputDirectory, size);

          if (info == nullptr) {
            return;
          }

          cx.self.m_ActiveDownloads.push_front(info);
          cx.seen.insert(std::move(lc));
          cx.seen.insert(
              QFileInfo(info->m_Output.fileName()).fileName().toLower().toStdWString());
        });

    log::debug("saw {} downloads", m_ActiveDownloads.size());

    emit update(-1);

  } catch (const std::bad_alloc&) {
    reportError(tr("Memory allocation error (in refreshing directory)."));
  }
}

bool DownloadManager::addDownload(const QStringList& URLs, QString gameName, int modID,
                                  int fileID, const ModRepositoryFileInfo* fileInfo)
{
  QString fileName = QFileInfo(URLs.first()).fileName();
  if (fileName.isEmpty()) {
    fileName = "unknown";
  } else {
    fileName = QUrl::fromPercentEncoding(fileName.toUtf8());
  }

  // Temporary URLs for S3-compatible storage are signed for a single method, removing
  // the ability to make HEAD requests to such URLs. We can use the
  // response-content-disposition GET parameter, setting the Content-Disposition header,
  // to predetermine intended file name without a subrequest.
  if (fileName.contains("response-content-disposition=")) {
    std::regex exp("filename=\"(.+)\"");
    std::cmatch result;
    if (std::regex_search(fileName.toStdString().c_str(), result, exp)) {
      fileName = MOBase::sanitizeFileName(QString::fromUtf8(result.str(1).c_str()));
      if (fileName.isEmpty()) {
        fileName = "unknown";
      }
    }
  }

  QUrl preferredUrl = QUrl::fromEncoded(URLs.first().toLocal8Bit());
  log::debug("selected download url: {}", preferredUrl.toString());
  QHttp2Configuration h2Conf;
  h2Conf.setSessionReceiveWindowSize(
      16777215);  // 16 MiB, based on Chrome and Firefox values
  h2Conf.setStreamReceiveWindowSize(16777215);
  QNetworkRequest request(preferredUrl);
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    m_NexusInterface->getAccessManager()->userAgent());
  request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
  request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                       QNetworkRequest::AlwaysNetwork);
  request.setHttp2Configuration(h2Conf);
  return addDownload(m_NexusInterface->getAccessManager()->get(request), URLs, fileName,
                     gameName, modID, fileID, fileInfo);
}

bool DownloadManager::addDownload(QNetworkReply* reply,
                                  const ModRepositoryFileInfo* fileInfo)
{
  QString fileName = getFileNameFromNetworkReply(reply);
  if (fileName.isEmpty()) {
    fileName = "unknown";
  }

  return addDownload(reply, QStringList(reply->url().toString()), fileName,
                     fileInfo->gameName, fileInfo->modID, fileInfo->fileID, fileInfo);
}

bool DownloadManager::addDownload(QNetworkReply* reply, const QStringList& URLs,
                                  const QString& fileName, QString gameName, int modID,
                                  int fileID, const ModRepositoryFileInfo* fileInfo)
{
  // download invoked from an already open network reply (i.e. download link in the
  // browser)
  DownloadInfo* newDownload = DownloadInfo::createNew(fileInfo, URLs);

  QString baseName = fileName;
  if (!fileInfo->fileName.isEmpty()) {
    baseName = fileInfo->fileName;
  } else {
    QString dispoName = getFileNameFromNetworkReply(reply);

    if (!dispoName.isEmpty()) {
      baseName = dispoName;
    }
  }

  // baseName could be a URL at this point so strip out the URL query
  int queryIndex = baseName.indexOf("?");
  if (queryIndex >= 0) {
    baseName.truncate(queryIndex);
  }

  startDisableDirWatcher();
  newDownload->setName(getDownloadFileName(baseName), false);
  endDisableDirWatcher();

  startDownload(reply, newDownload, false);
  //  emit update(-1);
  return true;
}

void DownloadManager::removePending(QString gameName, int modID, int fileID)
{
  QString gameShortName = gameName;
  QStringList games(m_ManagedGame->validShortNames());
  games += m_ManagedGame->gameShortName();
  for (auto game : games) {
    MOBase::IPluginGame* gamePlugin = m_OrganizerCore->getGame(game);
    if (gamePlugin != nullptr &&
        gamePlugin->gameNexusName().compare(gameName, Qt::CaseInsensitive) == 0) {
      gameShortName = gamePlugin->gameShortName();
      break;
    }
  }
  emit aboutToUpdate();
  for (auto iter : m_PendingDownloads) {
    if (gameShortName.compare(std::get<0>(iter), Qt::CaseInsensitive) == 0 &&
        (std::get<1>(iter) == modID) && (std::get<2>(iter) == fileID)) {
      m_PendingDownloads.removeAt(m_PendingDownloads.indexOf(iter));
      break;
    }
  }
  emit update(-1);
}

void DownloadManager::startDownload(QNetworkReply* reply, DownloadInfo* newDownload,
                                    bool resume)
{
  reply->setReadBufferSize(
      1024 * 1024);  // don't read more than 1MB at once to avoid memory troubles
  newDownload->m_Reply = reply;
  setState(newDownload, STATE_DOWNLOADING);
  if (newDownload->m_Urls.count() == 0) {
    newDownload->m_Urls = QStringList(reply->url().toString());
  }

  QIODevice::OpenMode mode = QIODevice::WriteOnly;
  if (resume) {
    mode |= QIODevice::Append;
  }

  newDownload->m_StartTime.start();
  createMetaFile(newDownload);

  if (!newDownload->m_Output.open(mode)) {
    reportError(tr("failed to download %1: could not open output file: %2")
                    .arg(reply->url().toString())
                    .arg(newDownload->m_Output.fileName()));
    return;
  }

  connect(newDownload->m_Reply, SIGNAL(downloadProgress(qint64, qint64)), this,
          SLOT(downloadProgress(qint64, qint64)));
  connect(newDownload->m_Reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)),
          this, SLOT(downloadError(QNetworkReply::NetworkError)));
  connect(newDownload->m_Reply, SIGNAL(readyRead()), this, SLOT(downloadReadyRead()));
  connect(newDownload->m_Reply, SIGNAL(metaDataChanged()), this,
          SLOT(metaDataChanged()));

  if (!resume) {
    newDownload->m_PreResumeSize = newDownload->m_Output.size();
    removePending(newDownload->m_FileInfo->gameName, newDownload->m_FileInfo->modID,
                  newDownload->m_FileInfo->fileID);

    emit aboutToUpdate();
    m_ActiveDownloads.append(newDownload);

    emit update(-1);
    emit downloadAdded();

    if (QFile::exists(m_OutputDirectory + "/" + newDownload->m_FileName)) {
      setState(newDownload, STATE_PAUSING);
      QCoreApplication::processEvents();
      if (QMessageBox::question(
              m_ParentWidget, tr("Download again?"),
              tr("A file with the same name \"%1\" has already been downloaded. "
                 "Do you want to download it again? The new file will receive a "
                 "different name.")
                  .arg(newDownload->m_FileName),
              QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
        if (reply->isFinished())
          setState(newDownload, STATE_CANCELED);
        else
          setState(newDownload, STATE_CANCELING);
      } else {
        startDisableDirWatcher();
        newDownload->setName(getDownloadFileName(newDownload->m_FileName, true), true);
        endDisableDirWatcher();
        if (newDownload->m_State == STATE_PAUSED)
          resumeDownload(indexByInfo(newDownload));
        else
          setState(newDownload, STATE_DOWNLOADING);
      }
    } else
      connect(newDownload->m_Reply, SIGNAL(finished()), this, SLOT(downloadFinished()));

    QCoreApplication::processEvents();

    if (newDownload->m_State != STATE_DOWNLOADING &&
        newDownload->m_State != STATE_READY &&
        newDownload->m_State != STATE_FETCHINGMODINFO && reply->isFinished()) {
      int index = indexByInfo(newDownload);
      if (index >= 0) {
        downloadFinished(index);
      }
      return;
    }
  } else
    connect(newDownload->m_Reply, SIGNAL(finished()), this, SLOT(downloadFinished()));
}

void DownloadManager::addNXMDownload(const QString& url)
{
  NXMUrl nxmInfo(url);

  QStringList validGames;
  MOBase::IPluginGame* foundGame = nullptr;
  validGames.append(m_ManagedGame->gameShortName());
  validGames.append(m_ManagedGame->validShortNames());
  for (auto game : validGames) {
    MOBase::IPluginGame* gamePlugin = m_OrganizerCore->getGame(game);

    // some game plugins give names in validShortNames() that may refer to other
    // plugins, like ttw returning "FalloutNV" and "Fallout3"; if these plugins
    // are not loaded, getGame() might return null
    if (!gamePlugin) {
      // log an error, it's most probably not normal
      log::error("no plugin for game '{}', an antivirus might have deleted it", game);
      continue;
    }

    if (nxmInfo.game().compare(gamePlugin->gameShortName(), Qt::CaseInsensitive) == 0 ||
        nxmInfo.game().compare(gamePlugin->gameNexusName(), Qt::CaseInsensitive) == 0) {
      foundGame = gamePlugin;
      break;
    }
  }
  log::debug("add nxm download: {}", url);
  if (foundGame == nullptr) {
    log::debug("download requested for wrong game (game: {}, url: {})",
               m_ManagedGame->gameShortName(), nxmInfo.game());
    QMessageBox::information(
        m_ParentWidget, tr("Wrong Game"),
        tr("The download link is for a mod for \"%1\" but this instance of MO "
           "has been set up for \"%2\".")
            .arg(nxmInfo.game())
            .arg(m_ManagedGame->gameShortName()),
        QMessageBox::Ok);
    return;
  }

  for (auto tuple : m_PendingDownloads) {
    if (std::get<0>(tuple).compare(foundGame->gameShortName(), Qt::CaseInsensitive) ==
            0,
        std::get<1>(tuple) == nxmInfo.modId() &&
            std::get<2>(tuple) == nxmInfo.fileId()) {
      const auto infoStr =
          tr("There is already a download queued for this file.\n\nMod %1\nFile %2")
              .arg(nxmInfo.modId())
              .arg(nxmInfo.fileId());

      log::debug("download requested is already queued (mod: {}, file: {})",
                 nxmInfo.modId(), nxmInfo.fileId());

      QMessageBox::information(m_ParentWidget, tr("Already Queued"), infoStr,
                               QMessageBox::Ok);
      return;
    }
  }

  for (DownloadInfo* download : m_ActiveDownloads) {
    if (download->m_FileInfo->modID == nxmInfo.modId() &&
        download->m_FileInfo->fileID == nxmInfo.fileId()) {
      if (download->m_State == STATE_DOWNLOADING || download->m_State == STATE_PAUSED ||
          download->m_State == STATE_STARTED) {
        QString debugStr(
            "download requested is already started (mod %1: %2, file %3: %4)");
        QString infoStr(tr("There is already a download started for this file.\n\nMod "
                           "%1:\t%2\nFile %3:\t%4"));

        // %1
        debugStr = debugStr.arg(download->m_FileInfo->modID);
        infoStr  = infoStr.arg(download->m_FileInfo->modID);

        // %2
        if (!download->m_FileInfo->name.isEmpty()) {
          debugStr = debugStr.arg(download->m_FileInfo->name);
          infoStr  = infoStr.arg(download->m_FileInfo->name);
        } else if (!download->m_FileInfo->modName.isEmpty()) {
          debugStr = debugStr.arg(download->m_FileInfo->modName);
          infoStr  = infoStr.arg(download->m_FileInfo->modName);
        } else {
          debugStr = debugStr.arg(QStringLiteral("<blank>"));
          infoStr  = infoStr.arg(QStringLiteral("<blank>"));
        }

        // %3
        debugStr = debugStr.arg(download->m_FileInfo->fileID);
        infoStr  = infoStr.arg(download->m_FileInfo->fileID);

        // %4
        if (!download->m_FileInfo->fileName.isEmpty()) {
          debugStr = debugStr.arg(download->m_FileInfo->fileName);
          infoStr  = infoStr.arg(download->m_FileInfo->fileName);
        } else if (!download->m_FileName.isEmpty()) {
          debugStr = debugStr.arg(download->m_FileName);
          infoStr  = infoStr.arg(download->m_FileName);
        } else {
          debugStr = debugStr.arg(QStringLiteral("<blank>"));
          infoStr  = infoStr.arg(QStringLiteral("<blank>"));
        }

        log::debug("{}", debugStr);
        QMessageBox::information(m_ParentWidget, tr("Already Started"), infoStr,
                                 QMessageBox::Ok);
        return;
      }
    }
  }

  emit aboutToUpdate();

  m_PendingDownloads.append(
      std::make_tuple(foundGame->gameShortName(), nxmInfo.modId(), nxmInfo.fileId()));

  emit update(-1);
  emit downloadAdded();
  ModRepositoryFileInfo* info = new ModRepositoryFileInfo();

  info->nexusKey          = nxmInfo.key();
  info->nexusExpires      = nxmInfo.expires();
  info->nexusDownloadUser = nxmInfo.userId();

  QObject* test = info;
  m_RequestIDs.insert(m_NexusInterface->requestFileInfo(
      foundGame->gameShortName(), nxmInfo.modId(), nxmInfo.fileId(), this,
      QVariant::fromValue(test), ""));
}

void DownloadManager::removeFile(int index, bool deleteFile)
{
  // Avoid triggering refreshes from DirWatcher
  ScopedDisableDirWatcher scopedDirWatcher(this);

  if (index >= m_ActiveDownloads.size()) {
    throw MyException(tr("remove: invalid download index %1").arg(index));
  }

  DownloadInfo* download = m_ActiveDownloads.at(index);
  QString filePath       = m_OutputDirectory + "/" + download->m_FileName;
  if ((download->m_State == STATE_STARTED) ||
      (download->m_State == STATE_DOWNLOADING)) {
    // shouldn't have been possible
    log::error("tried to remove active download");
    return;
  }

  if ((download->m_State == STATE_PAUSED) || (download->m_State == STATE_ERROR)) {
    filePath = download->m_Output.fileName();
  }

  if (deleteFile) {
    if (!shellDelete(QStringList(filePath), true)) {
      reportError(tr("failed to delete %1").arg(filePath));
      return;
    }

    QFile metaFile(filePath.append(".meta"));
    if (metaFile.exists() && !shellDelete(QStringList(filePath), true)) {
      reportError(tr("failed to delete meta file for %1").arg(filePath));
    }
  } else {
    QSettings metaSettings(filePath.append(".meta"), QSettings::IniFormat);
    if (!download->m_Hidden)
      metaSettings.setValue("removed", true);
  }
  m_DownloadRemoved(index);
}

class LessThanWrapper
{
public:
  LessThanWrapper(DownloadManager* manager) : m_Manager(manager) {}
  bool operator()(int LHS, int RHS)
  {
    return m_Manager->getFileName(LHS).compare(m_Manager->getFileName(RHS),
                                               Qt::CaseInsensitive) < 0;
  }

private:
  DownloadManager* m_Manager;
};

bool DownloadManager::ByName(int LHS, int RHS)
{
  return m_ActiveDownloads[LHS]->m_FileName < m_ActiveDownloads[RHS]->m_FileName;
}

void DownloadManager::refreshAlphabeticalTranslation()
{
  m_AlphabeticalTranslation.clear();
  int pos = 0;
  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin();
       iter != m_ActiveDownloads.end(); ++iter, ++pos) {
    m_AlphabeticalTranslation.push_back(pos);
  }

  std::sort(m_AlphabeticalTranslation.begin(), m_AlphabeticalTranslation.end(),
            LessThanWrapper(this));
}

void DownloadManager::restoreDownload(int index)
{

  if (index < 0) {
    DownloadState minState = STATE_READY;
    index                  = 0;

    for (QVector<DownloadInfo*>::const_iterator iter = m_ActiveDownloads.begin();
         iter != m_ActiveDownloads.end(); ++iter) {

      if ((*iter)->m_State >= minState) {
        restoreDownload(index);
      }
      index++;
    }
  } else {
    if (index >= m_ActiveDownloads.size()) {
      throw MyException(tr("restore: invalid download index: %1").arg(index));
    }

    DownloadInfo* download = m_ActiveDownloads.at(index);
    if (download->m_Hidden) {
      download->m_Hidden = false;

      QString filePath = m_OutputDirectory + "/" + download->m_FileName;

      // avoid dirWatcher triggering refreshes
      startDisableDirWatcher();
      QSettings metaSettings(filePath.append(".meta"), QSettings::IniFormat);
      metaSettings.setValue("removed", false);

      endDisableDirWatcher();
    }
  }
}

void DownloadManager::removeDownload(int index, bool deleteFile)
{
  try {
    // avoid dirWatcher triggering refreshes
    ScopedDisableDirWatcher scopedDirWatcher(this);

    emit aboutToUpdate();

    if (index < 0) {
      bool removeAll            = (index == -1);
      DownloadState removeState = (index == -2 ? STATE_INSTALLED : STATE_UNINSTALLED);

      index = 0;
      for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin();
           iter != m_ActiveDownloads.end();) {
        DownloadState downloadState = (*iter)->m_State;
        if ((removeAll && (downloadState >= STATE_READY)) ||
            (removeState == downloadState)) {
          removeFile(index, deleteFile);
          delete *iter;
          iter = m_ActiveDownloads.erase(iter);
        } else {
          ++iter;
          ++index;
        }
      }
    } else {
      if (index >= m_ActiveDownloads.size()) {
        reportError(tr("remove: invalid download index %1").arg(index));
        // emit update(-1);
        return;
      }

      removeFile(index, deleteFile);
      delete m_ActiveDownloads.at(index);
      m_ActiveDownloads.erase(m_ActiveDownloads.begin() + index);
    }
    emit update(-1);
  } catch (const std::exception& e) {
    log::error("failed to remove download: {}", e.what());
  }
  refreshList();
}

void DownloadManager::cancelDownload(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("cancel: invalid download index %1").arg(index));
    return;
  }

  if (m_ActiveDownloads.at(index)->m_State == STATE_DOWNLOADING) {
    setState(m_ActiveDownloads.at(index), STATE_CANCELING);
  }
}

void DownloadManager::pauseDownload(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("pause: invalid download index %1").arg(index));
    return;
  }

  DownloadInfo* info = m_ActiveDownloads.at(index);

  if (info->m_State == STATE_DOWNLOADING) {
    if ((info->m_Reply != nullptr) && (info->m_Reply->isRunning())) {
      setState(info, STATE_PAUSING);
    } else {
      setState(info, STATE_PAUSED);
    }
  } else if ((info->m_State == STATE_FETCHINGMODINFO) ||
             (info->m_State == STATE_FETCHINGFILEINFO) ||
             (info->m_State == STATE_FETCHINGMODINFO_MD5)) {
    setState(info, STATE_READY);
  }
}

void DownloadManager::resumeDownload(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("resume: invalid download index %1").arg(index));
    return;
  }
  DownloadInfo* info = m_ActiveDownloads[index];
  info->m_Tries      = AUTOMATIC_RETRIES;
  resumeDownloadInt(index);
}

void DownloadManager::resumeDownloadInt(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("resume (int): invalid download index %1").arg(index));
    return;
  }
  DownloadInfo* info = m_ActiveDownloads[index];

  // Check for finished download;
  if (info->m_TotalSize <= info->m_Output.size() && info->m_Reply != nullptr &&
      info->m_Reply->isFinished() && info->m_State != STATE_ERROR) {
    setState(info, STATE_DOWNLOADING);
    downloadFinished(index);
    return;
  }

  if (info->isPausedState() || info->m_State == STATE_PAUSING) {
    if (info->m_State == STATE_PAUSING) {
      if (info->m_Output.isOpen()) {
        writeData(info);
        if (info->m_State == STATE_PAUSING) {
          setState(info, STATE_PAUSED);
        }
      }
    }
    if ((info->m_Urls.size() == 0) ||
        ((info->m_Urls.size() == 1) && (info->m_Urls[0].size() == 0))) {
      emit showMessage(
          tr("No known download urls. Sorry, this download can't be resumed."));
      return;
    }
    if (info->m_State == STATE_ERROR) {
      info->m_CurrentUrl = (info->m_CurrentUrl + 1) % info->m_Urls.count();
    }
    log::debug("request resume from url {}", info->currentURL());
    QNetworkRequest request(QUrl::fromEncoded(info->currentURL().toLocal8Bit()));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      m_NexusInterface->getAccessManager()->userAgent());
    if (info->m_State != STATE_ERROR) {
      info->m_ResumePos      = info->m_Output.size();
      QByteArray rangeHeader = "bytes=" + QByteArray::number(info->m_ResumePos) + "-";
      request.setRawHeader("Range", rangeHeader);
    }
    info->m_DownloadLast     = 0;
    info->m_DownloadTimeLast = 0;
    info->m_DownloadAcc      = accumulator_set<qint64, stats<tag::rolling_mean>>(
        tag::rolling_window::window_size = 200);
    info->m_DownloadTimeAcc = accumulator_set<qint64, stats<tag::rolling_mean>>(
        tag::rolling_window::window_size = 200);
    log::debug("resume at {} bytes", info->m_ResumePos);
    startDownload(m_NexusInterface->getAccessManager()->get(request), info, true);
  }
  emit update(index);
}

DownloadManager::DownloadInfo* DownloadManager::downloadInfoByID(unsigned int id)
{
  auto iter = std::find_if(m_ActiveDownloads.begin(), m_ActiveDownloads.end(),
                           [id](DownloadInfo* info) {
                             return info->m_DownloadID == id;
                           });
  if (iter != m_ActiveDownloads.end()) {
    return *iter;
  } else {
    return nullptr;
  }
}

void DownloadManager::queryInfo(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("query: invalid download index %1").arg(index));
    return;
  }
  DownloadInfo* info = m_ActiveDownloads[index];

  if (info->m_FileInfo->repository != "Nexus") {
    log::warn("re-querying file info is currently only possible with Nexus");
    return;
  }

  if (info->m_State < DownloadManager::STATE_READY) {
    // UI shouldn't allow this
    return;
  }

  if (info->m_FileInfo->modID <= 0) {
    QString fileName = getFileName(index);
    QString ignore;
    NexusInterface::interpretNexusFileName(fileName, ignore, info->m_FileInfo->modID,
                                           true);
    if (info->m_FileInfo->modID < 0) {
      bool ok   = false;
      int modId = QInputDialog::getInt(nullptr, tr("Please enter the nexus mod id"),
                                       tr("Mod ID:"), 1, 1,
                                       std::numeric_limits<int>::max(), 1, &ok);
      // careful now: while the dialog was displayed, events were processed.
      // the download list might have changed and our info-ptr invalidated.
      if (ok)
        m_ActiveDownloads[index]->m_FileInfo->modID = modId;
      return;
    }
  }

  if (info->m_FileInfo->gameName.size() == 0) {
    SelectionDialog selection(
        tr("Please select the source game code for %1").arg(getFileName(index)));

    std::vector<std::pair<QString, QString>> choices =
        m_NexusInterface->getGameChoices(m_ManagedGame);
    if (choices.size() == 1) {
      info->m_FileInfo->gameName = choices[0].first;
    } else {
      for (auto choice : choices) {
        selection.addChoice(choice.first, choice.second, choice.first);
      }
      if (selection.exec() == QDialog::Accepted) {
        info->m_FileInfo->gameName = selection.getChoiceData().toString();
      } else {
        info->m_FileInfo->gameName = m_ManagedGame->gameShortName();
      }
    }
  }
  info->m_ReQueried = true;
  setState(info, STATE_FETCHINGMODINFO);
}

void DownloadManager::queryInfoMd5(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("query: invalid download index %1").arg(index));
    return;
  }
  DownloadInfo* info = m_ActiveDownloads[index];

  if (info->m_FileInfo->repository != "Nexus") {
    log::warn("re-querying file info is currently only possible with Nexus");
    return;
  }

  if (info->m_State < DownloadManager::STATE_READY) {
    // UI shouldn't allow this
    return;
  }

  info->m_GamesToQuery << m_ManagedGame->gameShortName();
  info->m_GamesToQuery << m_ManagedGame->validShortNames();

  QFile downloadFile(info->m_FileName);
  if (!downloadFile.exists()) {
    downloadFile.setFileName(m_OrganizerCore->downloadsPath() + "\\" +
                             info->m_FileName);
  }
  if (!downloadFile.exists()) {
    log::error("Can't find download file '{}'", info->m_FileName);
    return;
  }
  if (!downloadFile.open(QIODevice::ReadOnly)) {
    log::error("Can't open download file '{}'", info->m_FileName);
    return;
  }

  QCryptographicHash hash(QCryptographicHash::Md5);
  const qint64 progressStep = 10 * 1024 * 1024;
  QProgressDialog progress(tr("Hashing download file '%1'").arg(info->m_FileName),
                           tr("Cancel"), 0, downloadFile.size() / progressStep);
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(1000);

  for (qint64 i = 0; i < downloadFile.size(); i += progressStep) {
    progress.setValue(progress.value() + 1);
    if (progress.wasCanceled()) {
      break;
    }
    hash.addData(downloadFile.read(progressStep));
  }
  if (progress.wasCanceled()) {
    downloadFile.close();
    return;
  }

  progress.close();
  downloadFile.close();

  info->m_Hash      = hash.result();
  info->m_ReQueried = true;
  setState(info, STATE_FETCHINGMODINFO_MD5);
}

void DownloadManager::visitOnNexus(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("VisitNexus: invalid download index %1").arg(index));
    return;
  }
  DownloadInfo* info = m_ActiveDownloads[index];

  if (info->m_FileInfo->repository != "Nexus") {
    log::warn("Visiting mod page is currently only possible with Nexus");
    return;
  }

  if (info->m_State < DownloadManager::STATE_READY) {
    // UI shouldn't allow this
    return;
  }
  int modID = info->m_FileInfo->modID;

  QString gameName = info->m_FileInfo->gameName;
  if (modID > 0) {
    shell::Open(QUrl(m_NexusInterface->getModURL(modID, gameName)));
  } else {
    emit showMessage(tr("Nexus ID for this Mod is unknown"));
  }
}

void DownloadManager::openFile(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("OpenFile: invalid download index %1").arg(index));
    return;
  }

  QDir path = QDir(m_OutputDirectory);
  if (path.exists(getFileName(index))) {
    shell::Open(getFilePath(index));
    return;
  }

  shell::Explore(m_OutputDirectory);
  return;
}

void DownloadManager::openMetaFile(int index)
{
  if (index < 0 || index >= m_ActiveDownloads.size()) {
    log::error("OpenMetaFile: invalid download index {}", index);
    return;
  }

  const auto path     = QDir(m_OutputDirectory);
  const auto filePath = getFilePath(index);
  const auto metaPath = filePath + ".meta";

  if (path.exists(metaPath)) {
    shell::Open(metaPath);
    return;
  } else {
    QSettings metaFile(metaPath, QSettings::IniFormat);
    metaFile.setValue("removed", false);
  }

  if (path.exists(metaPath)) {
    shell::Open(metaPath);
    return;
  }

  shell::Explore(filePath);
}

void DownloadManager::openInDownloadsFolder(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("OpenFileInDownloadsFolder: invalid download index %1").arg(index));
    return;
  }

  const auto path = getFilePath(index);

  if (QFile::exists(path)) {
    shell::Explore(path);
    return;
  } else {
    const auto unfinished = path + ".unfinished";
    if (QFile::exists(unfinished)) {
      shell::Explore(unfinished);
      return;
    }
  }

  shell::Explore(m_OutputDirectory);
}

int DownloadManager::numTotalDownloads() const
{
  return m_ActiveDownloads.size();
}

int DownloadManager::numPendingDownloads() const
{
  return m_PendingDownloads.size();
}

std::tuple<QString, int, int> DownloadManager::getPendingDownload(int index)
{
  if ((index < 0) || (index >= m_PendingDownloads.size())) {
    throw MyException(tr("get pending: invalid download index %1").arg(index));
  }

  return m_PendingDownloads.at(index);
}

QString DownloadManager::getFilePath(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("get path: invalid download index %1").arg(index));
  }

  return m_OutputDirectory + "/" + m_ActiveDownloads.at(index)->m_FileName;
}

QString DownloadManager::getFileTypeString(int fileType)
{
  switch (fileType) {
  case 1:
    return tr("Main");
  case 2:
    return tr("Update");
  case 3:
    return tr("Optional");
  case 4:
    return tr("Old");
  case 5:
    return tr("Miscellaneous");
  case 6:
    return tr("Deleted");
  case 7:
    return tr("Archived");
  default:
    return tr("Unknown");
  }
}

QString DownloadManager::getDisplayName(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("display name: invalid download index %1").arg(index));
  }

  DownloadInfo* info = m_ActiveDownloads.at(index);

  QTextDocument doc;
  if (!info->m_FileInfo->name.isEmpty()) {
    doc.setHtml(info->m_FileInfo->name);
    return QString("%1 (%2, v%3)")
        .arg(doc.toPlainText())
        .arg(getFileTypeString(info->m_FileInfo->fileCategory))
        .arg(info->m_FileInfo->version.displayString());
  } else {
    doc.setHtml(info->m_FileName);
    return doc.toPlainText();
  }
}

QString DownloadManager::getFileName(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("file name: invalid download index %1").arg(index));
  }

  return m_ActiveDownloads.at(index)->m_FileName;
}

QDateTime DownloadManager::getFileTime(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("file time: invalid download index %1").arg(index));
  }

  DownloadInfo* info = m_ActiveDownloads.at(index);
  if (!info->m_Created.isValid()) {
    QFileInfo fileInfo(info->m_Output);
    info->m_Created = fileInfo.birthTime();
    if (!info->m_Created.isValid())
      info->m_Created = fileInfo.metadataChangeTime();
    if (!info->m_Created.isValid())
      info->m_Created = fileInfo.lastModified();
  }

  return info->m_Created;
}

qint64 DownloadManager::getFileSize(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("file size: invalid download index %1").arg(index));
  }

  return m_ActiveDownloads.at(index)->m_TotalSize;
}

std::pair<int, QString> DownloadManager::getProgress(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("progress: invalid download index %1").arg(index));
  }

  return m_ActiveDownloads.at(index)->m_Progress;
}

DownloadManager::DownloadState DownloadManager::getState(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("state: invalid download index %1").arg(index));
  }

  return m_ActiveDownloads.at(index)->m_State;
}

bool DownloadManager::isInfoIncomplete(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("infocomplete: invalid download index %1").arg(index));
  }

  DownloadInfo* info = m_ActiveDownloads.at(index);
  if (info->m_FileInfo->repository != "Nexus") {
    // other repositories currently don't support re-querying info anyway
    return false;
  }
  return (info->m_FileInfo->fileID == 0) || (info->m_FileInfo->modID == 0);
}

int DownloadManager::getModID(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("mod id: invalid download index %1").arg(index));
  }
  return m_ActiveDownloads.at(index)->m_FileInfo->modID;
}

QString DownloadManager::getDisplayGameName(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("mod id: invalid download index %1").arg(index));
  }
  QString gameName        = m_ActiveDownloads.at(index)->m_FileInfo->gameName;
  IPluginGame* gamePlugin = m_OrganizerCore->getGame(gameName);
  if (gamePlugin) {
    gameName = gamePlugin->gameName();
  }
  return gameName;
}

QString DownloadManager::getGameName(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("mod id: invalid download index %1").arg(index));
  }
  return m_ActiveDownloads.at(index)->m_FileInfo->gameName;
}

bool DownloadManager::isHidden(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("ishidden: invalid download index %1").arg(index));
  }
  return m_ActiveDownloads.at(index)->m_Hidden;
}

const ModRepositoryFileInfo* DownloadManager::getFileInfo(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("file info: invalid download index %1").arg(index));
  }

  return m_ActiveDownloads.at(index)->m_FileInfo;
}

void DownloadManager::markInstalled(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("mark installed: invalid download index %1").arg(index));
  }

  // Avoid triggering refreshes from DirWatcher
  ScopedDisableDirWatcher scopedDirWatcher(this);

  DownloadInfo* info = m_ActiveDownloads.at(index);
  QSettings metaFile(info->m_Output.fileName() + ".meta", QSettings::IniFormat);
  metaFile.setValue("installed", true);
  metaFile.setValue("uninstalled", false);

  setState(m_ActiveDownloads.at(index), STATE_INSTALLED);
}

void DownloadManager::markInstalled(QString fileName)
{
  int index = indexByName(fileName);
  if (index >= 0) {
    markInstalled(index);
  } else {
    DownloadInfo* info = getDownloadInfo(fileName);
    if (info != nullptr) {
      // Avoid triggering refreshes from DirWatcher
      ScopedDisableDirWatcher scopedDirWatcher(this);

      QSettings metaFile(info->m_Output.fileName() + ".meta", QSettings::IniFormat);
      metaFile.setValue("installed", true);
      metaFile.setValue("uninstalled", false);
      delete info;
    }
  }
}

DownloadManager::DownloadInfo* DownloadManager::getDownloadInfo(QString fileName)
{
  return DownloadInfo::createFromMeta(fileName, true, m_OutputDirectory);
}

void DownloadManager::markUninstalled(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("mark uninstalled: invalid download index %1").arg(index));
  }

  // Avoid triggering refreshes from DirWatcher
  ScopedDisableDirWatcher scopedDirWatcher(this);

  DownloadInfo* info = m_ActiveDownloads.at(index);
  QSettings metaFile(info->m_Output.fileName() + ".meta", QSettings::IniFormat);
  metaFile.setValue("uninstalled", true);

  setState(m_ActiveDownloads.at(index), STATE_UNINSTALLED);
}

void DownloadManager::markUninstalled(QString fileName)
{
  int index = indexByName(fileName);
  if (index >= 0) {
    markUninstalled(index);
  } else {
    QString filePath   = QDir::fromNativeSeparators(m_OutputDirectory) + "/" + fileName;
    DownloadInfo* info = getDownloadInfo(filePath);
    if (info != nullptr) {

      // Avoid triggering refreshes from DirWatcher
      ScopedDisableDirWatcher scopedDirWatcher(this);

      QSettings metaFile(info->m_Output.fileName() + ".meta", QSettings::IniFormat);
      metaFile.setValue("uninstalled", true);
      delete info;
    }
  }
}

QString DownloadManager::getDownloadFileName(const QString& baseName, bool rename) const
{
  QString fullPath = m_OutputDirectory + "/" + MOBase::sanitizeFileName(baseName);
  if (QFile::exists(fullPath) && rename) {
    int i = 1;
    while (QFile::exists(
        QString("%1/%2_%3").arg(m_OutputDirectory).arg(i).arg(baseName))) {
      ++i;
    }

    fullPath = QString("%1/%2_%3").arg(m_OutputDirectory).arg(i).arg(baseName);
  }
  return fullPath;
}

QString DownloadManager::getFileNameFromNetworkReply(QNetworkReply* reply)
{
  if (reply->hasRawHeader("Content-Disposition")) {
    std::regex exp("filename=\"(.+)\"");

    std::cmatch result;
    if (std::regex_search(reply->rawHeader("Content-Disposition").constData(), result,
                          exp)) {
      return MOBase::sanitizeFileName(QString::fromUtf8(result.str(1).c_str()));
    }
  }

  return QString();
}

void DownloadManager::setState(DownloadManager::DownloadInfo* info,
                               DownloadManager::DownloadState state)
{
  int row = 0;
  for (int i = 0; i < m_ActiveDownloads.size(); ++i) {
    if (m_ActiveDownloads[i] == info) {
      row = i;
      break;
    }
  }
  info->m_State = state;
  switch (state) {
  case STATE_PAUSED: {
    info->m_Reply->abort();
    info->m_Output.close();
    m_DownloadPaused(row);
  } break;
  case STATE_ERROR: {
    info->m_Reply->abort();
    info->m_Output.close();
    m_DownloadFailed(row);
  } break;
  case STATE_CANCELED: {
    info->m_Reply->abort();
    m_DownloadFailed(row);
  } break;
  case STATE_FETCHINGMODINFO: {
    m_RequestIDs.insert(m_NexusInterface->requestDescription(
        info->m_FileInfo->gameName, info->m_FileInfo->modID, this, info->m_DownloadID,
        QString()));
  } break;
  case STATE_FETCHINGFILEINFO: {
    m_RequestIDs.insert(m_NexusInterface->requestFiles(info->m_FileInfo->gameName,
                                                       info->m_FileInfo->modID, this,
                                                       info->m_DownloadID, QString()));
  } break;
  case STATE_FETCHINGMODINFO_MD5: {
    log::debug("Searching {} for MD5 of {}", info->m_GamesToQuery[0],
               QString(info->m_Hash.toHex()));
    m_RequestIDs.insert(m_NexusInterface->requestInfoFromMd5(
        info->m_GamesToQuery[0], info->m_Hash, this, info->m_DownloadID, QString()));
  } break;
  case STATE_READY: {
    createMetaFile(info);
    m_DownloadComplete(row);
  } break;
  default: /* NOP */
    break;
  }
  emit stateChanged(row, state);
}

DownloadManager::DownloadInfo* DownloadManager::findDownload(QObject* reply,
                                                             int* index) const
{
  // reverse search as newer, thus more relevant, downloads are at the end
  for (int i = m_ActiveDownloads.size() - 1; i >= 0; --i) {
    if (m_ActiveDownloads[i]->m_Reply == reply) {
      if (index != nullptr) {
        *index = i;
      }
      return m_ActiveDownloads[i];
    }
  }
  return nullptr;
}

void DownloadManager::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
  if (bytesTotal == 0) {
    return;
  }
  int index = 0;
  try {
    DownloadInfo* info = findDownload(this->sender(), &index);
    if (info != nullptr) {
      info->m_HasData = true;
      if (info->m_State == STATE_CANCELING) {
        setState(info, STATE_CANCELED);
      } else if (info->m_State == STATE_PAUSING) {
        setState(info, STATE_PAUSED);
      } else {
        if (bytesTotal > info->m_TotalSize) {
          info->m_TotalSize = bytesTotal;
        }
        int oldProgress        = info->m_Progress.first;
        info->m_Progress.first = ((info->m_ResumePos + bytesReceived) * 100) /
                                 (info->m_ResumePos + bytesTotal);

        qint64 elapsed = info->m_StartTime.elapsed();
        info->m_DownloadAcc(bytesReceived - info->m_DownloadLast);
        info->m_DownloadLast = bytesReceived;
        info->m_DownloadTimeAcc(elapsed - info->m_DownloadTimeLast);
        info->m_DownloadTimeLast = elapsed;

        // calculate the download speed
        const double speed = rolling_mean(info->m_DownloadAcc) /
                             (rolling_mean(info->m_DownloadTimeAcc) / 1000.0);
        ;

        const qint64 remaining = (bytesTotal - bytesReceived) / speed * 1000;

        info->m_Progress.second = tr("%1% - %2 - ~%3")
                                      .arg(info->m_Progress.first)
                                      .arg(MOBase::localizedByteSpeed(speed))
                                      .arg(MOBase::localizedTimeRemaining(remaining));

        TaskProgressManager::instance().updateProgress(info->m_TaskProgressId,
                                                       bytesReceived, bytesTotal);
        emit update(index);
      }
    }
  } catch (const std::bad_alloc&) {
    reportError(tr("Memory allocation error (in processing progress event)."));
  }
}

void DownloadManager::downloadReadyRead()
{
  try {
    writeData(findDownload(this->sender()));
  } catch (const std::bad_alloc&) {
    reportError(tr("Memory allocation error (in processing downloaded data)."));
  }
}

void DownloadManager::createMetaFile(DownloadInfo* info)
{
  // Avoid triggering refreshes from DirWatcher
  ScopedDisableDirWatcher scopedDirWatcher(this);

  QSettings metaFile(QString("%1.meta").arg(info->m_Output.fileName()),
                     QSettings::IniFormat);
  metaFile.setValue("gameName", info->m_FileInfo->gameName);
  metaFile.setValue("modID", info->m_FileInfo->modID);
  metaFile.setValue("fileID", info->m_FileInfo->fileID);
  metaFile.setValue("url", info->m_Urls.join(";"));
  metaFile.setValue("name", info->m_FileInfo->name);
  metaFile.setValue("description", info->m_FileInfo->description);
  metaFile.setValue("modName", info->m_FileInfo->modName);
  metaFile.setValue("version", info->m_FileInfo->version.canonicalString());
  metaFile.setValue("newestVersion", info->m_FileInfo->newestVersion.canonicalString());
  metaFile.setValue("fileTime", info->m_FileInfo->fileTime);
  metaFile.setValue("fileCategory", info->m_FileInfo->fileCategory);
  metaFile.setValue("category", info->m_FileInfo->categoryID);
  metaFile.setValue("repository", info->m_FileInfo->repository);
  metaFile.setValue("userData", info->m_FileInfo->userData);
  metaFile.setValue("installed", info->m_State == DownloadManager::STATE_INSTALLED);
  metaFile.setValue("uninstalled", info->m_State == DownloadManager::STATE_UNINSTALLED);
  metaFile.setValue("paused", (info->m_State == DownloadManager::STATE_PAUSED) ||
                                  (info->m_State == DownloadManager::STATE_ERROR));
  metaFile.setValue("removed", info->m_Hidden);

  // slightly hackish...
  for (int i = 0; i < m_ActiveDownloads.size(); ++i) {
    if (m_ActiveDownloads[i] == info) {
      emit update(i);
    }
  }
}

void DownloadManager::nxmDescriptionAvailable(QString, int, QVariant userData,
                                              QVariant resultData, int requestID)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  QVariantMap result = resultData.toMap();

  DownloadInfo* info = downloadInfoByID(userData.toInt());
  if (info == nullptr)
    return;
  info->m_FileInfo->categoryID = result["category_id"].toInt();
  QTextDocument doc;
  doc.setHtml(result["name"].toString().trimmed());
  info->m_FileInfo->modName = doc.toPlainText();
  if (info->m_FileInfo->fileID != 0) {
    setState(info, STATE_READY);
  } else {
    setState(info, STATE_FETCHINGFILEINFO);
  }
}

void DownloadManager::nxmFilesAvailable(QString, int, QVariant userData,
                                        QVariant resultData, int requestID)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  DownloadInfo* info = downloadInfoByID(userData.toInt());
  if (info == nullptr)
    return;

  QVariantMap result = resultData.toMap();
  QVariantList files = result["files"].toList();

  // MO sometimes prepends <digit>_ to the filename in case of duplicate downloads.
  // this may muck up the file name comparison
  QString alternativeLocalName = info->m_FileName;

  QRegularExpression expression("^\\d_(.*)$");
  auto match = expression.match(alternativeLocalName);
  if (match.hasMatch()) {
    alternativeLocalName = match.captured(1);
  }

  bool found = false;

  for (QVariant file : files) {
    QVariantMap fileInfo    = file.toMap();
    QString fileName        = fileInfo["file_name"].toString();
    QString fileNameVariant = fileName.mid(0).replace(' ', '_');
    if ((fileName == info->m_RemoteFileName) ||
        (fileNameVariant == info->m_RemoteFileName) || (fileName == info->m_FileName) ||
        (fileNameVariant == info->m_FileName) || (fileName == alternativeLocalName) ||
        (fileNameVariant == alternativeLocalName)) {
      info->m_FileInfo->name = fileInfo["name"].toString();
      info->m_FileInfo->version.parse(fileInfo["version"].toString());
      if (!info->m_FileInfo->version.isValid()) {
        info->m_FileInfo->version = info->m_FileInfo->newestVersion;
      }
      info->m_FileInfo->fileCategory = fileInfo["category_id"].toInt();
      info->m_FileInfo->fileTime =
          QDateTime::fromMSecsSinceEpoch(fileInfo["uploaded_timestamp"].toLongLong());
      info->m_FileInfo->fileID   = fileInfo["file_id"].toInt();
      info->m_FileInfo->fileName = fileInfo["file_name"].toString();
      info->m_FileInfo->description =
          BBCode::convertToHTML(fileInfo["description"].toString());
      found = true;
      break;
    }
  }

  if (info->m_ReQueried) {
    if (found) {
      emit showMessage(tr("Information updated"));
    } else if (result.count() == 0) {
      emit showMessage(tr("No matching file found on Nexus! Maybe this file is no "
                          "longer available or it was renamed?"));
    } else {
      SelectionDialog selection(tr("No file on Nexus matches the selected file by "
                                   "name. Please manually choose the correct one."));
      std::sort(files.begin(), files.end(),
                [](const QVariant& lhs, const QVariant& rhs) {
                  return lhs.toMap()["uploaded_timestamp"].toInt() >
                         rhs.toMap()["uploaded_timestamp"].toInt();
                });
      for (QVariant file : files) {
        QVariantMap fileInfo = file.toMap();
        if (fileInfo["category_id"].toInt() != NexusInterface::FileStatus::REMOVED &&
            fileInfo["category_id"].toInt() != NexusInterface::FileStatus::ARCHIVED)
          selection.addChoice(fileInfo["file_name"].toString(), "", file);
      }
      if (selection.exec() == QDialog::Accepted) {
        QVariantMap fileInfo   = selection.getChoiceData().toMap();
        info->m_FileInfo->name = fileInfo["name"].toString();
        info->m_FileInfo->version.parse(fileInfo["version"].toString());
        info->m_FileInfo->fileCategory = fileInfo["category_id"].toInt();
        info->m_FileInfo->fileID       = fileInfo["file_id"].toInt();
      } else {
        emit showMessage(tr("No matching file found on Nexus! Maybe this file is no "
                            "longer available or it was renamed?"));
      }
    }
  } else {
    if (info->m_FileInfo->fileID == 0) {
      log::warn("could not determine file id for {} (state {})", info->m_FileName,
                info->m_State);
    }
  }

  setState(info, STATE_READY);
}

void DownloadManager::nxmFileInfoAvailable(QString gameName, int modID, int fileID,
                                           QVariant userData, QVariant resultData,
                                           int requestID)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  ModRepositoryFileInfo* info =
      qobject_cast<ModRepositoryFileInfo*>(qvariant_cast<QObject*>(userData));

  QVariantMap result = resultData.toMap();
  info->name         = result["name"].toString();
  info->version.parse(result["version"].toString());
  if (!info->version.isValid()) {
    info->version = info->newestVersion;
  }
  info->fileName     = result["file_name"].toString();
  info->fileCategory = result["category_id"].toInt();
  info->fileTime =
      QDateTime::fromMSecsSinceEpoch(result["uploaded_timestamp"].toLongLong());
  info->description = BBCode::convertToHTML(result["description"].toString());

  info->repository = "Nexus";

  QStringList games(m_ManagedGame->validShortNames());
  games += m_ManagedGame->gameShortName();
  for (auto game : games) {
    MOBase::IPluginGame* gamePlugin = m_OrganizerCore->getGame(game);
    if (gamePlugin != nullptr &&
        gamePlugin->gameNexusName().compare(gameName, Qt::CaseInsensitive) == 0) {
      info->gameName = gamePlugin->gameShortName();
    }
  }

  info->modID  = modID;
  info->fileID = fileID;

  QObject* test = info;
  m_RequestIDs.insert(
      m_NexusInterface->requestDownloadURL(info->gameName, info->modID, info->fileID,
                                           this, QVariant::fromValue(test), QString()));
}

static int evaluateFileInfoMap(const QVariantMap& map,
                               const ServerList::container& preferredServers)
{
  int preference  = 0;
  bool found      = false;
  const auto name = map["short_name"].toString();

  for (const auto& server : preferredServers) {
    if (server.name() == name) {
      preference = server.preferred();
      found      = true;
      break;
    }
  }

  if (!found) {
    return 0;
  }

  return 100 + preference * 20;
}

// sort function to sort by best download server
//
bool ServerByPreference(const ServerList::container& preferredServers,
                        const QVariant& LHS, const QVariant& RHS)
{
  const auto a = evaluateFileInfoMap(LHS.toMap(), preferredServers);
  const auto b = evaluateFileInfoMap(RHS.toMap(), preferredServers);
  return (a > b);
}

int DownloadManager::startDownloadURLs(const QStringList& urls)
{
  ModRepositoryFileInfo info;
  addDownload(urls, "", -1, -1, &info);
  return m_ActiveDownloads.size() - 1;
}

int DownloadManager::startDownloadNexusFile(int modID, int fileID)
{
  int newID = m_ActiveDownloads.size();
  addNXMDownload(QString("nxm://%1/mods/%2/files/%3")
                     .arg(m_ManagedGame->gameShortName())
                     .arg(modID)
                     .arg(fileID));
  return newID;
}

QString DownloadManager::downloadPath(int id)
{
  return getFilePath(id);
}

boost::signals2::connection
DownloadManager::onDownloadComplete(const std::function<void(int)>& callback)
{
  return m_DownloadComplete.connect(callback);
}

boost::signals2::connection
DownloadManager::onDownloadPaused(const std::function<void(int)>& callback)
{
  return m_DownloadPaused.connect(callback);
}

boost::signals2::connection
DownloadManager::onDownloadFailed(const std::function<void(int)>& callback)
{
  return m_DownloadFailed.connect(callback);
}

boost::signals2::connection
DownloadManager::onDownloadRemoved(const std::function<void(int)>& callback)
{
  return m_DownloadRemoved.connect(callback);
}

int DownloadManager::indexByName(const QString& fileName) const
{
  for (int i = 0; i < m_ActiveDownloads.size(); ++i) {
    if (m_ActiveDownloads[i]->m_FileName == fileName) {
      return i;
    }
  }
  return -1;
}

int DownloadManager::indexByInfo(const DownloadInfo* info) const
{
  for (int i = 0; i < m_ActiveDownloads.size(); ++i) {
    if (m_ActiveDownloads[i] == info) {
      return i;
    }
  }
  return -1;
}

void DownloadManager::nxmDownloadURLsAvailable(QString gameName, int modID, int fileID,
                                               QVariant userData, QVariant resultData,
                                               int requestID)
{
  using namespace boost::placeholders;

  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  ModRepositoryFileInfo* info =
      qobject_cast<ModRepositoryFileInfo*>(qvariant_cast<QObject*>(userData));
  QVariantList resultList = resultData.toList();
  if (resultList.length() == 0) {
    removePending(gameName, modID, fileID);
    emit showMessage(tr("No download server available. Please try again later."));
    return;
  }

  const auto servers = m_OrganizerCore->settings().network().servers();

  std::sort(resultList.begin(), resultList.end(),
            boost::bind(&ServerByPreference, servers.getPreferred(), _1, _2));

  info->userData["downloadMap"] = resultList;

  QStringList URLs;

  foreach (const QVariant& server, resultList) {
    URLs.append(server.toMap()["URI"].toString());
  }
  addDownload(URLs, gameName, modID, fileID, info);
}

void DownloadManager::nxmFileInfoFromMd5Available(QString gameName, QVariant userData,
                                                  QVariant resultData, int requestID)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  DownloadInfo* info = downloadInfoByID(userData.toInt());

  // This can come back with multiple results with the same file was uploaded multiple
  // times (for whatever reason)
  auto resultlist = resultData.toList();
  int chosenIdx   = resultlist.count() > 1 ? -1 : 0;

  // Look for the exact file name
  if (chosenIdx < 0) {
    for (int i = 0; i < resultlist.count(); i++) {
      auto results     = resultlist[i].toMap();
      auto fileDetails = results["file_details"].toMap();

      if (fileDetails["file_name"].toString().compare(info->m_FileName,
                                                      Qt::CaseInsensitive) == 0) {
        if (chosenIdx < 0) {
          chosenIdx = i;  // intentional to not break in order to check other results
        } else {
          log::debug("Multiple files with same name found during MD5 search.");
          chosenIdx = -1;
          break;
        }
      }
    }
  }

  // Look for the only active one
  if (chosenIdx < 0) {
    for (int i = 0; i < resultlist.count(); i++) {
      auto results     = resultlist[i].toMap();
      auto fileDetails = results["file_details"].toMap();

      if (fileDetails["category_id"].toInt() != NexusInterface::FileStatus::REMOVED &&
          fileDetails["category_id"].toInt() != NexusInterface::FileStatus::ARCHIVED) {
        if (chosenIdx < 0) {
          chosenIdx = i;  // intentional to not break in order to check other results
        } else {
          log::debug("Multiple active files found during MD5 search.");
          chosenIdx = -1;
          break;
        }
      }
    }
  }

  // Unable to determine the correct mod / file.  Revert to the old method
  if (chosenIdx < 0) {
    // don't use the normal state set function as we don't want to create a meta file
    info->m_State = DownloadManager::STATE_READY;
    queryInfo(m_ActiveDownloads.indexOf(info));
    return;
  }

  auto results     = resultlist[chosenIdx].toMap();
  auto fileDetails = results["file_details"].toMap();
  auto modDetails  = results["mod"].toMap();

  info->m_FileInfo->name   = fileDetails["name"].toString();
  info->m_FileInfo->fileID = fileDetails["file_id"].toInt();
  info->m_FileInfo->description =
      BBCode::convertToHTML(fileDetails["description"].toString());
  info->m_FileInfo->version.parse(fileDetails["version"].toString());
  if (!info->m_FileInfo->version.isValid())
    info->m_FileInfo->version.parse(fileDetails["mod_version"].toString());
  info->m_FileInfo->fileCategory = fileDetails["category_id"].toInt();

  info->m_FileInfo->modID      = modDetails["mod_id"].toInt();
  info->m_FileInfo->modName    = modDetails["name"].toString();
  info->m_FileInfo->categoryID = modDetails["category_id"].toInt();

  QString gameShortName = gameName;
  QStringList games(m_ManagedGame->validShortNames());
  games += m_ManagedGame->gameShortName();
  for (auto game : games) {
    MOBase::IPluginGame* gamePlugin = m_OrganizerCore->getGame(game);
    if (gamePlugin != nullptr &&
        gamePlugin->gameNexusName().compare(gameName, Qt::CaseInsensitive) == 0) {
      gameShortName = gamePlugin->gameShortName();
      break;
    }
  }

  info->m_FileInfo->gameName = gameShortName;

  // If the file description is not present, send another query to get it
  if (!fileDetails["description"].isValid()) {
    info->m_RemoteFileName = fileDetails["file_name"].toString();
    setState(info, STATE_FETCHINGFILEINFO);
  } else {
    setState(info, STATE_READY);
  }
}

void DownloadManager::nxmRequestFailed(QString gameName, int modID, int fileID,
                                       QVariant userData, int requestID, int errorCode,
                                       const QString& errorString)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  DownloadInfo* userDataInfo = downloadInfoByID(userData.toInt());

  int index = 0;

  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin();
       iter != m_ActiveDownloads.end(); ++iter, ++index) {
    DownloadInfo* info = *iter;
    if (info != userDataInfo)
      continue;

    // MD5 searches continue until all possible games are done
    if (info->m_State == STATE_FETCHINGMODINFO_MD5) {
      if (info->m_GamesToQuery.count() >= 2) {
        info->m_GamesToQuery.pop_front();
        setState(info, STATE_FETCHINGMODINFO_MD5);
        return;
      } else {
        info->m_State = STATE_READY;
        queryInfo(index);
        emit update(index);
        return;
      }
    }

    if (info->m_FileInfo->modID == modID) {
      if (info->m_State < STATE_FETCHINGMODINFO) {
        m_ActiveDownloads.erase(iter);
        delete info;
      } else {
        setState(info, STATE_READY);
      }
      emit update(index);
      break;
    }
  }

  removePending(gameName, modID, fileID);
  emit showMessage(tr("Failed to request file info from nexus: %1").arg(errorString));
}

void DownloadManager::downloadFinished(int index)
{
  DownloadInfo* info;
  if (index > 0)
    info = m_ActiveDownloads[index];
  else {
    info = findDownload(this->sender(), &index);
    if (info == nullptr && index == 0) {
      info = m_ActiveDownloads[index];
    }
  }

  if (info != nullptr) {
    QNetworkReply* reply = info->m_Reply;
    QByteArray data;
    if (reply->isOpen() && info->m_HasData) {
      data = reply->readAll();
      info->m_Output.write(data);
    }
    info->m_Output.close();
    TaskProgressManager::instance().forgetMe(info->m_TaskProgressId);

    bool error = false;
    if ((info->m_State != STATE_CANCELING) && (info->m_State != STATE_PAUSING)) {
      bool textData = reply->header(QNetworkRequest::ContentTypeHeader)
                          .toString()
                          .startsWith("text", Qt::CaseInsensitive);
      if (textData)
        emit showMessage(
            tr("Warning: Content type is: %1")
                .arg(reply->header(QNetworkRequest::ContentTypeHeader).toString()));
      if ((info->m_Output.size() == 0) ||
          ((reply->error() != QNetworkReply::NoError) &&
           (reply->error() != QNetworkReply::OperationCanceledError))) {
        if (reply->error() == QNetworkReply::UnknownContentError)
          emit showMessage(
              tr("Download header content length: %1 downloaded file size: %2")
                  .arg(reply->header(QNetworkRequest::ContentLengthHeader).toLongLong())
                  .arg(info->m_Output.size()));
        if (info->m_Tries == 0) {
          emit showMessage(tr("Download failed: %1 (%2)")
                               .arg(reply->errorString())
                               .arg(reply->error()));
        }
        error = true;
        setState(info, STATE_ERROR);
      }
    }

    if (info->m_State == STATE_CANCELING) {
      setState(info, STATE_CANCELED);
    } else if (info->m_State == STATE_PAUSING) {
      if (info->m_Output.isOpen() && info->m_HasData) {
        info->m_Output.write(info->m_Reply->readAll());
      }
      setState(info, STATE_PAUSED);
    }

    if (info->m_State == STATE_CANCELED || (info->m_Tries == 0 && error)) {
      emit aboutToUpdate();
      info->m_Output.remove();
      delete info;
      m_ActiveDownloads.erase(m_ActiveDownloads.begin() + index);
      if (error)
        emit showMessage(
            tr("We were unable to download the file due to errors after four retries. "
               "There may be an issue with the Nexus servers."));
      emit update(-1);
    } else if (info->isPausedState() || info->m_State == STATE_PAUSING) {
      info->m_Output.close();
      createMetaFile(info);
      emit update(index);
    } else {
      QString url = info->m_Urls[info->m_CurrentUrl];
      if (info->m_FileInfo->userData.contains("downloadMap")) {
        foreach (const QVariant& server,
                 info->m_FileInfo->userData["downloadMap"].toList()) {
          QVariantMap serverMap = server.toMap();
          if (serverMap["URI"].toString() == url) {
            int deltaTime = info->m_StartTime.elapsed() / 1000;
            if (deltaTime > 5) {
              emit downloadSpeed(serverMap["short_name"].toString(),
                                 (info->m_TotalSize - info->m_PreResumeSize) /
                                     deltaTime);
            }  // no division by zero please! Also, if the download is shorter than a
               // few seconds, the result is way to inprecise
            break;
          }
        }
      }

      bool isNexus = info->m_FileInfo->repository == "Nexus";
      // need to change state before changing the file name, otherwise .unfinished is
      // appended
      if (isNexus) {
        setState(info, STATE_FETCHINGMODINFO);
      } else {
        setState(info, STATE_NOFETCH);
      }

      QString newName = getFileNameFromNetworkReply(reply);
      QString oldName = QFileInfo(info->m_Output).fileName();

      startDisableDirWatcher();
      if (!newName.isEmpty() && (oldName.isEmpty())) {
        info->setName(getDownloadFileName(newName), true);
      } else {
        info->setName(m_OutputDirectory + "/" + info->m_FileName,
                      true);  // don't rename but remove the ".unfinished" extension
      }
      endDisableDirWatcher();

      if (!isNexus) {
        setState(info, STATE_READY);
      }

      emit update(index);
    }
    reply->close();
    reply->deleteLater();

    if ((info->m_Tries > 0) && error) {
      --info->m_Tries;
      resumeDownloadInt(index);
    }
  } else {
    log::warn("no download index {}", index);
  }
}

void DownloadManager::downloadError(QNetworkReply::NetworkError error)
{
  if (error != QNetworkReply::OperationCanceledError) {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    log::warn("{} ({})",
              reply != nullptr ? reply->errorString() : "Download error occured",
              error);
  }
}

void DownloadManager::metaDataChanged()
{
  int index = 0;

  DownloadInfo* info = findDownload(this->sender(), &index);
  if (info != nullptr) {
    QString newName = getFileNameFromNetworkReply(info->m_Reply);
    if (!newName.isEmpty() && (info->m_FileName.isEmpty())) {
      startDisableDirWatcher();
      info->setName(getDownloadFileName(newName), true);
      endDisableDirWatcher();
      refreshAlphabeticalTranslation();
      if (!info->m_Output.isOpen() &&
          !info->m_Output.open(QIODevice::WriteOnly | QIODevice::Append)) {
        reportError(tr("failed to re-open %1").arg(info->m_FileName));
        setState(info, STATE_CANCELING);
      }
    }
  } else {
    log::warn("meta data event for unknown download");
  }
}

void DownloadManager::directoryChanged(const QString&)
{
  if (DownloadManager::m_DirWatcherDisabler == 0)
    refreshList();
}

void DownloadManager::managedGameChanged(MOBase::IPluginGame const* managedGame)
{
  m_ManagedGame = managedGame;
}

void DownloadManager::checkDownloadTimeout()
{
  for (int i = 0; i < m_ActiveDownloads.size(); ++i) {
    if (m_ActiveDownloads[i]->m_StartTime.elapsed() -
                m_ActiveDownloads[i]->m_DownloadTimeLast >
            5 * 1000 &&
        m_ActiveDownloads[i]->m_State == STATE_DOWNLOADING &&
        m_ActiveDownloads[i]->m_Reply != nullptr &&
        m_ActiveDownloads[i]->m_Reply->isOpen()) {
      pauseDownload(i);
      downloadFinished(i);
      resumeDownload(i);
    }
  }
}

void DownloadManager::writeData(DownloadInfo* info)
{
  if (info != nullptr) {
    qint64 ret = info->m_Output.write(info->m_Reply->readAll());
    if (ret < info->m_Reply->size()) {
      QString fileName =
          info->m_FileName;  // m_FileName may be destroyed after setState
      setState(info, DownloadState::STATE_CANCELED);

      log::error("Unable to write download \"{}\" to drive (return {})",
                 info->m_FileName, ret);

      reportError(tr("Unable to write download to drive (return %1).\n"
                     "Check the drive's available storage.\n\n"
                     "Canceling download \"%2\"...")
                      .arg(ret)
                      .arg(fileName));
    }
  }
}
