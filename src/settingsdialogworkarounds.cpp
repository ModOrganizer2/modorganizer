#include "settingsdialogworkarounds.h"
#include "ui_settingsdialog.h"
#include "helper.h"
#include <iplugingame.h>

WorkaroundsSettingsTab::WorkaroundsSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{
  ui->appIDEdit->setText(settings().steam().appID());

  LoadMechanism::EMechanism mechanismID = settings().game().loadMechanismType();
  int index = 0;

  if (settings().game().loadMechanism().isDirectLoadingSupported()) {
    ui->mechanismBox->addItem(QObject::tr("Mod Organizer"), LoadMechanism::LOAD_MODORGANIZER);
    if (mechanismID == LoadMechanism::LOAD_MODORGANIZER) {
      index = ui->mechanismBox->count() - 1;
    }
  }

  ui->mechanismBox->setCurrentIndex(index);

  ui->hideUncheckedBox->setChecked(settings().game().hideUncheckedPlugins());
  ui->forceEnableBox->setChecked(settings().game().forceEnableCoreFiles());
  ui->displayForeignBox->setChecked(settings().interface().displayForeign());
  ui->lockGUIBox->setChecked(settings().interface().lockGUI());
  ui->enableArchiveParsingBox->setChecked(settings().archiveParsing());

  setExecutableBlacklist(settings().executablesBlacklist());

  QObject::connect(ui->bsaDateBtn, &QPushButton::clicked, [&]{ on_bsaDateBtn_clicked(); });
  QObject::connect(ui->execBlacklistBtn, &QPushButton::clicked, [&]{ on_execBlacklistBtn_clicked(); });
  QObject::connect(ui->resetGeometryBtn, &QPushButton::clicked, [&]{ on_resetGeometryBtn_clicked(); });
}

void WorkaroundsSettingsTab::update()
{
  if (ui->appIDEdit->text() != settings().game().plugin()->steamAPPId()) {
    settings().steam().setAppID(ui->appIDEdit->text());
  } else {
    settings().steam().setAppID("");
  }

  settings().game().setLoadMechanism(static_cast<LoadMechanism::EMechanism>(
    ui->mechanismBox->itemData(ui->mechanismBox->currentIndex()).toInt()));

  settings().game().setHideUncheckedPlugins(ui->hideUncheckedBox->isChecked());
  settings().game().setForceEnableCoreFiles(ui->forceEnableBox->isChecked());
  settings().interface().setDisplayForeign(ui->displayForeignBox->isChecked());
  settings().interface().setLockGUI(ui->lockGUIBox->isChecked());
  settings().setArchiveParsing(ui->enableArchiveParsingBox->isChecked());
  settings().setExecutablesBlacklist(getExecutableBlacklist());
}

void WorkaroundsSettingsTab::on_execBlacklistBtn_clicked()
{
  bool ok = false;
  QString result = QInputDialog::getMultiLineText(
    &dialog(),
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

  helper::backdateBSAs(qApp->applicationDirPath().toStdWString(),
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
    settings().geometry().requestReset();
    qApp->exit(INT_MAX);
  }
}
