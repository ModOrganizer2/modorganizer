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

#include "nxmurl.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "iplugingame.h"
#include <nxmurl.h>
#include <taskprogressmanager.h>
#include "utility.h"
#include "selectiondialog.h"
#include "bbcode.h"
#include <utility.h>
#include <report.h>

#include <QTimer>
#include <QFileInfo>
#include <QRegExp>
#include <QDirIterator>
#include <QInputDialog>
#include <QMessageBox>
#include <QCoreApplication>
#include <QTextDocument>

#include <boost/bind.hpp>
#include <regex>


using namespace MOBase;


// TODO limit number of downloads, also display download during nxm requests, store modid/fileid with downloads


static const char UNFINISHED[] = ".unfinished";

unsigned int DownloadManager::DownloadInfo::s_NextDownloadID = 1U;


DownloadManager::DownloadInfo *DownloadManager::DownloadInfo::createNew(const ModRepositoryFileInfo *fileInfo, const QStringList &URLs)
{
  DownloadInfo *info = new DownloadInfo;
  info->m_DownloadID = s_NextDownloadID++;
  info->m_StartTime.start();
  info->m_PreResumeSize = 0LL;
  info->m_Progress = 0;
  info->m_ResumePos = 0;
  info->m_FileInfo = new ModRepositoryFileInfo(*fileInfo);
  info->m_Urls = URLs;
  info->m_CurrentUrl = 0;
  info->m_Tries = AUTOMATIC_RETRIES;
  info->m_State = STATE_STARTED;
  info->m_TaskProgressId = TaskProgressManager::instance().getId();

  return info;
}

