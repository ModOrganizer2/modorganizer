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

#include "apiuseraccount.h"
#include <QNetworkAccessManager>
#include <QTimer>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QElapsedTimer>
#include <QDialogButtonBox>
#include <QWebSocket>
#include <set>

namespace MOBase { class IPluginGame; }
class NXMAccessManager;

class ValidationProgressDialog : private QDialog
{
  Q_OBJECT;

public:
  ValidationProgressDialog(std::chrono::seconds timeout);

  void setParentWidget(QWidget* w);

  void start();
  void stop();

  using QDialog::show;

protected:
  void closeEvent(QCloseEvent* e) override;

private:
  std::chrono::seconds m_timeout;
  QProgressBar* m_bar;
  QDialogButtonBox* m_buttons;
  QTimer* m_timer;
  QElapsedTimer m_elapsed;

  void onButton(QAbstractButton* b);
  void onTimer();
};


class NexusSSOLogin
{
public:
  enum States
  {
    ConnectingToSSO,
    WaitingForToken,
    WaitingForBrowser,
    Finished,
    Timeout,
    ClosedByRemote,
    Cancelled,
    Error
  };

  std::function<void (QString)> keyChanged;
  std::function<void (States, QString)> stateChanged;

  NexusSSOLogin();

  void start();
  void cancel();

  bool isActive() const;

private:
  QWebSocket m_socket;
  QString m_guid;
  bool m_keyReceived;
  QString m_token;
  bool m_active;
  QTimer m_timeout;

  void setState(States s, const QString& error={});

  void close();
  void abort();

  void onConnected();
  void onMessage(const QString& s);
  void onDisconnected();
  void onError(QAbstractSocket::SocketError e);
  void onSslErrors(const QList<QSslError>& errors);
  void onTimeout();
};


class NexusKeyValidator
{
public:
  enum States
  {
    Connecting,
    Finished,
    InvalidJson,
    BadResponse,
    Timeout,
    Cancelled,
    Error
  };

  std::function<void (APIUserAccount)> finished;
  std::function<void (States, QString)> stateChanged;

  NexusKeyValidator(NXMAccessManager& am);
  ~NexusKeyValidator();

  void start(const QString& key);
  void cancel();

  bool isActive() const;

private:
  NXMAccessManager& m_manager;
  QNetworkReply* m_reply;
  QTimer m_timeout;
  bool m_active;

  void setState(States s, const QString& error={});

  void close();
  void abort();

  void onFinished();
  void onSslErrors(const QList<QSslError>& errors);
  void onTimeout();

  void handleError(
    int code, const QString& nexusMessage, const QString& httpError);
};


/**
 * @brief access manager extended to handle nxm links
 **/
class NXMAccessManager : public QNetworkAccessManager
{
  Q_OBJECT
public:
  enum ApiCheckFlagsEnum
  {
    NoFlags = 0,
    Force,
    HideProgress
  };

  Q_DECLARE_FLAGS(ApiCheckFlags, ApiCheckFlagsEnum)

  static const std::chrono::seconds ValidationTimeout;

  explicit NXMAccessManager(QObject *parent, const QString &moVersion);


  void setTopLevelWidget(QWidget* w);

  bool validated() const;

  bool validateAttempted() const;
  bool validateWaiting() const;

  void apiCheck(const QString &apiKey, ApiCheckFlags flags=NoFlags);

  void showCookies() const;

  void clearCookies();

  QString userAgent(const QString &subModule = QString()) const;
  const QString& MOVersion() const;

  void clearApiKey();

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
  void credentialsReceived(const APIUserAccount& user);

protected:

  virtual QNetworkReply *createRequest(
      QNetworkAccessManager::Operation operation, const QNetworkRequest &request,
      QIODevice *device);

private:
  enum States
  {
    NotChecked,
    Valid,
    Invalid
  };

  QWidget* m_TopLevel;
  mutable ValidationProgressDialog* m_ProgressDialog;
  QString m_MOVersion;
  NexusKeyValidator m_validator;
  States m_validationState;

  void startValidationCheck(const QString& key, bool showProgress);
  void onValidatorState(NexusKeyValidator::States s, const QString& e);
  void onValidatorFinished(const APIUserAccount& user);
  void onValidatorError(const QString& e);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(NXMAccessManager::ApiCheckFlags);

#endif // NXMACCESSMANAGER_H
