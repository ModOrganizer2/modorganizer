#include "settingsdialogdiagnostics.h"
#include "ui_settingsdialog.h"
#include "appconfig.h"
#include "organizercore.h"

DiagnosticsSettingsTab::DiagnosticsSettingsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
{
  ui->logLevelBox->setCurrentIndex(m_parent->logLevel());
  ui->dumpsTypeBox->setCurrentIndex(m_parent->crashDumpsType());
  ui->dumpsMaxEdit->setValue(m_parent->crashDumpsMax());
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

void DiagnosticsSettingsTab::update()
{
  m_Settings.setValue("Settings/log_level", ui->logLevelBox->currentIndex());
  m_Settings.setValue("Settings/crash_dumps_type", ui->dumpsTypeBox->currentIndex());
  m_Settings.setValue("Settings/crash_dumps_max", ui->dumpsMaxEdit->value());
}
