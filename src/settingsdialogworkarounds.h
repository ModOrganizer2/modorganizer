#ifndef SETTINGSDIALOGWORKAROUNDS_H
#define SETTINGSDIALOGWORKAROUNDS_H

#include "settings.h"
#include "settingsdialog.h"

class WorkaroundsSettingsTab : public SettingsTab
{
public:
  WorkaroundsSettingsTab(Settings& settings, SettingsDialog& dialog);

  // shows the blacklist dialog from the given settings, and changes the
  // settings when the user accepts it
  //
  static bool changeBlacklistNow(QWidget* parent, Settings& settings);

  // shows the blacklist dialog from the given string and returns the new
  // blacklist if the user accepted it
  //
  static std::optional<QString> changeBlacklistLater(QWidget* parent,
                                                     const QString& current);

  // shows the blacklist dialog from the given string and returns the new
  // blacklist if the user accepted it
  //
  static std::optional<QStringList> changeSkipFileSuffixes(QWidget* parent,
                                                           const QStringList& current);

  // shows the blacklist dialog from the given string and returns the new
  // blacklist if the user accepted it
  //
  static std::optional<QStringList> changeSkipDirectories(QWidget* parent,
                                                          const QStringList& current);

  void update();

private:
  QString m_ExecutableBlacklist;
  QStringList m_SkipFileSuffixes;
  QStringList m_SkipDirectories;

  void on_bsaDateBtn_clicked();
  void on_execBlacklistBtn_clicked();
  void on_skipFileSuffixBtn_clicked();
  void on_skipDirectoriesBtn_clicked();
  void on_resetGeometryBtn_clicked();
};

#endif  // SETTINGSDIALOGWORKAROUNDS_H
