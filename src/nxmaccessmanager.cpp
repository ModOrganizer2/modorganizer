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
#include "nexusoauthconfig.h"
#include "nxmurl.h"
#include "persistentcookiejar.h"
#include "report.h"
#include "selfupdater.h"
#include "settings.h"
#include "utility.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QPushButton>
#include <QEventLoop>
#include <QThread>
#include <QUrlQuery>

using namespace MOBase;
using namespace std::chrono_literals;

const QString NexusBaseUrl("https://api.nexusmods.com/v1");

ValidationProgressDialog::ValidationProgressDialog(Settings* s, NexusKeyValidator& v)
    : m_settings(s), m_validator(v), m_updateTimer(nullptr), m_first(true)
{
  ui.reset(new Ui::ValidationProgressDialog);
  ui->setupUi(this);

  connect(ui->hide, &QPushButton::clicked, [&] {
    onHide();
  });
  connect(ui->cancel, &QPushButton::clicked, [&] {
    onCancel();
  });
}

void ValidationProgressDialog::setParentWidget(QWidget* w)
{
  const auto wasVisible = isVisible();

  hide();
  setParent(w, windowFlags() | Qt::Dialog);
  setModal(false);

  if (w && wasVisible) {
    setVisible(true);
    raise();
  }
}

void ValidationProgressDialog::start()
{
  if (!m_updateTimer) {
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, [&] {
      onTimer();
    });
    m_updateTimer->setInterval(100ms);
  }

  updateProgress();
  m_updateTimer->start();

  show();
}

void ValidationProgressDialog::stop()
{
  if (m_updateTimer) {
    m_updateTimer->stop();
  }

  hide();
}

void ValidationProgressDialog::showEvent(QShowEvent* e)
{
  if (m_first) {
    if (m_settings) {
      m_settings->geometry().centerOnMainWindowMonitor(this);
    }

    m_first = false;
  }

  QDialog::showEvent(e);
}

void ValidationProgressDialog::closeEvent(QCloseEvent* e)
{
  hide();
  e->ignore();
}

void ValidationProgressDialog::onHide()
{
  hide();
}

void ValidationProgressDialog::onCancel()
{
  m_validator.cancel();
}

void ValidationProgressDialog::onTimer()
{
  updateProgress();
}

void ValidationProgressDialog::updateProgress()
{
  const auto* current = m_validator.currentAttempt();

  if (current) {
    ui->progress->setRange(0, current->timeout().count());
    ui->progress->setValue(current->elapsed().elapsed() / 1000);
  } else {
    // indeterminate
    ui->progress->setRange(0, 0);
  }

  if (const auto* a = m_validator.lastAttempt()) {
    ui->label->setText(a->message() + ". " + tr("Trying again..."));
  } else if (current) {
    ui->label->setText(tr("Connecting to Nexus..."));
  } else {
    ui->label->setText("?");
  }
}

ValidationAttempt::ValidationAttempt(std::chrono::seconds timeout)
    : m_reply(nullptr), m_result(None)
{
  m_timeout.setSingleShot(true);
  m_timeout.setInterval(timeout);

  QObject::connect(&m_timeout, &QTimer::timeout, [&] {
    onTimeout();
  });
}

void ValidationAttempt::start(NXMAccessManager& m, const NexusOAuthTokens& tokens)
{
  m_tokens = tokens;

  if (!sendRequest(m, tokens)) {
    return;
  }

  m_elapsed.start();
  m_timeout.start();

  log::debug("nexus: attempt started with timeout of {} seconds", timeout().count());
}

