#include "settingsdialognexus.h"
#include "log.h"
#include "nexusinterface.h"
#include "nexusoauthlogin.h"
#include "serverinfo.h"
#include "ui_settingsdialog.h"
#include <utility.h>

using namespace MOBase;

template <typename T>
class ServerItem : public QListWidgetItem
{
public:
  ServerItem(const QString& text, int sortRole = Qt::DisplayRole,
             QListWidget* parent = 0, int type = Type)
      : QListWidgetItem(text, parent, type), m_SortRole(sortRole)
  {}

  virtual bool operator<(const QListWidgetItem& other) const
  {
    return this->data(m_SortRole).value<T>() < other.data(m_SortRole).value<T>();
  }

private:
  int m_SortRole;
};

NexusConnectionUI::NexusConnectionUI(QWidget* parent, Settings* s,
                                     QAbstractButton* connectButton,
                                     QAbstractButton* disconnectButton,
                                     QListWidget* logList)
    : m_parent(parent), m_settings(s), m_connect(connectButton),
      m_disconnect(disconnectButton), m_log(logList)
{
  if (m_connect) {
    QObject::connect(m_connect, &QPushButton::clicked, [&] {
      connect();
    });
  }

  if (m_disconnect) {
    QObject::connect(m_disconnect, &QPushButton::clicked, [&] {
      disconnect();
    });
  }

  if (GlobalSettings::hasNexusOAuthTokens()) {
    addLog(tr("Connected."));
  } else {
    addLog(tr("Not connected."));
  }

  updateState();
}

void NexusConnectionUI::connect()
{
  if (m_nexusLogin && m_nexusLogin->isActive()) {
    m_nexusLogin->cancel();
    return;
  }

  if (!m_nexusLogin) {
    m_nexusLogin.reset(new NexusOAuthLogin(m_parent));

    m_nexusLogin->tokensReceived = [&](const NexusOAuthTokens& tokens) {
      onTokensReceived(tokens);
    };

    m_nexusLogin->stateChanged = [&](auto&& state, auto&& message) {
      onOAuthStateChanged(state, message);
    };
  }

  m_log->clear();
  m_pendingTokens.reset();
  m_nexusLogin->start();
  updateState();
}

void NexusConnectionUI::disconnect()
{
  clearTokens();
  m_log->clear();
  addLog(tr("Disconnected."));
}

void NexusConnectionUI::validateTokens(const NexusOAuthTokens& tokens)
{
  if (!m_nexusValidator) {
    m_nexusValidator.reset(new NexusKeyValidator(
        m_settings, *NexusInterface::instance().getAccessManager()));

    m_nexusValidator->finished = [&](auto&& r, auto&& m, auto&& u) {
      onValidatorFinished(r, m, u);
    };
  }

  addLog(tr("Authorizing with Nexus..."));
  m_nexusValidator->start(tokens, NexusKeyValidator::OneShot);
}

void NexusConnectionUI::onTokensReceived(const NexusOAuthTokens& tokens)
{
  m_pendingTokens = tokens;
  addLog(tr("Received authorization from Nexus."));
  validateTokens(tokens);
}

void NexusConnectionUI::onOAuthStateChanged(NexusOAuthLogin::State s, const QString& e)
{
  if (s != NexusOAuthLogin::State::Finished) {
    const auto log = NexusOAuthLogin::stateToString(s, e);

    for (auto&& line : log.split("\n")) {
      addLog(line);
    }
  }

  updateState();
}

void NexusConnectionUI::onValidatorFinished(ValidationAttempt::Result r,
                                            const QString& message,
                                            std::optional<APIUserAccount> user)
{
  Q_UNUSED(r);
  if (user) {
    NexusInterface::instance().setUserAccount(*user);
    addLog(tr("Received user account information"));

    if (m_pendingTokens) {
      if (persistTokens(*m_pendingTokens)) {
        addLog(tr("Linked with Nexus successfully."));
      } else {
        addLog(tr("Failed to store OAuth tokens."));
      }
    } else {
      addLog(tr("Linked with Nexus successfully."));
    }
  } else {
    if (message.isEmpty()) {
      // shouldn't happen
      addLog("Unknown error");
    } else {
      addLog(message);
    }
  }

  m_pendingTokens.reset();
  updateState();
}

