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

  bool loggedIn() const;

  bool loginAttempted() const;
  bool loginWaiting() const;

  void login(const QString &username, const QString &password);

  void showCookies() const;

  QString userAgent(const QString &subModule = QString()) const;

  void startLoginCheck();

  void refuseLogin();

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
  void loginSuccessful(bool necessary);

  void loginFailed(const QString &message);

  void credentialsReceived(const QString &userName, bool premium);

private slots:

  void loginChecked();
  void loginFinished();
  void loginError(QNetworkReply::NetworkError errorCode);
  void loginTimeout();

protected:

  virtual QNetworkReply *createRequest(
      QNetworkAccessManager::Operation operation, const QNetworkRequest &request,
      QIODevice *device);

private:

  void pageLogin();
// void dlLogin();

  bool hasLoginCookies() const;

  void retrieveCredentials();

private:

  static const std::set<int> s_PremiumAccountStates;

private:

  QTimer m_LoginTimeout;
  QNetworkReply *m_LoginReply;
  QProgressDialog *m_ProgressDialog { nullptr };

  QString m_MOVersion;
  QString m_NMMVersion;

  QString m_Username;
  QString m_Password;

  bool m_LoginAttempted;
  enum {
    LOGIN_NOT_CHECKED,
    LOGIN_CHECKING,
    LOGIN_NOT_VALID,
    LOGIN_ATTEMPT_FAILED,
    LOGIN_REFUSED,
    LOGIN_VALID
  } m_LoginState = LOGIN_NOT_CHECKED;

};

#endif // NXMACCESSMANAGER_H
