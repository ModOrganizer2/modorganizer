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

#ifndef TRANSFERSAVESDIALOG_H
#define TRANSFERSAVESDIALOG_H

#include "tutorabledialog.h"
#include "profile.h"

namespace Ui { class TransferSavesDialog; }
namespace MOBase { class IPluginGame; }

class SaveGame;

class TransferSavesDialog : public MOBase::TutorableDialog
{
  Q_OBJECT
  
public:
  explicit TransferSavesDialog(const Profile &profile, MOBase::IPluginGame const *gamePlugin, QWidget *parent = 0);
  ~TransferSavesDialog();
  
private slots:

  void on_moveToLocalBtn_clicked();

  void on_doneButton_clicked();

  void on_globalCharacterList_currentTextChanged(const QString &currentText);

  void on_localCharacterList_currentTextChanged(const QString &currentText);

  void on_copyToLocalBtn_clicked();

  void on_moveToGlobalBtn_clicked();

  void on_copyToGlobalBtn_clicked();

private:

  enum OverwriteMode {
    OVERWRITE_ASK,
    OVERWRITE_YES,
    OVERWRITE_NO
  };

private:

  void refreshGlobalCharacters();
  void refreshLocalCharacters();
  void refreshGlobalSaves();
  void refreshLocalSaves();
  bool testOverwrite(OverwriteMode &overwriteMode, const QString &destinationFile);

private:

  Ui::TransferSavesDialog *ui;

  Profile m_Profile;

  MOBase::IPluginGame const *m_GamePlugin;

  std::vector<SaveGame*> m_GlobalSaves;
  std::vector<SaveGame*> m_LocalSaves;

};

#endif // TRANSFERSAVESDIALOG_H
