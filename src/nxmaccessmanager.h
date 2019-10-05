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
namespace Ui { class ValidationProgressDialog; }
class NXMAccessManager;

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

  static QString stateToString(States s, const QString& e);

  NexusSSOLogin();

  void start();
  void cancel();

  bool isActive() const;

private:
  QWebSocket m_socket;
  QString m_guid;
  bool m_keyReceived;
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

  static QString stateToString(States s, const QString& e);

  NexusKeyValidator(NXMAccessManager& am);
  ~NexusKeyValidator();

  void start(const QString& key);
  void cancel();

  bool isActive() const;
  QElapsedTimer elapsed() const;
  std::chrono::seconds timeout() const;

private:
  NXMAccessManager& m_manager;
  QNetworkReply* m_reply;
  QTimer m_timeout;
  bool m_active;
  QElapsedTimer m_elapsed;

  void setState(States s, const QString& error={});

  void close();
  void abort();

  void onFinished();
  void onSslErrors(const QList<QSslError>& errors);
  void onTimeout();

  void handleError(
    int code, const QString& nexusMessage, const QString& httpError);
};


class ValidationProgressDialog : public QDialog
{
  Q_OBJECT;

public:
  ValidationProgressDialog(const NexusKeyValidator& v);

  void setParentWidget(QWidget* w);

  void start();
  void stop();

protected:
  void showEvent(QShowEvent* e) override;
  void closeEvent(QCloseEvent* e) override;

private:
  std::unique_ptr<Ui::ValidationProgressDialog> ui;
  const NexusKeyValidator& m_validator;
  QTimer* m_updateTimer;
  bool m_first;

  void onHide();
  void onCancel();
  void onTimer();
};


/**
 * @brief access manager extended to handle nxm links
 **/
class NXMAccessManager : public QNetworkAccessManager
{
  Q_OBJECT
public:
  static const std::chrono::seconds ValidationTimeout;

  explicit NXMAccessManager(QObject *parent, const QString &moVersion);


  void setTopLevelWidget(QWidget* w);

  bool validated() const;

  bool validateAttempted() const;
  bool validateWaiting() const;

  void apiCheck(const QString &apiKey, bool force=false);

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
  mutable std::unique_ptr<ValidationProgressDialog> m_ProgressDialog;
  QString m_MOVersion;
  NexusKeyValidator m_validator;
  States m_validationState;

  void startValidationCheck(const QString& key);
  void onValidatorState(NexusKeyValidator::States s, const QString& e);
  void onValidatorFinished(const APIUserAccount& user);
};

#endif // NXMACCESSMANAGER_H