void NexusConnectionUI::addLog(const QString& s)
{
  m_log->addItem(s);
  m_log->scrollToBottom();
}

bool NexusConnectionUI::persistTokens(const NexusOAuthTokens& tokens)
{
  const bool ret = GlobalSettings::setNexusOAuthTokens(tokens);
  if (ret) {
    NexusInterface::instance().getAccessManager()->setTokens(tokens);
  }

  updateState();

  emit keyChanged();

  return ret;
}

bool NexusConnectionUI::clearTokens()
{
  const auto ret = GlobalSettings::clearNexusOAuthTokens();

  NexusInterface::instance().getAccessManager()->clearTokens();
  updateState();

  emit keyChanged();

  return ret;
}

void NexusConnectionUI::updateState()
{
  auto setButton = [&](QAbstractButton* b, bool enabled, QString caption = {}) {
    if (b) {
      b->setEnabled(enabled);
      if (!caption.isEmpty()) {
        b->setText(caption);
      }
    }
  };

  if (m_nexusLogin && m_nexusLogin->isActive()) {
    setButton(m_connect, true, QObject::tr("Cancel"));
    setButton(m_disconnect, false);
  } else if (m_nexusValidator && m_nexusValidator->isActive()) {
    setButton(m_connect, false, QObject::tr("Connect to Nexus"));
    setButton(m_disconnect, false);
  } else if (GlobalSettings::hasNexusOAuthTokens()) {
    setButton(m_connect, true, QObject::tr("Connect to Nexus"));
    setButton(m_disconnect, true);
  } else {
    setButton(m_connect, true, QObject::tr("Connect to Nexus"));
    setButton(m_disconnect, false);
  }

  emit stateChanged();
}

NexusSettingsTab::NexusSettingsTab(Settings& s, SettingsDialog& d) : SettingsTab(s, d)
{
  ui->endorsementBox->setChecked(settings().nexus().endorsementIntegration());
  ui->trackedBox->setChecked(settings().nexus().trackedIntegration());
  ui->categoryMappingsBox->setChecked(settings().nexus().categoryMappings());
  ui->hideAPICounterBox->setChecked(settings().interface().hideAPICounter());

  // display server preferences
  for (const auto& server : s.network().servers()) {
    QString descriptor = server.name();

    if (!descriptor.compare("CDN", Qt::CaseInsensitive)) {
      descriptor += QStringLiteral(" (automatic)");
    }

    const auto averageSpeed = server.averageSpeed();
    if (averageSpeed > 0) {
      descriptor += QString(" (%1)").arg(MOBase::localizedByteSpeed(averageSpeed));
    }

    QListWidgetItem* newItem = new ServerItem<int>(descriptor, Qt::UserRole + 1);

    newItem->setData(Qt::UserRole, server.name());
    newItem->setData(Qt::UserRole + 1, server.preferred());

    if (server.preferred() > 0) {
      ui->preferredServersList->addItem(newItem);
    } else {
      ui->knownServersList->addItem(newItem);
    }

    ui->preferredServersList->sortItems(Qt::DescendingOrder);
  }

  m_connectionUI.reset(new NexusConnectionUI(&dialog(), &settings(), ui->nexusConnect,
                                             ui->nexusDisconnect, ui->nexusLog));

  QObject::connect(
      m_connectionUI.get(), &NexusConnectionUI::stateChanged, &d,
      [&] {
        updateNexusData();
      },
      Qt::QueuedConnection);

  QObject::connect(m_connectionUI.get(), &NexusConnectionUI::keyChanged, &d, [&] {
    dialog().setExitNeeded(Exit::Restart);
  });

  QObject::connect(ui->clearCacheButton, &QPushButton::clicked, [&] {
    clearCache();
  });
  QObject::connect(ui->associateButton, &QPushButton::clicked, [&] {
    associate();
  });
  QObject::connect(ui->useCustomBrowser, &QCheckBox::clicked, [&] {
    updateCustomBrowser();
  });
  QObject::connect(ui->browseCustomBrowser, &QPushButton::clicked, [&] {
    browseCustomBrowser();
  });

  updateNexusData();
  updateCustomBrowser();
}

