#include "settingsdialogsteam.h"
#include "ui_settingsdialog.h"

SteamSettingsTab::SteamSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{
  QString username, password;
  settings().getSteamLogin(username, password);

  ui->steamUserEdit->setText(username);
  ui->steamPassEdit->setText(password);
}

void SteamSettingsTab::update()
{
  settings().setSteamLogin(ui->steamUserEdit->text(), ui->steamPassEdit->text());
}
