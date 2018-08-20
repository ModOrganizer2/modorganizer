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

#ifndef NXMACCESSMANAGER_H
#define NXMACCESSMANAGER_H


#include <QNetworkAccessManager>
#include <QTimer>
#include <QNetworkReply>
#include <QProgressDialog>
#include <set>

namespace MOBase { class IPluginGame; }

/**
 * @brief access manager extended to handle nxm links
 **/
class NXMAccessManager : public QNetworkAccessManager
{
  Q_OBJECT
public:

  explicit NXMAccessManager(QObject *parent, const QString &moVersion);

  ~NXMAccessManager();

  void setNMMVersion(const QString &nmmVersion);

  bool validated() const;

  bool validateAttempted() const;
  bool validateWaiting() const;

  void apiCheck(const QString &apiKey);

  void showCookies() const;

  void clearCookies();

  QString userAgent(const QString &subModule = QString()) const;

  QString apiKey() const;

  void startValidationCheck();

  void refuseValidation();

signals:

  /**
   * @brief emitted when a nxm:// link is opened
   *
   * @param url the nxm-link
   **/
  void requestNXMDownload(const QString &url);

  /**
   * @brief emitted after a successful login or if login was not necessary
   *
   * @param necessary true if a login was necessary and succeeded, false if the user is still logged in
   **/
  void validateSuccessful(bool necessary);

  void validateFailed(const QString &message);

  void credentialsReceived(const QString &userName, bool premium);

private slots:

  void validateFinished();
  void validateError(QNetworkReply::NetworkError errorCode);
  void validateTimeout();

protected:

  virtual QNetworkReply *createRequest(
      QNetworkAccessManager::Operation operation, const QNetworkRequest &request,
      QIODevice *device);

private:

  QTimer m_ValidateTimeout;
  QNetworkReply *m_ValidateReply;
  QProgressDialog *m_ProgressDialog { nullptr };

  QString m_MOVersion;
  QString m_NMMVersion;

  QString m_ApiKey;

  bool m_ValidateAttempted;
  enum {
    VALIDATE_NOT_CHECKED,
    VALIDATE_CHECKING,
    VALIDATE_NOT_VALID,
    VALIDATE_ATTEMPT_FAILED,
    VALIDATE_REFUSED,
    VALIDATE_VALID
  } m_ValidateState = VALIDATE_NOT_CHECKED;

};

#endif // NXMACCESSMANAGER_H
