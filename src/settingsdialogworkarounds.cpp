#include "settingsdialogworkarounds.h"
#include "settings.h"
#include "spawn.h"
#include "ui_settingsdialog.h"
#include <iplugingame.h>
#include <log.h>
#include <report.h>

WorkaroundsSettingsTab::WorkaroundsSettingsTab(Settings& s, SettingsDialog& d)
    : SettingsTab(s, d)
{
  // options
  ui->forceEnableBox->setChecked(settings().game().forceEnableCoreFiles());
  ui->lockGUIBox->setChecked(settings().interface().lockGUI());
  ui->enableArchiveParsingBox->setChecked(settings().archiveParsing());

  // steam
  QString username, password;
  settings().steam().login(username, password);
  if (username.length() > 0)
    MOBase::log::getDefault().addToBlacklist(username.toStdString(), "STEAM_USERNAME");
  if (password.length() > 0)
    MOBase::log::getDefault().addToBlacklist(password.toStdString(), "STEAM_PASSWORD");

  ui->appIDEdit->setText(settings().steam().appID());
  ui->steamUserEdit->setText(username);
  ui->steamPassEdit->setText(password);

  // network
  ui->offlineBox->setChecked(settings().network().offlineMode());
  ui->proxyBox->setChecked(settings().network().useProxy());
  ui->useCustomBrowser->setChecked(settings().network().useCustomBrowser());
  ui->browserCommand->setText(settings().network().customBrowserCommand());

  // buttons
  m_ExecutableBlacklist = settings().executablesBlacklist();
  m_SkipFileSuffixes    = settings().skipFileSuffixes();
  m_SkipDirectories     = settings().skipDirectories();

  QObject::connect(ui->bsaDateBtn, &QPushButton::clicked, [&] {
    on_bsaDateBtn_clicked();
  });
  QObject::connect(ui->execBlacklistBtn, &QPushButton::clicked, [&] {
    on_execBlacklistBtn_clicked();
  });
  QObject::connect(ui->skipFileSuffixBtn, &QPushButton::clicked, [&] {
    on_skipFileSuffixBtn_clicked();
  });
  QObject::connect(ui->skipDirectoriesBtn, &QPushButton::clicked, [&] {
    on_skipDirectoriesBtn_clicked();
  });
  QObject::connect(ui->resetGeometryBtn, &QPushButton::clicked, [&] {
    on_resetGeometryBtn_clicked();
  });
}

void WorkaroundsSettingsTab::update()
{
  // options
  settings().game().setForceEnableCoreFiles(ui->forceEnableBox->isChecked());
  settings().interface().setLockGUI(ui->lockGUIBox->isChecked());
  settings().setArchiveParsing(ui->enableArchiveParsingBox->isChecked());

  // steam
  if (ui->appIDEdit->text() != settings().game().plugin()->steamAPPId()) {
    settings().steam().setAppID(ui->appIDEdit->text());
  } else {
    settings().steam().setAppID("");
  }
  settings().steam().setLogin(ui->steamUserEdit->text(), ui->steamPassEdit->text());

  // network
  settings().network().setOfflineMode(ui->offlineBox->isChecked());
  settings().network().setUseProxy(ui->proxyBox->isChecked());
  settings().network().setUseCustomBrowser(ui->useCustomBrowser->isChecked());
  settings().network().setCustomBrowserCommand(ui->browserCommand->text());

  // buttons
  settings().setExecutablesBlacklist(m_ExecutableBlacklist);
  settings().setSkipFileSuffixes(m_SkipFileSuffixes);
  settings().setSkipDirectories(m_SkipDirectories);
}

bool WorkaroundsSettingsTab::changeBlacklistNow(QWidget* parent, Settings& settings)
{
  const auto current = settings.executablesBlacklist();

  if (auto s = changeBlacklistLater(parent, current)) {
    settings.setExecutablesBlacklist(*s);
    return true;
  }

  return false;
}

