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

#include "selfupdater.h"
#include "utility.h"
#include "installationmanager.h"
#include "messagedialog.h"
#include "downloadmanager.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include <versioninfo.h>
#include <gameinfo.h>
#include <skyriminfo.h>
#include <report.h>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QDir>
#include <QLibrary>
#include <QProcess>
#include <QApplication>
#include <util.h>
#include <boost/bind.hpp>


using namespace MOBase;
using namespace MOShared;


typedef Archive* (*CreateArchiveType)();


template <typename T> static T resolveFunction(QLibrary &lib, const char *name)
{
  T temp = reinterpret_cast<T>(lib.resolve(name));
  if (temp == nullptr) {
    throw std::runtime_error(QObject::tr("invalid 7-zip32.dll: %1").arg(lib.errorString()).toLatin1().constData());
  }
  return temp;
}


SelfUpdater::SelfUpdater(NexusInterface *nexusInterface)
  : m_Parent(nullptr)
  , m_Interface(nexusInterface)
  , m_UpdateRequestID(-1)
  , m_Reply(nullptr)
  , m_Attempts(3)
{
  QLibrary archiveLib("dlls\\archive.dll");
  if (!archiveLib.load()) {
    throw MyException(tr("archive.dll not loaded: \"%1\"").arg(archiveLib.errorString()));
  }

  CreateArchiveType CreateArchiveFunc = resolveFunction<CreateArchiveType>(archiveLib, "CreateArchive");

  m_CurrentArchive = CreateArchiveFunc();
  if (!m_CurrentArchive->isValid()) {
    throw MyException(InstallationManager::getErrorString(m_CurrentArchive->getLastError()));
  }

  connect(m_Progress, SIGNAL(canceled()), this, SLOT(downloadCancel()));

  VS_FIXEDFILEINFO version = GetFileVersion(ToWString(QApplication::applicationFilePath()));

  m_MOVersion = VersionInfo(version.dwFileVersionMS >> 16,
                            version.dwFileVersionMS & 0xFFFF,
                            version.dwFileVersionLS >> 16);
}


SelfUpdater::~SelfUpdater()
{
  delete m_CurrentArchive;
}

void SelfUpdater::setUserInterface(QWidget *widget)
{
  m_Parent = widget;
}

void SelfUpdater::testForUpdate()
{
  if (QFile::exists(QCoreApplication::applicationDirPath() + "/mo_test_update.7z")) {
    emit updateAvailable();
    return;
  }

  if (m_UpdateRequestID == -1) {
    m_UpdateRequestID = m_Interface->requestDescription(
              SkyrimInfo::getNexusModIDStatic(), this, QVariant(),
              QString(), ToQString(SkyrimInfo::getNexusInfoUrlStatic()),
              SkyrimInfo::getNexusGameIDStatic());
  }
}

void SelfUpdater::startUpdate()
{
  if (QFile::exists(QCoreApplication::applicationDirPath() + "/mo_test_update.7z")) {
    m_UpdateFile.setFileName(QCoreApplication::applicationDirPath() + "/mo_test_update.7z");
    installUpdate();
    return;
  }

  if ((m_UpdateRequestID == -1) &&
      (!m_NewestVersion.isEmpty())) {
    if (QMessageBox::question(m_Parent, tr("Update"),
          tr("An update is available (newest version: %1), do you want to install it?").arg(m_NewestVersion),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      m_UpdateRequestID = m_Interface->requestFiles(SkyrimInfo::getNexusModIDStatic(),
                                                    this, m_NewestVersion,
                                                    ToQString(SkyrimInfo::getNexusInfoUrlStatic()));
    }
  }
}


void SelfUpdater::showProgress()
{
  if (m_Progress == nullptr) {
    m_Progress = new QProgressDialog(m_Parent, Qt::Dialog);
  }
  m_Progress->setModal(true);
  m_Progress->show();
  m_Progress->setValue(0);
  m_Progress->setWindowTitle(tr("Update"));
  m_Progress->setLabelText(tr("Download in progress"));
}

void SelfUpdater::closeProgress()
{
  if (m_Progress != nullptr) {
    m_Progress->hide();
    m_Progress->deleteLater();
    m_Progress = nullptr;
  }
}

void SelfUpdater::download(const QString &downloadLink, const QString &fileName)
{
  QNetworkAccessManager *accessManager = m_Interface->getAccessManager();
  QUrl dlUrl(downloadLink);
  QNetworkRequest request(dlUrl);
  m_Canceled = false;
  m_Reply = accessManager->get(request);
  m_UpdateFile.setFileName(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getOrganizerDirectory()).append("\\").append(fileName)));
  m_UpdateFile.open(QIODevice::WriteOnly);
  showProgress();

  connect(m_Reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
  connect(m_Reply, SIGNAL(finished()), this, SLOT(downloadFinished()));
  connect(m_Reply, SIGNAL(readyRead()), this, SLOT(downloadReadyRead()));
}


