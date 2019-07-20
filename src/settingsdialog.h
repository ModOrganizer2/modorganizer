/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "tutorabledialog.h"
#include "nxmaccessmanager.h"
#include <iplugin.h>
#include <QListWidgetItem>

class PluginContainer;
class Settings;

namespace Ui {
    class SettingsDialog;
}


/**
 * dialog used to change settings for Mod Organizer. On top of the
 * settings managed by the "Settings" class, this offers a button to open the
 * CategoriesDialog
 **/
class SettingsDialog : public MOBase::TutorableDialog
{
    Q_OBJECT

public:
  explicit SettingsDialog(
    PluginContainer *pluginContainer, Settings* settings, QWidget *parent = 0);

  ~SettingsDialog();

  /**
  * @brief get stylesheet of settings buttons with colored background
  * @return string of stylesheet
  */
  QString getColoredButtonStyleSheet() const;

  Ui::SettingsDialog *ui;

public slots:

  virtual void accept();

signals:

  void retryApiConnection();

private:

  void storeSettings(QListWidgetItem *pluginItem);
  void normalizePath(QLineEdit *lineEdit);

public:
  QString getExecutableBlacklist() { return m_ExecutableBlacklist; }
  void setExecutableBlacklist(QString blacklist) { m_ExecutableBlacklist = blacklist; }
  bool getResetGeometries();
  bool getApiKeyChanged();

private slots:
  void on_associateButton_clicked();
  void on_baseDirEdit_editingFinished();
  void on_browseBaseDirBtn_clicked();
  void on_browseCacheDirBtn_clicked();
  void on_browseDownloadDirBtn_clicked();
  void on_browseGameDirBtn_clicked();
  void on_browseModDirBtn_clicked();
  void on_browseOverwriteDirBtn_clicked();
  void on_browseProfilesDirBtn_clicked();
  void on_bsaDateBtn_clicked();
  void on_cacheDirEdit_editingFinished();
  void on_clearCacheButton_clicked();
  void on_downloadDirEdit_editingFinished();
  void on_execBlacklistBtn_clicked();
  void on_modDirEdit_editingFinished();
  void on_nexusConnect_clicked();
  void on_nexusDisconnect_clicked();
  void on_nexusManualKey_clicked();
  void on_overwriteDirEdit_editingFinished();
  void on_pluginsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_profilesDirEdit_editingFinished();
  void on_resetGeometryBtn_clicked();

  void deleteBlacklistItem();

private:
  Settings* m_settings;
  PluginContainer *m_PluginContainer;

  bool m_GeometriesReset;
  bool m_keyChanged;

  QString m_ExecutableBlacklist;
  std::unique_ptr<NexusSSOLogin> m_nexusLogin;
  std::unique_ptr<NexusKeyValidator> m_nexusValidator;

  void validateKey(const QString& key);
  bool setKey(const QString& key);
  bool clearKey();

  void updateNexusState();
  void updateNexusButtons();
  void updateNexusData();

  void onSSOKeyChanged(const QString& key);
  void onSSOStateChanged(NexusSSOLogin::States s, const QString& e);

  void onValidatorStateChanged(NexusKeyValidator::States s, const QString& e);
  void onValidatorFinished(const APIUserAccount& user);

  void addNexusLog(const QString& s);
};

#endif // SETTINGSDIALOG_H
