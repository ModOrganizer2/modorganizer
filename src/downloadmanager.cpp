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
#include "report.h"
#include "nxmurl.h"
#include <gameinfo.h>
#include <nxmurl.h>
#include "utility.h"
#include "json.h"
#include "selectiondialog.h"
#include <QTimer>
#include <QFileInfo>
#include <QRegExp>
#include <QDirIterator>
#include <QInputDialog>
#include <boost/bind.hpp>
#include <regex>
#include <QMessageBox>
#include <QCoreApplication>


using QtJson::Json;
using namespace MOBase;


// TODO limit number of downloads, also display download during nxm requests, store modid/fileid with downloads


static const char UNFINISHED[] = ".unfinished";

unsigned int DownloadManager::DownloadInfo::s_NextDownloadID = 1U;


DownloadManager::DownloadInfo *DownloadManager::DownloadInfo::createNew(const NexusInfo &nexusInfo, int modID, int fileID, const QStringList &URLs)
{
  DownloadInfo *info = new DownloadInfo;
  info->m_DownloadID = s_NextDownloadID++;
  info->m_StartTime.start();
  info->m_Progress = 0;
  info->m_ResumePos = 0;
  info->m_ModID = modID;
  info->m_FileID = fileID;
  info->m_NexusInfo = nexusInfo;
  info->m_Urls = URLs;
  info->m_CurrentUrl = 0;
  info->m_Tries = AUTOMATIC_RETRIES;
  info->m_State = STATE_STARTED;

  return info;
}

