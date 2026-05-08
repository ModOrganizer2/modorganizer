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
#include "nexusoauthlogin.h"
#include "nxmurl.h"
#include "persistentcookiejar.h"
#include "report.h"
#include "selfupdater.h"
#include "settings.h"
#include "utility.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QPushButton>
#include <QThread>
#include <QUrlQuery>

using namespace MOBase;
using namespace std::chrono_literals;

const QString NexusUserUrl("https://users.nexusmods.com/oauth/");
const QString NexusV1BaseUrl("https://api.nexusmods.com/v1/");

ValidationProgressDialog::ValidationProgressDialog(Settings* s, NexusKeyValidator& v)
    : m_Settings(s), m_Validator(v), m_UpdateTimer(nullptr), m_First(true)
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
  if (!m_UpdateTimer) {
    m_UpdateTimer = new QTimer(this);
    connect(m_UpdateTimer, &QTimer::timeout, [&] {
      onTimer();
    });
    m_UpdateTimer->setInterval(100ms);
  }

  updateProgress();
  m_UpdateTimer->start();

  show();
}

void ValidationProgressDialog::stop()
{
  if (m_UpdateTimer) {
    m_UpdateTimer->stop();
  }

  hide();
}

void ValidationProgressDialog::showEvent(QShowEvent* e)
{
  if (m_First) {
    if (m_Settings) {
      m_Settings->geometry().centerOnMainWindowMonitor(this);
    }

    m_First = false;
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
  m_Validator.cancel();
}

void ValidationProgressDialog::onTimer()
{
  updateProgress();
}

void ValidationProgressDialog::updateProgress()
{
  const auto* current = m_Validator.currentAttempt();

  if (current) {
    ui->progress->setRange(0, current->timeout().count());
    ui->progress->setValue(current->elapsed().elapsed() / 1000);
  } else {
    // indeterminate
    ui->progress->setRange(0, 0);
  }

  if (const auto* a = m_Validator.lastAttempt()) {
    ui->label->setText(a->message() + ". " + tr("Trying again..."));
  } else if (current) {
    ui->label->setText(tr("Connecting to Nexus..."));
  } else {
    ui->label->setText("?");
  }
}

ValidationAttempt::ValidationAttempt(std::chrono::seconds timeout)
    : m_Reply(nullptr), m_Result(None)
{
  m_Timeout.setSingleShot(true);
  m_Timeout.setInterval(timeout);

  QObject::connect(&m_Timeout, &QTimer::timeout, [&] {
    onTimeout();
  });
}

void ValidationAttempt::start(NXMAccessManager& m, const NexusOAuthTokens& tokens)
{
  m_Tokens = tokens;

  if (!sendRequest(m, tokens)) {
    return;
  }

  m_Elapsed.start();
  m_Timeout.start();

  log::debug("nexus: attempt started with timeout of {} seconds", timeout().count());
}

bool ValidationAttempt::sendRequest(NXMAccessManager& m, const NexusOAuthTokens& tokens)
{

  if (tokens.accessToken.isEmpty() && tokens.apiKey.isEmpty()) {
    setFailure(HardError, QObject::tr("No access token or API key"));
    return false;
  }

  QNetworkRequest request;
  QString requestUrl;
  if (!tokens.accessToken.isEmpty()) {
    requestUrl = NexusUserUrl + "userinfo";
    m_Reply =
        NexusInterface::instance().getAccessManager()->makeOAuthGetRequest(requestUrl);
  } else {
    requestUrl = NexusV1BaseUrl + "users/validate";
    request.setUrl(requestUrl);
    request.setRawHeader("APIKEY", tokens.apiKey.toUtf8());
    m_Reply = m.get(request);
  }

  if (!m_Reply) {
    setFailure(SoftError, QObject::tr("Failed to request %1").arg(requestUrl));
    return false;
  }

  QObject::connect(m_Reply, &QNetworkReply::finished, [&] {
    onFinished();
  });

  QObject::connect(m_Reply, &QNetworkReply::sslErrors, [&](auto&& errors) {
    onSslErrors(errors);
  });

  return true;
}

void ValidationAttempt::cancel()
{
  if (!m_Reply || m_Result != None) {
    // not running
    return;
  }

  setFailure(Cancelled, QObject::tr("Cancelled"));

  if (m_Reply) {
    m_Reply->abort();
  }

  cleanup();
}

bool ValidationAttempt::done() const
{
  return (m_Result != None);
}

ValidationAttempt::Result ValidationAttempt::result() const
{
  return m_Result;
}

const QString& ValidationAttempt::message() const
{
  return m_Message;
}

std::chrono::seconds ValidationAttempt::timeout() const
{
  return std::chrono::duration_cast<std::chrono::seconds>(
      m_Timeout.intervalAsDuration());
}

QElapsedTimer ValidationAttempt::elapsed() const
{
  return m_Elapsed;
}

void ValidationAttempt::onFinished()
{
  if (m_Result == Cancelled) {
    return;
  }

  log::debug("nexus: request has finished");

  if (!m_Reply) {
    // shouldn't happen
    log::error("nexus: reply is null");
    setFailure(HardError, QObject::tr("Internal error"));
    return;
  }

  const auto code =
      m_Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  if (code == 0) {
    // request wasn't even sent
    log::error("nexus: code is 0");
    setFailure(SoftError, m_Reply->errorString());
    return;
  }

  const auto doc       = QJsonDocument::fromJson(m_Reply->readAll());
  const auto headers   = m_Reply->rawHeaderPairs();
  const auto httpError = m_Reply->errorString();

  const QJsonObject data = doc.object();

  if (code != 200) {
    // http request failed

    QString s = m_Reply->errorString();

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

  if (!m_Tokens.accessToken.isEmpty()) {
    if (!data.contains("sub")) {
      setFailure(HardError, QObject::tr("Bad response"));
      return;
    }

    const QString id       = data.value("sub").toString();
    const QString name     = data.value("name").toString();
    const auto roles       = data.value("membership_roles").toArray();
    QStringList validRoles = {"premium", "lifetimepremium"};
    bool premium           = false;
    for (auto role : roles) {
      QString roleVal = role.toString();
      if (validRoles.contains(roleVal)) {
        premium = true;
        break;
      }
    }

    if (m_Tokens.accessToken.isEmpty()) {
      setFailure(HardError, QObject::tr("Access token is empty"));
      return;
    }

    const auto user =
        APIUserAccount()
            .accessToken(m_Tokens.accessToken)
            .id(QString("%1").arg(id))
            .name(name)
            .type(premium ? APIUserAccountTypes::Premium : APIUserAccountTypes::Regular)
            .limits(NexusInterface::defaultAPILimits());

    setSuccess(user);
  } else if (!m_Tokens.apiKey.isEmpty()) {
    if (!data.contains("user_id")) {
      setFailure(HardError, QObject::tr("Bad response"));
      return;
    }

    const int id       = data.value("user_id").toInt();
    const QString key  = data.value("key").toString();
    const QString name = data.value("name").toString();
    const bool premium = data.value("is_premium").toBool();

    const auto user =
        APIUserAccount()
            .apiKey(m_Tokens.apiKey)
            .id(QString("%1").arg(id))
            .name(name)
            .type(premium ? APIUserAccountTypes::Premium : APIUserAccountTypes::Regular)
            .limits(NexusInterface::parseLimits(headers));

    setSuccess(user);
  }
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

  m_Result  = r;
  m_Message = error;

  if (failure) {
    failure();
  }
}

void ValidationAttempt::setSuccess(const APIUserAccount& user)
{
  log::debug("nexus connection successful");
  cleanup();

  m_Result  = Success;
  m_Message = "";

  if (success) {
    success(user);
  }
}

void ValidationAttempt::cleanup()
{
  m_Timeout.stop();

  if (m_Reply) {
    m_Reply->disconnect();
    m_Reply->deleteLater();
    m_Reply = nullptr;
  }
}

NexusKeyValidator::NexusKeyValidator(Settings* s, NXMAccessManager& am)
    : m_Settings(s), m_Manager(am)
{}

NexusKeyValidator::~NexusKeyValidator()
{
  cancel();
}

std::vector<std::chrono::seconds> NexusKeyValidator::getTimeouts() const
{
  if (m_Settings) {
    return m_Settings->nexus().validationTimeouts();
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

  m_Tokens = tokens;

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
  m_Attempts.clear();

  for (auto&& t : timeouts) {
    m_Attempts.push_back(std::make_unique<ValidationAttempt>(t));
  }
}

void NexusKeyValidator::cancel()
{
  log::debug("nexus: connection cancelled");

  for (auto&& a : m_Attempts) {
    a->cancel();
  }
}

bool NexusKeyValidator::isActive() const
{
  for (auto&& a : m_Attempts) {
    if (!a->done()) {
      return true;
    }
  }

  return false;
}

const ValidationAttempt* NexusKeyValidator::lastAttempt() const
{
  const ValidationAttempt* last = nullptr;

  for (auto&& a : m_Attempts) {
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
  for (auto&& a : m_Attempts) {
    if (!a->done()) {
      return a.get();
    }
  }

  return nullptr;
}

bool NexusKeyValidator::nextTry()
{
  if (!m_Tokens) {
    log::error("nexus: validator invoked without tokens");
    return false;
  }

  for (auto&& a : m_Attempts) {
    if (!a->done()) {
      a->success = [&](auto&& user) {
        onAttemptSuccess(*a, user);
      };
      a->failure = [&] {
        onAttemptFailure(*a);
      };

      a->start(m_Manager, *m_Tokens);
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
  m_Attempts.clear();
  if (finished) {
    finished(r, message, user);
  }
}

NXMAccessManager::NXMAccessManager(QObject* parent, Settings* s,
                                   const QString& moVersion)
    : QNetworkAccessManager(parent), m_Settings(s), m_MOVersion(moVersion),
      m_Validator(s, *this), m_ValidationState(NotChecked)
{
  NexusOAuthTokens tokens;
  GlobalSettings::nexusOAuthTokens(tokens);
  GlobalSettings::nexusApiKey(tokens.apiKey);
  m_Tokens = tokens;
  m_NexusOAuth.reset(new QOAuth2AuthorizationCodeFlow);
  m_NexusOAuthReplyHandler.reset(new QOAuthHttpServerReplyHandler(
      QHostAddress::LocalHost, NexusOAuth::redirectPort(), this));
  m_NexusOAuth->setReplyHandler(m_NexusOAuthReplyHandler.get());

  connect(m_NexusOAuth.get(), &QOAuth2AuthorizationCodeFlow::requestFailed, this,
          [&](QAbstractOAuth::Error error) {
            handleOAuthError(QObject::tr("Authorization failed (%1)").arg(int(error)));
          });

  connect(m_NexusOAuth.get(), &QOAuth2AuthorizationCodeFlow::granted, this, [&]() {
    notifyTokens();
  });

  connect(m_NexusOAuth.get(), &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, this,
          [&](const QUrl& url) {
            shell::Open(url);
            setOAuthState(OAuthState::WaitingForBrowser);
          });

  connect(m_NexusOAuth.get(), &QOAuth2AuthorizationCodeFlow::accessTokenAboutToExpire,
          this, [&] {
            if (!m_NexusOAuthReplyHandler->isListening() &&
                !m_NexusOAuthReplyHandler->listen(QHostAddress::LocalHost,
                                                  NexusOAuth::redirectPort())) {
              handleOAuthError(QObject::tr("Failed to bind to localhost on port %1.")
                                   .arg(NexusOAuth::redirectPort()));
              return;
            }
          });

  connect(m_NexusOAuth.get(), &QOAuth2AuthorizationCodeFlow::statusChanged, this,
          [&](QAbstractOAuth::Status status) {
            switch (status) {
            case QAbstractOAuth::Status::RefreshingToken:
              setOAuthState(OAuthState::Refreshing);
              break;
            case QAbstractOAuth::Status::TemporaryCredentialsReceived:
              setOAuthState(OAuthState::Authorizing);
              break;
            case QAbstractOAuth::Status::Granted:
              setOAuthState(OAuthState::Finished);
              break;
            default:
              break;
            }
          });

  connect(this, &NXMAccessManager::tokensReceived, this,
          &NXMAccessManager::saveRefreshedTokens);

  m_Validator.finished = [&](auto&& r, auto&& m, auto&& u) {
    onValidatorFinished(r, m, u);
  };

  m_Validator.attemptFinished = [&](auto&& a) {
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
    m_Validator.cancel();
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
  QUrl url(NexusV1BaseUrl + "/");
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
  m_Tokens = tokens;
}

std::optional<NexusOAuthTokens> NXMAccessManager::tokens() const
{
  return m_Tokens;
}

void NXMAccessManager::handleOAuthError(const QString& message)
{
  m_NexusOAuthReplyHandler->close();
  emit updateOAuthState(OAuthState::Error, message);
  emit authorizationEnded();
}

void NXMAccessManager::notifyTokens()
{
  if (!m_NexusOAuth) {
    handleOAuthError(QObject::tr("Internal error: OAuth flow is missing."));
    return;
  }

  QVariantMap payload;

  auto scopeTokens = m_NexusOAuth->grantedScopeTokens();
  QStringList scopes;
  for (auto token : scopeTokens) {
    scopes.append(QString::fromUtf8(token.constData()));
  }
  payload["access_token"]  = m_NexusOAuth->token();
  payload["refresh_token"] = m_NexusOAuth->refreshToken();
  payload["scope"]         = scopes.join(" ");
  payload["expiration_at"] = m_NexusOAuth->expirationAt();

  const auto extras = m_NexusOAuth->extraTokens();
  payload.insert(extras);

  auto tokens = makeTokensFromResponse(payload);
  if (!tokens.isValid()) {
    handleOAuthError(QObject::tr("Invalid OAuth token payload."));
    return;
  }

  tokens.scope = scopes.join(" ");

  emit tokensReceived(tokens);

  startValidationCheck(tokens);
  emit authorizationEnded();
}

void NXMAccessManager::saveRefreshedTokens(const NexusOAuthTokens tokens)
{
  NexusOAuthTokens finalTokens;
  if (GlobalSettings::hasNexusOAuthTokens() || GlobalSettings::hasNexusApiKey()) {
    NexusOAuthTokens oldTokens;
    GlobalSettings::nexusOAuthTokens(oldTokens);
    GlobalSettings::nexusApiKey(oldTokens.apiKey);
    NexusOAuthTokens newTokens(tokens);
    if (tokens.apiKey.isEmpty()) {
      newTokens.apiKey = oldTokens.apiKey;
    }
    finalTokens = newTokens;
  } else {
    finalTokens = tokens;
  }
  const bool ret  = GlobalSettings::setNexusOAuthTokens(finalTokens);
  const bool ret2 = GlobalSettings::setNexusApiKey(finalTokens.apiKey);
  if (ret && ret2) {
    setTokens(finalTokens);
  }
}

void NXMAccessManager::setOAuthState(OAuthState state, const QString& message)
{
  emit updateOAuthState(state, message);
}

QString NXMAccessManager::stateToString(OAuthState state, const QString& details)
{
  switch (state) {
  case OAuthState::Initializing:
    return QObject::tr("Connecting to Nexus...");

  case OAuthState::WaitingForBrowser:
    return QObject::tr("Opened Nexus in browser.") + "\n" +
           QObject::tr("Switch to your browser and accept the request.");

  case OAuthState::Authorizing:
    return QObject::tr("Waiting for Nexus...");

  case OAuthState::Finished:
    return QObject::tr("Finished.");

  case OAuthState::Cancelled:
    return QObject::tr("Cancelled.");

  case OAuthState::Error:
  default:
    return details.isEmpty() ? QObject::tr("An unknown error has occurred.") : details;
  }
}

void NXMAccessManager::startValidationCheck(const NexusOAuthTokens& tokens)
{
  m_ValidationState = NotChecked;
  m_Validator.start(tokens, NexusKeyValidator::Retry);

  if (m_ProgressDialog) {
    // don't show the progress dialog on startup for the first attempt; the
    // dialog will be shown in onValidatorAttemptFinished() if it failed
    startProgress();
  }
}

void NXMAccessManager::onValidatorFinished(ValidationAttempt::Result r,
                                           const QString& message,
                                           std::optional<APIUserAccount> user)
{
  stopProgress();

  if (user) {
    m_ValidationState = Valid;
    emit credentialsReceived(*user);
    emit validateSuccessful(true);
  } else {
    if (r == ValidationAttempt::Cancelled) {
      m_ValidationState = NotChecked;
    } else {
      m_ValidationState = Invalid;
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
  if (m_ValidationState == Valid) {
    return true;
  }

  if (m_Validator.isActive()) {
    const_cast<NXMAccessManager*>(this)->startProgress();
  }

  return false;
}

void NXMAccessManager::refuseValidation()
{
  m_ValidationState = Invalid;
}

bool NXMAccessManager::validateAttempted() const
{
  return (m_ValidationState != NotChecked);
}

bool NXMAccessManager::validateWaiting() const
{
  return m_Validator.isActive();
}

void NXMAccessManager::connectOrRefresh(const NexusOAuthTokens tokens)
{
  const auto clientId = NexusOAuth::clientId();
  if (clientId.isEmpty()) {
    handleOAuthError(QObject::tr("No OAuth client id configured."));
    return;
  }
  m_NexusOAuth->setAuthorizationUrl(QUrl(NexusOAuth::authorizeUrl()));
  m_NexusOAuth->setTokenUrl(QUrl(NexusOAuth::tokenUrl()));
  m_NexusOAuth->setClientIdentifier(clientId);
  m_NexusOAuth->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);
  QSet<QByteArray> scope = {"openid", "profile", "email"};
  m_NexusOAuth->setRequestedScopeTokens(scope);
  m_NexusOAuthReplyHandler->close();
  m_NexusOAuthReplyHandler->setCallbackPath(QUrl(NexusOAuth::redirectUri()).path());
  QFile logo(":/MO/gui/app_icon");
  logo.open(QIODevice::ReadOnly);
  QByteArray imageData = logo.readAll();
  logo.close();
  QByteArray base64Data = imageData.toBase64();
  QString imageSrc =
      QString("data:image/png;base64,") + QString::fromLatin1(base64Data);
  m_NexusOAuthReplyHandler->setCallbackText(
      QString("<style>\n"
              "    body {\n"
              "        text-align: center;\n"
              "        background-color: #2b2b2b;\n"
              "        color: white;\n"
              "        font-family: sans-serif;\n"
              "        font-size: 18px;\n"
              "    }\n"
              "</style>\n"
              "<img src=\"%1\" alt=\"Mod Organizer\">\n")
          .arg(imageSrc) +
      QObject::tr("<p><strong>Authorization complete.<br>You may close this "
                  "window.</strong></p>\n"));
  if (!m_NexusOAuthReplyHandler->listen(QHostAddress::LocalHost,
                                        NexusOAuth::redirectPort())) {
    handleOAuthError(QObject::tr("Failed to bind to localhost on port %1.")
                         .arg(NexusOAuth::redirectPort()));
    return;
  }
  if (!tokens.accessToken.isEmpty()) {
    m_NexusOAuth->setToken(tokens.accessToken);
    m_NexusOAuth->setRefreshToken(tokens.refreshToken);
    scope.clear();
    for (const QString scopeItem : tokens.scope.split(" ")) {
      scope.insert(scopeItem.toUtf8());
    }
    m_NexusOAuth->setRequestedScopeTokens(scope);

    setOAuthState(OAuthState::Refreshing);
    m_NexusOAuth->refreshTokens();
  } else {
    setOAuthState(OAuthState::Initializing);
    m_NexusOAuth->grant();
  }
}

void NXMAccessManager::cancelAuth()
{
  if (m_NexusOAuthReplyHandler) {
    m_NexusOAuthReplyHandler->close();
  }

  m_NexusOAuth.reset();
  m_NexusOAuthReplyHandler.reset();
  setOAuthState(OAuthState::Cancelled);
}

void NXMAccessManager::addAPIHeaders(QNetworkRequest& request)
{
  request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
  request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                       QNetworkRequest::AlwaysNetwork);
  request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader,
                    userAgent().toUtf8());
  request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader,
                    "application/json");
  request.setRawHeader("Protocol-Version", "1.0.0");
  request.setRawHeader("Application-Name", "MO2");
  request.setRawHeader("Application-Version", MOVersion().toUtf8());
}

QNetworkReply* NXMAccessManager::makeOAuthGetRequest(const QUrl url)
{
  if (!m_NexusOAuth->token().isEmpty()) {
    QNetworkRequest request(url);
    m_NexusOAuth->prepareRequest(&request, "GET");
    addAPIHeaders(request);
    return m_NexusOAuth->networkAccessManager()->get(request);
  }
  return nullptr;
}

QNetworkReply* NXMAccessManager::makeOAuthPostRequest(const QUrl url,
                                                      const QByteArray payload = {})
{
  if (!m_NexusOAuth->token().isEmpty()) {
    QNetworkRequest request(url);
    m_NexusOAuth->prepareRequest(&request, "POST", payload);
    addAPIHeaders(request);
    return m_NexusOAuth->networkAccessManager()->post(request, payload);
  }
  return nullptr;
}

QNetworkReply* NXMAccessManager::makeOAuthDeleteRequest(QNetworkRequest request)
{
  if (!m_NexusOAuth->token().isEmpty()) {
    m_NexusOAuth->prepareRequest(&request, "DELETE");
    addAPIHeaders(request);
    return m_NexusOAuth->networkAccessManager()->deleteResource(request);
  }
  return nullptr;
}

QNetworkReply* NXMAccessManager::makeOAuthCustomRequest(QNetworkRequest request,
                                                        const QByteArray& verb,
                                                        const QByteArray& data)
{
  if (!m_NexusOAuth->token().isEmpty()) {
    m_NexusOAuth->prepareRequest(&request, verb, data);
    addAPIHeaders(request);
    return m_NexusOAuth->networkAccessManager()->sendCustomRequest(request, verb, data);
  }
  return nullptr;
}

void NXMAccessManager::apiCheck(const NexusOAuthTokens& tokens, bool force)
{
  if (m_Validator.isActive()) {
    return;
  }

  setTokens(tokens);

  if (m_Settings && m_Settings->network().offlineMode()) {
    m_ValidationState = NotChecked;
    return;
  }

  if (force) {
    m_ValidationState = NotChecked;
  }

  if (m_ValidationState == Valid) {
    emit validateSuccessful(false);
    return;
  }

  if (m_NexusOAuth->token().isEmpty() && !tokens.accessToken.isEmpty()) {
    connectOrRefresh(tokens);
  } else if (!tokens.apiKey.isEmpty()) {
    startValidationCheck(tokens);
  }
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

void NXMAccessManager::clearCredentials()
{
  m_Validator.cancel();
  // TODO: Verify revocation process
  // if (m_Tokens && !m_Tokens->accessToken.isEmpty()) {
  //  QNetworkRequest request(NexusOAuth::tokenUrl());
  //  QUrlQuery params;
  //  params.addQueryItem("token", m_Tokens->refreshToken);
  //  params.addQueryItem("token_type_hint", "refresh_token");
  //  m_NexusOAuth->prepareRequest(&request, "POST",
  //                               params.toString(QUrl::FullyEncoded).toUtf8());
  //  m_NexusOAuth->networkAccessManager()->post(
  //      request, params.toString(QUrl::FullyEncoded).toUtf8());
  //}
  m_Tokens.reset();
  m_NexusOAuth->setToken("");
  m_NexusOAuthReplyHandler->close();
  emit credentialsReceived(APIUserAccount());
}

void NXMAccessManager::startProgress()
{
  if (!m_ProgressDialog) {
    m_ProgressDialog.reset(new ValidationProgressDialog(m_Settings, m_Validator));
  }

  m_ProgressDialog->start();
}

void NXMAccessManager::stopProgress()
{
  if (m_ProgressDialog) {
    m_ProgressDialog->stop();
  }
}
