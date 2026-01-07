#ifndef SETTINGSDIALOGNEXUS_H
#define SETTINGSDIALOGNEXUS_H

#include "nexusoauthlogin.h"
#include "nexusoauthtokens.h"
#include "nxmaccessmanager.h"
#include "settings.h"
#include "settingsdialog.h"
#include <memory>
#include <optional>

class QAbstractButton;
class QListWidget;
// used by the settings dialog and the create instance dialog
//
class NexusConnectionUI : public QObject
{
  Q_OBJECT;

public:
  NexusConnectionUI(QWidget* parent, Settings* s, QAbstractButton* connectButton,
                    QAbstractButton* disconnectButton, QListWidget* logList);

  void connect();
  void disconnect();

signals:
  void stateChanged();
  void keyChanged();

private:
  QWidget* m_parent;
  Settings* m_settings;
  QAbstractButton* m_connect;
  QAbstractButton* m_disconnect;
  QListWidget* m_log;

  std::unique_ptr<NexusOAuthLogin> m_nexusLogin;
  std::unique_ptr<NexusKeyValidator> m_nexusValidator;
  std::optional<NexusOAuthTokens> m_pendingTokens;

  void addLog(const QString& s);

  void updateState();

  void validateTokens(const NexusOAuthTokens& tokens);
  bool persistTokens(const NexusOAuthTokens& tokens);
  bool clearTokens();

  void onTokensReceived(const NexusOAuthTokens& tokens);
  void onOAuthStateChanged(NexusOAuthLogin::State s, const QString& message);

  void onValidatorFinished(ValidationAttempt::Result r, const QString& message,
                           std::optional<APIUserAccount> user);
};

class NexusSettingsTab : public SettingsTab
{
public:
  NexusSettingsTab(Settings& settings, SettingsDialog& dialog);
  void update();

private:
  std::unique_ptr<NexusConnectionUI> m_connectionUI;

  void clearCache();
  void associate();

  void updateNexusData();
  void updateCustomBrowser();
  void browseCustomBrowser();
};

#endif  // SETTINGSDIALOGNEXUS_H
