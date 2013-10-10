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

#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "utility.h"
#include "json.h"
#include "selectiondialog.h"
#include <utility.h>
#include <regex>

using QtJson::Json;

using namespace MOBase;
using namespace MOShared;


NexusBridge::NexusBridge()
  : m_Interface(NexusInterface::instance())
  , m_Url(MOBase::ToQString(MOShared::GameInfo::instance().getNexusInfoUrl()))
{
}


void NexusBridge::requestDescription(int modID, QVariant userData)
{
  m_RequestIDs.insert(m_Interface->requestDescription(modID, this, userData, m_Url));
}

void NexusBridge::requestFiles(int modID, QVariant userData)
{
  m_RequestIDs.insert(m_Interface->requestFiles(modID, this, userData, m_Url));
}

void NexusBridge::requestFileInfo(int modID, int fileID, QVariant userData)
{
  m_RequestIDs.insert(m_Interface->requestFileInfo(modID, fileID, this, userData, m_Url));
}

void NexusBridge::requestDownloadURL(int modID, int fileID, QVariant userData)
{
  m_RequestIDs.insert(m_Interface->requestDownloadURL(modID, fileID, this, userData, m_Url));
}

void NexusBridge::requestToggleEndorsement(int modID, bool endorse, QVariant userData)
{
  m_RequestIDs.insert(m_Interface->requestToggleEndorsement(modID, endorse, this, userData, m_Url));
}

void NexusBridge::nxmDescriptionAvailable(int modID, QVariant userData, QVariant resultData, int requestID)
{
  std::set<int>::iterator iter = m_RequestIDs.find(requestID);
  if (iter != m_RequestIDs.end()) {
    m_RequestIDs.erase(iter);

    emit descriptionAvailable(modID, userData, resultData);
  }
}

void NexusBridge::nxmFilesAvailable(int modID, QVariant userData, QVariant resultData, int requestID)
{
  std::set<int>::iterator iter = m_RequestIDs.find(requestID);
  if (iter != m_RequestIDs.end()) {
    m_RequestIDs.erase(iter);

    QList<ModRepositoryFileInfo> fileInfoList;

    QVariantList resultList = resultData.toList();

    foreach(QVariant file, resultList) {
      ModRepositoryFileInfo temp;
      QVariantMap fileInfo = file.toMap();
      temp.uri = fileInfo["uri"].toString();
      temp.name = fileInfo["name"].toString();
      temp.description = fileInfo["description"].toString();
      temp.version = VersionInfo(fileInfo["version"].toString());
      temp.categoryID = fileInfo["category_id"].toInt();
      temp.fileID = fileInfo["id"].toInt();
      temp.fileSize = fileInfo["size"].toInt();
      fileInfoList.append(temp);
    }

    emit filesAvailable(modID, userData, fileInfoList);
  }
}

void NexusBridge::nxmFileInfoAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID)
{
  std::set<int>::iterator iter = m_RequestIDs.find(requestID);
  if (iter != m_RequestIDs.end()) {
    m_RequestIDs.erase(iter);
    emit fileInfoAvailable(modID, fileID, userData, resultData);
  }
}

void NexusBridge::nxmDownloadURLsAvailable(int modID, int fileID, QVariant userData, QVariant resultData, int requestID)
{
  std::set<int>::iterator iter = m_RequestIDs.find(requestID);
  if (iter != m_RequestIDs.end()) {
    m_RequestIDs.erase(iter);
    emit downloadURLsAvailable(modID, fileID, userData, resultData);
  }
}

void NexusBridge::nxmEndorsementToggled(int modID, QVariant userData, QVariant resultData, int requestID)
{
  std::set<int>::iterator iter = m_RequestIDs.find(requestID);
  if (iter != m_RequestIDs.end()) {
    m_RequestIDs.erase(iter);
    emit endorsementToggled(modID, userData, resultData);
  }
}