std::optional<QString>
WorkaroundsSettingsTab::changeBlacklistLater(QWidget* parent, const QString& current)
{
  bool ok = false;

  QString result = QInputDialog::getMultiLineText(
      parent, QObject::tr("Executables Blacklist"),
      QObject::tr("Enter one executable per line to be blacklisted from the virtual "
                  "file system.\n"
                  "Mods and other virtualized files will not be visible to these "
                  "executables and\n"
                  "any executables launched by them.\n\n"
                  "Example:\n"
                  "    Chrome.exe\n"
                  "    Firefox.exe"),
      current.split(";").join("\n"), &ok);

  if (!ok) {
    return {};
  }

  QStringList blacklist;
  for (auto exec : result.split("\n")) {
    if (exec.trimmed().endsWith(".exe", Qt::CaseInsensitive)) {
      blacklist << exec.trimmed();
    }
  }

  return blacklist.join(";");
}

std::optional<QString>
WorkaroundsSettingsTab::changeSkipFileSuffixes(QWidget* parent, const QString& current)
{
  bool ok = false;

  QString result = QInputDialog::getMultiLineText(
      parent, QObject::tr("Skip File Suffixes"),
      QObject::tr(
          "Enter one file suffix per line to be skipped / ignored from the virtual "
          "file system.\n"
          "Not to be confused with file extensions, file suffixes are simply how the "
          "filename ends.\n\n"
          "Example:\n"
          "  .txt - Would skip all files that end with .txt, <any text>.txt\n"
          "  some_file.txt - Would skip all files that end with some_file.txt, <any text>some_file.txt"),
      current.split(";").join("\n"), &ok);

  if (!ok) {
    return {};
  }

  QStringList fileSuffixes;
  for (auto& suffix : result.split("\n")) {
    fileSuffixes << suffix.trimmed();
  }

  return fileSuffixes.join(";");
}

std::optional<QString>
WorkaroundsSettingsTab::changeSkipDirectories(QWidget* parent, const QString& current)
{
  bool ok = false;

  QString result = QInputDialog::getMultiLineText(
      parent, QObject::tr("Skip Directories"),
      QObject::tr(
          "Enter one directory per line to be skipped / ignored from the virtual "
          "file system.\n\n"
          "Example:\n"
          "  .git\n"
          "  instructions"),
      current.split(";").join("\n"), &ok);

  if (!ok) {
    return {};
  }

  QStringList directories;
  for (auto& dir : result.split("\n")) {
    directories << dir.trimmed();
  }

  return directories.join(";");
}

void WorkaroundsSettingsTab::on_execBlacklistBtn_clicked()
{
  if (auto s = changeBlacklistLater(parentWidget(), m_ExecutableBlacklist)) {
    m_ExecutableBlacklist = *s;
  }
}

void WorkaroundsSettingsTab::on_skipFileSuffixBtn_clicked() 
{
  if (auto s = changeSkipFileSuffixes(parentWidget(), m_SkipFileSuffixes)) {
    m_SkipFileSuffixes = *s;
  }
}

void WorkaroundsSettingsTab::on_skipDirectoriesBtn_clicked() 
{
  if (auto s = changeSkipDirectories(parentWidget(), m_SkipDirectories)) {
    m_SkipDirectories = *s;
  }
}

void WorkaroundsSettingsTab::on_bsaDateBtn_clicked()
{
  const auto* game = qApp->property("managed_game").value<MOBase::IPluginGame*>();
  QDir dir         = game->dataDirectory();

  helper::backdateBSAs(parentWidget(), qApp->applicationDirPath().toStdWString(),
                       dir.absolutePath().toStdWString());
}

void WorkaroundsSettingsTab::on_resetGeometryBtn_clicked()
{
  const auto r =
      MOBase::TaskDialog(parentWidget())
          .title(QObject::tr("Restart Mod Organizer"))
          .main(QObject::tr("Restart Mod Organizer"))
          .content(QObject::tr("Geometries will be reset to their default values."))
          .icon(QMessageBox::Question)
          .button({QObject::tr("Restart Mod Organizer"), QMessageBox::Ok})
          .button({QObject::tr("Cancel"), QMessageBox::Cancel})
          .exec();

  if (r == QMessageBox::Ok) {
    settings().geometry().requestReset();
    ExitModOrganizer(Exit::Restart);
    dialog().close();
  }
}
