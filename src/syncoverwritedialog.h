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

#ifndef SYNCOVERWRITEDIALOG_H
#define SYNCOVERWRITEDIALOG_H


#include "tutorabledialog.h"
#include <QTreeWidgetItem>
#include <directoryentry.h>


namespace Ui {
class SyncOverwriteDialog;
}

class SyncOverwriteDialog : public MOBase::TutorableDialog
{
  Q_OBJECT
  
public:
  explicit SyncOverwriteDialog(const QString &path, MOShared::DirectoryEntry *directoryStructure, QWidget *parent = 0);
  ~SyncOverwriteDialog();

  void apply(const QString &modDirectory);
private:
  void refresh(const QString &path);
  void readTree(const QString &path, MOShared::DirectoryEntry *directoryStructure, QTreeWidgetItem *subTree);
  void applyTo(QTreeWidgetItem *item, const QString &path, const QString &modDirectory);

private:

  Ui::SyncOverwriteDialog *ui;
  QString m_SourcePath;
  MOShared::DirectoryEntry *m_DirectoryStructure;

};

#endif // SYNCOVERWRITEDIALOG_H
