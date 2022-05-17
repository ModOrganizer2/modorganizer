#ifndef SETTINGSDIALOGNEXUS_H
#define SETTINGSDIALOGNEXUS_H

#include "nxmaccessmanager.h"
#include "settings.h"
#include "settingsdialog.h"

// used by the settings dialog and the create instance dialog
//
class NexusConnectionUI : public QObject
{
  Q_OBJECT;

public:
  NexusConnectionUI(QWidget* parent, Settings* s, QAbstractButton* connectButton,
                    QAbstractButton* disconnectButton, QAbstractButton* manualButton,
                    QListWidget* logList);

  void connect();
  void manual();
  void disconnect();

signals:
  void stateChanged();
  void keyChanged();

private:
  QWidget* m_parent;
  Settings* m_settings;
  QAbstractButton* m_connect;
  QAbstractButton* m_disconnect;
  QAbstractButton* m_manual;
  QListWidget* m_log;

  std::unique_ptr<NexusSSOLogin> m_nexusLogin;
  std::unique_ptr<NexusKeyValidator> m_nexusValidator;

  void addLog(const QString& s);

  void updateState();

  void validateKey(const QString& key);
  bool setKey(const QString& key);
  bool clearKey();

  void onSSOKeyChanged(const QString& key);
  void onSSOStateChanged(NexusSSOLogin::States s, const QString& e);

  void onValidatorFinished(ValidationAttempt::Result r, const QString& message,
                           std::optional<APIUserAccount> useR);
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