DownloadManager::DownloadInfo *DownloadManager::DownloadInfo::createFromMeta(const QString &filePath)
{
  DownloadInfo *info = new DownloadInfo;

  QString metaFileName = filePath + ".meta";
  QSettings metaFile(metaFileName, QSettings::IniFormat);
  if (metaFile.value("removed", false).toBool()) {
    return NULL;
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
  info->m_ModID  = metaFile.value("modID", 0).toInt();
  info->m_FileID = metaFile.value("fileID", 0).toInt();
  info->m_CurrentUrl = 0;
  info->m_Urls = metaFile.value("url", "").toString().split(";");
  info->m_Tries = 0;
  info->m_NexusInfo.m_Name     = metaFile.value("name", 0).toString();
  info->m_NexusInfo.m_ModName  = metaFile.value("modName", "").toString();
  info->m_NexusInfo.m_Version  = metaFile.value("version", 0).toString();
  info->m_NexusInfo.m_NewestVersion = metaFile.value("newestVersion", "").toString();
  info->m_NexusInfo.m_Category = metaFile.value("category", 0).toInt();

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
  m_Output.setFileName(newName);
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
  : IDownloadManager(parent), m_NexusInterface(nexusInterface), m_DirWatcher()
{
  connect(&m_DirWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(directoryChanged(QString)));
}


DownloadManager::~DownloadManager()
{
  for (QVector<DownloadInfo*>::iterator iter = m_ActiveDownloads.begin(); iter != m_ActiveDownloads.end(); ++iter) {
    delete *iter;
  }
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
  // further loops: busy waiting for all downloads to complete. This could be neater...
  while (!done) {
    QCoreApplication::processEvents();
    done = true;
    foreach (DownloadInfo *info, m_ActiveDownloads) {
      if ((info->m_State < STATE_CANCELED) ||
          (info->m_State != STATE_FETCHINGFILEINFO) || (info->m_State != STATE_FETCHINGMODINFO)) {
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
  m_DirWatcher.addPath(m_OutputDirectory);
  refreshList();
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

void DownloadManager::refreshList()
{
  // remove finished downloads
  for (QVector<DownloadInfo*>::iterator Iter = m_ActiveDownloads.begin(); Iter != m_ActiveDownloads.end();) {
    if (((*Iter)->m_State == STATE_READY) || ((*Iter)->m_State == STATE_INSTALLED)) {
      delete *Iter;
      Iter = m_ActiveDownloads.erase(Iter);
    } else {
      ++Iter;
    }
  }

  QStringList nameFilters(m_SupportedExtensions);
  foreach (const QString &extension, m_SupportedExtensions) {
    nameFilters.append("*." + extension);
  }

  nameFilters.append(QString("*").append(UNFINISHED));

  QDir dir(QDir::fromNativeSeparators(m_OutputDirectory));

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

    DownloadInfo *info = DownloadInfo::createFromMeta(fileName);
    if (info != NULL) {
      m_ActiveDownloads.push_front(info);
    }
  }
  qDebug("downloads after refresh: %d", m_ActiveDownloads.size());
  emit update(-1);
}


bool DownloadManager::addDownload(const QStringList &URLs,
                                  int modID, int fileID, const NexusInfo &nexusInfo)
{
  QString fileName = QFileInfo(URLs.first()).fileName();
  if (fileName.isEmpty()) {
    fileName = "unknown";
  }

  QNetworkRequest request(URLs.first());
  return addDownload(m_NexusInterface->getAccessManager()->get(request), URLs, fileName, modID, fileID, nexusInfo);
}

bool DownloadManager::addDownload(QNetworkReply *reply, const QStringList &URLs, const QString &fileName,
                                  int modID, int fileID, const NexusInfo &nexusInfo)
{
  // download invoked from an already open network reply (i.e. download link in the browser)
  DownloadInfo *newDownload = DownloadInfo::createNew(nexusInfo, modID, fileID, URLs);

  QString baseName = fileName;

  if (!nexusInfo.m_FileName.isEmpty()) {
    baseName = nexusInfo.m_FileName;
  } else {
    QString dispoName = getFileNameFromNetworkReply(reply);

    if (!dispoName.isEmpty()) {
      baseName = dispoName;
    }
  }

  if (QFile::exists(m_OutputDirectory + "/" + baseName) &&
      (QMessageBox::question(NULL, tr("Download again?"), tr("A file with the same name has already been downloaded. "
                             "Do you want to download it again? The new file will receive a different name."),
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)) {
    delete newDownload;
    return false;
  }

  newDownload->setName(getDownloadFileName(baseName), false);

  startDownload(reply, newDownload, false);

  emit update(-1);
  return true;
}


void DownloadManager::startDownload(QNetworkReply *reply, DownloadInfo *newDownload, bool resume)
{
  newDownload->m_Reply = reply;
  setState(newDownload, STATE_DOWNLOADING);
  if (newDownload->m_Urls.count() == 0) {
    newDownload->m_Urls = QStringList(reply->url().toString());
  }

  QIODevice::OpenMode mode = QIODevice::WriteOnly;
  if (resume) {
    mode |= QIODevice::Append;
  }

  if (!newDownload->m_Output.open(mode)) {
    reportError(tr("failed to download %1: could not open output file: %2")
                .arg(reply->url().toString()).arg(newDownload->m_Output.fileName()));
    return;
  }

  connect(newDownload->m_Reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
  connect(newDownload->m_Reply, SIGNAL(finished()), this, SLOT(downloadFinished()));
  connect(newDownload->m_Reply, SIGNAL(readyRead()), this, SLOT(downloadReadyRead()));
  connect(newDownload->m_Reply, SIGNAL(metaDataChanged()), this, SLOT(metaDataChanged()));

  if (!resume) {
    emit aboutToUpdate();

    m_ActiveDownloads.append(newDownload);

    emit update(-1);
  }
}


void DownloadManager::addNXMDownload(const QString &url)
{
  NXMUrl nxmInfo(url);
  m_RequestIDs.insert(m_NexusInterface->requestFileInfo(nxmInfo.getModId(), nxmInfo.getFileId(), this, QVariant()));
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

  if (m_ActiveDownloads.at(index)->m_State == STATE_DOWNLOADING) {
    setState(m_ActiveDownloads.at(index), STATE_PAUSING);
    qDebug("pausing %d - %s", index, m_ActiveDownloads[index]->m_FileName.toUtf8().constData());
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
    if (info->m_State == STATE_ERROR) {
      info->m_CurrentUrl = (info->m_CurrentUrl + 1) % info->m_Urls.count();
    }
    qDebug("request resume from url %s", qPrintable(info->currentURL()));
    QNetworkRequest request(info->currentURL());
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
    return NULL;
  }
}


void DownloadManager::queryInfo(int index)
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    reportError(tr("invalid index %1").arg(index));
    return;
  }
  DownloadInfo *info = m_ActiveDownloads[index];

  if (info->m_State < DownloadManager::STATE_READY) {
    // UI shouldn't allow this
    return;
  }

  if (info->m_ModID == 0UL) {
    QString fileName = getFileName(index);
    QString ignore;
    NexusInterface::interpretNexusFileName(fileName, ignore, info->m_ModID, true);
    if (info->m_ModID < 0) {
      QString modIDString;
      while (modIDString.isEmpty()) {
        modIDString = QInputDialog::getText(NULL, tr("Please enter the nexus mod id"), tr("Mod ID:"), QLineEdit::Normal,
                                            QString(), NULL, 0, Qt::ImhFormattedNumbersOnly);
        if (modIDString.isNull()) {
          // canceled
          return;
        } else if (modIDString.contains(QRegExp("[^0-9]"))) {
          qDebug("illegal character in mod-id");
          modIDString.clear();
        }
      }
      info->m_ModID = modIDString.toInt(NULL, 10);
    }
  }
  info->m_ReQueried = true;
  setState(info, STATE_FETCHINGMODINFO);
//  m_RequestIDs.insert(m_NexusInterface->requestFiles(info->m_ModID, this, qVariantFromValue(static_cast<void*>(info))));
}


int DownloadManager::numTotalDownloads() const
{
  return m_ActiveDownloads.size();
}


QString DownloadManager::getFilePath(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_OutputDirectory + "/" + m_ActiveDownloads.at(index)->m_FileName;
}


QString DownloadManager::getFileName(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_ActiveDownloads.at(index)->m_FileName;
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
  return (info->m_FileID == 0) || (info->m_ModID == 0) || info->m_NexusInfo.m_Version.isEmpty();
}


int DownloadManager::getModID(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }
  return m_ActiveDownloads.at(index)->m_ModID;
}


NexusInfo DownloadManager::getNexusInfo(int index) const
{
  if ((index < 0) || (index >= m_ActiveDownloads.size())) {
    throw MyException(tr("invalid index"));
  }

  return m_ActiveDownloads.at(index)->m_NexusInfo;
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
    std::tr1::regex exp("filename=\"(.*)\"");

    std::tr1::cmatch result;
    if (std::tr1::regex_search(reply->rawHeader("Content-Disposition").constData(), result, exp)) {
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
      m_RequestIDs.insert(m_NexusInterface->requestDescription(info->m_ModID, this, info->m_DownloadID));
    } break;
    case STATE_FETCHINGFILEINFO: {
      m_RequestIDs.insert(m_NexusInterface->requestFiles(info->m_ModID, this, info->m_DownloadID));
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
      if (index != NULL) {
        *index = i;
      }
      return m_ActiveDownloads[i];
    }
  }
  return NULL;
}


void DownloadManager::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
  if (bytesTotal == 0) {
    return;
  }
  int index = 0;
  DownloadInfo *info = findDownload(this->sender(), &index);
  if (info != NULL) {
    if (info->m_State == STATE_CANCELING) {
      setState(info, STATE_CANCELED);
    } else if (info->m_State == STATE_PAUSING) {
      setState(info, STATE_PAUSED);
    } else {
      if (bytesTotal > info->m_TotalSize) {
        qDebug("file size %s: %lld", qPrintable(info->m_FileName), bytesTotal);
        info->m_TotalSize = bytesTotal;
      }
      int oldProgress = info->m_Progress;
      info->m_Progress = ((info->m_ResumePos + bytesReceived) * 100) / (info->m_ResumePos + bytesTotal);
      if (oldProgress != info->m_Progress) {
        emit update(index);
      }
    }
  }
}


void DownloadManager::downloadReadyRead()
{
  DownloadInfo *info = findDownload(this->sender());
  if (info != NULL) {
    info->m_Output.write(info->m_Reply->readAll());
  }
}


void DownloadManager::createMetaFile(DownloadInfo *info)
{
  QSettings metaFile(QString("%1.meta").arg(info->m_Output.fileName()), QSettings::IniFormat);
  metaFile.setValue("modID", info->m_ModID);
  metaFile.setValue("fileID", info->m_FileID);
  metaFile.setValue("url", info->m_Urls.join(";"));
  metaFile.setValue("name", info->m_NexusInfo.m_Name);
  metaFile.setValue("modName", info->m_NexusInfo.m_ModName);
  metaFile.setValue("version", info->m_NexusInfo.m_Version);
  metaFile.setValue("fileCategory", info->m_NexusInfo.m_FileCategory);
  metaFile.setValue("newestVersion", info->m_NexusInfo.m_NewestVersion);
  metaFile.setValue("category", info->m_NexusInfo.m_Category);
  metaFile.setValue("installed", info->m_State == DownloadManager::STATE_INSTALLED);
  metaFile.setValue("uninstalled", info->m_State == DownloadManager::STATE_UNINSTALLED);
  metaFile.setValue("paused", (info->m_State == DownloadManager::STATE_PAUSED) ||
                              (info->m_State == DownloadManager::STATE_ERROR));

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
  if (info == NULL) return;

  info->m_NexusInfo.m_Category = result["category_id"].toInt();
  info->m_NexusInfo.m_ModName = result["name"].toString().trimmed();
  info->m_NexusInfo.m_NewestVersion = result["version"].toString();

  if (info->m_FileID != 0) {
    setState(info, STATE_READY);
  } else {
    setState(info, STATE_FETCHINGFILEINFO);
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
  if (info == NULL) return;

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
      info->m_NexusInfo.m_Name = fileInfo["name"].toString();
      info->m_NexusInfo.m_Version = fileInfo["version"].toString();
      info->m_NexusInfo.m_FileCategory = fileInfo["category_id"].toInt();
      info->m_FileID = fileInfo["id"].toInt();
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
        info->m_NexusInfo.m_Name = fileInfo["name"].toString();
        info->m_NexusInfo.m_Version = fileInfo["version"].toString();
        info->m_NexusInfo.m_FileCategory = fileInfo["category_id"].toInt();
        info->m_FileID = fileInfo["id"].toInt();
      } else {
        emit showMessage(tr("No matching file found on Nexus! Maybe this file is no longer available or it was renamed?"));
      }
    }
  } else {
    if (info->m_FileID == 0) {
      qWarning("could not determine file id for %s (state %d)",
               info->m_FileName.toUtf8().constData(), info->m_State);
    }
  }

  setState(info, STATE_READY);
}


