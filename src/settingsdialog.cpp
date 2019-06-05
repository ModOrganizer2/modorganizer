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

#include "settingsdialog.h"

#include "ui_settingsdialog.h"
#include "ui_nexusmanualkey.h"
#include "categoriesdialog.h"
#include "helper.h"
#include "noeditdelegate.h"
#include "iplugingame.h"
#include "settings.h"
#include "instancemanager.h"
#include "nexusinterface.h"
#include "nxmaccessmanager.h"
#include "plugincontainer.h"

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <QDirIterator>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QColorDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QDesktopServices>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


using namespace MOBase;


class NexusManualKeyDialog : public QDialog
{
public:
  NexusManualKeyDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::NexusManualKeyDialog)
  {
    ui->setupUi(this);

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


SettingsDialog::SettingsDialog(PluginContainer *pluginContainer, Settings* settings, QWidget *parent)
  : TutorableDialog("SettingsDialog", parent)
  , ui(new Ui::SettingsDialog)
  , m_settings(settings)
  , m_PluginContainer(pluginContainer)
  , m_nexusLogin(new QWebSocket)
  , m_KeyReceived(false)
  , m_KeyCleared(false)
  , m_GeometriesReset(false)
{
  ui->setupUi(this);
  ui->pluginSettingsList->setStyleSheet("QTreeWidget::item {padding-right: 10px;}");

  QShortcut *delShortcut
      = new QShortcut(QKeySequence(Qt::Key_Delete), ui->pluginBlacklist);
  connect(delShortcut, SIGNAL(activated()), this, SLOT(deleteBlacklistItem()));
  connect(m_nexusLogin, SIGNAL(connected()), this, SLOT(dispatchLogin()));
  connect(m_nexusLogin, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(authError(QAbstractSocket::SocketError)));
  connect(m_nexusLogin, SIGNAL(textMessageReceived(const QString &)), this, SLOT(receiveApiKey(const QString &)));
  connect(m_nexusLogin, SIGNAL(disconnected()), this, SLOT(completeApiConnection()));
  m_loginTimer.callOnTimeout(this, &SettingsDialog::loginPing);

  updateNexusButtons();
}

SettingsDialog::~SettingsDialog()
{
  m_loginTimer.stop();
  m_nexusLogin->close();
  disconnect(this);
  delete ui;
}

QString SettingsDialog::getColoredButtonStyleSheet() const
{
  return QString("QPushButton {"
    "background-color: %1;"
    "color: %2;"
    "border: 1px solid;"
    "padding: 3px;"
    "}");
}

void SettingsDialog::setButtonColor(QPushButton *button, const QColor &color)
{
  button->setStyleSheet(
    QString("QPushButton {"
      "background-color: rgba(%1, %2, %3, %4);"
      "color: %5;"
      "border: 1px solid;"
      "padding: 3px;"
      "}")
    .arg(color.red())
    .arg(color.green())
    .arg(color.blue())
    .arg(color.alpha())
    .arg(Settings::getIdealTextColor(color).name())
    );
};

void SettingsDialog::accept()
{
  QString newModPath = ui->modDirEdit->text();
  newModPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  if ((QDir::fromNativeSeparators(newModPath) !=
       QDir::fromNativeSeparators(
           Settings::instance().getModDirectory(true))) &&
      (QMessageBox::question(
           nullptr, tr("Confirm"),
           tr("Changing the mod directory affects all your profiles! "
              "Mods not present (or named differently) in the new location "
              "will be disabled in all profiles. "
              "There is no way to undo this unless you backed up your "
              "profiles manually. Proceed?"),
           QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)) {
    return;
  }

  storeSettings(ui->pluginsList->currentItem());
  TutorableDialog::accept();
}

bool SettingsDialog::getResetGeometries()
{
  return ui->resetGeometryBtn->isChecked();
}

bool SettingsDialog::getApiKeyChanged()
{
  return m_KeyReceived || m_KeyCleared;
}

void SettingsDialog::on_categoriesBtn_clicked()
{
  CategoriesDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}

void SettingsDialog::on_execBlacklistBtn_clicked()
{
  bool ok = false;
  QString result = QInputDialog::getMultiLineText(
    this,
    tr("Executables Blacklist"),
    tr("Enter one executable per line to be blacklisted from the virtual file system.\n"
       "Mods and other virtualized files will not be visible to these executables and\n"
       "any executables launched by them.\n\n"
       "Example:\n"
       "    Chrome.exe\n"
       "    Firefox.exe"),
    m_ExecutableBlacklist.split(";").join("\n"),
    &ok
    );
  if (ok) {
    QStringList blacklist;
    for (auto exec : result.split("\n")) {
      if (exec.trimmed().endsWith(".exe", Qt::CaseInsensitive)) {
        blacklist << exec.trimmed();
      }
    }
    m_ExecutableBlacklist = blacklist.join(";");
  }
}

void SettingsDialog::on_bsaDateBtn_clicked()
{
  IPluginGame const *game
      = qApp->property("managed_game").value<IPluginGame *>();
  QDir dir = game->dataDirectory();

  Helper::backdateBSAs(qApp->applicationDirPath().toStdWString(),
                       dir.absolutePath().toStdWString());
}

void SettingsDialog::on_browseBaseDirBtn_clicked()
{
  QString temp = QFileDialog::getExistingDirectory(
      this, tr("Select base directory"), ui->baseDirEdit->text());
  if (!temp.isEmpty()) {
    ui->baseDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseDownloadDirBtn_clicked()
{
  QString searchPath = ui->downloadDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select download directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->downloadDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseModDirBtn_clicked()
{
  QString searchPath = ui->modDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select mod directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->modDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseCacheDirBtn_clicked()
{
  QString searchPath = ui->cacheDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select cache directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->cacheDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseProfilesDirBtn_clicked()
{
  QString searchPath = ui->profilesDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select profiles directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->profilesDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseOverwriteDirBtn_clicked()
{
  QString searchPath = ui->overwriteDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select overwrite directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->overwriteDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseGameDirBtn_clicked()
{
  QFileInfo oldGameExe(ui->managedGameDirEdit->text());

  QString temp = QFileDialog::getOpenFileName(this, tr("Select game executable"), oldGameExe.absolutePath(), oldGameExe.fileName());
  if (!temp.isEmpty()) {
    ui->managedGameDirEdit->setText(temp);
  }
}

void SettingsDialog::on_containsBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_ContainsColor, this, "Color Picker: Mod contains selected plugin", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_ContainsColor = result;
    setButtonColor(ui->containsBtn, result);
  }
}

void SettingsDialog::on_containedBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_ContainedColor, this, "ColorPicker: Plugin is Contained in selected Mod", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_ContainedColor = result;
    setButtonColor(ui->containedBtn, result);
  }
}

void SettingsDialog::on_overwrittenBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwrittenColor, this, "ColorPicker: Is overwritten (loose files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwrittenColor = result;
    setButtonColor(ui->overwrittenBtn, result);
  }
}

void SettingsDialog::on_overwritingBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwritingColor, this, "ColorPicker: Is overwriting (loose files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwritingColor = result;
    setButtonColor(ui->overwritingBtn, result);
  }
}

void SettingsDialog::on_overwrittenArchiveBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwrittenArchiveColor, this, "ColorPicker: Is overwritten (archive files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwrittenArchiveColor = result;
    setButtonColor(ui->overwrittenArchiveBtn, result);
  }
}

