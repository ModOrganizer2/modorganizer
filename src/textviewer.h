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

#ifndef TEXTVIEWER_H
#define TEXTVIEWER_H

#include <QDialog>
#include <QTabWidget>
#include <finddialog.h>
#include <set>
#include "loghighlighter.h"

namespace Ui {
    class TextViewer;
}


/**
 * @brief tabbed text editor
 **/
class TextViewer : public QDialog
{
  Q_OBJECT

public:

  /**
   * @brief constructor
   * @param parent parent widget
   **/
  explicit TextViewer(QWidget *parent = 0);

  ~TextViewer();

  /**
   * @brief set the description text to inform the user what he's editing
   *
   * @param description the description to set
   **/
  void setDescription(const QString &description);

  /**
   * @brief add a new tab with the specified file open
   *
   * @param fileName name of the file to open
   * @param writable if true, the file can be modified
   **/
  void addFile(const QString &fileName, bool writable);

protected:

  void closeEvent(QCloseEvent *event);
  bool eventFilter(QObject *obj, QEvent *event);

private slots:

  void saveFile();
  void modified();
  void patternChanged(QString newPattern);
  void findNext();

private:

  void saveFile(const QTextEdit *editor);
  void find();

private:

  Ui::TextViewer *ui;
  QTabWidget *m_EditorTabs;
  std::set<QTextEdit*> m_Modified;
  FindDialog *m_FindDialog;
  QString m_FindPattern;

};

#endif // TEXTVIEWER_H