void NexusBridge::nxmRequestFailed(int modID, QVariant userData, int requestID, const QString &errorMessage)
{
  std::set<int>::iterator iter = m_RequestIDs.find(requestID);
  if (iter != m_RequestIDs.end()) {
    m_RequestIDs.erase(iter);
    emit requestFailed(modID, userData, errorMessage);
  }
}


QAtomicInt NexusInterface::NXMRequestInfo::s_NextID(0);


NexusInterface::NexusInterface()
  : m_NMMVersion()
{
  atexit(&cleanup);
  m_AccessManager = new NXMAccessManager(this);

  m_DiskCache = new QNetworkDiskCache(this);
  connect(m_AccessManager, SIGNAL(requestNXMDownload(QString)), this, SLOT(downloadRequestedNXM(QString)));
}


void NexusInterface::cleanup()
{
  delete NexusInterface::s_Instance;
  NexusInterface::s_Instance = NULL;
}


NXMAccessManager *NexusInterface::getAccessManager()
{
  return m_AccessManager;
}


NexusInterface *NexusInterface::s_Instance = NULL;


NexusInterface *NexusInterface::instance()
{
  if (s_Instance == NULL) {
    s_Instance = new NexusInterface;
  }
  return s_Instance;
}


void NexusInterface::setCacheDirectory(const QString &directory)
{
  m_DiskCache->setCacheDirectory(directory);
  m_AccessManager->setCache(m_DiskCache);
}


void NexusInterface::setNMMVersion(const QString &nmmVersion)
{
  m_NMMVersion = nmmVersion;
}


void NexusInterface::interpretNexusFileName(const QString &fileName, QString &modName, int &modID, bool query)
{
  static std::tr1::regex exp("^([a-zA-Z0-9_\\- ]*?)([-_ ][VvRr]?[0-9_]+)?-([1-9][0-9]+).*");
  static std::tr1::regex simpleexp("^([a-zA-Z0-9_]+)");

//  std::tr1::match_results<std::string::const_iterator> result;
  QByteArray fileNameUTF8 = fileName.toUtf8();
  std::tr1::cmatch result;
  if (std::tr1::regex_search(fileNameUTF8.constData(), result, exp)) {
    modName = QString::fromUtf8(result[1].str().c_str());
    modName = modName.replace('_', ' ').trimmed();

    std::string candidate = result[3].str();
    std::string candidate2 = result[2].str();
    if (candidate2.length() != 0 && (candidate2.find_last_of("VvRr") == std::string::npos)) {
      // well, that second match might be an id too...
      unsigned offset = strspn(candidate2.c_str(), "-_ ");
      if (offset < candidate2.length() && query) {
        SelectionDialog selection(tr("Failed to guess mod id for \"%1\", please pick the correct one").arg(fileName));
        QString r2Highlight(fileName);
        r2Highlight.insert(result.position(2) + result.length(2), "* ").insert(result.position(2) + offset, " *");
        QString r3Highlight(fileName);
        r3Highlight.insert(result.position(3) + result.length(3), "* ").insert(result.position(3), " *");

        selection.addChoice(candidate.c_str(), r3Highlight, strtol(candidate.c_str(), NULL, 10));
        selection.addChoice(candidate2.c_str() + offset, r2Highlight, abs(strtol(candidate2.c_str() + offset, NULL, 10)));
        if (selection.exec() == QDialog::Accepted) {
          modID = selection.getChoiceData().toInt();
        } else {
          modID = -1;
        }
      } else {
        modID = -1;
      }
    } else {
      modID = strtol(candidate.c_str(), NULL, 10);
    }
    qDebug("mod id guessed: %s -> %d", qPrintable(fileName), modID);
  } else if (std::tr1::regex_search(fileNameUTF8.constData(), result, simpleexp)) {
    qDebug("simple expression matched, using name only");
    modName = QString::fromUtf8(result[1].str().c_str());
    modName = modName.replace('_', ' ').trimmed();

    modID = -1;
  } else {
    qDebug("no expression matched!");
    modName.clear();
    modID = -1;
  }
}


