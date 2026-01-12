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

#ifndef NEXUSOAUTHTOKENS_H
#define NEXUSOAUTHTOKENS_H

#include <QDateTime>
#include <QJsonObject>
#include <chrono>
#include <optional>

struct NexusOAuthTokens
{
  QString accessToken;
  QString refreshToken;
  QString scope;
  QString tokenType;
  QDateTime expiresAt;

  bool isValid() const { return !accessToken.isEmpty() && expiresAt.isValid(); }

  bool isExpired(std::chrono::seconds skew = std::chrono::seconds(60)) const
  {
    if (!expiresAt.isValid()) {
      return true;
    }

    const auto now = QDateTime::currentDateTimeUtc();
    return now.addSecs(skew.count()) >= expiresAt;
  }

  QJsonObject toJson() const
  {
    QJsonObject json;
    json.insert(QStringLiteral("access_token"), accessToken);
    json.insert(QStringLiteral("refresh_token"), refreshToken);
    json.insert(QStringLiteral("scope"), scope);
    json.insert(QStringLiteral("token_type"), tokenType);
    json.insert(QStringLiteral("expires_at"), expiresAt.toString(Qt::ISODateWithMs));
    return json;
  }

  static std::optional<NexusOAuthTokens> fromJson(const QJsonObject& json)
  {
    NexusOAuthTokens tokens;
    tokens.accessToken  = json.value(QStringLiteral("access_token")).toString();
    tokens.refreshToken = json.value(QStringLiteral("refresh_token")).toString();
    tokens.scope        = json.value(QStringLiteral("scope")).toString();
    tokens.tokenType    = json.value(QStringLiteral("token_type")).toString();
    tokens.expiresAt    = QDateTime::fromString(
        json.value(QStringLiteral("expires_at")).toString(), Qt::ISODateWithMs);
    if (!tokens.expiresAt.isValid()) {
      tokens.expiresAt = QDateTime::fromString(
          json.value(QStringLiteral("expires_at")).toString(), Qt::ISODate);
    }

    if (!tokens.isValid()) {
      return std::nullopt;
    }

    if (tokens.expiresAt.isValid() && tokens.expiresAt.timeSpec() != Qt::UTC) {
      tokens.expiresAt = tokens.expiresAt.toUTC();
    }

    return tokens;
  }
};

inline NexusOAuthTokens makeTokensFromResponse(const QJsonObject& json)
{
  NexusOAuthTokens tokens;
  tokens.accessToken  = json.value(QStringLiteral("access_token")).toString();
  tokens.refreshToken = json.value(QStringLiteral("refresh_token")).toString();
  tokens.scope        = json.value(QStringLiteral("scope")).toString();
  tokens.tokenType    = json.value(QStringLiteral("token_type")).toString();

  const auto expiresIn = json.value(QStringLiteral("expires_in")).toInt();
  if (expiresIn > 0) {
    tokens.expiresAt =
        QDateTime::currentDateTimeUtc().addSecs(static_cast<qint64>(expiresIn));
  }

  return tokens;
}

#endif  // NEXUSOAUTHTOKENS_H
