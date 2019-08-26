#include "settingsdialognexus.h"
#include "ui_settingsdialog.h"
#include "ui_nexusmanualkey.h"
#include "nexusinterface.h"
#include "serverinfo.h"
#include "log.h"
#include <utility.h>

using namespace MOBase;

template <typename T>
class ServerItem : public QListWidgetItem {
public:
  ServerItem(const QString &text, int sortRole = Qt::DisplayRole, QListWidget *parent = 0, int type = Type)
    : QListWidgetItem(text, parent, type), m_SortRole(sortRole) {}

  virtual bool operator< ( const QListWidgetItem & other ) const {
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

    connect(ui->openBrowser, &QPushButton::clicked, [&]{ openBrowser(); });
    connect(ui->paste, &QPushButton::clicked, [&]{ paste(); });
    connect(ui->clear, &QPushButton::clicked, [&]{ clear(); });
  }

  void accept() override
  {
    m_key = ui->key->toPlainText();
    QDialog::accept();
  }

  const QString& key() const
  {
    return m_key;
  }

  void openBrowser()
  {
    shell::OpenLink(QUrl("https://www.nexusmods.com/users/myaccount?tab=api"));
  }

  void paste()
  {
    const auto text = QApplication::clipboard()->text();
    if (!text.isEmpty()) {
      ui->key->setPlainText(text);
    }
  }

  void clear()
  {
    ui->key->clear();
  }

private:
  std::unique_ptr<Ui::NexusManualKeyDialog> ui;
  QString m_key;
};


NexusSettingsTab::NexusSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{
  ui->offlineBox->setChecked(settings().offlineMode());
  ui->proxyBox->setChecked(settings().useProxy());
  ui->endorsementBox->setChecked(settings().endorsementIntegration());
  ui->hideAPICounterBox->setChecked(settings().hideAPICounter());

  // display server preferences
  for (const auto& server : s.getServers()) {
    QString descriptor = server.name();

    if (!descriptor.compare("CDN", Qt::CaseInsensitive)) {
      descriptor += QStringLiteral(" (automatic)");
    }

    const auto averageSpeed = server.averageSpeed();
    if (averageSpeed > 0) {
      descriptor += QString(" (%1 kbps)").arg(averageSpeed / 1024);
    }

    QListWidgetItem *newItem = new ServerItem<int>(descriptor, Qt::UserRole + 1);

    newItem->setData(Qt::UserRole, server.name());
    newItem->setData(Qt::UserRole + 1, server.preferred());

    if (server.preferred() > 0) {
      ui->preferredServersList->addItem(newItem);
    } else {
      ui->knownServersList->addItem(newItem);
    }

    ui->preferredServersList->sortItems(Qt::DescendingOrder);
  }

  QObject::connect(ui->nexusConnect, &QPushButton::clicked, [&]{ on_nexusConnect_clicked(); });
  QObject::connect(ui->nexusManualKey, &QPushButton::clicked, [&]{ on_nexusManualKey_clicked(); });
  QObject::connect(ui->nexusDisconnect, &QPushButton::clicked, [&]{ on_nexusDisconnect_clicked(); });
  QObject::connect(ui->clearCacheButton, &QPushButton::clicked, [&]{ on_clearCacheButton_clicked(); });
  QObject::connect(ui->associateButton, &QPushButton::clicked, [&]{ on_associateButton_clicked(); });

  updateNexusState();
}

void NexusSettingsTab::update()
{
  settings().setOfflineMode(ui->offlineBox->isChecked());
  settings().setUseProxy(ui->proxyBox->isChecked());
  settings().setEndorsementIntegration(ui->endorsementBox->isChecked());
  settings().setHideAPICounter(ui->hideAPICounterBox->isChecked());

  auto servers = settings().getServers();

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
    const QString key = ui->preferredServersList->item(i)->data(Qt::UserRole).toString();
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
      log::error(
        "while setting preference to {}, server '{}' not found",
        newPreferred, key);
    }
  }

  settings().updateServers(servers);
}

void NexusSettingsTab::on_nexusConnect_clicked()
{
  if (m_nexusLogin && m_nexusLogin->isActive()) {
    m_nexusLogin->cancel();
    return;
  }

  if (!m_nexusLogin) {
    m_nexusLogin.reset(new NexusSSOLogin);

    m_nexusLogin->keyChanged = [&](auto&& s){
      onSSOKeyChanged(s);
    };

    m_nexusLogin->stateChanged = [&](auto&& s, auto&& e){
      onSSOStateChanged(s, e);
    };
  }

  ui->nexusLog->clear();
  m_nexusLogin->start();
  updateNexusState();
}

void NexusSettingsTab::on_nexusManualKey_clicked()
{
  if (m_nexusValidator && m_nexusValidator->isActive()) {
    m_nexusValidator->cancel();
    return;
  }

  NexusManualKeyDialog d(&dialog());
  if (d.exec() != QDialog::Accepted) {
    return;
  }

  const auto key = d.key();
  if (key.isEmpty()) {
    clearKey();
    return;
  }

  ui->nexusLog->clear();
  validateKey(key);
}