void SelfUpdater::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
  if (m_Reply != nullptr) {
    if (m_Canceled) {
      m_Reply->abort();
    } else {
      if (bytesTotal != 0) {
        if (m_Progress != nullptr) {
          m_Progress->setValue((bytesReceived * 100) / bytesTotal);
        }
      }
    }
  }
}


void SelfUpdater::downloadReadyRead()
{
  if (m_Reply != nullptr) {
    m_UpdateFile.write(m_Reply->readAll());
  }
}


void SelfUpdater::downloadFinished()
{
  int error = QNetworkReply::NoError;

  if (m_Reply != nullptr) {
    m_UpdateFile.write(m_Reply->readAll());

    error = m_Reply->error();

    if (m_Reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("text", Qt::CaseInsensitive)) {
      m_Canceled = true;
    }

    closeProgress();

    m_Reply->close();
    m_Reply->deleteLater();
    m_Reply = nullptr;
  }

  m_UpdateFile.close();

  if ((m_UpdateFile.size() == 0) ||
      (error != QNetworkReply::NoError) ||
      m_Canceled) {
    if (!m_Canceled) {
      reportError(tr("Download failed: %1").arg(error));
    }
    m_UpdateFile.remove();
    return;
  }

  qDebug("download: %s", m_UpdateFile.fileName().toUtf8().constData());

  try {
    installUpdate();
  } catch (const std::exception &e) {
    reportError(tr("Failed to install update: %1").arg(e.what()));
  }
}


void SelfUpdater::downloadCancel()
{
  m_Canceled = true;
}


void SelfUpdater::installUpdate()
{
  const QString mopath = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getOrganizerDirectory()));

  QString backupPath = mopath + "/update_backup";
  QDir().mkdir(backupPath);

  // rename files that are currently open so we can unpack the update
  if (!m_CurrentArchive->open(ToWString(QDir::toNativeSeparators(m_UpdateFile.fileName())).c_str(),
          new MethodCallback<SelfUpdater, void, LPSTR>(this, &SelfUpdater::queryPassword))) {
    throw MyException(tr("failed to open archive \"%1\": %2")
                      .arg(m_UpdateFile.fileName())
                      .arg(InstallationManager::getErrorString(m_CurrentArchive->getLastError())));
  }

  // move all files contained in the archive out of the way,
  // otherwise we can't overwrite everything
  FileData* const *data;
  size_t size;
  m_CurrentArchive->getFileList(data, size);

  for (size_t i = 0; i < size; ++i) {
    QString outputName = ToQString(data[i]->getFileName());
    if (outputName.startsWith("ModOrganizer\\", Qt::CaseInsensitive)) {
      outputName = outputName.mid(13);
      data[i]->addOutputFileName(ToWString(outputName).c_str());
    } else if (outputName != "ModOrganizer") {
      data[i]->addOutputFileName(ToWString(outputName).c_str());
    }
    QFileInfo file(mopath + "/" + outputName);
    if (file.exists() && file.isFile()) {
      if (!shellMove(QStringList(mopath + "/" + outputName),
                     QStringList(backupPath + "/" + outputName))) {
        reportError(tr("failed to move outdated files: %1. Please update manually.").arg(windowsErrorString(::GetLastError())));
        return;
      }
    }
  }

  // now unpack the archive into the mo directory
  if (!m_CurrentArchive->extract(GameInfo::instance().getOrganizerDirectory().c_str(),
         new MethodCallback<SelfUpdater, void, float>(this, &SelfUpdater::updateProgress),
         new MethodCallback<SelfUpdater, void, LPCWSTR>(this, &SelfUpdater::updateProgressFile),
         new MethodCallback<SelfUpdater, void, LPCWSTR>(this, &SelfUpdater::report7ZipError))) {
    throw std::runtime_error("extracting failed");
  }

  m_CurrentArchive->close();

  m_UpdateFile.remove();

  QMessageBox::information(m_Parent, tr("Update"), tr("Update installed, Mod Organizer will now be restarted."));

  QProcess newProcess;
  if (QFile::exists(mopath + "/ModOrganizer.exe")) {
    newProcess.startDetached(mopath + "/ModOrganizer.exe", QStringList("update"));
  } else {
    newProcess.startDetached(mopath + "/ModOrganiser.exe", QStringList("update"));
  }
  emit restart();
}

