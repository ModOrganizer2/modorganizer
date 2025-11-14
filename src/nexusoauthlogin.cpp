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

#include "nexusoauthlogin.h"
#include "nexusoauthconfig.h"
#include "utility.h"
#include <QAbstractOAuth>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonValue>
#include <QMultiMap>
#include <QRandomGenerator>
#include <QUrl>
#include <QVariant>
#include <QtNetworkAuth/QOAuth2AuthorizationCodeFlow>
#include <QtNetworkAuth/QOAuthHttpServerReplyHandler>

using namespace MOBase;

namespace
{
QString callbackPath()
{
  return QUrl(NexusOAuth::redirectUri()).path();
}
}  // namespace

NexusOAuthLogin::NexusOAuthLogin(QObject* parent) : QObject(parent), m_active(false) {}

NexusOAuthLogin::~NexusOAuthLogin() = default;

QString NexusOAuthLogin::stateToString(State state, const QString& details)
{
  switch (state) {
  case State::Initializing:
    return QObject::tr("Connecting to Nexus...");

  case State::WaitingForBrowser:
    return QObject::tr("Opened Nexus in browser.") + "\n" +
           QObject::tr("Switch to your browser and accept the request.");

  case State::Authorizing:
    return QObject::tr("Waiting for Nexus...");

  case State::Finished:
    return QObject::tr("Finished.");

  case State::Cancelled:
    return QObject::tr("Cancelled.");

  case State::Error:
  default:
    return details.isEmpty() ? QObject::tr("An unknown error has occurred.") : details;
  }
}

void NexusOAuthLogin::start()
{
  if (m_active) {
    cancel();
  }

  const auto clientId = NexusOAuth::clientId();
  if (clientId.isEmpty()) {
    handleError(QObject::tr("No OAuth client id configured."));
    return;
  }

  m_flow.reset(new QOAuth2AuthorizationCodeFlow);
  m_flow->setAuthorizationUrl(QUrl(NexusOAuth::authorizeUrl()));
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  m_flow->setAccessTokenUrl(QUrl(NexusOAuth::tokenUrl()));
#else
  m_flow->setTokenUrl(QUrl(NexusOAuth::tokenUrl()));
#endif
  m_flow->setClientIdentifier(clientId);
  m_flow->setScope(QString());
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
  m_flow->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);
#endif
  m_flow->setModifyParametersFunction(
      [this](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant>* parameters) {
        injectPkceChallenge(stage, parameters);
      });

  m_replyHandler.reset(new QOAuthHttpServerReplyHandler(
      QHostAddress::LocalHost, NexusOAuth::redirectPort(), this));
  m_replyHandler->setCallbackPath(callbackPath());
  m_replyHandler->setCallbackText(QObject::tr(
      "<html><body><h2>Mod Organizer</h2><p>Authorization complete. You may close this "
      "window.</p></body></html>"));
  if (!m_replyHandler->isListening() &&
      !m_replyHandler->listen(QHostAddress::LocalHost, NexusOAuth::redirectPort())) {
    handleError(QObject::tr("Failed to bind to localhost on port %1.")
                    .arg(NexusOAuth::redirectPort()));
    return;
  }

  m_flow->setReplyHandler(m_replyHandler.get());

  QObject::connect(m_flow.get(), &QAbstractOAuth::authorizeWithBrowser, this,
                   [&](const QUrl& url) {
                     shell::Open(url);
                     setState(State::WaitingForBrowser);
                   });

  QObject::connect(m_flow.get(), &QAbstractOAuth::statusChanged, this,
                   [&](QAbstractOAuth::Status status) {
                     switch (status) {
                     case QAbstractOAuth::Status::RefreshingToken:
                     case QAbstractOAuth::Status::TemporaryCredentialsReceived:
                       setState(State::Authorizing);
                       break;

                     case QAbstractOAuth::Status::Granted:
                       setState(State::Finished);
                       break;

                     default:
                       break;
                     }
                   });

  QObject::connect(m_flow.get(), &QAbstractOAuth::requestFailed, this,
                   [&](QAbstractOAuth::Error error) {
                     handleError(
                         QObject::tr("Authorization failed (%1)").arg(int(error)));
                   });

  QObject::connect(m_flow.get(), &QAbstractOAuth::granted, this, [&] {
    notifyTokens();
  });

  m_active = true;
  setState(State::Initializing);
  m_flow->grant();
}

void NexusOAuthLogin::cancel()
{
  if (m_replyHandler) {
    m_replyHandler->close();
  }

  m_flow.reset();
  m_replyHandler.reset();
  if (m_active) {
    m_active = false;
    setState(State::Cancelled);
  }
}

bool NexusOAuthLogin::isActive() const
{
  return m_active;
}

void NexusOAuthLogin::setState(State state, const QString& message)
{
  if (stateChanged) {
    stateChanged(state, message);
  }
}

void NexusOAuthLogin::notifyTokens()
{
  if (!m_flow) {
    handleError(QObject::tr("Internal error: OAuth flow is missing."));
    return;
  }

  QJsonObject payload;
  payload.insert(QStringLiteral("access_token"), m_flow->token());

  const auto extras = m_flow->extraTokens();
  for (auto it = extras.constBegin(); it != extras.constEnd(); ++it) {
    payload.insert(it.key(), QJsonValue::fromVariant(it.value()));
  }

  auto tokens = makeTokensFromResponse(payload);
  if (!tokens.isValid()) {
    handleError(QObject::tr("Invalid OAuth token payload."));
    return;
  }

  tokens.scope = m_flow->scope();

  m_flow.reset();
  m_replyHandler.reset();
  m_codeVerifier.clear();
  m_active = false;
  if (tokensReceived) {
    tokensReceived(tokens);
  }
}

void NexusOAuthLogin::handleError(const QString& message)
{
  if (m_replyHandler) {
    m_replyHandler->close();
  }
  m_flow.reset();
  m_replyHandler.reset();
  m_codeVerifier.clear();
  m_active = false;
  if (stateChanged) {
    stateChanged(State::Error, message);
  }
}

namespace
{
QByteArray randomBytes(int length)
{
  QByteArray bytes;
  bytes.resize(length);
  QRandomGenerator::system()->generate(bytes.begin(), bytes.end());
  return bytes;
}
}  // namespace

void NexusOAuthLogin::injectPkceChallenge(QAbstractOAuth::Stage stage,
                                          QMultiMap<QString, QVariant>* parameters)
{
  if (!parameters) {
    return;
  }

  switch (stage) {
  case QAbstractOAuth::Stage::RequestingAuthorization: {
    m_codeVerifier = randomBytes(32).toBase64(QByteArray::Base64UrlEncoding |
                                              QByteArray::OmitTrailingEquals);
    const auto challenge =
        QCryptographicHash::hash(m_codeVerifier, QCryptographicHash::Sha256)
            .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    parameters->insert(QStringLiteral("code_challenge"), QString::fromUtf8(challenge));
    parameters->insert(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    parameters->insert(QStringLiteral("redirect_uri"), NexusOAuth::redirectUri());
    break;
  }

  case QAbstractOAuth::Stage::RequestingAccessToken: {
    if (!m_codeVerifier.isEmpty()) {
      parameters->insert(QStringLiteral("code_verifier"),
                         QString::fromUtf8(m_codeVerifier));
    }
    parameters->insert(QStringLiteral("redirect_uri"), NexusOAuth::redirectUri());
    break;
  }

  default:
    break;
  }
}
