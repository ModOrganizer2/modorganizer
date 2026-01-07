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

#ifndef NEXUSOAUTHLOGIN_H
#define NEXUSOAUTHLOGIN_H

#include "nexusoauthtokens.h"
#include <QObject>
#include <functional>
#include <memory>

class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;

class NexusOAuthLogin : public QObject
{
  Q_OBJECT

public:
  enum class State
  {
    Initializing,
    WaitingForBrowser,
    Authorizing,
    Finished,
    Cancelled,
    Error
  };

  explicit NexusOAuthLogin(QObject* parent = nullptr);
  ~NexusOAuthLogin();

  void start();
  void cancel();
  bool isActive() const;

  std::function<void(const NexusOAuthTokens&)> tokensReceived;
  std::function<void(State, QString)> stateChanged;

  static QString stateToString(State state, const QString& details = {});

private:
  std::unique_ptr<QOAuth2AuthorizationCodeFlow> m_flow;
  std::unique_ptr<QOAuthHttpServerReplyHandler> m_replyHandler;
  bool m_active;

  void setState(State state, const QString& message = {});
  void notifyTokens();
  void handleError(const QString& message);
  void injectPkceChallenge(QAbstractOAuth::Stage stage,
                           QMultiMap<QString, QVariant>* parameters);

  QByteArray m_codeVerifier;
};

#endif  // NEXUSOAUTHLOGIN_H
