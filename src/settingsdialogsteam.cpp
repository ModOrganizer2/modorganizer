#include "settingsdialogsteam.h"
#include "ui_settingsdialog.h"

SteamSettingsTab::SteamSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{
  QString username, password;
  settings().steam().login(username, password);

  ui->steamUserEdit->setText(username);
  ui->steamPassEdit->setText(password);
}

void SteamSettingsTab::update()
{
  settings().steam().setLogin(ui->steamUserEdit->text(), ui->steamPassEdit->text());
}