void NexusSettingsTab::on_nexusDisconnect_clicked()
{
  clearKey();
  ui->nexusLog->clear();
  addNexusLog(QObject::tr("Disconnected."));
}

void NexusSettingsTab::on_clearCacheButton_clicked()
{
  QDir(Settings::instance().getCacheDirectory()).removeRecursively();
  NexusInterface::instance(dialog().m_PluginContainer)->clearCache();
}

void NexusSettingsTab::on_associateButton_clicked()
{
  Settings::instance().registerAsNXMHandler(true);
}

void NexusSettingsTab::validateKey(const QString& key)
{
  if (!m_nexusValidator) {
    m_nexusValidator.reset(new NexusKeyValidator(
      *NexusInterface::instance(dialog().m_PluginContainer)->getAccessManager()));

    m_nexusValidator->stateChanged = [&](auto&& s, auto&& e){
      onValidatorStateChanged(s, e);
    };

    m_nexusValidator->finished = [&](auto&& user) {
      onValidatorFinished(user);
    };
  }

  addNexusLog(QObject::tr("Checking API key..."));
  m_nexusValidator->start(key);
}

void NexusSettingsTab::onSSOKeyChanged(const QString& key)
{
  if (key.isEmpty()) {
    clearKey();
  } else {
    addNexusLog(QObject::tr("Received API key."));
    validateKey(key);
  }
}

void NexusSettingsTab::onSSOStateChanged(NexusSSOLogin::States s, const QString& e)
{
  if (s != NexusSSOLogin::Finished) {
    // finished state is handled in onSSOKeyChanged()
    const auto log = NexusSSOLogin::stateToString(s, e);

    for (auto&& line : log.split("\n")) {
      addNexusLog(line);
    }
  }

  updateNexusState();
}

void NexusSettingsTab::onValidatorStateChanged(
  NexusKeyValidator::States s, const QString& e)
{
  if (s != NexusKeyValidator::Finished) {
    // finished state is handled in onValidatorFinished()
    const auto log = NexusKeyValidator::stateToString(s, e);

    for (auto&& line : log.split("\n")) {
      addNexusLog(line);
    }
  }

  updateNexusState();
}

void NexusSettingsTab::onValidatorFinished(const APIUserAccount& user)
{
  NexusInterface::instance(dialog().m_PluginContainer)->setUserAccount(user);

  if (!user.apiKey().isEmpty()) {
    if (setKey(user.apiKey())) {
      addNexusLog(QObject::tr("Linked with Nexus successfully."));
    }
  }
}

void NexusSettingsTab::addNexusLog(const QString& s)
{
  ui->nexusLog->addItem(s);
  ui->nexusLog->scrollToBottom();
}

bool NexusSettingsTab::setKey(const QString& key)
{
  dialog().m_keyChanged = true;
  const bool ret = settings().setNexusApiKey(key);
  updateNexusState();
  return ret;
}

bool NexusSettingsTab::clearKey()
{
  dialog().m_keyChanged = true;
  const auto ret = settings().clearNexusApiKey();

  NexusInterface::instance(dialog().m_PluginContainer)->getAccessManager()->clearApiKey();
  updateNexusState();

  return ret;
}

void NexusSettingsTab::updateNexusState()
{
  updateNexusButtons();
  updateNexusData();
}

void NexusSettingsTab::updateNexusButtons()
{
  if (m_nexusLogin && m_nexusLogin->isActive()) {
    // api key is in the process of being retrieved
    ui->nexusConnect->setText(QObject::tr("Cancel"));
    ui->nexusConnect->setEnabled(true);
    ui->nexusDisconnect->setEnabled(false);
    ui->nexusManualKey->setText(QObject::tr("Enter API Key Manually"));
    ui->nexusManualKey->setEnabled(false);
  }
  else if (m_nexusValidator && m_nexusValidator->isActive()) {
    // api key is in the process of being tested
    ui->nexusConnect->setText(QObject::tr("Connect to Nexus"));
    ui->nexusConnect->setEnabled(false);
    ui->nexusDisconnect->setEnabled(false);
    ui->nexusManualKey->setText(QObject::tr("Cancel"));
    ui->nexusManualKey->setEnabled(true);
  }
  else if (settings().hasNexusApiKey()) {
    // api key is present
    ui->nexusConnect->setText(QObject::tr("Connect to Nexus"));
    ui->nexusConnect->setEnabled(false);
    ui->nexusDisconnect->setEnabled(true);
    ui->nexusManualKey->setText(QObject::tr("Enter API Key Manually"));
    ui->nexusManualKey->setEnabled(false);
  } else {
    // api key not present
    ui->nexusConnect->setText(QObject::tr("Connect to Nexus"));
    ui->nexusConnect->setEnabled(true);
    ui->nexusDisconnect->setEnabled(false);
    ui->nexusManualKey->setText(QObject::tr("Enter API Key Manually"));
    ui->nexusManualKey->setEnabled(true);
  }
}

void NexusSettingsTab::updateNexusData()
{
  const auto user = NexusInterface::instance(dialog().m_PluginContainer)
    ->getAPIUserAccount();

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