void SelfUpdater::queryPassword(LPSTR)
{
  // nop
}

void SelfUpdater::updateProgress(float)
{
  // nop
}

void SelfUpdater::updateProgressFile(LPCWSTR)
{
  // nop
}

void SelfUpdater::report7ZipError(LPCWSTR errorMessage)
{
  QMessageBox::critical(m_Parent, tr("Error"), ToQString(errorMessage));
}


QString SelfUpdater::retrieveNews(const QString &description)
{
  QStringList temp = description.split("[s][/s]");
  if (temp.length() < 2) {
    return QString();
  } else {
    return temp.at(1);
  }
}


void SelfUpdater::nxmDescriptionAvailable(int, QVariant, QVariant resultData, int requestID)
{
  if (requestID == m_UpdateRequestID) {
    m_UpdateRequestID = -1;

    QVariantMap result = resultData.toMap();
    QString motd = retrieveNews(result["description"].toString()).trimmed();
    if (motd.length() != 0) {
      emit motdAvailable(motd);
    }

    m_NewestVersion = result["version"].toString();
    if (m_NewestVersion.isEmpty()) {
      QTimer::singleShot(5000, this, SLOT(testForUpdate()));
    }
    VersionInfo currentVersion(m_MOVersion);
    VersionInfo newestVersion(m_NewestVersion);

    if (!m_NewestVersion.isEmpty() && (currentVersion < newestVersion)) {
      emit updateAvailable();
    } else if (newestVersion < currentVersion) {
      qDebug("this version is newer than the current version on nexus (%s vs %s)",
             currentVersion.canonicalString().toUtf8().constData(),
             newestVersion.canonicalString().toUtf8().constData());
    }
  }
}