bool ValidationAttempt::sendRequest(NXMAccessManager& m, const NexusOAuthTokens& tokens)
{
  const QString requestUrl(NexusBaseUrl + "/users/validate");
  QNetworkRequest request(requestUrl);

  if (tokens.accessToken.isEmpty()) {
    setFailure(HardError, QObject::tr("Access token is empty"));
    return false;
  }

  const auto bearer = QStringLiteral("Bearer %1").arg(tokens.accessToken);
  request.setRawHeader("Authorization", bearer.toUtf8());
  request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader,
                    m.userAgent().toUtf8());
  request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader,
                    "application/json");
  request.setRawHeader("Protocol-Version", "1.0.0");
  request.setRawHeader("Application-Name", "MO2");
  request.setRawHeader("Application-Version", m.MOVersion().toUtf8());

  m_reply = m.get(request);

  if (!m_reply) {
    setFailure(SoftError, QObject::tr("Failed to request %1").arg(requestUrl));
    return false;
  }

  QObject::connect(m_reply, &QNetworkReply::finished, [&] {
    onFinished();
  });

  QObject::connect(m_reply, &QNetworkReply::sslErrors, [&](auto&& errors) {
    onSslErrors(errors);
  });

  return true;
}

void ValidationAttempt::cancel()
{
  if (!m_reply || m_result != None) {
    // not running
    return;
  }

  setFailure(Cancelled, QObject::tr("Cancelled"));

  if (m_reply) {
    m_reply->abort();
  }

  cleanup();
}

bool ValidationAttempt::done() const
{
  return (m_result != None);
}

ValidationAttempt::Result ValidationAttempt::result() const
{
  return m_result;
}

const QString& ValidationAttempt::message() const
{
  return m_message;
}

std::chrono::seconds ValidationAttempt::timeout() const
{
  return std::chrono::duration_cast<std::chrono::seconds>(
      m_timeout.intervalAsDuration());
}

QElapsedTimer ValidationAttempt::elapsed() const
{
  return m_elapsed;
}

void ValidationAttempt::onFinished()
{
  if (m_result == Cancelled) {
    return;
  }

  log::debug("nexus: request has finished");

  if (!m_reply) {
    // shouldn't happen
    log::error("nexus: reply is null");
    setFailure(HardError, QObject::tr("Internal error"));
    return;
  }

  const auto code =
      m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  if (code == 0) {
    // request wasn't even sent
    log::error("nexus: code is 0");
    setFailure(SoftError, m_reply->errorString());
    return;
  }

  const auto doc       = QJsonDocument::fromJson(m_reply->readAll());
  const auto headers   = m_reply->rawHeaderPairs();
  const auto httpError = m_reply->errorString();

  const QJsonObject data = doc.object();

  if (code != 200) {
    // http request failed

    QString s = m_reply->errorString();

    const auto nexusMessage = data.value("message").toString();
    if (!nexusMessage.isEmpty()) {
      if (!s.isEmpty()) {
        s += ", ";
      }

      s += nexusMessage;
    }

    if (s.isEmpty()) {
      s = QObject::tr("HTTP code %1").arg(code);
    } else {
      s += QString(" (%1)").arg(code);
    }

    setFailure(SoftError, s);
    return;
  }

  if (doc.isNull()) {
    setFailure(HardError, QObject::tr("Invalid JSON"));
    return;
  }

  if (!data.contains("user_id")) {
    setFailure(HardError, QObject::tr("Bad response"));
    return;
  }

  const int id       = data.value("user_id").toInt();
  const QString name = data.value("name").toString();
  const bool premium = data.value("is_premium").toBool();

  if (m_tokens.accessToken.isEmpty()) {
    setFailure(HardError, QObject::tr("Access token is empty"));
    return;
  }

  const auto user =
      APIUserAccount()
          .accessToken(m_tokens.accessToken)
          .id(QString("%1").arg(id))
          .name(name)
          .type(premium ? APIUserAccountTypes::Premium : APIUserAccountTypes::Regular)
          .limits(NexusInterface::parseLimits(headers));

  setSuccess(user);
}

void ValidationAttempt::onSslErrors(const QList<QSslError>& errors)
{
  log::error("nexus: ssl errors");

  for (auto& e : errors) {
    log::error("  . {}", e.errorString());
  }

  setFailure(HardError, QObject::tr("SSL error"));
}

void ValidationAttempt::onTimeout()
{
  setFailure(SoftError, QObject::tr("Timed out"));
}

void ValidationAttempt::setFailure(Result r, const QString& error)
{
  if (r != Cancelled) {
    // don't spam the log
    log::error("nexus: {}", error);
  }

  cleanup();

  m_result  = r;
  m_message = error;

  if (failure) {
    failure();
  }
}

