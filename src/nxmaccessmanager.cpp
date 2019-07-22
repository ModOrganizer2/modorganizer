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
using namespace std::chrono_literals;

const QString NexusBaseUrl("https://api.nexusmods.com/v1");
const std::chrono::seconds NXMAccessManager::ValidationTimeout = 10s;
const QString NexusSSO("wss://sso.nexusmods.com");
const QString NexusSSOPage("https://www.nexusmods.com/sso?id=%1&application=modorganizer2");


ValidationProgressDialog::ValidationProgressDialog(std::chrono::seconds t)
  : m_timeout(t), m_bar(nullptr), m_buttons(nullptr), m_timer(nullptr)
{
  m_bar = new QProgressBar;
  m_bar->setTextVisible(false);

  auto* label = new QLabel(tr("Validating Nexus Connection"));
  label->setAlignment(Qt::AlignHCenter);

  auto* vbox = new QVBoxLayout(this);
  vbox->addWidget(label);
  vbox->addWidget(m_bar);

  m_buttons = new QDialogButtonBox;
  m_buttons->addButton(tr("Hide"), QDialogButtonBox::RejectRole);
  connect(m_buttons, &QDialogButtonBox::clicked, [&](auto* b){ onButton(b); });
  vbox->addWidget(m_buttons);
}

void ValidationProgressDialog::setParentWidget(QWidget* w)
{
  const auto wasVisible = isVisible();

  hide();
  setParent(w, windowFlags() | Qt::Dialog);
  setModal(false);
  setVisible(wasVisible);
}

void ValidationProgressDialog::start()
{
  if (!m_timer) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, [&]{ onTimer(); });
    m_timer->setInterval(100ms);
  }

  m_bar->setRange(0, m_timeout.count());
  m_bar->setValue(0);

  m_elapsed.start();
  m_timer->start();

  show();
}

void ValidationProgressDialog::stop()
{
  if (m_timer) {
    m_timer->stop();
  }

  hide();
}

void ValidationProgressDialog::closeEvent(QCloseEvent* e)
{
  hide();
  e->ignore();
}

void ValidationProgressDialog::onButton(QAbstractButton* b)
{
  if (m_buttons->buttonRole(b) == QDialogButtonBox::RejectRole) {
    hide();
  } else {
    qCritical() << "validation dialog: unknown button pressed";
  }
}

void ValidationProgressDialog::onTimer()
{
  m_bar->setValue(m_elapsed.elapsed() / 1000);
}


NexusSSOLogin::NexusSSOLogin()
  : m_keyReceived(false), m_active(false)
{
  m_timeout.setInterval(NXMAccessManager::ValidationTimeout);
  m_timeout.setSingleShot(true);

  QObject::connect(
    &m_socket, &QWebSocket::connected,
    [&]{ onConnected(); });

  QObject::connect(
    &m_socket, qOverload<QAbstractSocket::SocketError>(&QWebSocket::error),
    [&](auto&& e){ onError(e); });

  QObject::connect(
    &m_socket, &QWebSocket::sslErrors,
    [&](auto&& errors){ onSslErrors(errors); });

  QObject::connect(
    &m_socket, &QWebSocket::textMessageReceived,
    [&](auto&& s){ onMessage(s); });

  QObject::connect(
    &m_socket, &QWebSocket::disconnected,
    [&]{ onDisconnected(); });

  QObject::connect(&m_timeout, &QTimer::timeout, [&]{ onTimeout(); });
}

QString NexusSSOLogin::stateToString(States s, const QString& e)
{
  switch (s)
  {
    case ConnectingToSSO:
      return QObject::tr("Connecting to Nexus...");

    case WaitingForToken:
      return QObject::tr("Waiting for Nexus...");

    case WaitingForBrowser:
      return QObject::tr(
        "Opened Nexus in browser.\n"
        "Switch to your browser and accept the request.");

    case Finished:
      return QObject::tr("Finished.");

    case Timeout:
      return QObject::tr(
        "No answer from Nexus.\n"
        "A firewall might be blocking Mod Organizer.");

    case ClosedByRemote:
      return QObject::tr("Nexus closed the connection.");

    case Cancelled:
      return QObject::tr("Cancelled.");

    case Error:  // fall-through
    default:
    {
      if (e.isEmpty()) {
        return QString("%1").arg(s);
      } else {
        return e;
      }
    }
  }
}

void NexusSSOLogin::start()
{
  m_active = true;
  setState(ConnectingToSSO);
  m_timeout.start();
  m_socket.open(NexusSSO);
}

void NexusSSOLogin::cancel()
{
  if (m_active) {
    abort();
    setState(Cancelled);
  }
}

void NexusSSOLogin::close()
{
  if (m_active) {
    m_active = false;
    m_timeout.stop();
    m_socket.close();
  }
}

void NexusSSOLogin::abort()
{
  m_active = false;
  m_timeout.stop();
  m_socket.abort();
}

bool NexusSSOLogin::isActive() const
{
  return m_active;
}

