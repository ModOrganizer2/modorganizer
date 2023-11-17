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

#include "bbcode.h"
#include "downloadmanager.h"
#include "iplugingame.h"
#include "messagedialog.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "organizercore.h"
#include "pluginmanager.h"
#include "settings.h"
#include "shared/util.h"
#include "updatedialog.h"
#include "utility.h"
#include <report.h>
#include <versioninfo.h>

#include <QAbstractButton>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <Qt>
#include <QtAlgorithms>
#include <QtDebug>

#include <boost/bind/bind.hpp>

#include <Windows.h>  //for VS_FIXEDFILEINFO, GetLastError

#include <exception>
#include <map>
#include <stddef.h>  //for size_t
#include <stdexcept>

using namespace MOBase;
using namespace MOShared;

SelfUpdater::SelfUpdater(NexusInterface* nexusInterface)
    : m_Parent(nullptr), m_Interface(nexusInterface), m_Reply(nullptr), m_Attempts(3)
{
  m_MOVersion = createVersionInfo();
}

SelfUpdater::~SelfUpdater() {}

void SelfUpdater::setUserInterface(QWidget* widget)
{
  m_Parent = widget;
}

void SelfUpdater::setPluginManager(PluginManager* pluginManager)
{
  m_Interface->setPluginManager(pluginManager);
}

void SelfUpdater::testForUpdate(const Settings& settings)
{
  if (settings.network().offlineMode()) {
    log::debug("not checking for updates, in offline mode");
    return;
  }

  if (!settings.checkForUpdates()) {
    log::debug("not checking for updates, disabled");
    return;
  }

  // TODO: if prereleases are disabled we could just request the latest release
  // directly
  try {
    m_GitHub.releases(
        GitHub::Repository("Modorganizer2", "modorganizer"),
        [this](const QJsonArray& releases) {
          if (releases.isEmpty()) {
            // error message already logged
            return;
          }

          // We store all releases:
          CandidatesMap mreleases;
          for (const QJsonValue& releaseVal : releases) {
            QJsonObject release = releaseVal.toObject();
            if (!release["draft"].toBool() && (Settings::instance().usePrereleases() ||
                                               !release["prerelease"].toBool())) {
              auto version       = VersionInfo(release["tag_name"].toString());
              mreleases[version] = release;
            }
          }

          if (!mreleases.empty()) {
            auto lastKey = mreleases.begin()->first;
            if (lastKey > this->m_MOVersion) {

              // Fill m_UpdateCandidates with version strictly greater than the
              // current version:
              m_UpdateCandidates.clear();
              for (auto p : mreleases) {
                if (p.first > this->m_MOVersion) {
                  m_UpdateCandidates.insert(p);
                }
              }
              log::info("update available: {} -> {}",
                        this->m_MOVersion.displayString(3), lastKey.displayString(3));
              emit updateAvailable();
            } else if (lastKey < this->m_MOVersion) {
              // this could happen if the user switches from using prereleases to
              // stable builds. Should we downgrade?
              log::debug("This version is newer than the latest released one: {} -> {}",
                         this->m_MOVersion.displayString(3), lastKey.displayString(3));
            }
          }
        });
  }
  // Catch all is bad by design, should be improved
  catch (...) {
    log::debug("Unable to connect to github.com to check version");
  }
}

void SelfUpdater::startUpdate()
{
  // the button can't be pressed if there isn't an update candidate
  Q_ASSERT(!m_UpdateCandidates.empty());

  auto latestRelease = m_UpdateCandidates.begin()->second;

  UpdateDialog dialog(m_Parent);
  dialog.setVersions(MOShared::createVersionInfo().displayString(3),
                     latestRelease["tag_name"].toString());

  // We concatenate release details. We only include pre-release if those are
  // the latest release:
  QString details;
  bool includePreRelease = true;
  for (auto& p : m_UpdateCandidates) {
    auto& release = p.second;

    // Ignore details for pre-release after a release has been found:
    if (release["prerelease"].toBool() && !includePreRelease) {
      continue;
    }

    // Stop including pre-release as soon as we find a non-prerelease:
    if (!release["prerelease"].toBool()) {
      includePreRelease = false;
    }

    details += "\n## " + release["name"].toString() + "\n---\n";
    details += release["body"].toString();
  }

  // Need to call setDetailedText to create the QTextEdit and then be able to retrieve
  // it:
  dialog.setChangeLogs(details);

  int res = dialog.exec();

  if (dialog.result() == QDialog::Accepted) {
    bool found = false;
    for (const QJsonValue& assetVal : latestRelease["assets"].toArray()) {
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

void SelfUpdater::openOutputFile(const QString& fileName)
{
  QString outputPath =
      QDir::fromNativeSeparators(qApp->property("dataPath").toString()) + "/" +
      fileName;
  log::debug("downloading to {}", outputPath);
  m_UpdateFile.setFileName(outputPath);
  m_UpdateFile.open(QIODevice::WriteOnly);
}

void SelfUpdater::download(const QString& downloadLink)
{
  QNetworkAccessManager* accessManager = m_Interface->getAccessManager();
  QUrl dlUrl(downloadLink);
  QNetworkRequest request(dlUrl);
  m_Canceled = false;
  m_Reply    = accessManager->get(request);
  showProgress();

  connect(m_Reply, SIGNAL(downloadProgress(qint64, qint64)), this,
          SLOT(downloadProgress(qint64, qint64)));
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
      QUrl url =
          m_Reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
      m_UpdateFile.reset();
      download(url.toString());
      return;
    }
    m_UpdateFile.write(m_Reply->readAll());

    error = m_Reply->error();

    if (m_Reply->header(QNetworkRequest::ContentTypeHeader)
            .toString()
            .startsWith("text", Qt::CaseInsensitive)) {
      m_Canceled = true;
    }

    closeProgress();

    m_Reply->close();
    m_Reply->deleteLater();
    m_Reply = nullptr;
  }

  m_UpdateFile.close();

  if ((m_UpdateFile.size() == 0) || (error != QNetworkReply::NoError) || m_Canceled) {
    if (!m_Canceled) {
      reportError(tr("Download failed: %1").arg(error));
    }
    m_UpdateFile.remove();
    return;
  }

  log::debug("download: {}", m_UpdateFile.fileName());

  try {
    installUpdate();
  } catch (const std::exception& e) {
    reportError(tr("Failed to install update: %1").arg(e.what()));
  }
}

void SelfUpdater::downloadCancel()
{
  m_Canceled = true;
}

void SelfUpdater::installUpdate()
{
  const QString parameters = "/DIR=\"" + qApp->applicationDirPath() + "\" ";
  const auto r             = shell::Execute(m_UpdateFile.fileName(), parameters);

  if (r.success()) {
    QCoreApplication::quit();
  } else {
    reportError(
        tr("Failed to start %1: %2").arg(m_UpdateFile.fileName()).arg(r.toString()));
  }

  m_UpdateFile.remove();
}

void SelfUpdater::report7ZipError(QString const& errorMessage)
{
  QMessageBox::critical(m_Parent, tr("Error"), errorMessage);
}
