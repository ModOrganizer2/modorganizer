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

#ifndef WORKAROUNDDIALOG_H
#define WORKAROUNDDIALOG_H

#include "tutorabledialog.h"
#include <iplugin.h>
#include <QDialog>
#include <QListWidgetItem>

class PluginContainer;

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
  explicit SettingsDialog(PluginContainer *pluginContainer, QWidget *parent = 0);
  ~SettingsDialog();

public slots:

  virtual void accept();

signals:

  void resetDialogs();

private:

  void storeSettings(QListWidgetItem *pluginItem);

public:

  QColor getOverwritingColor() { return m_OverwritingColor; }
  QColor getOverwrittenColor() { return m_OverwrittenColor; }
  QColor getContainsColor() { return m_ContainsColor; }
  QColor getContainedColor() { return m_ContainedColor; }

  void setOverwritingColor(QColor col) { m_OverwritingColor = col; }
  void setOverwrittenColor(QColor col) { m_OverwrittenColor = col; }
  void setContainsColor(QColor col) { m_ContainsColor = col; }
  void setContainedColor(QColor col) { m_ContainedColor = col; }

private slots:
  void on_loginCheckBox_toggled(bool checked);

  void on_categoriesBtn_clicked();

  void on_bsaDateBtn_clicked();

  void on_browseDownloadDirBtn_clicked();

  void on_browseModDirBtn_clicked();

  void on_browseCacheDirBtn_clicked();

  void on_resetDialogsButton_clicked();

  void on_pluginsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

  void deleteBlacklistItem();

  void on_associateButton_clicked();

  void on_clearCacheButton_clicked();

  void on_browseBaseDirBtn_clicked();

  void on_browseOverwriteDirBtn_clicked();

  void on_browseProfilesDirBtn_clicked();

  void on_overwritingBtn_clicked();

  void on_overwrittenBtn_clicked();

  void on_containsBtn_clicked();

  void on_containedBtn_clicked();

  void on_resetColorsBtn_clicked();

private:
    Ui::SettingsDialog *ui;
    PluginContainer *m_PluginContainer;

    QColor m_OverwritingColor;
    QColor m_OverwrittenColor;
    QColor m_ContainsColor;
    QColor m_ContainedColor;
};



#endif // WORKAROUNDDIALOG_H