void SettingsDialog::on_overwritingArchiveBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwritingArchiveColor, this, "ColorPicker: Is overwriting (archive files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwritingArchiveColor = result;
    setButtonColor(ui->overwritingArchiveBtn, result);
  }
}

void SettingsDialog::on_resetColorsBtn_clicked()
{
  m_OverwritingColor = QColor(255, 0, 0, 64);
  m_OverwrittenColor = QColor(0, 255, 0, 64);
  m_OverwritingArchiveColor = QColor(255, 0, 255, 64);
  m_OverwrittenArchiveColor = QColor(0, 255, 255, 64);
  m_ContainsColor = QColor(0, 0, 255, 64);
  m_ContainedColor = QColor(0, 0, 255, 64);

  setButtonColor(ui->overwritingBtn, m_OverwritingColor);
  setButtonColor(ui->overwrittenBtn, m_OverwrittenColor);
  setButtonColor(ui->overwritingArchiveBtn, m_OverwritingArchiveColor);
  setButtonColor(ui->overwrittenArchiveBtn, m_OverwrittenArchiveColor);
  setButtonColor(ui->containsBtn, m_ContainsColor);
  setButtonColor(ui->containedBtn, m_ContainedColor);
}

void SettingsDialog::on_resetDialogsButton_clicked()
{
  if (QMessageBox::question(this, tr("Confirm?"),
          tr("This will make all dialogs show up again where you checked the \"Remember selection\"-box. Continue?"),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit resetDialogs();
  }
}

void SettingsDialog::on_nexusConnect_clicked()
{
  fetchNexusApiKey();
}

void SettingsDialog::on_nexusManualKey_clicked()
{
  NexusManualKeyDialog dialog(this);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const auto key = dialog.key();

  if (key.isEmpty()) {
    clearKey();
  } else {
    if (setKey(key)) {
      testApiKey();
    }
  }
}

void SettingsDialog::fetchNexusApiKey()
{
  QUrl url = QUrl("wss://sso.nexusmods.com");
  m_nexusLogin->open(url);
  updateNexusButtons();
}

void SettingsDialog::dispatchLogin()
{
  m_KeyReceived = false;
  QJsonObject login;
  if (m_UUID.isEmpty()) {
    boost::uuids::random_generator generator;
    boost::uuids::uuid sessionId = generator();
    m_UUID = boost::uuids::to_string(sessionId).c_str();
  }
  login.insert(QString("id"), QJsonValue(m_UUID));
  login.insert(QString("token"), QJsonValue(m_AuthToken));
  login.insert(QString("protocol"), 2);
  QJsonDocument loginDoc(login);
  QString finalMessage(loginDoc.toJson());
  m_nexusLogin->sendTextMessage(finalMessage);
  QDesktopServices::openUrl(QUrl(QString("https://www.nexusmods.com/sso?id=%1&application=%2").arg(m_UUID).arg("modorganizer2")));
  m_loginTimer.start(30000);
}

void SettingsDialog::loginPing()
{
  if (m_nexusLogin->isValid()) {
    m_nexusLogin->ping();
    m_totalPings++;
  }
  if (m_totalPings >= 60) {
    m_loginTimer.stop();
    m_totalPings = 0;
    m_nexusLogin->close(QWebSocketProtocol::CloseCodeGoingAway, "Timeout: No response received after thirty minutes. Cancelling request.");
  }
}

void SettingsDialog::authError(QAbstractSocket::SocketError error)
{
  auto errorInfo = m_nexusLogin->errorString();
  qCritical() << "An error occurred: " << errorInfo;
}

void SettingsDialog::receiveApiKey(const QString &response)
{
  QJsonDocument responseDoc = QJsonDocument::fromJson(response.toUtf8());
  QVariantMap responseData = responseDoc.object().toVariantMap();
  if (responseData["success"].toBool()) {
    QVariantMap data = responseData["data"].toMap();
    if (data.contains("connection_token")) {
      m_AuthToken = data["connection_token"].toString();
    } else {
      const auto key = data["api_key"].toString();

      m_nexusLogin->close();
      m_loginTimer.stop();
      m_totalPings = 0;

      if (key.isEmpty()) {
        clearKey();
      } else {
        setKey(key);
      }
    }
  } else {
    QString error("There was a problem with SSO initialization: %1");
    qCritical() << error.arg(responseData["error"].toString());
    m_nexusLogin->close();
  }
}

void SettingsDialog::completeApiConnection()
{
  if (!m_KeyReceived && !m_loginTimer.isActive()) {
    QMessageBox::warning(qApp->activeWindow(), tr("Error"),
      tr("Failed to retrieve a Nexus API key! Please try again. "
        "A browser window should open asking you to authorize."));

    // try again
    fetchNexusApiKey();
  }
}

bool SettingsDialog::setKey(const QString& key)
{
  m_KeyReceived = true;
  const bool ret = m_settings->setNexusApiKey(key);
  updateNexusButtons();
  return ret;
}

bool SettingsDialog::clearKey()
{
  m_KeyCleared = true;
  const auto ret = m_settings->clearNexusApiKey();
  updateNexusButtons();
  return ret;
}

void SettingsDialog::testApiKey()
{
  QString key;
  if (!m_settings->getNexusApiKey(key)) {
    qWarning().nospace() << "can't test API key, nothing stored";
    return;
  }

  auto* am = NexusInterface::instance(m_PluginContainer)->getAccessManager();
  am->apiCheck(key, true);
}

void SettingsDialog::updateNexusButtons()
{
  if (m_nexusLogin->state() != QAbstractSocket::UnconnectedState) {
    // api key is in the process of being retrieved
    ui->nexusConnect->setText("Connecting the API. Please login within the browser and accept the request. This will time out after 30 minutes.");
    ui->nexusConnect->setEnabled(false);
    ui->nexusDisconnect->setEnabled(false);
    ui->nexusManualKey->setEnabled(false);
  }
  else if (m_settings->hasNexusApiKey()) {
    // api key is present
    ui->nexusConnect->setText("Nexus API Key Stored");
    ui->nexusConnect->setEnabled(false);
    ui->nexusDisconnect->setEnabled(true);
    ui->nexusManualKey->setEnabled(false);
  } else {
    // api key not present
    ui->nexusConnect->setText("Connect to Nexus");
    ui->nexusConnect->setEnabled(true);
    ui->nexusDisconnect->setEnabled(false);
    ui->nexusManualKey->setEnabled(true);
  }
}

void SettingsDialog::storeSettings(QListWidgetItem *pluginItem)
{
  if (pluginItem != nullptr) {
    QVariantMap settings = pluginItem->data(Qt::UserRole + 1).toMap();

    for (int i = 0; i < ui->pluginSettingsList->topLevelItemCount(); ++i) {
      const QTreeWidgetItem *item = ui->pluginSettingsList->topLevelItem(i);
      settings[item->text(0)] = item->data(1, Qt::DisplayRole);
    }

    pluginItem->setData(Qt::UserRole + 1, settings);
  }
}

void SettingsDialog::on_pluginsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
  storeSettings(previous);

  ui->pluginSettingsList->clear();
  IPlugin *plugin = static_cast<IPlugin*>(current->data(Qt::UserRole).value<void*>());
  ui->authorLabel->setText(plugin->author());
  ui->versionLabel->setText(plugin->version().canonicalString());
  ui->descriptionLabel->setText(plugin->description());

  QVariantMap settings = current->data(Qt::UserRole + 1).toMap();
  QVariantMap descriptions = current->data(Qt::UserRole + 2).toMap();
  ui->pluginSettingsList->setEnabled(settings.count() != 0);
  for (auto iter = settings.begin(); iter != settings.end(); ++iter) {
    QTreeWidgetItem *newItem = new QTreeWidgetItem(QStringList(iter.key()));
    QVariant value = *iter;
    QString description;
    {
      auto descriptionIter = descriptions.find(iter.key());
      if (descriptionIter != descriptions.end()) {
        description = descriptionIter->toString();
      }
    }

    ui->pluginSettingsList->setItemDelegateForColumn(0, new NoEditDelegate());
    newItem->setData(1, Qt::DisplayRole, value);
    newItem->setData(1, Qt::EditRole, value);
    newItem->setToolTip(1, description);

    newItem->setFlags(newItem->flags() | Qt::ItemIsEditable);
    ui->pluginSettingsList->addTopLevelItem(newItem);
  }

  ui->pluginSettingsList->resizeColumnToContents(0);
  ui->pluginSettingsList->resizeColumnToContents(1);
}

