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
#include "nexusinterface.h"
#include "nexusoauthconfig.h"
#include "nxmaccessmanager.h"
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

void NexusOAuthLogin::start()
{
  if (m_active) {
    cancel();
  }
  auto accessManager = NexusInterface::instance().getAccessManager();
  connect(accessManager, &NXMAccessManager::authorizationEnded, this,
          &NexusOAuthLogin::authorizationEnded);

  accessManager->tokensReceived = tokensReceived;
  accessManager->stateChanged   = stateChanged;

  NexusInterface::instance().getAccessManager()->connectOrRefresh(NexusOAuthTokens());
  m_active = true;
}

void NexusOAuthLogin::authorizationEnded()
{
  m_active = false;
}

void NexusOAuthLogin::cancel()
{
  NexusInterface::instance().getAccessManager()->cancelAuth();
  m_active = false;
}

bool NexusOAuthLogin::isActive() const
{
  return m_active;
}