void DownloadManager::nxmFileInfoAvailable(int modID, int fileID, QVariant, QVariant resultData, int requestID)
{
  std::set<int>::iterator idIter = m_RequestIDs.find(requestID);
  if (idIter == m_RequestIDs.end()) {
    return;
  } else {
    m_RequestIDs.erase(idIter);
  }

  NexusInfo info;

  QVariantMap result = resultData.toMap();

  info.m_Name = result["name"].toString();
  info.m_Version = result["version"].toString();
  info.m_FileName = result["uri"].toString();

  m_RequestIDs.insert(m_NexusInterface->requestDownloadURL(modID, fileID, this, qVariantFromValue(info)));
}


// sort function to sort by best download server
bool DownloadManager::ServerByPreference(const std::map<QString, int> &preferredServers, const QVariant &LHS, const QVariant &RHS)
{
  int LHSVal = 0;
  int RHSVal = 0;

  QVariantMap LHSMap = LHS.toMap();
  QVariantMap RHSMap = RHS.toMap();

  int LHSUsers = LHSMap["ConnectedUsers"].toInt();
  int RHSUsers = RHSMap["ConnectedUsers"].toInt();
  // 0 users is probably a sign that the server is offline. Since there is currently no
  // mechanism to try a different server, we avoid those without users
  if (LHSUsers == 0) {
    LHSVal -= 500;
  } else {
    LHSVal -= LHSUsers;
  }
  if (RHSUsers == 0) {
    RHSVal -= 500;
  } else {
    RHSVal -= RHSUsers;
  }

  // user preference. This is a bit silly because the more servers on the preferred list the higher the boost
  auto LHSPreference = preferredServers.find(LHSMap["Name"].toString());
  auto RHSPreference = preferredServers.find(RHSMap["Name"].toString());

  if (LHSPreference != preferredServers.end()) {
    LHSVal += 100 + LHSPreference->second * 20;
  }
  if (RHSPreference != preferredServers.end()) {
    RHSVal += 100 + RHSPreference->second * 20;
  }

  // premium isn't valued high because premium servers already get a massive boost for having few users online
  if (LHSMap["IsPremium"].toBool()) LHSVal += 5;
  if (RHSMap["IsPremium"].toBool()) RHSVal += 5;

  return RHSVal < LHSVal;
}