DownloadManager::DownloadInfo *DownloadManager::DownloadInfo::createFromMeta(const QString &filePath, bool showHidden)
{
  DownloadInfo *info = new DownloadInfo;

  QString metaFileName = filePath + ".meta";
  QSettings metaFile(metaFileName, QSettings::IniFormat);
  if (!showHidden && metaFile.value("removed", false).toBool()) {
    return nullptr;
  } else {
    info->m_Hidden = metaFile.value("removed", false).toBool();
  }

  QString fileName = QFileInfo(filePath).fileName();

  if (fileName.endsWith(UNFINISHED)) {
    info->m_FileName = fileName.mid(0, fileName.length() - strlen(UNFINISHED));
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
  info->m_TotalSize = QFileInfo(filePath).size();
  info->m_PreResumeSize = info->m_TotalSize;
  info->m_CurrentUrl = 0;
  info->m_Urls = metaFile.value("url", "").toString().split(";");
  info->m_Tries = 0;
  info->m_TaskProgressId = TaskProgressManager::instance().getId();
  int modID = metaFile.value("modID", 0).toInt();
  int fileID = metaFile.value("fileID", 0).toInt();
  info->m_FileInfo = new ModRepositoryFileInfo(modID, fileID);
  info->m_FileInfo->name     = metaFile.value("name", "").toString();
  if (info->m_FileInfo->name == "0") {
    // bug in earlier version
    info->m_FileInfo->name = "";
  }
  info->m_FileInfo->modName  = metaFile.value("modName", "").toString();
  info->m_FileInfo->modID  = modID;
  info->m_FileInfo->fileID = fileID;
  info->m_FileInfo->description = metaFile.value("description").toString();
  info->m_FileInfo->version.parse(metaFile.value("version", "0").toString());
  info->m_FileInfo->newestVersion.parse(metaFile.value("newestVersion", "0").toString());
  info->m_FileInfo->categoryID = metaFile.value("category", 0).toInt();
  info->m_FileInfo->fileCategory = metaFile.value("fileCategory", 0).toInt();
  info->m_FileInfo->repository = metaFile.value("repository", "Nexus").toString();
  info->m_FileInfo->userData = metaFile.value("userData").toMap();

  return info;
}

void DownloadManager::DownloadInfo::setName(QString newName, bool renameFile)
{
  QString oldMetaFileName = QString("%1.meta").arg(m_FileName);
  m_FileName = QFileInfo(newName).fileName();
  if ((m_State == DownloadManager::STATE_STARTED) ||
      (m_State == DownloadManager::STATE_DOWNLOADING)) {
    newName.append(UNFINISHED);
  }
  if (renameFile) {
    if ((newName != m_Output.fileName()) && !m_Output.rename(newName)) {
      reportError(tr("failed to rename \"%1\" to \"%2\"").arg(m_Output.fileName()).arg(newName));
      return;
    }

    QFile metaFile(oldMetaFileName);
    if (metaFile.exists()) {
      metaFile.rename(newName.mid(0).append(".meta"));
    }
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


DownloadManager::DownloadManager(NexusInterface *nexusInterface, QObject *parent)
  : IDownloadManager(parent), m_NexusInterface(nexusInterface), m_DirWatcher(), m_ShowHidden(false),
    m_DateExpression("/Date\\((\\d+)\\)/")
{
  connect(&m_DirWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(directoryChanged(QString)));
}


DownloadManager::~DownloadManager()
{
  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin(); iter != m_ActiveDownloads.end(); ++iter) {
    delete *iter;
  }
  m_ActiveDownloads.clear();
}


bool DownloadManager::downloadsInProgress()
{
  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin(); iter != m_ActiveDownloads.end(); ++iter) {
    if ((*iter)->m_State < STATE_READY) {
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

  bool done = false;
  QTime startTime = QTime::currentTime();
  // further loops: busy waiting for all downloads to complete. This could be neater...
  while (!done && (startTime.secsTo(QTime::currentTime()) < 5)) {
    QCoreApplication::processEvents();
    done = true;
    foreach (DownloadInfo *info, m_ActiveDownloads) {
      if ((info->m_State < STATE_CANCELED) ||
          (info->m_State == STATE_FETCHINGFILEINFO) ||
          (info->m_State == STATE_FETCHINGMODINFO)) {
        done = false;
        break;
      }
    }
    if (!done) {
      ::Sleep(100);
    }
  }
}


void DownloadManager::setOutputDirectory(const QString &outputDirectory)
{
  QStringList directories = m_DirWatcher.directories();
  if (directories.length() != 0) {
    m_DirWatcher.removePaths(directories);
  }
  m_OutputDirectory = QDir::fromNativeSeparators(outputDirectory);
  refreshList();
  m_DirWatcher.addPath(m_OutputDirectory);
}


void DownloadManager::setPreferredServers(const std::map<QString, int> &preferredServers)
{
  m_PreferredServers = preferredServers;
}


void DownloadManager::setSupportedExtensions(const QStringList &extensions)
{
  m_SupportedExtensions = extensions;
  refreshList();
}

void DownloadManager::setShowHidden(bool showHidden)
{
  m_ShowHidden = showHidden;
  refreshList();
}

void DownloadManager::refreshList()
{
  try {
    int downloadsBefore = m_ActiveDownloads.size();

    // remove finished downloads
    for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin(); iter != m_ActiveDownloads.end();) {
      if (((*iter)->m_State == STATE_READY) || ((*iter)->m_State == STATE_INSTALLED) || ((*iter)->m_State == STATE_UNINSTALLED)) {
        delete *iter;
        iter = m_ActiveDownloads.erase(iter);
      } else {
        ++iter;
      }
    }

    QStringList nameFilters(m_SupportedExtensions);
    foreach (const QString &extension, m_SupportedExtensions) {
      nameFilters.append("*." + extension);
    }

    nameFilters.append(QString("*").append(UNFINISHED));
    QDir dir(QDir::fromNativeSeparators(m_OutputDirectory));

    // find orphaned meta files and delete them (sounds cruel but it's better for everyone)
    QStringList orphans;
    QStringList metaFiles = dir.entryList(QStringList() << "*.meta");
    foreach (const QString &metaFile, metaFiles) {
      QString baseFile = metaFile.left(metaFile.length() - 5);
      if (!QFile::exists(dir.absoluteFilePath(baseFile))) {
        orphans.append(dir.absoluteFilePath(metaFile));
      }
    }
    if (orphans.size() > 0) {
      qDebug("%d orphaned meta files will be deleted", orphans.size());
      shellDelete(orphans, true);
    }

    // add existing downloads to list
    foreach (QString file, dir.entryList(nameFilters, QDir::Files, QDir::Time)) {
      bool Exists = false;
      for (QVector<DownloadInfo*>::const_iterator Iter = m_ActiveDownloads.begin(); Iter != m_ActiveDownloads.end() && !Exists; ++Iter) {
        if (QString::compare((*Iter)->m_FileName, file, Qt::CaseInsensitive) == 0) {
          Exists = true;
        } else if (QString::compare(QFileInfo((*Iter)->m_Output.fileName()).fileName(), file, Qt::CaseInsensitive) == 0) {
          Exists = true;
        }
      }
      if (Exists) {
        continue;
      }

      QString fileName = QDir::fromNativeSeparators(m_OutputDirectory) + "/" + file;

      DownloadInfo *info = DownloadInfo::createFromMeta(fileName, m_ShowHidden);
      if (info != nullptr) {
        m_ActiveDownloads.push_front(info);
      }
    }

    if (m_ActiveDownloads.size() != downloadsBefore) {
      qDebug("downloads after refresh: %d", m_ActiveDownloads.size());
    }
    emit update(-1);
  } catch (const std::bad_alloc&) {
    reportError(tr("Memory allocation error (in refreshing directory)."));
  }
}


bool DownloadManager::addDownload(const QStringList &URLs,
                                  int modID, int fileID, const ModRepositoryFileInfo *fileInfo)
{
  QString fileName = QFileInfo(URLs.first()).fileName();
  if (fileName.isEmpty()) {
    fileName = "unknown";
  }

  QUrl preferredUrl = QUrl::fromEncoded(URLs.first().toLocal8Bit());
  qDebug("selected download url: %s", qPrintable(preferredUrl.toString()));
  QNetworkRequest request(preferredUrl);
  return addDownload(m_NexusInterface->getAccessManager()->get(request), URLs, fileName, modID, fileID, fileInfo);
}


bool DownloadManager::addDownload(QNetworkReply *reply, const ModRepositoryFileInfo *fileInfo)
{
  QString fileName = getFileNameFromNetworkReply(reply);
  if (fileName.isEmpty()) {
    fileName = "unknown";
  }

  return addDownload(reply, QStringList(reply->url().toString()), fileName, fileInfo->modID, fileInfo->fileID, fileInfo);
}


bool DownloadManager::addDownload(QNetworkReply *reply, const QStringList &URLs, const QString &fileName,
                                  int modID, int fileID, const ModRepositoryFileInfo *fileInfo)
{
  // download invoked from an already open network reply (i.e. download link in the browser)
  DownloadInfo *newDownload = DownloadInfo::createNew(fileInfo, URLs);

  QString baseName = fileName;
  if (!fileInfo->fileName.isEmpty()) {
    baseName = fileInfo->fileName;
  } else {
    QString dispoName = getFileNameFromNetworkReply(reply);

    if (!dispoName.isEmpty()) {
      baseName = dispoName;
    }
  }
  if (QFile::exists(m_OutputDirectory + "/" + baseName) &&
      (QMessageBox::question(nullptr, tr("Download again?"), tr("A file with the same name has already been downloaded. "
                             "Do you want to download it again? The new file will receive a different name."),
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)) {
    removePending(modID, fileID);
    delete newDownload;
    return false;
  }
  newDownload->setName(getDownloadFileName(baseName), false);

  startDownload(reply, newDownload, false);
//  emit update(-1);
  return true;
}


void DownloadManager::removePending(int modID, int fileID)
{
  emit aboutToUpdate();
  for (auto iter = m_PendingDownloads.begin(); iter != m_PendingDownloads.end(); ++iter) {
    if ((iter->first == modID) && (iter->second == fileID)) {
      m_PendingDownloads.erase(iter);
      break;
    }
  }
  emit update(-1);
}


void DownloadManager::startDownload(QNetworkReply *reply, DownloadInfo *newDownload, bool resume)
{
  reply->setReadBufferSize(1024 * 1024); // don't read more than 1MB at once to avoid memory troubles
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
                .arg(reply->url().toString()).arg(newDownload->m_Output.fileName()));
    return;
  }

  connect(newDownload->m_Reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
  connect(newDownload->m_Reply, SIGNAL(finished()), this, SLOT(downloadFinished()));
  connect(newDownload->m_Reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(downloadError(QNetworkReply::NetworkError)));
  connect(newDownload->m_Reply, SIGNAL(readyRead()), this, SLOT(downloadReadyRead()));
  connect(newDownload->m_Reply, SIGNAL(metaDataChanged()), this, SLOT(metaDataChanged()));

  if (!resume) {
    newDownload->m_PreResumeSize = newDownload->m_Output.size();
    removePending(newDownload->m_FileInfo->modID, newDownload->m_FileInfo->fileID);

    emit aboutToUpdate();
    m_ActiveDownloads.append(newDownload);

    emit update(-1);
    emit downloadAdded();

    if (reply->isFinished()) {
      // it's possible the download has already finished before this function ran
      downloadFinished();
    }
  }
}


void DownloadManager::addNXMDownload(const QString &url)
{
  NXMUrl nxmInfo(url);

  QString managedGame = m_ManagedGame->getGameShortName();
  qDebug("add nxm download: %s", qPrintable(url));
  if (nxmInfo.game().compare(managedGame, Qt::CaseInsensitive) != 0) {
    qDebug("download requested for wrong game (game: %s, url: %s)", qPrintable(managedGame), qPrintable(nxmInfo.game()));
    QMessageBox::information(nullptr, tr("Wrong Game"), tr("The download link is for a mod for \"%1\" but this instance of MO "
    "has been set up for \"%2\".").arg(nxmInfo.game()).arg(managedGame), QMessageBox::Ok);
    return;
  }

  emit aboutToUpdate();
  m_PendingDownloads.append(std::make_pair(nxmInfo.modId(), nxmInfo.fileId()));

  emit update(-1);
  emit downloadAdded();
  m_RequestIDs.insert(m_NexusInterface->requestFileInfo(nxmInfo.modId(), nxmInfo.fileId(), this, nxmInfo.fileId(), ""));
}


void DownloadManager::removeFile(int index, bool deleteFile)
{
  if (index >= m_ActiveDownloads.size()) {
    throw MyException(tr("invalid index"));
  }

  DownloadInfo *download = m_ActiveDownloads.at(index);
  QString filePath = m_OutputDirectory + "/" + download->m_FileName;
  if ((download->m_State == STATE_STARTED) ||
      (download->m_State == STATE_DOWNLOADING)) {
    // shouldn't have been possible
    qCritical("tried to remove active download");
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
    metaSettings.setValue("removed", true);
  }
}

class LessThanWrapper
{
public:
  LessThanWrapper(DownloadManager *manager) : m_Manager(manager) {}
  bool operator()(int LHS, int RHS) {
    return m_Manager->getFileName(LHS).compare(m_Manager->getFileName(RHS), Qt::CaseInsensitive) < 0;

  }

private:
  DownloadManager *m_Manager;
};


bool DownloadManager::ByName(int LHS, int RHS)
{
  return m_ActiveDownloads[LHS]->m_FileName < m_ActiveDownloads[RHS]->m_FileName;
}


void DownloadManager::refreshAlphabeticalTranslation()
{
  m_AlphabeticalTranslation.clear();
  int pos = 0;
  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin(); iter != m_ActiveDownloads.end(); ++iter, ++pos) {
    m_AlphabeticalTranslation.push_back(pos);
  }

  qSort(m_AlphabeticalTranslation.begin(), m_AlphabeticalTranslation.end(), LessThanWrapper(this));
}


void DownloadManager::restoreDownload(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  DownloadInfo *download = m_ActiveDownloads.at(index);
  download->m_Hidden = false;

  QString filePath = m_OutputDirectory + "/" + download->m_FileName;

  QSettings metaSettings(filePath.append(".meta"), QSettings::IniFormat);
  metaSettings.setValue("removed", false);
}


void DownloadManager::removeDownload(int index, bool deleteFile)
{
  try {
    emit aboutToUpdate();

    if (index < 0) {
      DownloadState minState = index < -1 ? STATE_INSTALLED : STATE_READY;
      index = 0;
      for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin(); iter != m_ActiveDownloads.end();) {
        if ((*iter)->m_State >= minState) {
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
        reportError(tr("invalid index %1").arg(index));
        return;
      }

      removeFile(index, deleteFile);
      delete m_ActiveDownloads.at(index);
      m_ActiveDownloads.erase(m_ActiveDownloads.begin() + index);
    }
    emit update(-1);
  } catch (const std::exception &e) {
    qCritical("failed to remove download: %s", e.what());
  }
}


void DownloadManager::cancelDownload(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("invalid index %1").arg(index));
    return;
  }

  if (m_ActiveDownloads.at(index)->m_State == STATE_DOWNLOADING) {
    setState(m_ActiveDownloads.at(index), STATE_CANCELING);
  }
}


