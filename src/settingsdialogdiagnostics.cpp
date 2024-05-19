#include "settingsdialogdiagnostics.h"
#include "organizercore.h"
#include "shared/appconfig.h"
#include "ui_settingsdialog.h"
#include <log.h>

using namespace MOBase;

DiagnosticsSettingsTab::DiagnosticsSettingsTab(Settings& s, SettingsDialog& d)
    : SettingsTab(s, d)
{
  setLogLevel();
  setLootLogLevel();
  setCrashDumpTypesBox();

  ui->dumpsMaxEdit->setValue(settings().diagnostics().maxCoreDumps());

  QString logsPath = QUrl::fromLocalFile(qApp->property("dataPath").toString() + "/" +
                                         QString::fromStdWString(AppConfig::logPath()))
                         .toString();

  ui->diagnosticsExplainedLabel->setText(
      ui->diagnosticsExplainedLabel->text()
          .replace("LOGS_FULL_PATH", logsPath)
          .replace("LOGS_DIR", QString::fromStdWString(AppConfig::logPath()))
          .replace("DUMPS_FULL_PATH",
                   QUrl::fromLocalFile(
                       QString::fromStdWString(OrganizerCore::getGlobalCoreDumpPath()))
                       .toString())
          .replace("DUMPS_DIR", QString::fromStdWString(AppConfig::dumpsDir())));
}

void DiagnosticsSettingsTab::setLogLevel()
{
  ui->logLevelBox->clear();

  ui->logLevelBox->addItem(QObject::tr("Debug"), log::Debug);
  ui->logLevelBox->addItem(QObject::tr("Info (recommended)"), log::Info);
  ui->logLevelBox->addItem(QObject::tr("Warning"), log::Warning);
  ui->logLevelBox->addItem(QObject::tr("Error"), log::Error);

  const auto sel = settings().diagnostics().logLevel();

  for (int i = 0; i < ui->logLevelBox->count(); ++i) {
    if (ui->logLevelBox->itemData(i) == sel) {
      ui->logLevelBox->setCurrentIndex(i);
      break;
    }
  }
}

void DiagnosticsSettingsTab::setLootLogLevel()
{
  using L = lootcli::LogLevels;

  auto v = [](L level) {
    return QVariant(static_cast<int>(level));
  };

  ui->lootLogLevel->clear();

  ui->lootLogLevel->addItem(QObject::tr("Trace"), v(L::Trace));
  ui->lootLogLevel->addItem(QObject::tr("Debug"), v(L::Debug));
  ui->lootLogLevel->addItem(QObject::tr("Info (recommended)"), v(L::Info));
  ui->lootLogLevel->addItem(QObject::tr("Warning"), v(L::Warning));
  ui->lootLogLevel->addItem(QObject::tr("Error"), v(L::Error));

  const auto sel = settings().diagnostics().lootLogLevel();

  for (int i = 0; i < ui->lootLogLevel->count(); ++i) {
    if (ui->lootLogLevel->itemData(i) == v(sel)) {
      ui->lootLogLevel->setCurrentIndex(i);
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

  add(QObject::tr("None"), env::CoreDumpTypes::None);
  add(QObject::tr("Mini (recommended)"), env::CoreDumpTypes::Mini);
  add(QObject::tr("Data"), env::CoreDumpTypes::Data);
  add(QObject::tr("Full"), env::CoreDumpTypes::Full);

  const auto current = static_cast<int>(settings().diagnostics().coreDumpType());

  for (int i = 0; i < ui->dumpsTypeBox->count(); ++i) {
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

  settings().diagnostics().setCoreDumpType(
      static_cast<env::CoreDumpTypes>(ui->dumpsTypeBox->currentData().toInt()));

  settings().diagnostics().setMaxCoreDumps(ui->dumpsMaxEdit->value());

  settings().diagnostics().setLootLogLevel(
      static_cast<lootcli::LogLevels>(ui->lootLogLevel->currentData().toInt()));
}