int NexusInterface::requestDescription(int modID, QObject *receiver, QVariant userData, const QString &url)
{
  NXMRequestInfo requestInfo(modID, NXMRequestInfo::TYPE_DESCRIPTION, userData, url);
  m_RequestQueue.enqueue(requestInfo);

  connect(this, SIGNAL(nxmDescriptionAvailable(int,QVariant,QVariant,int)),
          receiver, SLOT(nxmDescriptionAvailable(int,QVariant,QVariant,int)), Qt::UniqueConnection);

  connect(this, SIGNAL(nxmRequestFailed(int,QVariant,int,QString)),
          receiver, SLOT(nxmRequestFailed(int,QVariant,int,QString)), Qt::UniqueConnection);

  nextRequest();
  return requestInfo.m_ID;
}


int NexusInterface::requestUpdates(const std::vector<int> &modIDs, QObject *receiver, QVariant userData, const QString &url)
{
  NXMRequestInfo requestInfo(modIDs, NXMRequestInfo::TYPE_GETUPDATES, userData, url);
  m_RequestQueue.enqueue(requestInfo);

  connect(this, SIGNAL(nxmUpdatesAvailable(std::vector<int>,QVariant,QVariant,int)),
          receiver, SLOT(nxmUpdatesAvailable(std::vector<int>,QVariant,QVariant,int)), Qt::UniqueConnection);

  connect(this, SIGNAL(nxmRequestFailed(int,QVariant,int,QString)),
          receiver, SLOT(nxmRequestFailed(int,QVariant,int,QString)), Qt::UniqueConnection);

  nextRequest();
  return requestInfo.m_ID;
}


void NexusInterface::fakeFiles()
{
  static int id = 42;

  QVariantList result;
  QVariantMap fileMap;
  fileMap["uri"] = "fakeURI";
  fileMap["name"] = "fakeName";
  fileMap["description"] = "fakeDescription";
  fileMap["version"] = "1.0.0";
  fileMap["category_id"] = "1";
  fileMap["id"] = "1";
  fileMap["size"] = "512";
  result.append(fileMap);

  emit nxmFilesAvailable(1234, "fake", result, id++);
}


int NexusInterface::requestFiles(int modID, QObject *receiver, QVariant userData, const QString &url)
{
  NXMRequestInfo requestInfo(modID, NXMRequestInfo::TYPE_FILES, userData, url);
  m_RequestQueue.enqueue(requestInfo);
  connect(this, SIGNAL(nxmFilesAvailable(int,QVariant,QVariant,int)),
          receiver, SLOT(nxmFilesAvailable(int,QVariant,QVariant,int)), Qt::UniqueConnection);

  connect(this, SIGNAL(nxmRequestFailed(int,QVariant,int,QString)),
          receiver, SLOT(nxmRequestFailed(int,QVariant,int,QString)), Qt::UniqueConnection);

//  QTimer::singleShot(1000, this, SLOT(fakeFiles()));
//  static int fID = 42;
//  return fID++;

  nextRequest();
  return requestInfo.m_ID;
}


int NexusInterface::requestFileInfo(int modID, int fileID, QObject *receiver, QVariant userData, const QString &url)
{
  NXMRequestInfo requestInfo(modID, fileID, NXMRequestInfo::TYPE_FILEINFO, userData, url);
  m_RequestQueue.enqueue(requestInfo);

  connect(this, SIGNAL(nxmFileInfoAvailable(int,int,QVariant,QVariant,int)),
          receiver, SLOT(nxmFileInfoAvailable(int,int,QVariant,QVariant,int)), Qt::UniqueConnection);

  connect(this, SIGNAL(nxmRequestFailed(int,QVariant,int,QString)),
          receiver, SLOT(nxmRequestFailed(int,QVariant,int,QString)), Qt::UniqueConnection);

  nextRequest();
  return requestInfo.m_ID;
}