void NexusSSOLogin::setState(States s, const QString& error)
{
  if (stateChanged) {
    stateChanged(s, error);
  }
}

void NexusSSOLogin::onConnected()
{
  setState(WaitingForToken);

  m_keyReceived = false;

  boost::uuids::random_generator generator;
  boost::uuids::uuid sessionId = generator();
  m_guid = boost::uuids::to_string(sessionId).c_str();

  QJsonObject data;
  data.insert(QString("id"), QJsonValue(m_guid));
  data.insert(QString("protocol"), 2);

  const QString message = QJsonDocument(data).toJson();
  m_socket.sendTextMessage(message);
}

void NexusSSOLogin::onMessage(const QString& s)
{
  const QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8());
  const QVariantMap root = doc.object().toVariantMap();

  if (!root["success"].toBool()) {
    close();

    setState(Error, QString("There was a problem with SSO initialization: %1")
      .arg(root["error"].toString()));

    return;
  }

  const QVariantMap data = root["data"].toMap();

  if (data.contains("connection_token")) {
    // first answer

    // open browser
    const auto url = NexusSSOPage.arg(m_guid);
    shell::OpenLink(url);

    m_timeout.stop();
    setState(WaitingForBrowser);
  } else {
    // second answer
    const auto key = data["api_key"].toString();
    close();

    if (keyChanged) {
      keyChanged(key);
    }

    setState(Finished);
  }
}

void NexusSSOLogin::onDisconnected()
{
  if (m_active) {
    m_active = false;

    if (!m_keyReceived) {
      setState(ClosedByRemote);
    }
  }
}

void NexusSSOLogin::onError(QAbstractSocket::SocketError e)
{
  if (m_active) {
    setState(Error, m_socket.errorString());
    close();
  }
}

void NexusSSOLogin::onSslErrors(const QList<QSslError>& errors)
{
  if (m_active) {
    for (const auto& e : errors) {
      setState(Error, e.errorString());
    }
  }
}

void NexusSSOLogin::onTimeout()
{
  abort();
  setState(Timeout);
}


NexusKeyValidator::NexusKeyValidator(NXMAccessManager& am)
  : m_manager(am), m_reply(nullptr), m_active(false)
{
  m_timeout.setInterval(NXMAccessManager::ValidationTimeout);
  m_timeout.setSingleShot(true);

  QObject::connect(&m_timeout, &QTimer::timeout, [&]{ onTimeout(); });
}

NexusKeyValidator::~NexusKeyValidator()
{
  abort();
}

QString NexusKeyValidator::stateToString(States s, const QString& e)
{
  switch (s)
  {
    case NexusKeyValidator::Connecting:
      return QObject::tr("Connecting to Nexus...");

    case NexusKeyValidator::Finished:
      return QObject::tr("Finished.");

    case NexusKeyValidator::InvalidJson:
      return QObject::tr("Invalid JSON");

    case NexusKeyValidator::BadResponse:
      return QObject::tr("Bad response");

    case NexusKeyValidator::Timeout:
      return QObject::tr("There was a timeout during the request");

    case NexusKeyValidator::Cancelled:
      return QObject::tr("Cancelled");

    case NexusKeyValidator::Error:  // fall-through
    default:
    {
      if (e.isEmpty()) {
        return QString("%1").arg(s);
      } else {
        return e;
      }
    }
  }
}

void NexusKeyValidator::start(const QString& key)
{
  if (m_reply) {
    abort();
    return;
  }

  m_active = true;
  setState(Connecting);

  const QString requestUrl(NexusBaseUrl + "/users/validate");
  QNetworkRequest request(requestUrl);

  request.setRawHeader("APIKEY", key.toUtf8());
  request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, m_manager.userAgent().toUtf8());
  request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/json");
  request.setRawHeader("Protocol-Version", "1.0.0");
  request.setRawHeader("Application-Name", "MO2");
  request.setRawHeader("Application-Version", m_manager.MOVersion().toUtf8());

  m_reply = m_manager.get(request);
  if (!m_reply) {
    close();
    setState(Error, QObject::tr("Failed to request %1").arg(requestUrl));
    return;
  }

  m_timeout.start(NXMAccessManager::ValidationTimeout);

  QObject::connect(
    m_reply, &QNetworkReply::finished,
    [&]{ onFinished(); });

  QObject::connect(
    m_reply, &QNetworkReply::sslErrors,
    [&](auto&& errors){ onSslErrors(errors); });
}

void NexusKeyValidator::cancel()
{
  if (m_active) {
    abort();
    setState(Cancelled);
  }
}

bool NexusKeyValidator::isActive() const
{
  return m_active;
}

void NexusKeyValidator::close()
{
  m_active = false;
  m_timeout.stop();

  if (m_reply) {
    m_reply->disconnect();
    m_reply->deleteLater();
    m_reply = nullptr;
  }
}

void NexusKeyValidator::abort()
{
  m_active = false;
  m_timeout.stop();

  if (m_reply) {
    m_reply->disconnect();
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
  }
}

