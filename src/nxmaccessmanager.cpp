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
#include "nxmurl.h"
#include "report.h"
#include "utility.h"
#include "selfupdater.h"
#include "persistentcookiejar.h"
#include "settings.h"
#include <gameinfo.h>
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
  QString const Nexus_Management_URL("http://nmm.nexusmods.com");
}

// unfortunately Nexus doesn't seem to document these states, all I know is all these listed
// are considered premium (27 should be lifetime premium)
const std::set<int> NXMAccessManager::s_PremiumAccountStates { 4, 6, 13, 27, 31, 32 };


NXMAccessManager::NXMAccessManager(QObject *parent, const QString &moVersion)
  : QNetworkAccessManager(parent)
  , m_LoginReply(nullptr)
  , m_MOVersion(moVersion)
{
  m_LoginTimeout.setSingleShot(true);
  m_LoginTimeout.setInterval(30000);
  setCookieJar(new PersistentCookieJar(
      QDir::fromNativeSeparators(Settings::instance().getCacheDirectory() + "/nexus_cookies.dat")));
}

NXMAccessManager::~NXMAccessManager()
{
  if (m_LoginReply != nullptr) {
    m_LoginReply->deleteLater();
    m_LoginReply = nullptr;
  }
}

void NXMAccessManager::setNMMVersion(const QString &nmmVersion)
{
  m_NMMVersion = nmmVersion;
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
  QUrl url(Nexus_Management_URL + "/");
  for (const QNetworkCookie &cookie : cookieJar()->cookiesForUrl(url)) {
    qDebug("%s - %s (expires: %s)",
           cookie.name().constData(), cookie.value().constData(),
           qPrintable(cookie.expirationDate().toString()));
  }
}


void NXMAccessManager::startLoginCheck()
{
  if (hasLoginCookies()) {
    qDebug("validating login cookies");
    QNetworkRequest request(Nexus_Management_URL + "/Sessions/?Validate");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("User-Agent", userAgent().toUtf8());

    m_LoginReply = get(request);
    m_LoginTimeout.start();
    m_LoginState = LOGIN_CHECKING;
    connect(m_LoginReply, SIGNAL(finished()), this, SLOT(loginChecked()));
    connect(m_LoginReply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(loginError(QNetworkReply::NetworkError)));
  }
}


void NXMAccessManager::retrieveCredentials()
{
  qDebug("retrieving credentials");

  QNetworkRequest request(Nexus_Management_URL + "/Core/Libs/Flamework/Entities/User?GetCredentials");
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  request.setRawHeader("User-Agent", userAgent().toUtf8());

  QNetworkReply *reply = get(request);
  QTimer timeout;
  connect(&timeout, &QTimer::timeout, [reply] () {
    reply->deleteLater();
  });
  timeout.start();

  connect(reply, &QNetworkReply::finished, [reply, this] () {
    QJsonDocument jdoc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray credentialsData = jdoc.array();
    emit credentialsReceived(credentialsData.at(2).toString(),
                             s_PremiumAccountStates.find(credentialsData.at(1).toInt())
                                                         != s_PremiumAccountStates.end());
    reply->deleteLater();
  });

  connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
          [=] (QNetworkReply::NetworkError) {
    qDebug("failed to retrieve account credentials: %s", qPrintable(reply->errorString()));
    reply->deleteLater();
  });
}


bool NXMAccessManager::loggedIn() const
{
  if (m_LoginState == LOGIN_CHECKING) {
    QProgressDialog progress;
    progress.setLabelText(tr("Verifying Nexus login"));
    progress.show();
    while (m_LoginState == LOGIN_CHECKING) {
      QCoreApplication::processEvents();
      QThread::msleep(100);
    }
    progress.hide();
  }

  return m_LoginState == LOGIN_VALID;
}


void NXMAccessManager::refuseLogin()
{
  m_LoginState = LOGIN_REFUSED;
}


bool NXMAccessManager::loginAttempted() const
{
  return m_LoginState != LOGIN_NOT_CHECKED;
}


bool NXMAccessManager::loginWaiting() const
{
  return m_LoginReply != nullptr;
}


void NXMAccessManager::login(const QString &username, const QString &password)
{
  if (m_LoginReply != nullptr) {
    return;
  }

  if (m_LoginState == LOGIN_VALID) {
    emit loginSuccessful(false);
    return;
  }

  m_Username = username;
  m_Password = password;
  pageLogin();
}