int NexusInterface::requestDownloadURL(int modID, int fileID, QObject *receiver, QVariant userData, const QString &url)
{
  NXMRequestInfo requestInfo(modID, fileID, NXMRequestInfo::TYPE_DOWNLOADURL, userData, url);
  m_RequestQueue.enqueue(requestInfo);

  connect(this, SIGNAL(nxmDownloadURLsAvailable(int,int,QVariant,QVariant,int)),
          receiver, SLOT(nxmDownloadURLsAvailable(int,int,QVariant,QVariant,int)), Qt::UniqueConnection);

  connect(this, SIGNAL(nxmRequestFailed(int,QVariant,int,QString)),
          receiver, SLOT(nxmRequestFailed(int,QVariant,int,QString)), Qt::UniqueConnection);

  nextRequest();
  return requestInfo.m_ID;
}


int NexusInterface::requestToggleEndorsement(int modID, bool endorse, QObject *receiver, QVariant userData, const QString &url)
{
  NXMRequestInfo requestInfo(modID, NXMRequestInfo::TYPE_TOGGLEENDORSEMENT, userData, url);
  requestInfo.m_Endorse = endorse;
  m_RequestQueue.enqueue(requestInfo);

  connect(this, SIGNAL(nxmEndorsementToggled(int,QVariant,QVariant,int)),
          receiver, SLOT(nxmEndorsementToggled(int,QVariant,QVariant,int)), Qt::UniqueConnection);

  connect(this, SIGNAL(nxmRequestFailed(int,QVariant,int,QString)),
          receiver, SLOT(nxmRequestFailed(int,QVariant,int,QString)), Qt::UniqueConnection);

  nextRequest();
  return requestInfo.m_ID;
}


void NexusInterface::nextRequest()
{
  if ((m_ActiveRequest.size() >= MAX_ACTIVE_DOWNLOADS) || (m_RequestQueue.isEmpty())) {
    return;
  }

  NXMRequestInfo info = m_RequestQueue.dequeue();
  info.m_Timeout = new QTimer(this);
  info.m_Timeout->setInterval(60000);

  QString url;
  switch (info.m_Type) {
    case NXMRequestInfo::TYPE_DESCRIPTION: {
      url = QString("%1/Mods/%2/").arg(info.m_URL).arg(info.m_ModID);
    } break;
    case NXMRequestInfo::TYPE_FILES: {
      url = QString("%1/Files/indexfrommod/%2/").arg(info.m_URL).arg(info.m_ModID);
    } break;
    case NXMRequestInfo::TYPE_FILEINFO: {
      url = QString("%1/Files/%2/").arg(info.m_URL).arg(info.m_FileID);
    } break;
    case NXMRequestInfo::TYPE_DOWNLOADURL: {
      url = QString("%1/Files/download/%2").arg(info.m_URL).arg(info.m_FileID);
    } break;
    case NXMRequestInfo::TYPE_TOGGLEENDORSEMENT: {
      url = QString("%1/Mods/toggleendorsement/%2?lvote=%3").arg(info.m_URL).arg(info.m_ModID).arg(!info.m_Endorse);
    } break;
    case NXMRequestInfo::TYPE_GETUPDATES: {
      QString modIDList = VectorJoin<int>(info.m_ModIDList, ",");
      modIDList = "[" + modIDList + "]";
      url = QString("%1/Mods/GetUpdates?ModList=%2").arg(info.m_URL).arg(modIDList);
    } break;
  }

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml");
#pragma message("automatically insert the correct version number")
  request.setRawHeader("User-Agent", QString("Mod Organizer v1.0.5 (compatible to Nexus Client v%1)").arg(m_NMMVersion).toUtf8());

  info.m_Reply = m_AccessManager->get(request);

  connect(info.m_Reply, SIGNAL(finished()), this, SLOT(requestFinished()));
  connect(info.m_Reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(requestError(QNetworkReply::NetworkError)));
  connect(info.m_Timeout, SIGNAL(timeout()), this, SLOT(requestTimeout()));
  info.m_Timeout->start();
  m_ActiveRequest.push_back(info);
}