void NexusKeyValidator::setState(States s, const QString& error)
{
  if (stateChanged) {
    stateChanged(s, error);
  }
}

void NexusKeyValidator::onFinished()
{
  if (!m_reply) {
    // shouldn't happen
    return;
  }

  m_timeout.stop();

  const auto code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const auto doc = QJsonDocument::fromJson(m_reply->readAll());
  const auto headers = m_reply->rawHeaderPairs();
  const auto error = m_reply->errorString();

  close();

  const QJsonObject data = doc.object();

  if (code != 200) {
    handleError(code, data.value("message").toString(), error);
    return;
  }

  if (doc.isNull()) {
    setState(InvalidJson);
    return;
  }

  if (!data.contains("user_id")) {
    setState(BadResponse);
    return;
  }

  const int id = data.value("user_id").toInt();
  const QString key = data.value("key").toString();
  const QString name = data.value("name").toString();
  const bool premium = data.value("is_premium").toBool();

  const auto user = APIUserAccount()
    .apiKey(key)
    .id(QString("%1").arg(id))
    .name(name)
    .type(premium ? APIUserAccountTypes::Premium : APIUserAccountTypes::Regular)
    .limits(NexusInterface::parseLimits(headers));

  if (finished) {
    setState(Finished);
    finished(user);
  }
}

void NexusKeyValidator::onSslErrors(const QList<QSslError>& errors)
{
  if (m_active) {
    for (const auto& e : errors) {
      setState(Error, e.errorString());
    }
  }
}

void NexusKeyValidator::onTimeout()
{
  abort();
  setState(Timeout);
}

void NexusKeyValidator::handleError(
  int code, const QString& nexusMessage, const QString& httpError)
{
  QString s = httpError;

  if (!nexusMessage.isEmpty()) {
    if (!s.isEmpty()) {
      s += ", ";
    }

    s += nexusMessage;
  }

  if (code != 0) {
    if (s.isEmpty()) {
      s = QString("HTTP code %1").arg(code);
    } else {
      s += QString(" (%1)").arg(code);
    }
  }

  setState(Error, s);
}



NXMAccessManager::NXMAccessManager(QObject *parent, const QString &moVersion)
  : QNetworkAccessManager(parent)
  , m_ProgressDialog(new ValidationProgressDialog(ValidationTimeout))
  , m_MOVersion(moVersion)
  , m_validator(*this)
  , m_validationState(NotChecked)
{
  m_validator.stateChanged = [&](auto&& s, auto&& e){ onValidatorState(s, e); };
  m_validator.finished = [&](auto&& user){ onValidatorFinished(user); };

  setCookieJar(new PersistentCookieJar(QDir::fromNativeSeparators(
    Settings::instance().getCacheDirectory() + "/nexus_cookies.dat")));

  if (networkAccessible() == QNetworkAccessManager::UnknownAccessibility) {
    // why is this necessary all of a sudden?
    setNetworkAccessible(QNetworkAccessManager::Accessible);
  }
}

void NXMAccessManager::setTopLevelWidget(QWidget* w)
{
  m_ProgressDialog->setParentWidget(w);
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
  QUrl url(NexusBaseUrl + "/");
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

void NXMAccessManager::startValidationCheck(const QString& key)
{
  m_validationState = NotChecked;
  m_validator.start(key);
  m_ProgressDialog->start();
}

void NXMAccessManager::onValidatorState(
  NexusKeyValidator::States s, const QString& e)
{
  if (s == NexusKeyValidator::Connecting || s == NexusKeyValidator::Finished) {
    // no-op, success is handled in onValidatorFinished()
    return;
  }

  m_ProgressDialog->stop();
  m_validationState = Invalid;
  emit validateFailed(NexusKeyValidator::stateToString(s, e));
}

void NXMAccessManager::onValidatorFinished(const APIUserAccount& user)
{
  m_ProgressDialog->stop();

  m_validationState = Valid;
  emit credentialsReceived(user);
  emit validateSuccessful(true);
}

bool NXMAccessManager::validated() const
{
  if (m_validator.isActive()) {
    m_ProgressDialog->show();
  }

  return (m_validationState == Valid);
}

void NXMAccessManager::refuseValidation()
{
  m_validationState = Invalid;
}

bool NXMAccessManager::validateAttempted() const
{
  return (m_validationState != NotChecked);
}

bool NXMAccessManager::validateWaiting() const
{
  return m_validator.isActive();
}

void NXMAccessManager::apiCheck(const QString &apiKey, bool force)
{
  if (m_validator.isActive()) {
    return;
  }

  if (force) {
    m_validationState = NotChecked;
  }

  if (m_validationState == Valid) {
    emit validateSuccessful(false);
    return;
  }

  startValidationCheck(apiKey);
}

const QString& NXMAccessManager::MOVersion() const
{
  return m_MOVersion;
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

void NXMAccessManager::clearApiKey()
{
  m_validator.cancel();
  emit credentialsReceived(APIUserAccount());
}