void NexusSettingsTab::update()
{
  settings().nexus().setEndorsementIntegration(ui->endorsementBox->isChecked());
  settings().nexus().setTrackedIntegration(ui->trackedBox->isChecked());
  settings().nexus().setCategoryMappings(ui->categoryMappingsBox->isChecked());
  settings().interface().setHideAPICounter(ui->hideAPICounterBox->isChecked());

  auto servers = settings().network().servers();

  // store server preference
  for (int i = 0; i < ui->knownServersList->count(); ++i) {
    const QString key = ui->knownServersList->item(i)->data(Qt::UserRole).toString();

    bool found = false;

    for (auto& server : servers) {
      if (server.name() == key) {
        server.setPreferred(0);
        found = true;
        break;
      }
    }

    if (!found) {
      log::error("while setting preferred to 0, server '{}' not found", key);
    }
  }

  const int count = ui->preferredServersList->count();

  for (int i = 0; i < count; ++i) {
    const QString key =
        ui->preferredServersList->item(i)->data(Qt::UserRole).toString();
    const int newPreferred = count - i;

    bool found = false;

    for (auto& server : servers) {

      if (server.name() == key) {
        server.setPreferred(newPreferred);
        found = true;
        break;
      }
    }

    if (!found) {
      log::error("while setting preference to {}, server '{}' not found", newPreferred,
                 key);
    }
  }

  settings().network().updateServers(servers);
}

void NexusSettingsTab::clearCache()
{
  QDir(Settings::instance().paths().cache()).removeRecursively();
  NexusInterface::instance().clearCache();
}

void NexusSettingsTab::associate()
{
  Settings::instance().nexus().registerAsNXMHandler(true);
}

void NexusSettingsTab::updateNexusData()
{
  const auto user = NexusInterface::instance().getAPIUserAccount();

  if (user.isValid()) {
    ui->nexusUserID->setText(user.id());
    ui->nexusName->setText(user.name());
    ui->nexusAccount->setText(localizedUserAccountType(user.type()));

    ui->nexusDailyRequests->setText(QString("%1/%2")
                                        .arg(user.limits().remainingDailyRequests)
                                        .arg(user.limits().maxDailyRequests));

    ui->nexusHourlyRequests->setText(QString("%1/%2")
                                         .arg(user.limits().remainingHourlyRequests)
                                         .arg(user.limits().maxHourlyRequests));
  } else {
    ui->nexusUserID->setText(QObject::tr("N/A"));
    ui->nexusName->setText(QObject::tr("N/A"));
    ui->nexusAccount->setText(QObject::tr("N/A"));
    ui->nexusDailyRequests->setText(QObject::tr("N/A"));
    ui->nexusHourlyRequests->setText(QObject::tr("N/A"));
  }
}

void NexusSettingsTab::updateCustomBrowser()
{
  ui->browserCommand->setEnabled(ui->useCustomBrowser->isChecked());
}

void NexusSettingsTab::browseCustomBrowser()
{
  const QString Filters =
      QObject::tr("Executables (*.exe)") + ";;" + QObject::tr("All Files (*.*)");

  QString file = QFileDialog::getOpenFileName(
      parentWidget(), QObject::tr("Select the browser executable"),
      ui->browserCommand->text(), Filters);

  if (file.isNull() || file == "") {
    return;
  }

  ui->browserCommand->setText(file + " \"%1\"");
}