void DownloadManager::pauseDownload(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("invalid index %1").arg(index));
    return;
  }

  DownloadInfo *info = m_ActiveDownloads.at(index);

  if (info->m_State == STATE_DOWNLOADING) {
    if ((info->m_Reply != nullptr) && (info->m_Reply->isRunning())) {
      setState(info, STATE_PAUSING);
    } else {
      setState(info, STATE_PAUSED);
    }
  } else if ((info->m_State == STATE_FETCHINGMODINFO) || (info->m_State == STATE_FETCHINGFILEINFO)) {
    setState(info, STATE_READY);
  }
}

void DownloadManager::resumeDownload(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("invalid index %1").arg(index));
    return;
  }
  DownloadInfo *info = m_ActiveDownloads[index];
  info->m_Tries = AUTOMATIC_RETRIES;
  resumeDownloadInt(index);
}

void DownloadManager::resumeDownloadInt(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("invalid index %1").arg(index));
    return;
  }
  DownloadInfo *info = m_ActiveDownloads[index];
  if (info->isPausedState()) {
    if ((info->m_Urls.size() == 0)
        || ((info->m_Urls.size() == 1) && (info->m_Urls[0].size() == 0))) {
      emit showMessage(tr("No known download urls. Sorry, this download can't be resumed."));
      return;
    }
    if (info->m_State == STATE_ERROR) {
      info->m_CurrentUrl = (info->m_CurrentUrl + 1) % info->m_Urls.count();
    }
    qDebug("request resume from url %s", qPrintable(info->currentURL()));
    QNetworkRequest request(QUrl::fromEncoded(info->currentURL().toLocal8Bit()));
    info->m_ResumePos = info->m_Output.size();
    qDebug("resume at %lld bytes", info->m_ResumePos);
    QByteArray rangeHeader = "bytes=" + QByteArray::number(info->m_ResumePos) + "-";
    request.setRawHeader("Range", rangeHeader);
    startDownload(m_NexusInterface->getAccessManager()->get(request), info, true);
  }
  emit update(index);
}


