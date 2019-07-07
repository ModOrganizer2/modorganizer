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

#include "nxmaccessmanager.h"
#include "iplugingame.h"
#include "nexusinterface.h"
#include "nxmurl.h"
#include "report.h"
#include "utility.h"
#include "selfupdater.h"
#include "persistentcookiejar.h"
#include "settings.h"
#include <QMessageBox>
#include <QPushButton>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QCoreApplication>
#include <QDir>
#include <QUrlQuery>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>

using namespace MOBase;

namespace {
  QString const nexusBaseUrl("https://api.nexusmods.com/v1");
}

NXMAccessManager::NXMAccessManager(QObject *parent, const QString &moVersion)
  : QNetworkAccessManager(parent)
  , m_TopLevel(nullptr)
  , m_ValidateReply(nullptr)
  , m_ProgressDialog(nullptr)
  , m_MOVersion(moVersion)
{
  m_ValidateTimeout.setSingleShot(true);
  m_ValidateTimeout.setInterval(30000);
  connect(&m_ValidateTimeout, SIGNAL(timeout()), this, SLOT(validateTimeout()));
  setCookieJar(new PersistentCookieJar(
      QDir::fromNativeSeparators(Settings::instance().getCacheDirectory() + "/nexus_cookies.dat")));

  if (networkAccessible() == QNetworkAccessManager::UnknownAccessibility) {
    // why is this necessary all of a sudden?
    setNetworkAccessible(QNetworkAccessManager::Accessible);
  }
}

NXMAccessManager::~NXMAccessManager()
{
  if (m_ValidateReply != nullptr) {
    m_ValidateReply->deleteLater();
    m_ValidateReply = nullptr;
  }
}

void NXMAccessManager::setTopLevelWidget(QWidget* w)
{
  m_TopLevel = w;

  if (m_ProgressDialog) {
    const auto wasVisible = m_ProgressDialog->isVisible();

    m_ProgressDialog->hide();
    m_ProgressDialog->setParent(w, m_ProgressDialog->windowFlags() | Qt::Dialog);
    m_ProgressDialog->setModal(false);
    m_ProgressDialog->setVisible(wasVisible);
  }
}

QNetworkReply *NXMAccessManager::createRequest(
    QNetworkAccessManager::Operation operation, const QNetworkRequest &request,
    QIODevice *device)
{
  if (request.url().scheme() != "nxm") {
    return QNetworkAccessManager::createRequest(operation, request, device);
  }
  if (operation == GetOperation) {
    emit requestNXMDownload(request.url().toString());

    // eat the request, everything else will be done by the download manager
    return QNetworkAccessManager::createRequest(QNetworkAccessManager::GetOperation,
                                                QNetworkRequest(QUrl()));
  } else if (operation == PostOperation) {
    return QNetworkAccessManager::createRequest(operation, request, device);;
  } else {
    return QNetworkAccessManager::createRequest(operation, request, device);
  }
}


void NXMAccessManager::showCookies() const
{
  QUrl url(nexusBaseUrl + "/");
  for (const QNetworkCookie &cookie : cookieJar()->cookiesForUrl(url)) {
    qDebug("%s - %s (expires: %s)",
           cookie.name().constData(), cookie.value().constData(),
           qUtf8Printable(cookie.expirationDate().toString()));
  }
}

void NXMAccessManager::clearCookies()
{
  PersistentCookieJar *jar = qobject_cast<PersistentCookieJar*>(cookieJar());
  if (jar != nullptr) {
    jar->clear();
  } else {
    qWarning("failed to clear cookies, invalid cookie jar");
  }
}

void NXMAccessManager::startValidationCheck()
{
  qDebug("Checking Nexus API Key...");
  QString requestString = nexusBaseUrl + "/users/validate";

  QNetworkRequest request(requestString);
  request.setRawHeader("APIKEY", m_ApiKey.toUtf8());
  request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, userAgent().toUtf8());
  request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/json");
  request.setRawHeader("Protocol-Version", "1.0.0");
  request.setRawHeader("Application-Name", "MO2");
  request.setRawHeader("Application-Version", m_MOVersion.toUtf8());

  if (!m_ProgressDialog) {
    m_ProgressDialog = new QProgressDialog(m_TopLevel);
    m_ProgressDialog->setModal(false);
  }

  m_ProgressDialog->setLabelText(tr("Validating Nexus Connection"));
  QList<QPushButton*> buttons = m_ProgressDialog->findChildren<QPushButton*>();
  buttons.at(0)->setEnabled(false);
  m_ProgressDialog->show();
  QCoreApplication::processEvents(); // for some reason the whole app hangs during the login. This way the user has at least a little feedback

  m_ValidateReply = get(request);
  m_ValidateTimeout.start();
  m_ValidateState = VALIDATE_CHECKING;
  connect(m_ValidateReply, SIGNAL(finished()), this, SLOT(validateFinished()));
  connect(m_ValidateReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(validateError(QNetworkReply::NetworkError)));
}


bool NXMAccessManager::validated() const
{
  if (m_ValidateState == VALIDATE_CHECKING) {
    if (!m_ProgressDialog) {
      m_ProgressDialog = new QProgressDialog(m_TopLevel);
      m_ProgressDialog->setModal(false);
    }

    m_ProgressDialog->setLabelText(tr("Validating Nexus Connection"));
    QList<QPushButton*> buttons = m_ProgressDialog->findChildren<QPushButton*>();
    buttons.at(0)->setEnabled(false);
    m_ProgressDialog->show();
    while (m_ValidateState == VALIDATE_CHECKING) {
      QCoreApplication::processEvents();
      QThread::msleep(100);
    }

    m_ProgressDialog->hide();
    m_ProgressDialog->deleteLater();
    m_ProgressDialog = nullptr;
  }

  return m_ValidateState == VALIDATE_VALID;
}