void SettingsDialog::deleteBlacklistItem()
{
  ui->pluginBlacklist->takeItem(ui->pluginBlacklist->currentIndex().row());
}

void SettingsDialog::on_associateButton_clicked()
{
  Settings::instance().registerAsNXMHandler(true);
}

void SettingsDialog::on_clearCacheButton_clicked()
{
  QDir(Settings::instance().getCacheDirectory()).removeRecursively();
  NexusInterface::instance(m_PluginContainer)->clearCache();
}

void SettingsDialog::on_nexusDisconnect_clicked()
{
  clearKey();
}

void SettingsDialog::normalizePath(QLineEdit *lineEdit)
{
  QString text = lineEdit->text();
  while (text.endsWith('/') || text.endsWith('\\')) {
    text.chop(1);
  }
  lineEdit->setText(text);
}

void SettingsDialog::on_baseDirEdit_editingFinished()
{
  normalizePath(ui->baseDirEdit);
}

void SettingsDialog::on_downloadDirEdit_editingFinished()
{
  normalizePath(ui->downloadDirEdit);
}

void SettingsDialog::on_modDirEdit_editingFinished()
{
  normalizePath(ui->modDirEdit);
}

void SettingsDialog::on_cacheDirEdit_editingFinished()
{
  normalizePath(ui->cacheDirEdit);
}

void SettingsDialog::on_profilesDirEdit_editingFinished()
{
  normalizePath(ui->profilesDirEdit);
}

void SettingsDialog::on_overwriteDirEdit_editingFinished()
{
  normalizePath(ui->overwriteDirEdit);
}

void SettingsDialog::on_resetGeometryBtn_clicked()
{
  m_GeometriesReset = true;
  ui->resetGeometryBtn->setChecked(true);
}
