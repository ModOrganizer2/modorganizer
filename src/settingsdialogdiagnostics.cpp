#include "settingsdialogdiagnostics.h"
#include "ui_settingsdialog.h"
#include "appconfig.h"
#include "organizercore.h"
#include <log.h>

using namespace MOBase;

DiagnosticsSettingsTab::DiagnosticsSettingsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
{
  setLevelsBox();
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

void DiagnosticsSettingsTab::setLevelsBox()
{
  ui->logLevelBox->clear();

  ui->logLevelBox->addItem(QObject::tr("Debug"), log::Debug);
  ui->logLevelBox->addItem(QObject::tr("Info (recommended)"), log::Info);
  ui->logLevelBox->addItem(QObject::tr("Warning"), log::Warning);
  ui->logLevelBox->addItem(QObject::tr("Error"), log::Error);

  for (int i=0; i<ui->logLevelBox->count(); ++i) {
    if (ui->logLevelBox->itemData(i) == m_parent->logLevel()) {
      ui->logLevelBox->setCurrentIndex(i);
      break;
    }
  }
}

void DiagnosticsSettingsTab::update()
{
  m_Settings.setValue("Settings/log_level", ui->logLevelBox->currentData().toInt());
  m_Settings.setValue("Settings/crash_dumps_type", ui->dumpsTypeBox->currentIndex());
  m_Settings.setValue("Settings/crash_dumps_max", ui->dumpsMaxEdit->value());
}