DownloadManager::DownloadInfo *DownloadManager::downloadInfoByID(unsigned int id)
{
  auto iter = std::find_if(m_ActiveDownloads.begin(), m_ActiveDownloads.end(),
               [id](DownloadInfo *info) { return info->m_DownloadID == id; });
  if (iter != m_ActiveDownloads.end()) {
    return *iter;
  } else {
    return nullptr;
  }
}


void DownloadManager::queryInfo(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("invalid index %1").arg(index));
    return;
  }
  DownloadInfo *info = m_ActiveDownloads[index];

  if (info->m_FileInfo->repository != "Nexus") {
    qWarning("re-querying file info is currently only possible with Nexus");
    return;
  }

  if (info->m_State < DownloadManager::STATE_READY) {
    // UI shouldn't allow this
    return;
  }

  if (info->m_FileInfo->modID <= 0) {
    QString fileName = getFileName(index);
    QString ignore;
    NexusInterface::interpretNexusFileName(fileName, ignore, info->m_FileInfo->modID, true);
    if (info->m_FileInfo->modID < 0) {
      QString modIDString;
      while (modIDString.isEmpty()) {
        modIDString = QInputDialog::getText(nullptr, tr("Please enter the nexus mod id"), tr("Mod ID:"), QLineEdit::Normal,
                                            QString(), nullptr, 0, Qt::ImhFormattedNumbersOnly);
        if (modIDString.isNull()) {
          // canceled
          return;
        } else if (modIDString.contains(QRegExp("[^0-9]"))) {
          qDebug("illegal character in mod-id");
          modIDString.clear();
        }
      }
      info->m_FileInfo->modID = modIDString.toInt(nullptr, 10);
    }
  }
  info->m_ReQueried = true;
  setState(info, STATE_FETCHINGMODINFO);
}


