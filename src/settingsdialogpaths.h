#ifndef SETTINGSDIALOGPATHS_H
#define SETTINGSDIALOGPATHS_H

#include "settings.h"
#include "settingsdialog.h"

class PathsSettingsTab : public SettingsTab
{
public:
  PathsSettingsTab(Settings& settings, SettingsDialog& dialog);
  void update();

private:
  void on_browseBaseDirBtn_clicked();
  void on_browseCacheDirBtn_clicked();
  void on_browseDownloadDirBtn_clicked();
  void on_browseGameDirBtn_clicked();
  void on_browseModDirBtn_clicked();
  void on_browseOverwriteDirBtn_clicked();
  void on_browseProfilesDirBtn_clicked();

  void on_baseDirEdit_editingFinished();
  void on_cacheDirEdit_editingFinished();
  void on_downloadDirEdit_editingFinished();
  void on_modDirEdit_editingFinished();
  void on_overwriteDirEdit_editingFinished();
  void on_profilesDirEdit_editingFinished();

  void normalizePath(QLineEdit* lineEdit);

  QDir m_gameDir;
};

#endif  // SETTINGSDIALOGPATHS_H
