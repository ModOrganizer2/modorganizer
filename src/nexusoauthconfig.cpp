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

#include "nexusoauthconfig.h"
#include <QProcessEnvironment>

namespace
{
QString envOrDefault(const char* name, const QString& fallback)
{
  const auto value = qEnvironmentVariable(name);
  if (!value.isEmpty()) {
    return value;
  }

  return fallback;
}
}  // namespace

namespace NexusOAuth
{
QString clientId()
{
  return envOrDefault("MO2_NEXUS_CLIENT_ID", QStringLiteral("modorganizer2"));
}

quint16 redirectPort()
{
  return 28635;
}

QString redirectUri()
{
  return QStringLiteral("http://127.0.0.1:%1/callback").arg(redirectPort());
}

QString authorizeUrl()
{
  return QStringLiteral("https://users.nexusmods.com/oauth/authorize");
}

QString tokenUrl()
{
  return QStringLiteral("https://users.nexusmods.com/oauth/token");
}
}  // namespace NexusOAuth