int DownloadManager::numTotalDownloads() const
{
  return m_ActiveDownloads.size();
}

int DownloadManager::numPendingDownloads() const
{
  return m_PendingDownloads.size();
}

std::pair<int, int> DownloadManager::getPendingDownload(int index)
{
  if ((index < 0) || (index >= m_PendingDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_PendingDownloads.at(index);
}

QString DownloadManager::getFilePath(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_OutputDirectory + "/" + m_ActiveDownloads.at(index)->m_FileName;
}

QString DownloadManager::getFileTypeString(int fileType)
{
  switch (fileType) {
    case 1: return tr("Main");
    case 2: return tr("Update");
    case 3: return tr("Optional");
    case 4: return tr("Old");
    case 5: return tr("Misc");
    default: return tr("Unknown");
  }
}

QString DownloadManager::getDisplayName(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  DownloadInfo *info = m_ActiveDownloads.at(index);

  if (!info->m_FileInfo->name.isEmpty()) {
    return QString("%1 (%2, v%3)").arg(info->m_FileInfo->name)
                                  .arg(getFileTypeString(info->m_FileInfo->fileCategory))
                                  .arg(info->m_FileInfo->version.displayString());
  } else {
    return info->m_FileName;
  }
}

QString DownloadManager::getFileName(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_ActiveDownloads.at(index)->m_FileName;
}

QDateTime DownloadManager::getFileTime(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  DownloadInfo *info = m_ActiveDownloads.at(index);
  if (!info->m_Created.isValid()) {
    info->m_Created = QFileInfo(info->m_Output).created();
  }

  return info->m_Created;
}

qint64 DownloadManager::getFileSize(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_ActiveDownloads.at(index)->m_TotalSize;
}


int DownloadManager::getProgress(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_ActiveDownloads.at(index)->m_Progress;
}


DownloadManager::DownloadState DownloadManager::getState(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_ActiveDownloads.at(index)->m_State;
}


bool DownloadManager::isInfoIncomplete(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  DownloadInfo *info = m_ActiveDownloads.at(index);
  if (info->m_FileInfo->repository != "Nexus") {
    // other repositories currently don't support re-querying info anyway
    return false;
  }
  return (info->m_FileInfo->fileID == 0) || (info->m_FileInfo->modID == 0);
}


int DownloadManager::getModID(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }
  return m_ActiveDownloads.at(index)->m_FileInfo->modID;
}

bool DownloadManager::isHidden(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }
  return m_ActiveDownloads.at(index)->m_Hidden;
}


const ModRepositoryFileInfo *DownloadManager::getFileInfo(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_ActiveDownloads.at(index)->m_FileInfo;
}


void DownloadManager::markInstalled(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  DownloadInfo *info = m_ActiveDownloads.at(index);
  QSettings metaFile(info->m_Output.fileName() + ".meta", QSettings::IniFormat);
  metaFile.setValue("installed", true);
  metaFile.setValue("uninstalled", false);

  setState(m_ActiveDownloads.at(index), STATE_INSTALLED);
}


void DownloadManager::markUninstalled(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  DownloadInfo *info = m_ActiveDownloads.at(index);
  QSettings metaFile(info->m_Output.fileName() + ".meta", QSettings::IniFormat);
  metaFile.setValue("uninstalled", true);

  setState(m_ActiveDownloads.at(index), STATE_UNINSTALLED);
}


QString DownloadManager::getDownloadFileName(const QString &baseName) const
{
  QString fullPath = m_OutputDirectory + "/" + baseName;
  if (QFile::exists(fullPath)) {
    int i = 1;
    while (QFile::exists(QString("%1/%2_%3").arg(m_OutputDirectory).arg(i).arg(baseName))) {
      ++i;
    }

    fullPath = QString("%1/%2_%3").arg(m_OutputDirectory).arg(i).arg(baseName);
  }
  return fullPath;
}


QString DownloadManager::getFileNameFromNetworkReply(QNetworkReply *reply)
{
  if (reply->hasRawHeader("Content-Disposition")) {
    std::regex exp("filename=\"(.*)\"");

    std::cmatch result;
    if (std::regex_search(reply->rawHeader("Content-Disposition").constData(), result, exp)) {
      return QString::fromUtf8(result.str(1).c_str());
    }
  }

  return QString();
}


