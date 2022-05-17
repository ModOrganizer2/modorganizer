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

#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include <QDialog>

namespace Ui {
    class MessageDialog;
}

/**
 * borderless dialog used to display short messages that will automatically
 * vanish after a moment
 **/
class MessageDialog : public QDialog
{
    Q_OBJECT

public:
 /**
  * @brief constructor
  *
  * @param text the message to display
  * @param reference parent widget. This will also be used to position the message at the bottom center of the dialog
  **/
 
  explicit MessageDialog(const QString &text, QWidget *reference);

  ~MessageDialog();

  /**
   * factory function for message dialogs. This can be used as a fire-and-forget. The message
   * will automatically positioned to the reference dialog and get a reasonable view time
   *
   * @param text the text to display. The length of this text is used to determine how long the dialog is to be shown
   * @param reference the reference widget on top of which the message should be displayed
   * @param true if the message should bring MO to front to ensure this message is visible
   **/
  static void showMessage(const QString &text, QWidget *reference, bool bringToFront = true);

protected:

  virtual void resizeEvent(QResizeEvent *event);

private:
    Ui::MessageDialog *ui;
};

#endif // MESSAGEDIALOG_H
