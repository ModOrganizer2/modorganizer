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
#include "nexusoauthtokens.h"
#include "ui_validationprogressdialog.h"
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QTimer>
#include <optional>
#include <set>

namespace MOBase
{
class IPluginGame;
}
class NXMAccessManager;
class Settings;

class ValidationAttempt
{
public:
  enum Result
  {
    None,
    Success,
    SoftError,
    HardError,
    Cancelled
  };

  std::function<void(APIUserAccount)> success;
  std::function<void()> failure;

  ValidationAttempt(std::chrono::seconds timeout);
  ValidationAttempt(const ValidationAttempt&)            = delete;
  ValidationAttempt& operator=(const ValidationAttempt&) = delete;

  void start(NXMAccessManager& m, const NexusOAuthTokens& tokens);
  void cancel();

  bool done() const;
  Result result() const;
  const QString& message() const;
  std::chrono::seconds timeout() const;
  QElapsedTimer elapsed() const;

private:
  QNetworkReply* m_reply;
  Result m_result;
  QString m_message;
  QTimer m_timeout;
  QElapsedTimer m_elapsed;
  NexusOAuthTokens m_tokens;

  bool sendRequest(NXMAccessManager& m, const NexusOAuthTokens& tokens);

  void onFinished();
  void onSslErrors(const QList<QSslError>& errors);
  void onTimeout();

  void setFailure(Result r, const QString& error);
  void setSuccess(const APIUserAccount& user);

  void cleanup();
};

class NexusKeyValidator
{
public:
  enum Behaviour
  {
    OneShot = 0,
    Retry
  };

  using FinishedCallback = void(ValidationAttempt::Result, const QString&,
                                std::optional<APIUserAccount>);

  std::function<FinishedCallback> finished;
  std::function<void(const ValidationAttempt&)> attemptFinished;

  NexusKeyValidator(Settings* s, NXMAccessManager& am);
  ~NexusKeyValidator();

  void start(const NexusOAuthTokens& tokens, Behaviour b);
  void cancel();

  bool isActive() const;
  const ValidationAttempt* lastAttempt() const;
  const ValidationAttempt* currentAttempt() const;

private:
  Settings* m_settings;
  NXMAccessManager& m_manager;
  std::optional<NexusOAuthTokens> m_tokens;
  std::vector<std::unique_ptr<ValidationAttempt>> m_attempts;

  void createAttempts(const std::vector<std::chrono::seconds>& timeouts);
  std::vector<std::chrono::seconds> getTimeouts() const;

  bool nextTry();
  void onAttemptSuccess(const ValidationAttempt& a, const APIUserAccount& u);
  void onAttemptFailure(const ValidationAttempt& a);

  void setFinished(ValidationAttempt::Result r, const QString& message,
                   std::optional<APIUserAccount> user);
};

class ValidationProgressDialog : public QDialog
{
  Q_OBJECT;

public:
  ValidationProgressDialog(Settings* s, NexusKeyValidator& v);

  void setParentWidget(QWidget* w);

  void start();
  void stop();

protected:
  void showEvent(QShowEvent* e) override;
  void closeEvent(QCloseEvent* e) override;

private:
  std::unique_ptr<Ui::ValidationProgressDialog> ui;
  Settings* m_settings;
  NexusKeyValidator& m_validator;
  QTimer* m_updateTimer;
  bool m_first;

  void onHide();
  void onCancel();
  void onTimer();
  void updateProgress();
};

/**
 * @brief access manager extended to handle nxm links
 **/
class NXMAccessManager : public QNetworkAccessManager
{
  Q_OBJECT
public:
  NXMAccessManager(QObject* parent, Settings* s, const QString& moVersion);

  void setTopLevelWidget(QWidget* w);

  bool validated() const;

  bool validateAttempted() const;
  bool validateWaiting() const;

  void apiCheck(const NexusOAuthTokens& tokens, bool force = false);
  void setTokens(const NexusOAuthTokens& tokens);
  std::optional<NexusOAuthTokens> tokens() const;
  bool ensureFreshToken();

  void showCookies() const;

  void clearCookies();

  QString userAgent(const QString& subModule = QString()) const;
  const QString& MOVersion() const;

  void clearTokens();

  void refuseValidation();

signals:

  /**
   * @brief emitted when a nxm:// link is opened
   *
   * @param url the nxm-link
   **/
  void requestNXMDownload(const QString& url);

  /**
   * @brief emitted after a successful login or if login was not necessary
   *
   * @param necessary true if a login was necessary and succeeded, false if the user is
   *still logged in
   **/
  void validateSuccessful(bool necessary);
  void validateFailed(const QString& message);
  void credentialsReceived(const APIUserAccount& user);

protected:
  virtual QNetworkReply* createRequest(QNetworkAccessManager::Operation operation,
                                       const QNetworkRequest& request,
                                       QIODevice* device);

private:
  enum States
  {
    NotChecked,
    Valid,
    Invalid
  };

  QWidget* m_TopLevel;
  Settings* m_Settings;
  mutable std::unique_ptr<ValidationProgressDialog> m_ProgressDialog;
  QString m_MOVersion;
  NexusKeyValidator m_validator;
  States m_validationState;
  std::optional<NexusOAuthTokens> m_tokens;

  void startValidationCheck(const NexusOAuthTokens& tokens);
  std::optional<NexusOAuthTokens>
  refreshTokensBlocking(const NexusOAuthTokens& current);

  void onValidatorFinished(ValidationAttempt::Result r, const QString& message,
                           std::optional<APIUserAccount>);

  void onValidatorAttemptFinished(const ValidationAttempt& a);

  void startProgress();
  void stopProgress();
};

#endif  // NXMACCESSMANAGER_H