void ValidationAttempt::setSuccess(const APIUserAccount& user)
{
  log::debug("nexus connection successful");
  cleanup();

  m_result  = Success;
  m_message = "";

  if (success) {
    success(user);
  }
}

void ValidationAttempt::cleanup()
{
  m_timeout.stop();

  if (m_reply) {
    m_reply->disconnect();
    m_reply->deleteLater();
    m_reply = nullptr;
  }
}

NexusKeyValidator::NexusKeyValidator(Settings* s, NXMAccessManager& am)
    : m_settings(s), m_manager(am)
{}

NexusKeyValidator::~NexusKeyValidator()
{
  cancel();
}

std::vector<std::chrono::seconds> NexusKeyValidator::getTimeouts() const
{
  if (m_settings) {
    return m_settings->nexus().validationTimeouts();
  } else {
    return {10s, 15s, 20s};
  }
}

void NexusKeyValidator::start(const NexusOAuthTokens& tokens, Behaviour b)
{
  if (isActive()) {
    log::debug("nexus: trying to start while ongoing; ignoring");
    return;
  }

  m_tokens = tokens;

  const auto timeouts = getTimeouts();

  switch (b) {
  case OneShot: {
    createAttempts({timeouts[0]});
    break;
  }

  case Retry: {
    createAttempts(timeouts);
    break;
  }
  }

  nextTry();
}

void NexusKeyValidator::createAttempts(
    const std::vector<std::chrono::seconds>& timeouts)
{
  m_attempts.clear();

  for (auto&& t : timeouts) {
    m_attempts.push_back(std::make_unique<ValidationAttempt>(t));
  }
}

void NexusKeyValidator::cancel()
{
  log::debug("nexus: connection cancelled");

  for (auto&& a : m_attempts) {
    a->cancel();
  }
}

bool NexusKeyValidator::isActive() const
{
  for (auto&& a : m_attempts) {
    if (!a->done()) {
      return true;
    }
  }

  return false;
}

const ValidationAttempt* NexusKeyValidator::lastAttempt() const
{
  const ValidationAttempt* last = nullptr;

  for (auto&& a : m_attempts) {
    if (a->done()) {
      last = a.get();
    } else {
      break;
    }
  }

  return last;
}

const ValidationAttempt* NexusKeyValidator::currentAttempt() const
{
  for (auto&& a : m_attempts) {
    if (!a->done()) {
      return a.get();
    }
  }

  return nullptr;
}

bool NexusKeyValidator::nextTry()
{
  if (!m_tokens) {
    log::error("nexus: validator invoked without tokens");
    return false;
  }

  for (auto&& a : m_attempts) {
    if (!a->done()) {
      a->success = [&](auto&& user) {
        onAttemptSuccess(*a, user);
      };
      a->failure = [&] {
        onAttemptFailure(*a);
      };

      a->start(m_manager, *m_tokens);
      return true;
    }
  }

  // no more
  return false;
}

void NexusKeyValidator::onAttemptSuccess(const ValidationAttempt& a,
                                         const APIUserAccount& u)
{
  if (attemptFinished) {
    attemptFinished(a);
  }

  setFinished(ValidationAttempt::Success, "", u);
}

void NexusKeyValidator::onAttemptFailure(const ValidationAttempt& a)
{
  if (attemptFinished) {
    attemptFinished(a);
  }

  switch (a.result()) {
  case ValidationAttempt::SoftError: {
    if (!nextTry()) {
      setFinished(a.result(), a.message(), {});
    }

    break;
  }

  case ValidationAttempt::HardError: {
    cancel();
    setFinished(a.result(), a.message(), {});
    break;
  }

  case ValidationAttempt::Cancelled: {
    setFinished(ValidationAttempt::Cancelled, QObject::tr("Cancelled"), {});
    break;
  }
  }
}

void NexusKeyValidator::setFinished(ValidationAttempt::Result r, const QString& message,
                                    std::optional<APIUserAccount> user)
{
  if (finished) {
    finished(r, message, user);
  }
}

