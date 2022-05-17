#include "settingsdialognexus.h"
#include "log.h"
#include "nexusinterface.h"
#include "serverinfo.h"
#include "ui_nexusmanualkey.h"
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

class NexusManualKeyDialog : public QDialog
{
public:
  NexusManualKeyDialog(QWidget* parent)
      : QDialog(parent), ui(new Ui::NexusManualKeyDialog)
  {
    ui->setupUi(this);
    ui->key->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    connect(ui->openBrowser, &QPushButton::clicked, [&] {
      openBrowser();
    });
    connect(ui->paste, &QPushButton::clicked, [&] {
      paste();
    });
    connect(ui->clear, &QPushButton::clicked, [&] {
      clear();
    });
  }

  void accept() override
  {
    m_key = ui->key->toPlainText();
    QDialog::accept();
  }

  const QString& key() const { return m_key; }

  void openBrowser()
  {
    shell::Open(QUrl("https://www.nexusmods.com/users/myaccount?tab=api"));
  }

  void paste()
  {
    const auto text = QApplication::clipboard()->text();
    if (!text.isEmpty()) {
      ui->key->setPlainText(text);
    }
  }

  void clear() { ui->key->clear(); }

private:
  std::unique_ptr<Ui::NexusManualKeyDialog> ui;
  QString m_key;
};

NexusConnectionUI::NexusConnectionUI(QWidget* parent, Settings* s,
                                     QAbstractButton* connectButton,
                                     QAbstractButton* disconnectButton,
                                     QAbstractButton* manualButton,
                                     QListWidget* logList)
    : m_parent(parent), m_settings(s), m_connect(connectButton),
      m_disconnect(disconnectButton), m_manual(manualButton), m_log(logList)
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

  if (m_manual) {
    QObject::connect(manualButton, &QPushButton::clicked, [&] {
      manual();
    });
  }

  if (GlobalSettings::hasNexusApiKey()) {
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
    m_nexusLogin.reset(new NexusSSOLogin);

    m_nexusLogin->keyChanged = [&](auto&& s) {
      onSSOKeyChanged(s);
    };

    m_nexusLogin->stateChanged = [&](auto&& s, auto&& e) {
      onSSOStateChanged(s, e);
    };
  }

  m_log->clear();
  m_nexusLogin->start();
  updateState();
}

void NexusConnectionUI::manual()
{
  if (m_nexusValidator && m_nexusValidator->isActive()) {
    m_nexusValidator->cancel();
    return;
  }

  NexusManualKeyDialog d(m_parent);
  if (d.exec() != QDialog::Accepted) {
    return;
  }

  const auto key = d.key();
  if (key.isEmpty()) {
    clearKey();
    return;
  }

  m_log->clear();
  validateKey(key);
}

void NexusConnectionUI::disconnect()
{
  clearKey();
  m_log->clear();
  addLog(tr("Disconnected."));
}

void NexusConnectionUI::validateKey(const QString& key)
{
  if (!m_nexusValidator) {
    m_nexusValidator.reset(new NexusKeyValidator(
        m_settings, *NexusInterface::instance().getAccessManager()));

    m_nexusValidator->finished = [&](auto&& r, auto&& m, auto&& u) {
      onValidatorFinished(r, m, u);
    };
  }

  addLog(tr("Checking API key..."));
  m_nexusValidator->start(key, NexusKeyValidator::OneShot);
}

void NexusConnectionUI::onSSOKeyChanged(const QString& key)
{
  if (key.isEmpty()) {
    clearKey();
  } else {
    addLog(tr("Received API key."));
    validateKey(key);
  }
}

void NexusConnectionUI::onSSOStateChanged(NexusSSOLogin::States s, const QString& e)
{
  if (s != NexusSSOLogin::Finished) {
    // finished state is handled in onSSOKeyChanged()
    const auto log = NexusSSOLogin::stateToString(s, e);

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
  if (user) {
    NexusInterface::instance().setUserAccount(*user);
    addLog(tr("Received user account information"));

    if (setKey(user->apiKey())) {
      addLog(tr("Linked with Nexus successfully."));
    } else {
      addLog(tr("Failed to set API key"));
    }
  } else {
    if (message.isEmpty()) {
      // shouldn't happen
      addLog("Unknown error");
    } else {
      addLog(message);
    }
  }

  updateState();
}

void NexusConnectionUI::addLog(const QString& s)
{
  m_log->addItem(s);
  m_log->scrollToBottom();
}

bool NexusConnectionUI::setKey(const QString& key)
{
  const bool ret = GlobalSettings::setNexusApiKey(key);
  updateState();

  emit keyChanged();

  return ret;
}

bool NexusConnectionUI::clearKey()
{
  const auto ret = GlobalSettings::clearNexusApiKey();

  NexusInterface::instance().getAccessManager()->clearApiKey();
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
    // api key is in the process of being retrieved
    setButton(m_connect, true, QObject::tr("Cancel"));
    setButton(m_disconnect, false);
    setButton(m_manual, false, QObject::tr("Enter API Key Manually"));
  } else if (m_nexusValidator && m_nexusValidator->isActive()) {
    // api key is in the process of being tested
    setButton(m_connect, false, QObject::tr("Connect to Nexus"));
    setButton(m_disconnect, false);
    setButton(m_manual, true, QObject::tr("Cancel"));
  } else if (GlobalSettings::hasNexusApiKey()) {
    // api key is present
    setButton(m_connect, false, QObject::tr("Connect to Nexus"));
    setButton(m_disconnect, true);
    setButton(m_manual, false, QObject::tr("Enter API Key Manually"));
  } else {
    // api key not present
    setButton(m_connect, true, QObject::tr("Connect to Nexus"));
    setButton(m_disconnect, false);
    setButton(m_manual, true, QObject::tr("Enter API Key Manually"));
  }

  emit stateChanged();
}

NexusSettingsTab::NexusSettingsTab(Settings& s, SettingsDialog& d) : SettingsTab(s, d)
{
  ui->endorsementBox->setChecked(settings().nexus().endorsementIntegration());
  ui->trackedBox->setChecked(settings().nexus().trackedIntegration());
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
                                             ui->nexusDisconnect, ui->nexusManualKey,
                                             ui->nexusLog));

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