void DownloadManager::setState(DownloadManager::DownloadInfo *info, DownloadManager::DownloadState state)
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
    case STATE_PAUSED:
    case STATE_ERROR: {
      info->m_Reply->abort();
    } break;
    case STATE_CANCELED: {
      info->m_Reply->abort();
    } break;
    case STATE_FETCHINGMODINFO: {
      m_RequestIDs.insert(m_NexusInterface->requestDescription(info->m_FileInfo->modID, this, info->m_DownloadID, QString()));
    } break;
    case STATE_FETCHINGFILEINFO: {
      m_RequestIDs.insert(m_NexusInterface->requestFiles(info->m_FileInfo->modID, this, info->m_DownloadID, QString()));
    } break;
    case STATE_READY: {
      createMetaFile(info);
      emit downloadComplete(row);
    } break;
    default: /* NOP */ break;
  }
  emit stateChanged(row, state);
}


DownloadManager::DownloadInfo *DownloadManager::findDownload(QObject *reply, int *index) const
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
    DownloadInfo *info = findDownload(this->sender(), &index);
    if (info != nullptr) {
      if (info->m_State == STATE_CANCELING) {
        setState(info, STATE_CANCELED);
      } else if (info->m_State == STATE_PAUSING) {
        setState(info, STATE_PAUSED);
      } else {
        if (bytesTotal > info->m_TotalSize) {
          info->m_TotalSize = bytesTotal;
        }
        int oldProgress = info->m_Progress;
        info->m_Progress = ((info->m_ResumePos + bytesReceived) * 100) / (info->m_ResumePos + bytesTotal);
        TaskProgressManager::instance().updateProgress(info->m_TaskProgressId, bytesReceived, bytesTotal);
        if (oldProgress != info->m_Progress) {
          emit update(index);
        }
      }
    }
  } catch (const std::bad_alloc&) {
    reportError(tr("Memory allocation error (in processing progress event)."));
  }
}


void DownloadManager::downloadReadyRead()
{
  try {
    DownloadInfo *info = findDownload(this->sender());
    if (info != nullptr) {
      info->m_Output.write(info->m_Reply->readAll());
    }
  } catch (const std::bad_alloc&) {
    reportError(tr("Memory allocation error (in processing downloaded data)."));
  }
}


void DownloadManager::createMetaFile(DownloadInfo *info)
{
  QSettings metaFile(QString("%1.meta").arg(info->m_Output.fileName()), QSettings::IniFormat);
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


void DownloadManager::nxmDescriptionAvailable(int, QVariant userData, QVariant resultData, int requestID)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  QVariantMap result = resultData.toMap();

  DownloadInfo *info = downloadInfoByID(userData.toInt());
  if (info == nullptr) return;
  info->m_FileInfo->categoryID = result["category_id"].toInt();
  info->m_FileInfo->modName = result["name"].toString().trimmed();
  info->m_FileInfo->newestVersion.parse(result["version"].toString());
  if (info->m_FileInfo->fileID != 0) {
    setState(info, STATE_READY);
  } else {
    setState(info, STATE_FETCHINGFILEINFO);
  }
}


QDateTime DownloadManager::matchDate(const QString &timeString)
{
  if (m_DateExpression.exactMatch(timeString)) {
    return QDateTime::fromMSecsSinceEpoch(m_DateExpression.cap(1).toLongLong());
  } else {
    qWarning("date not matched: %s", qPrintable(timeString));
    return QDateTime::currentDateTime();
  }
}


static EFileCategory convertFileCategory(int id)
{
  // TODO: need to handle file categories in the mod page plugin
  switch (id) {
    case 0: return TYPE_MAIN;
    case 1: return TYPE_UPDATE;
    case 2: return TYPE_OPTION;
    default: return TYPE_MAIN;
  }
}


