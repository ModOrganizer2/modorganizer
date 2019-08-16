#include "settingsdialogworkarounds.h"
#include "ui_settingsdialog.h"
#include "helper.h"
#include <iplugingame.h>

WorkaroundsSettingsTab::WorkaroundsSettingsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
{
  ui->appIDEdit->setText(m_parent->getSteamAppID());

  LoadMechanism::EMechanism mechanismID = m_parent->getLoadMechanism();
  int index = 0;

  if (m_parent->loadMechanism().isDirectLoadingSupported()) {
    ui->mechanismBox->addItem(QObject::tr("Mod Organizer"), LoadMechanism::LOAD_MODORGANIZER);
    if (mechanismID == LoadMechanism::LOAD_MODORGANIZER) {
      index = ui->mechanismBox->count() - 1;
    }
  }

  ui->mechanismBox->setCurrentIndex(index);

  ui->hideUncheckedBox->setChecked(m_parent->hideUncheckedPlugins());
  ui->forceEnableBox->setChecked(m_parent->forceEnableCoreFiles());
  ui->displayForeignBox->setChecked(m_parent->displayForeign());
  ui->lockGUIBox->setChecked(m_parent->lockGUI());
  ui->enableArchiveParsingBox->setChecked(m_parent->archiveParsing());

  setExecutableBlacklist(m_parent->executablesBlacklist());

  QObject::connect(ui->bsaDateBtn, &QPushButton::clicked, [&]{ on_bsaDateBtn_clicked(); });
  QObject::connect(ui->execBlacklistBtn, &QPushButton::clicked, [&]{ on_execBlacklistBtn_clicked(); });
  QObject::connect(ui->resetGeometryBtn, &QPushButton::clicked, [&]{ on_resetGeometryBtn_clicked(); });
}

void WorkaroundsSettingsTab::update()
{
  if (ui->appIDEdit->text() != m_parent->gamePlugin()->steamAPPId()) {
    m_Settings.setValue("Settings/app_id", ui->appIDEdit->text());
  } else {
    m_Settings.remove("Settings/app_id");
  }
  m_Settings.setValue("Settings/load_mechanism", ui->mechanismBox->itemData(ui->mechanismBox->currentIndex()).toInt());
  m_Settings.setValue("Settings/hide_unchecked_plugins", ui->hideUncheckedBox->isChecked());
  m_Settings.setValue("Settings/force_enable_core_files", ui->forceEnableBox->isChecked());
  m_Settings.setValue("Settings/display_foreign", ui->displayForeignBox->isChecked());
  m_Settings.setValue("Settings/lock_gui", ui->lockGUIBox->isChecked());
  m_Settings.setValue("Settings/archive_parsing_experimental", ui->enableArchiveParsingBox->isChecked());

  m_Settings.setValue("Settings/executable_blacklist", getExecutableBlacklist());
}

void WorkaroundsSettingsTab::on_execBlacklistBtn_clicked()
{
  bool ok = false;
  QString result = QInputDialog::getMultiLineText(
    parentWidget(),
    QObject::tr("Executables Blacklist"),
    QObject::tr("Enter one executable per line to be blacklisted from the virtual file system.\n"
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

void WorkaroundsSettingsTab::on_bsaDateBtn_clicked()
{
  const auto* game = qApp->property("managed_game").value<MOBase::IPluginGame*>();
  QDir dir = game->dataDirectory();

  Helper::backdateBSAs(qApp->applicationDirPath().toStdWString(),
    dir.absolutePath().toStdWString());
}

void WorkaroundsSettingsTab::on_resetGeometryBtn_clicked()
{
  const auto caption = QObject::tr("Restart Mod Organizer?");
  const auto text = QObject::tr(
    "In order to reset the geometry, Mod Organizer must be restarted.\n"
    "Restart now?");

  const auto res = QMessageBox::question(
    nullptr, caption, text, QMessageBox::Yes | QMessageBox::Cancel);

  if (res == QMessageBox::Yes) {
    m_parent->geometry().requestReset();
    qApp->exit(INT_MAX);
  }
}
