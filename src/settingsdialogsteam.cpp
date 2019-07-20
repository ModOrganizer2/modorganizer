#include "settingsdialogsteam.h"
#include "ui_settingsdialog.h"

SteamSettingsTab::SteamSettingsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
{
  QString username, password;
  m_parent->getSteamLogin(username, password);

  ui->steamUserEdit->setText(username);
  ui->steamPassEdit->setText(password);
}

void SteamSettingsTab::update()
{
  m_parent->setSteamLogin(ui->steamUserEdit->text(), ui->steamPassEdit->text());
}