void DownloadManager::nxmFilesAvailable(int, QVariant userData, QVariant resultData, int requestID)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  DownloadInfo *info = downloadInfoByID(userData.toInt());
  if (info == nullptr) return;

  QVariantList result = resultData.toList();

  // MO sometimes prepends <digit>_ to the filename in case of duplicate downloads.
  // this may muck up the file name comparison
  QString alternativeLocalName = info->m_FileName;

  QRegExp expression("^\\d_(.*)$");
  if (expression.indexIn(alternativeLocalName) == 0) {
    alternativeLocalName = expression.cap(1);
  }

  bool found = false;

  foreach(QVariant file, result) {
    QVariantMap fileInfo = file.toMap();
    QString fileName = fileInfo["uri"].toString();
    QString fileNameVariant = fileName.mid(0).replace(' ', '_');
    if ((fileName == info->m_FileName) || (fileName == alternativeLocalName) ||
        (fileNameVariant == info->m_FileName) || (fileNameVariant == alternativeLocalName)) {
      info->m_FileInfo->name = fileInfo["name"].toString();
      info->m_FileInfo->version.parse(fileInfo["version"].toString());
      if (!info->m_FileInfo->version.isValid()) {
        info->m_FileInfo->version = info->m_FileInfo->newestVersion;
      }
      //Nexus has HTMLd these so unhtml them if necessary
      QTextDocument doc;
      doc.setHtml(info->m_FileInfo->modName);
      info->m_FileInfo->modName = doc.toPlainText();
      doc.setHtml(info->m_FileInfo->name);
      info->m_FileInfo->name = doc.toPlainText();
      info->m_FileInfo->fileCategory = convertFileCategory(fileInfo["category_id"].toInt());
      info->m_FileInfo->fileTime = matchDate(fileInfo["date"].toString());
      info->m_FileInfo->fileID = fileInfo["id"].toInt();
      found = true;
      break;
    }
  }

  if (info->m_ReQueried) {
    if (found) {
      emit showMessage(tr("Information updated"));
    } else if (result.count() == 0) {
      emit showMessage(tr("No matching file found on Nexus! Maybe this file is no longer available or it was renamed?"));
    } else {
      SelectionDialog selection(tr("No file on Nexus matches the selected file by name. Please manually choose the correct one."));
      foreach(QVariant file, result) {
        QVariantMap fileInfo = file.toMap();
        selection.addChoice(fileInfo["uri"].toString(), "", file);
      }
      if (selection.exec() == QDialog::Accepted) {
        QVariantMap fileInfo = selection.getChoiceData().toMap();
        info->m_FileInfo->name = fileInfo["name"].toString();
        info->m_FileInfo->version.parse(fileInfo["version"].toString());
        info->m_FileInfo->fileCategory = convertFileCategory(fileInfo["category_id"].toInt());
        info->m_FileInfo->fileID = fileInfo["id"].toInt();
      } else {
        emit showMessage(tr("No matching file found on Nexus! Maybe this file is no longer available or it was renamed?"));
      }
    }
  } else {
    if (info->m_FileInfo->fileID == 0) {
      qWarning("could not determine file id for %s (state %d)",
               info->m_FileName.toUtf8().constData(), info->m_State);
    }
  }

  setState(info, STATE_READY);
}


void DownloadManager::nxmFileInfoAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  ModRepositoryFileInfo *info = new ModRepositoryFileInfo();

  QVariantMap result = resultData.toMap();
  info->name = result["name"].toString();
  info->version.parse(result["version"].toString());
  if (!info->version.isValid()) {
    info->version = info->newestVersion;
  }
  info->fileName = result["uri"].toString();
  info->fileCategory = result["category_id"].toInt();
  info->fileTime = matchDate(result["date"].toString());
  info->description = BBCode::convertToHTML(result["description"].toString());

  info->repository = "Nexus";
  info->modID = modID;
  info->fileID = fileID;

  QObject *test = info;
  m_RequestIDs.insert(m_NexusInterface->requestDownloadURL(modID, fileID, this, qVariantFromValue(test), QString()));
}

static int evaluateFileInfoMap(const QVariantMap &map, const std::map<QString, int> &preferredServers)
{
  int result = 0;

  int users = map["ConnectedUsers"].toInt();
  // 0 users is probably a sign that the server is offline. Since there is currently no
  // mechanism to try a different server, we avoid those without users
  if (users == 0) {
    result -= 500;
  } else {
    result -= users;
  }

  auto preference = preferredServers.find(map["Name"].toString());

  if (preference != preferredServers.end()) {
    result += 100 + preference->second * 20;
  }

  if (map["IsPremium"].toBool()) result += 5;

  return result;
}

// sort function to sort by best download server
bool DownloadManager::ServerByPreference(const std::map<QString, int> &preferredServers, const QVariant &LHS, const QVariant &RHS)
{
  return evaluateFileInfoMap(LHS.toMap(), preferredServers) > evaluateFileInfoMap(RHS.toMap(), preferredServers);
}

int DownloadManager::startDownloadURLs(const QStringList &urls)
{
  ModRepositoryFileInfo info;
  addDownload(urls, -1, -1, &info);
  return m_ActiveDownloads.size() - 1;
}

/* This doesn't appear to be used by anything
int DownloadManager::startDownloadNexusFile(int modID, int fileID)
{
  int newID = m_ActiveDownloads.size();
  addNXMDownload(QString("nxm://%1/mods/%2/files/%3").arg(ToQString(MOShared::GameInfo::instance().getGameName())).arg(modID).arg(fileID));
  return newID;
}
*/
QString DownloadManager::downloadPath(int id)
{
  return getFilePath(id);
}

int DownloadManager::indexByName(const QString &fileName) const
{
  for (int i = 0; i < m_ActiveDownloads.size(); ++i) {
    if (m_ActiveDownloads[i]->m_FileName == fileName) {
      return i;
    }
  }
  return -1;
}

void DownloadManager::nxmDownloadURLsAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  ModRepositoryFileInfo *info = qobject_cast<ModRepositoryFileInfo*>(qvariant_cast<QObject*>(userData));
  QVariantList resultList = resultData.toList();
  if (resultList.length() == 0) {
    removePending(modID, fileID);
    emit showMessage(tr("No download server available. Please try again later."));
    return;
  }

  std::sort(resultList.begin(), resultList.end(), boost::bind(&DownloadManager::ServerByPreference, m_PreferredServers, _1, _2));

  info->userData["downloadMap"] = resultList;

  QStringList URLs;

  foreach (const QVariant &server, resultList) {
    URLs.append(server.toMap()["URI"].toString());
  }
  addDownload(URLs, modID, fileID, info);
}