NXMAccessManager::NXMAccessManager(QObject* parent, Settings* s,
                                   const QString& moVersion)
    : QNetworkAccessManager(parent), m_Settings(s), m_MOVersion(moVersion),
      m_validator(s, *this), m_validationState(NotChecked)
{
  m_validator.finished = [&](auto&& r, auto&& m, auto&& u) {
    onValidatorFinished(r, m, u);
  };

  m_validator.attemptFinished = [&](auto&& a) {
    onValidatorAttemptFinished(a);
  };

  if (m_Settings) {
    setCookieJar(new PersistentCookieJar(QDir::fromNativeSeparators(
        m_Settings->paths().cache() + "/nexus_cookies.dat")));
  }
}

void NXMAccessManager::setTopLevelWidget(QWidget* w)
{
  if (w) {
    if (m_ProgressDialog) {
      m_ProgressDialog->setParentWidget(w);
    }
  } else {
    m_ProgressDialog.reset();
    m_validator.cancel();
  }
}

QNetworkReply*
NXMAccessManager::createRequest(QNetworkAccessManager::Operation operation,
                                const QNetworkRequest& request, QIODevice* device)
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
    return QNetworkAccessManager::createRequest(operation, request, device);
    ;
  } else {
    return QNetworkAccessManager::createRequest(operation, request, device);
  }
}

void NXMAccessManager::showCookies() const
{
  QUrl url(NexusBaseUrl + "/");
  for (const QNetworkCookie& cookie : cookieJar()->cookiesForUrl(url)) {
    log::debug("{} - {} (expires: {})", cookie.name().constData(),
               cookie.value().constData(), cookie.expirationDate().toString());
  }
}

void NXMAccessManager::clearCookies()
{
  PersistentCookieJar* jar = qobject_cast<PersistentCookieJar*>(cookieJar());
  if (jar != nullptr) {
    jar->clear();
  } else {
    log::warn("failed to clear cookies, invalid cookie jar");
  }
}

void NXMAccessManager::setTokens(const NexusOAuthTokens& tokens)
{
  m_tokens = tokens;
}

std::optional<NexusOAuthTokens> NXMAccessManager::tokens() const
{
  return m_tokens;
}

bool NXMAccessManager::ensureFreshToken()
{
  if (!m_tokens) {
    log::warn("nexus: no OAuth tokens available");
    return false;
  }

  if (!m_tokens->isExpired()) {
    return true;
  }

  const auto refreshed = refreshTokensBlocking(*m_tokens);
  if (!refreshed) {
    return false;
  }

  setTokens(*refreshed);
  GlobalSettings::setNexusOAuthTokens(*refreshed);
  return true;
}

void NXMAccessManager::startValidationCheck(const NexusOAuthTokens& tokens)
{
  m_validationState = NotChecked;
  m_validator.start(tokens, NexusKeyValidator::Retry);

  if (m_ProgressDialog) {
    // don't show the progress dialog on startup for the first attempt; the
    // dialog will be shown in onValidatorAttemptFinished() if it failed
    startProgress();
  }
}

std::optional<NexusOAuthTokens>
NXMAccessManager::refreshTokensBlocking(const NexusOAuthTokens& current)
{
  if (current.refreshToken.isEmpty()) {
    log::error("nexus: refresh token missing, user interaction required");
    return std::nullopt;
  }

  QNetworkRequest request{QUrl(NexusOAuth::tokenUrl())};
  request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader,
                    "application/x-www-form-urlencoded");
  request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader,
                    userAgent().toUtf8());
  request.setRawHeader("Protocol-Version", "1.0.0");
  request.setRawHeader("Application-Name", "MO2");
  request.setRawHeader("Application-Version", MOVersion().toUtf8());

  QUrlQuery formData;
  formData.addQueryItem(QStringLiteral("grant_type"),
                        QStringLiteral("refresh_token"));
  formData.addQueryItem(QStringLiteral("refresh_token"), current.refreshToken);
  formData.addQueryItem(QStringLiteral("client_id"), NexusOAuth::clientId());

  auto reply = post(request, formData.toString(QUrl::FullyEncoded).toUtf8());
  if (!reply) {
    log::error("nexus: failed to issue refresh token request");
    return std::nullopt;
  }

  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  if (reply->error() != QNetworkReply::NoError) {
    log::error("nexus: refresh token request failed - {}", reply->errorString());
    reply->deleteLater();
    return std::nullopt;
  }

  const auto payload = QJsonDocument::fromJson(reply->readAll());
  reply->deleteLater();
  if (!payload.isObject()) {
    log::error("nexus: invalid refresh token payload");
    return std::nullopt;
  }

  auto tokens = makeTokensFromResponse(payload.object());
  if (tokens.refreshToken.isEmpty()) {
    tokens.refreshToken = current.refreshToken;
  }
  if (tokens.scope.isEmpty()) {
    tokens.scope = current.scope;
  }
  if (tokens.tokenType.isEmpty()) {
    tokens.tokenType = current.tokenType;
  }

  if (!tokens.isValid()) {
    return std::nullopt;
  }

  return tokens;
}

