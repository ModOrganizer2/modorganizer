#include "settingsdialogdiagnostics.h"
#include "ui_settingsdialog.h"
#include "appconfig.h"
#include "organizercore.h"
#include <log.h>

using namespace MOBase;

DiagnosticsSettingsTab::DiagnosticsSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{
  setLevelsBox();
  setCrashDumpTypesBox();

  ui->dumpsMaxEdit->setValue(settings().diagnostics().crashDumpsMax());

  QString logsPath = qApp->property("dataPath").toString()
    + "/" + QString::fromStdWString(AppConfig::logPath());

  ui->diagnosticsExplainedLabel->setText(
    ui->diagnosticsExplainedLabel->text()
    .replace("LOGS_FULL_PATH", logsPath)
    .replace("LOGS_DIR", QString::fromStdWString(AppConfig::logPath()))
    .replace("DUMPS_FULL_PATH", QString::fromStdWString(OrganizerCore::crashDumpsPath()))
    .replace("DUMPS_DIR", QString::fromStdWString(AppConfig::dumpsDir()))
  );
}

void DiagnosticsSettingsTab::setLevelsBox()
{
  ui->logLevelBox->clear();

  ui->logLevelBox->addItem(QObject::tr("Debug"), log::Debug);
  ui->logLevelBox->addItem(QObject::tr("Info (recommended)"), log::Info);
  ui->logLevelBox->addItem(QObject::tr("Warning"), log::Warning);
  ui->logLevelBox->addItem(QObject::tr("Error"), log::Error);

  for (int i=0; i<ui->logLevelBox->count(); ++i) {
    if (ui->logLevelBox->itemData(i) == settings().diagnostics().logLevel()) {
      ui->logLevelBox->setCurrentIndex(i);
      break;
    }
  }
}

void DiagnosticsSettingsTab::setCrashDumpTypesBox()
{
  ui->dumpsTypeBox->clear();

  auto add = [&](auto&& text, auto&& type) {
    ui->dumpsTypeBox->addItem(text, static_cast<int>(type));
  };

  add(QObject::tr("None"), CrashDumpsType::None);
  add(QObject::tr("Mini (recommended)"), CrashDumpsType::Mini);
  add(QObject::tr("Data"), CrashDumpsType::Data);
  add(QObject::tr("Full"), CrashDumpsType::Full);

  const auto current = static_cast<int>(
    settings().diagnostics().crashDumpsType());

  for (int i=0; i<ui->dumpsTypeBox->count(); ++i) {
    if (ui->dumpsTypeBox->itemData(i) == current) {
      ui->dumpsTypeBox->setCurrentIndex(i);
      break;
    }
  }
}

void DiagnosticsSettingsTab::update()
{
  settings().diagnostics().setLogLevel(
    static_cast<log::Levels>(ui->logLevelBox->currentData().toInt()));

  settings().diagnostics().setCrashDumpsType(
    static_cast<CrashDumpsType>(ui->dumpsTypeBox->currentData().toInt()));

  settings().diagnostics().setCrashDumpsMax(ui->dumpsMaxEdit->value());
}