void NXMAccessManager::refuseValidation()
{
  m_ValidateState = VALIDATE_REFUSED;
}


bool NXMAccessManager::validateAttempted() const
{
  return m_ValidateState != VALIDATE_NOT_CHECKED;
}


bool NXMAccessManager::validateWaiting() const
{
  return m_ValidateReply != nullptr;
}


void NXMAccessManager::apiCheck(const QString &apiKey, bool force)
{
  if (m_ValidateReply != nullptr) {
    return;
  }

  if (force) {
    m_ValidateState = VALIDATE_NOT_CHECKED;
  }

  if (m_ValidateState == VALIDATE_VALID) {
    emit validateSuccessful(false);
    return;
  }

  m_ApiKey = apiKey;
  startValidationCheck();
}


QString NXMAccessManager::userAgent(const QString &subModule) const
{
  QStringList comments;
  QString os;
  if (QSysInfo::productType() == "windows")
    comments << ((QSysInfo::kernelType() == "winnt") ? "Windows_NT " : "Windows ") + QSysInfo::kernelVersion();
  else
    comments << QSysInfo::kernelType().left(1).toUpper() + QSysInfo::kernelType().mid(1)
    << QSysInfo::productType().left(1).toUpper() + QSysInfo::kernelType().mid(1) + " " + QSysInfo::productVersion();
  if (!subModule.isEmpty()) {
    comments << "module: " + subModule;
  }
  comments << ((QSysInfo::buildCpuArchitecture() == "x86_64") ? "x64" : "x86");

  return  QString("Mod Organizer/%1 (%2) Qt/%3").arg(m_MOVersion, comments.join("; "), qVersion());
}


QString NXMAccessManager::apiKey() const
{
  return m_ApiKey;
}

void NXMAccessManager::clearApiKey()
{
  m_ApiKey = "";
  m_ValidateState = VALIDATE_NOT_VALID;

  emit credentialsReceived(APIUserAccount());
}

void NXMAccessManager::validateTimeout()
{
  m_ValidateTimeout.stop();
  if (m_ProgressDialog != nullptr) {
    m_ProgressDialog->hide();
    m_ProgressDialog->deleteLater();
    m_ProgressDialog = nullptr;
  }
  m_ApiKey.clear();
  m_ValidateState = VALIDATE_NOT_VALID;

  if (m_ValidateReply != nullptr) {
    m_ValidateReply->deleteLater();
    m_ValidateReply = nullptr;
  }

  emit validateFailed(tr("There was a timeout during the request"));
}


void NXMAccessManager::validateError(QNetworkReply::NetworkError)
{
  m_ValidateTimeout.stop();
  if (m_ProgressDialog != nullptr) {
    m_ProgressDialog->hide();
    m_ProgressDialog->deleteLater();
    m_ProgressDialog = nullptr;
  }
  m_ApiKey.clear();
  m_ValidateState = VALIDATE_NOT_VALID;

  if (m_ValidateReply != nullptr) {
    m_ValidateReply->disconnect();
    QString error = m_ValidateReply->errorString();
    m_ValidateReply->deleteLater();
    m_ValidateReply = nullptr;
    emit validateFailed(error);
  } else {
    emit validateFailed(tr("Unknown error"));
  }
}


void NXMAccessManager::validateFinished()
{
  m_ValidateTimeout.stop();
  if (m_ProgressDialog != nullptr) {
    m_ProgressDialog->deleteLater();
    m_ProgressDialog = nullptr;
  }

  if (m_ValidateReply != nullptr) {
    QJsonDocument jdoc = QJsonDocument::fromJson(m_ValidateReply->readAll());
    if (!jdoc.isNull()) {
      QJsonObject credentialsData = jdoc.object();
      if (credentialsData.contains("user_id")) {
        int id = credentialsData.value("user_id").toInt();
        QString name = credentialsData.value("name").toString();
        bool premium = credentialsData.value("is_premium").toBool();

        const auto user = APIUserAccount()
          .id(QString("%1").arg(id))
          .name(name)
          .type(premium ? APIUserAccountTypes::Premium : APIUserAccountTypes::Regular)
          .limits(NexusInterface::parseLimits(m_ValidateReply));


        emit credentialsReceived(user);

        m_ValidateReply->deleteLater();
        m_ValidateReply = nullptr;

        m_ValidateState = VALIDATE_VALID;
        emit validateSuccessful(true);

      } else {
        m_ApiKey.clear();
        m_ValidateState = VALIDATE_NOT_VALID;
        emit validateFailed(tr("Validation failed, please reauthenticate in the Settings -> Nexus tab: %1").arg(credentialsData.value("message").toString()));
      }
    } else {
      m_ApiKey.clear();
      m_ValidateState = VALIDATE_NOT_CHECKED;
      emit validateFailed(tr("Could not parse response. Invalid JSON."));
    }
  }
  else {
    m_ApiKey.clear();
    m_ValidateState = VALIDATE_NOT_CHECKED;
    emit validateFailed(tr("Unknown error."));
  }
}