void NexusInterface::downloadRequestedNXM(const QString &url)
{
  emit requestNXMDownload(url);
}

void NexusInterface::requestFinished(std::list<NXMRequestInfo>::iterator iter)
{
  QNetworkReply *reply = iter->m_Reply;

  if (reply->error() != QNetworkReply::NoError) {
    qWarning("request failed: %s", reply->errorString().toUtf8().constData());
    emit nxmRequestFailed(iter->m_ModID, iter->m_UserData, iter->m_ID, reply->errorString());
  } else {
    QByteArray data = reply->readAll();
    if (data.isNull() || data.isEmpty() || (strcmp(data.constData(), "null") == 0)) {
      QString nexusError(reply->rawHeader("NexusErrorInfo"));
      if (nexusError.length() == 0) {
        nexusError = tr("empty response");
      }

      emit nxmRequestFailed(iter->m_ModID, iter->m_UserData, iter->m_ID, nexusError);
    } else {
      bool ok;
      QVariant result = Json::parse(data, ok);
      if (result.isValid() && ok) {
        switch (iter->m_Type) {
          case NXMRequestInfo::TYPE_DESCRIPTION: {
            emit nxmDescriptionAvailable(iter->m_ModID, iter->m_UserData, result, iter->m_ID);
          } break;
          case NXMRequestInfo::TYPE_FILES: {
            emit nxmFilesAvailable(iter->m_ModID, iter->m_UserData, result, iter->m_ID);
          } break;
          case NXMRequestInfo::TYPE_FILEINFO: {
            emit nxmFileInfoAvailable(iter->m_ModID, iter->m_FileID, iter->m_UserData, result, iter->m_ID);
          } break;
          case NXMRequestInfo::TYPE_DOWNLOADURL: {
            emit nxmDownloadURLsAvailable(iter->m_ModID, iter->m_FileID, iter->m_UserData, result, iter->m_ID);
          } break;
          case NXMRequestInfo::TYPE_GETUPDATES: {
            emit nxmUpdatesAvailable(iter->m_ModIDList, iter->m_UserData, result, iter->m_ID);
          } break;
          case NXMRequestInfo::TYPE_TOGGLEENDORSEMENT: {
            emit nxmEndorsementToggled(iter->m_ModID, iter->m_UserData, result, iter->m_ID);
          } break;
        }
      } else {
        emit nxmRequestFailed(iter->m_ModID, iter->m_UserData, iter->m_ID, tr("invalid response"));
      }
    }
  }
}


void NexusInterface::requestFinished()
{
  QNetworkReply *reply = static_cast<QNetworkReply*>(sender());
  for (std::list<NXMRequestInfo>::iterator iter = m_ActiveRequest.begin(); iter != m_ActiveRequest.end(); ++iter) {
    if (iter->m_Reply == reply) {
      iter->m_Timeout->stop();
      iter->m_Timeout->deleteLater();
      requestFinished(iter);
      iter->m_Reply->deleteLater();
      m_ActiveRequest.erase(iter);
      nextRequest();
      return;
    }
  }
}


void NexusInterface::requestError(QNetworkReply::NetworkError)
{
  QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
  if (reply == NULL) {
    qWarning("invalid sender type");
    return;
  }

  qCritical("request error: %s", reply->errorString().toUtf8().constData());
}


void NexusInterface::requestTimeout()
{
  QTimer *timer =  qobject_cast<QTimer*>(sender());
  if (timer == NULL) {
    qWarning("invalid sender type");
    return;
  }
  qWarning("request timeout");
  for (std::list<NXMRequestInfo>::iterator iter = m_ActiveRequest.begin(); iter != m_ActiveRequest.end(); ++iter) {
    if (iter->m_Timeout == timer) {
      // this abort causes a "request failed" which cleans up the rest
      iter->m_Reply->abort();
      return;
    }
  }
}