int DownloadManager::startDownloadURLs(const QStringList &urls)
{
  addDownload(urls, -1);
  return m_ActiveDownloads.size() - 1;
}

int DownloadManager::startDownloadNexusFile(int modID, int fileID)
{
  int newID = m_ActiveDownloads.size();
  addNXMDownload(QString("nxm://%1/mods/%2/files/%3").arg(ToQString(MOShared::GameInfo::instance().getGameName())).arg(modID).arg(fileID));
  return newID;
}

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

  NexusInfo info = userData.value<NexusInfo>();
  QVariantList resultList = resultData.toList();
  if (resultList.length() == 0) {
    emit showMessage(tr("No download server available. Please try again later."));
    return;
  }

  std::sort(resultList.begin(), resultList.end(), boost::bind(&DownloadManager::ServerByPreference, m_PreferredServers, _1, _2));

  QStringList URLs;

  foreach (const QVariant &server, resultList) {
    URLs.append(server.toMap()["URI"].toString());
  }

  addDownload(URLs, modID, fileID, info);
}


void DownloadManager::nxmRequestFailed(int modID, QVariant, int requestID, const QString &errorString)
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
    if (info->m_ModID == modID) {
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
  emit showMessage(tr("Failed to request file info from nexus: %1").arg(errorString));
}


