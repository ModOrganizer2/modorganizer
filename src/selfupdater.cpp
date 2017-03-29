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

#include "archive.h"
#include "callback.h"
#include "utility.h"
#include "installationmanager.h"
#include "iplugingame.h"
#include "messagedialog.h"
#include "downloadmanager.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "settings.h"
#include "bbcode.h"
#include <versioninfo.h>
#include <report.h>
#include <util.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProcess>
#include <QProgressDialog>
#include <QRegExp>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QAbstractButton>

#include <Qt>
#include <QtDebug>
#include <QtAlgorithms>

#include <boost/bind.hpp>

#include <Windows.h> //for VS_FIXEDFILEINFO, GetLastError

#include <exception>
#include <map>
#include <stddef.h> //for size_t
#include <stdexcept>

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
  , m_Reply(nullptr)
  , m_Attempts(3)
{
  QLibrary archiveLib(QCoreApplication::applicationDirPath() + "\\dlls\\archive.dll");
  if (!archiveLib.load()) {
    throw MyException(tr("archive.dll not loaded: \"%1\"").arg(archiveLib.errorString()));
  }

  CreateArchiveType CreateArchiveFunc = resolveFunction<CreateArchiveType>(archiveLib, "CreateArchive");

  m_ArchiveHandler = CreateArchiveFunc();
  if (!m_ArchiveHandler->isValid()) {
    throw MyException(InstallationManager::getErrorString(m_ArchiveHandler->getLastError()));
  }

  VS_FIXEDFILEINFO version = GetFileVersion(ToWString(QApplication::applicationFilePath()));

  m_MOVersion = VersionInfo(version.dwFileVersionMS >> 16,
                            version.dwFileVersionMS & 0xFFFF,
                            version.dwFileVersionLS >> 16,
                            version.dwFileVersionLS & 0xFFFF);
}


SelfUpdater::~SelfUpdater()
{
  delete m_ArchiveHandler;
}

void SelfUpdater::setUserInterface(QWidget *widget)
{
  m_Parent = widget;
}

void SelfUpdater::testForUpdate()
{
  // TODO: if prereleases are disabled we could just request the latest release
  // directly
  m_GitHub.releases(GitHub::Repository("LePresidente", "modorganizer"),
                    [this](const QJsonArray &releases) {
    QJsonObject newest;
    for (const QJsonValue &releaseVal : releases) {
      QJsonObject release = releaseVal.toObject();
      if (!release["draft"].toBool() && (Settings::instance().usePrereleases()
                                         || !release["prerelease"].toBool())) {
        if (newest.empty() || (VersionInfo(release["tag_name"].toString())
                               > VersionInfo(newest["tag_name"].toString()))) {
          newest = release;
        }
      }
    }

    if (!newest.empty()) {
      VersionInfo newestVer(newest["tag_name"].toString());
      if (newestVer > this->m_MOVersion) {
        m_UpdateCandidate = newest;
        qDebug("update available: %s -> %s",
               qPrintable(this->m_MOVersion.displayString()),
               qPrintable(newestVer.displayString()));
        emit updateAvailable();
      } else if (newestVer < this->m_MOVersion) {
        // this could happen if the user switches from using prereleases to
        // stable builds. Should we downgrade?
        qDebug("this version is newer than the newest installed one: %s -> %s",
               qPrintable(this->m_MOVersion.displayString()),
               qPrintable(newestVer.displayString()));
      }
    }
  });
}

void SelfUpdater::startUpdate()
{
  // the button can't be pressed if there isn't an update candidate
  Q_ASSERT(!m_UpdateCandidate.empty());

  QMessageBox query(QMessageBox::Question,
                    tr("New update available (%1)")
                        .arg(m_UpdateCandidate["tag_name"].toString()),
                    BBCode::convertToHTML(m_UpdateCandidate["body"].toString()),
                    QMessageBox::Yes | QMessageBox::Cancel, m_Parent);

  query.button(QMessageBox::Yes)->setText(tr("Install"));

  int res = query.exec();

  if (query.result() == QMessageBox::Yes) {
    bool found = false;
    for (const QJsonValue &assetVal : m_UpdateCandidate["assets"].toArray()) {
      QJsonObject asset = assetVal.toObject();
      if (asset["content_type"].toString() == "application/x-msdownload") {
        openOutputFile(asset["name"].toString());
        download(asset["browser_download_url"].toString());
        found = true;
        break;
      }
    }
    if (!found) {
      QMessageBox::warning(
          m_Parent, tr("Download failed"),
          tr("Failed to find correct download, please try again later."));
    }
  }
}


void SelfUpdater::showProgress()
{
  if (m_Progress == nullptr) {
    m_Progress = new QProgressDialog(m_Parent, Qt::Dialog);
    connect(m_Progress, SIGNAL(canceled()), this, SLOT(downloadCancel()));
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

void SelfUpdater::openOutputFile(const QString &fileName)
{
  QString outputPath = QDir::fromNativeSeparators(qApp->property("dataPath").toString()) + "/" + fileName;
  qDebug("downloading to %s", qPrintable(outputPath));
  m_UpdateFile.setFileName(outputPath);
  m_UpdateFile.open(QIODevice::WriteOnly);
}

void SelfUpdater::download(const QString &downloadLink)
{
  QNetworkAccessManager *accessManager = m_Interface->getAccessManager();
  QUrl dlUrl(downloadLink);
  QNetworkRequest request(dlUrl);
  m_Canceled = false;
  m_Reply = accessManager->get(request);
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
    if (m_Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 302) {
      QUrl url = m_Reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
      m_UpdateFile.reset();
      download(url.toString());
      return;
    }
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
  const QString mopath
      = QDir::fromNativeSeparators(qApp->property("dataPath").toString());

  HINSTANCE res = ::ShellExecuteW(
      nullptr, L"open", m_UpdateFile.fileName().toStdWString().c_str(), nullptr,
      nullptr, SW_SHOW);

  if (res > (HINSTANCE)32) {
    QCoreApplication::quit();
  } else {
    reportError(tr("Failed to start %1: %2")
                    .arg(m_UpdateFile.fileName())
                    .arg((int)res));
  }

  m_UpdateFile.remove();
}

void SelfUpdater::report7ZipError(QString const &errorMessage)
{
  QMessageBox::critical(m_Parent, tr("Error"), errorMessage);
}