QString NXMAccessManager::userAgent(const QString &subModule) const
{
  QStringList comments;
  comments << "compatible to Nexus Client v" + m_NMMVersion;
  if (!subModule.isEmpty()) {
    comments << "module: " + subModule;
  }

  return  QString("Mod Organizer v%1 (%2)").arg(m_MOVersion, comments.join("; "));
}


void NXMAccessManager::pageLogin()
{
  qDebug("logging %s in on Nexus", qPrintable(m_Username));

  QString requestString = (Nexus_Management_URL + "/Sessions/?Login&uri=%1")
                            .arg(QString(QUrl::toPercentEncoding(Nexus_Management_URL)));

  QNetworkRequest request(requestString);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  QByteArray postDataQuery;
  QUrlQuery postData;
  postData.addQueryItem("username", m_Username);
  postData.addQueryItem("password", m_Password);
  postDataQuery = postData.query(QUrl::FullyEncoded).toUtf8();

  request.setRawHeader("User-Agent", userAgent().toUtf8());

  m_ProgressDialog = new QProgressDialog(nullptr);
  m_ProgressDialog->setLabelText(tr("Logging into Nexus"));
  QList<QPushButton*> buttons = m_ProgressDialog->findChildren<QPushButton*>();
  buttons.at(0)->setEnabled(false);
  m_ProgressDialog->show();
  QCoreApplication::processEvents(); // for some reason the whole app hangs during the login. This way the user has at least a little feedback

  m_LoginReply = post(request, postDataQuery);
  m_LoginTimeout.start();
  m_LoginState = LOGIN_CHECKING;
  connect(m_LoginReply, SIGNAL(finished()), this, SLOT(loginFinished()));
  connect(m_LoginReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(loginError(QNetworkReply::NetworkError)));
}


void NXMAccessManager::loginTimeout()
{
  m_LoginReply->deleteLater();
  m_LoginReply = nullptr;
  m_LoginAttempted = false; // this usually means we might have success later
  m_Username.clear();
  m_Password.clear();
  m_LoginState = LOGIN_NOT_VALID;

  emit loginFailed(tr("timeout"));
}


void NXMAccessManager::loginError(QNetworkReply::NetworkError)
{
  qDebug("login error");
  if (m_ProgressDialog != nullptr) {
    m_ProgressDialog->deleteLater();
    m_ProgressDialog = nullptr;
  }
  m_Username.clear();
  m_Password.clear();
  m_LoginState = LOGIN_NOT_VALID;

  if (m_LoginReply != nullptr) {
    emit loginFailed(m_LoginReply->errorString());
    m_LoginReply->deleteLater();
    m_LoginReply = nullptr;
  } else {
    emit loginFailed(tr("Unknown error"));
  }
}


bool NXMAccessManager::hasLoginCookies() const
{
  QUrl url(Nexus_Management_URL + "/");
  QList<QNetworkCookie> cookies = cookieJar()->cookiesForUrl(url);
  for (const QNetworkCookie &cookie : cookies) {
    if (cookie.name() == "sid") {
      return true;
    }
  }
  return false;
}


void NXMAccessManager::loginFinished()
{
  if (m_ProgressDialog != nullptr) {
    m_ProgressDialog->deleteLater();
    m_ProgressDialog = nullptr;
  }

  m_LoginReply->deleteLater();
  m_LoginReply = nullptr;
  m_Username.clear();
  m_Password.clear();

  if (hasLoginCookies()) {
    m_LoginState = LOGIN_VALID;
    retrieveCredentials();
    emit loginSuccessful(true);
  } else {
    m_LoginState = LOGIN_NOT_VALID;
    emit loginFailed(tr("Please check your password"));
  }
}


void NXMAccessManager::loginChecked()
{
  QNetworkReply *reply = static_cast<QNetworkReply*>(sender());
  QByteArray data = reply->readAll();
  m_LoginState = data == "null" ? LOGIN_NOT_VALID
                                : LOGIN_VALID;
  if (m_LoginState == LOGIN_VALID) {
    retrieveCredentials();
  } else {
    qDebug("cookies seem to be invalid");
  }
  m_LoginReply->deleteLater();
  m_LoginReply = nullptr;
}