void NXMAccessManager::onValidatorFinished(ValidationAttempt::Result r,
                                           const QString& message,
                                           std::optional<APIUserAccount> user)
{
  stopProgress();

  if (user) {
    m_validationState = Valid;
    emit credentialsReceived(*user);
    emit validateSuccessful(true);
  } else {
    if (r == ValidationAttempt::Cancelled) {
      m_validationState = NotChecked;
    } else {
      m_validationState = Invalid;
      emit validateFailed(message);
    }
  }
}

void NXMAccessManager::onValidatorAttemptFinished(const ValidationAttempt& a)
{
  if (!m_ProgressDialog) {
    switch (a.result()) {
    case ValidationAttempt::SoftError:
    case ValidationAttempt::HardError: {
      startProgress();
      break;
    }

    case ValidationAttempt::None:
    case ValidationAttempt::Success:
    case ValidationAttempt::Cancelled:
    default: {
      // don't show the dialog
      break;
    }
    }
  }
}

bool NXMAccessManager::validated() const
{
  if (m_validationState == Valid) {
    return true;
  }

  if (m_validator.isActive()) {
    const_cast<NXMAccessManager*>(this)->startProgress();
  }

  return false;
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

void NXMAccessManager::apiCheck(const NexusOAuthTokens& tokens, bool force)
{
  if (m_validator.isActive()) {
    return;
  }

  setTokens(tokens);

  if (m_Settings && m_Settings->network().offlineMode()) {
    m_validationState = NotChecked;
    return;
  }

  if (force) {
    m_validationState = NotChecked;
  }

  if (m_validationState == Valid) {
    emit validateSuccessful(false);
    return;
  }

  startValidationCheck(tokens);
}

const QString& NXMAccessManager::MOVersion() const
{
  return m_MOVersion;
}

QString NXMAccessManager::userAgent(const QString& subModule) const
{
  QStringList comments;
  QString os;
  if (QSysInfo::productType() == "windows")
    comments << ((QSysInfo::kernelType() == "winnt") ? "Windows_NT " : "Windows ") +
                    QSysInfo::kernelVersion();
  else
    comments << QSysInfo::kernelType().left(1).toUpper() + QSysInfo::kernelType().mid(1)
             << QSysInfo::productType().left(1).toUpper() +
                    QSysInfo::kernelType().mid(1) + " " + QSysInfo::productVersion();
  if (!subModule.isEmpty()) {
    comments << "module: " + subModule;
  }
  comments << ((QSysInfo::buildCpuArchitecture() == "x86_64") ? "x64" : "x86");

  return QString("Mod Organizer/%1 (%2) Qt/%3")
      .arg(m_MOVersion, comments.join("; "), qVersion());
}

void NXMAccessManager::clearTokens()
{
  m_validator.cancel();
  m_tokens.reset();
  emit credentialsReceived(APIUserAccount());
}

void NXMAccessManager::startProgress()
{
  if (!m_ProgressDialog) {
    m_ProgressDialog.reset(new ValidationProgressDialog(m_Settings, m_validator));
  }

  m_ProgressDialog->start();
}

void NXMAccessManager::stopProgress()
{
  if (m_ProgressDialog) {
    m_ProgressDialog->stop();
  }
}