void DownloadManager::downloadFinished()
{
  int index = 0;

  DownloadInfo *info = findDownload(this->sender(), &index);
  if (info != NULL) {
    QNetworkReply *reply = info->m_Reply;
    QByteArray data = info->m_Reply->readAll();
    info->m_Output.write(data);
    info->m_Output.close();

    bool error = false;

    if ((info->m_State != STATE_CANCELING) &&
        (info->m_State != STATE_PAUSING)) {
      if ((info->m_Output.size() == 0) ||
          ((reply->error() != QNetworkReply::NoError) && (reply->error() != QNetworkReply::OperationCanceledError)) ||
          reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("text", Qt::CaseInsensitive)) {
        if (info->m_Tries == 0) {
          emit showMessage(tr("Download failed: %1 (%2)").arg(reply->errorString()).arg(reply->error()));
        }
        error = true;
        setState(info, STATE_PAUSING);
      }
    }

    if (info->m_State == STATE_CANCELING) {
      setState(info, STATE_CANCELED);
    } else if (info->m_State == STATE_PAUSING) {
      info->m_Output.write(info->m_Reply->readAll());

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
      setState(info, STATE_FETCHINGMODINFO); // need to set this state before changing the file name, otherwise .unfinished is appended

      QString newName = getFileNameFromNetworkReply(reply);
      QString oldName = QFileInfo(info->m_Output).fileName();
      if (!newName.isEmpty() && (newName != oldName)) {
        info->setName(getDownloadFileName(newName), true);
      } else {
        info->setName(m_OutputDirectory + "/" + info->m_FileName, true); // don't rename but remove the ".unfinished" extension
      }

      emit update(index);
    }
    reply->close();
    reply->deleteLater();

    if ((info->m_Tries > 0) && error) {
      --info->m_Tries;
      resumeDownload(index);
    }
  } else {
    qWarning("no download index %d", index);
  }
}


void DownloadManager::metaDataChanged()
{
  int index = 0;

  DownloadInfo *info = findDownload(this->sender(), &index);
  if (info != NULL) {
    QString newName = getFileNameFromNetworkReply(info->m_Reply);
    if (!newName.isEmpty() && (newName != info->m_FileName)) {
      info->setName(getDownloadFileName(newName), true);
      refreshAlphabeticalTranslation();
      if (!info->m_Output.open(QIODevice::WriteOnly | QIODevice::Append)) {
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