void DownloadManager::nxmRequestFailed(int modID, int fileID, QVariant userData, int requestID, const QString &errorString)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  int index = 0;

  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin(); iter != m_ActiveDownloads.end(); ++iter, ++index) {
    DownloadInfo *info = *iter;
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

  removePending(modID, fileID);
  emit showMessage(tr("Failed to request file info from nexus: %1").arg(errorString));
}


void DownloadManager::downloadFinished()
{
  int index = 0;

  DownloadInfo *info = findDownload(this->sender(), &index);
  if (info != nullptr) {
    QNetworkReply *reply = info->m_Reply;
    QByteArray data;
    if (reply->isOpen()) {
      data = reply->readAll();
      info->m_Output.write(data);
    }
    info->m_Output.close();
    TaskProgressManager::instance().forgetMe(info->m_TaskProgressId);

    bool error = false;
    if ((info->m_State != STATE_CANCELING) &&
        (info->m_State != STATE_PAUSING)) {
      bool textData = reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("text", Qt::CaseInsensitive);
      if ((info->m_Output.size() == 0) ||
          ((reply->error() != QNetworkReply::NoError) && (reply->error() != QNetworkReply::OperationCanceledError)) ||
          textData) {
        if (info->m_Tries == 0) {
          if (textData && (reply->error() == QNetworkReply::NoError)) {
            emit showMessage(tr("Download failed. Server reported: %1").arg(QString(data)));
          } else {
            emit showMessage(tr("Download failed: %1 (%2)").arg(reply->errorString()).arg(reply->error()));
          }
        }
        error = true;
        setState(info, STATE_PAUSING);
      }
    }

    if (info->m_State == STATE_CANCELING) {
      setState(info, STATE_CANCELED);
    } else if (info->m_State == STATE_PAUSING) {
      if (info->m_Output.isOpen()) {
        info->m_Output.write(info->m_Reply->readAll());
      }

      if (error) {
        setState(info, STATE_ERROR);
      } else {
        setState(info, STATE_PAUSED);
      }
    }

    if (info->m_State == STATE_CANCELED) {
      emit aboutToUpdate();
      info->m_Output.remove();
      delete info;
      m_ActiveDownloads.erase(m_ActiveDownloads.begin() + index);
      emit update(-1);
    } else if (info->isPausedState()) {
      info->m_Output.close();
      createMetaFile(info);
      emit update(index);
    } else {
      QString url = info->m_Urls[info->m_CurrentUrl];
      if (info->m_FileInfo->userData.contains("downloadMap")) {
        foreach (const QVariant &server, info->m_FileInfo->userData["downloadMap"].toList()) {
          QVariantMap serverMap = server.toMap();
          if (serverMap["URI"].toString() == url) {
            int deltaTime = info->m_StartTime.secsTo(QTime::currentTime());
            if (deltaTime > 5) {
              emit downloadSpeed(serverMap["Name"].toString(), (info->m_TotalSize - info->m_PreResumeSize) / deltaTime);
            } // no division by zero please! Also, if the download is shorter than a few seconds, the result is way to inprecise
            break;
          }
        }
      }

      bool isNexus = info->m_FileInfo->repository == "Nexus";
      // need to change state before changing the file name, otherwise .unfinished is appended
      if (isNexus) {
        setState(info, STATE_FETCHINGMODINFO);
      } else {
        setState(info, STATE_NOFETCH);
      }

      QString newName = getFileNameFromNetworkReply(reply);
      QString oldName = QFileInfo(info->m_Output).fileName();
      if (!newName.isEmpty() && (newName != oldName)) {
        info->setName(getDownloadFileName(newName), true);
      } else {
        info->setName(m_OutputDirectory + "/" + info->m_FileName, true); // don't rename but remove the ".unfinished" extension
      }

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
    qWarning("no download index %d", index);
  }
}


void DownloadManager::downloadError(QNetworkReply::NetworkError error)
{
  if (error != QNetworkReply::OperationCanceledError) {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    qWarning("%s (%d)", reply != nullptr ? qPrintable(reply->errorString())
                                         : "Download error occured",
             error);
  }
}


void DownloadManager::metaDataChanged()
{
  int index = 0;

  DownloadInfo *info = findDownload(this->sender(), &index);
  if (info != nullptr) {
    QString newName = getFileNameFromNetworkReply(info->m_Reply);
    if (!newName.isEmpty() && (newName != info->m_FileName)) {
      info->setName(getDownloadFileName(newName), true);
      refreshAlphabeticalTranslation();
      if (!info->m_Output.isOpen() && !info->m_Output.open(QIODevice::WriteOnly | QIODevice::Append)) {
        reportError(tr("failed to re-open %1").arg(info->m_FileName));
        setState(info, STATE_CANCELING);
      }
    }
  } else {
    qWarning("meta data event for unknown download");
  }
}

void DownloadManager::directoryChanged(const QString&)
{
  refreshList();
}

void DownloadManager::managedGameChanged(MOBase::IPluginGame const *managedGame)
{
  m_ManagedGame = managedGame;
}