void SelfUpdater::nxmFilesAvailable(int, QVariant userData, QVariant resultData, int requestID)
{
  if (requestID != m_UpdateRequestID) {
    return;
  }
  QString version = userData.toString();

  m_UpdateRequestID = -1;

  if (!resultData.canConvert<QVariantList>()) {
    qCritical("invalid files result: %s", resultData.toString().toUtf8().constData());
    reportError(tr("Failed to parse response. Please report this as a bug and include the file mo_interface.log."));
    return;
  }

  QVariantList result = resultData.toList();

  QRegExp updateExpList(QString("updates version ([0-9., ]*) to %1").arg(version));
  QRegExp updateExpRange(QString("updates version ([0-9.]*) - ([0-9.]*) to %1").arg(version));
  int updateFileID = -1;
  QString updateFileName;
  int mainFileID = -1;
  QString mainFileName;
  int mainFileSize = 0;

  for(QVariant file : result) {
    QVariantMap fileInfo = file.toMap();
    if (!fileInfo["uri"].toString().endsWith(".7z")) {
      continue;
    }

    if (fileInfo["version"].toString() == version) {
      if (fileInfo["category_id"].toInt() == 2) {
        QString description = fileInfo["description"].toString();
        // update
        if (updateExpList.indexIn(description) != -1) {
          // there is an update for the newest version of MO, but does
          // it apply to the current version?
          QStringList supportedVersions = updateExpList.cap(1).split(QRegExp(",[ ]*"), QString::SkipEmptyParts);
          if (supportedVersions.contains(m_MOVersion.canonicalString())) {
            updateFileID = fileInfo["id"].toInt();
            updateFileName = fileInfo["uri"].toString();
          } else {
            qDebug("update not supported from %s", m_MOVersion.canonicalString().toUtf8().constData());
          }
        } else if (updateExpRange.indexIn(description) != -1) {
          VersionInfo rangeLowEnd(updateExpRange.cap(1));
          VersionInfo rangeHighEnd(updateExpRange.cap(2));
          if ((rangeLowEnd <= m_MOVersion) &&
              (m_MOVersion <= rangeHighEnd)) {
            updateFileID = fileInfo["id"].toInt();
            updateFileName = fileInfo["uri"].toString();
            break;
          } else {
            qDebug("update not supported from %s", m_MOVersion.canonicalString().toUtf8().constData());
          }
        } else {
          qWarning("invalid update description: %s",
                   description.toUtf8().constData());
        }
      } else if (fileInfo["category_id"].toInt() == 1) {
        mainFileID = fileInfo["id"].toInt();
        mainFileName = fileInfo["uri"].toString();
        mainFileSize = fileInfo["size"].toInt();
      }
    }
  }

  if (updateFileID != -1) {
    qDebug("update available: %d", updateFileID);
    m_UpdateRequestID = m_Interface->requestDownloadURL(SkyrimInfo::getNexusModIDStatic(),
                                    updateFileID, this, updateFileName,
                                    ToQString(SkyrimInfo::getNexusInfoUrlStatic()));
  } else if (mainFileID != -1) {
    qDebug("full download required: %d", mainFileID);
    if (QMessageBox::question(m_Parent, tr("Update"),
            tr("No incremental update available for this version, "
               "the complete package needs to be downloaded (%1 kB)").arg(mainFileSize),
            QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok) {
      m_UpdateRequestID = m_Interface->requestDownloadURL(SkyrimInfo::getNexusModIDStatic(),
                                    mainFileID, this, mainFileName,
                                    ToQString(SkyrimInfo::getNexusInfoUrlStatic()));
    }
  } else {
    qCritical("no file for update found");
    MessageDialog::showMessage(tr("no file for update found. Please update manually."), m_Parent);
    closeProgress();
  }
}


void SelfUpdater::nxmRequestFailed(int, int, QVariant, int requestID, const QString &errorMessage)
{
  if (requestID == m_UpdateRequestID) {
    m_UpdateRequestID = -1;
    if (m_Attempts > 0) {
      QTimer::singleShot(60000, this, SLOT(testForUpdate()));
      --m_Attempts;
    } else {
      qWarning("Failed to retrieve update information: %s", qPrintable(errorMessage));
      MessageDialog::showMessage(tr("Failed to retrieve update information: %1").arg(errorMessage), m_Parent, false);
    }
  }
}


void SelfUpdater::nxmDownloadURLsAvailable(int, int, QVariant userData, QVariant resultData, int requestID)
{
  if (requestID == m_UpdateRequestID) {
    m_UpdateRequestID = -1;
    QVariantList serverList = resultData.toList();
    if (serverList.count() != 0) {
      std::map<QString, int> dummy;
      qSort(serverList.begin(), serverList.end(), boost::bind(&DownloadManager::ServerByPreference, dummy, _1, _2));


      QVariantMap dlServer = serverList.first().toMap();

      download(dlServer["URI"].toString(), userData.toString());
    } else {
      MessageDialog::showMessage(tr("No download server available. Please try again later."), m_Parent);
      closeProgress();
    }
  }
}
