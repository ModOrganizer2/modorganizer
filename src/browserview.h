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

#ifndef NEXUSVIEW_H
#define NEXUSVIEW_H


class QEvent;
class QUrl;
class QWidget;
#include <QWebView>
#include <QWebPage>

/**
 * @brief web view used to display a nexus page
 **/
class BrowserView : public QWebView
{
    Q_OBJECT

public:

  explicit BrowserView(QWidget *parent = 0);

signals:

  /**
   * @brief emitted when the user opens a new window to be displayed in another tab
   *
   * @param newView the view for the newly opened window
   **/
  void initTab(BrowserView *newView);

  /**
   * @brief emitted when the user requests a link to be opened in a new tab by middle-clicking
   *
   * @param url the url to open
   */
  void openUrlInNewTab(const QUrl &url);

  /**
   * @brief Ctrl-f was clicked. The containing dialog should activate its find-facility
   */
  void startFind();

  /**
   * @brief F3 was pressed. The containing dialog should search again
   */
  void findAgain();

protected:

  virtual QWebView *createWindow(QWebPage::WebWindowType type);

  virtual bool eventFilter(QObject *obj, QEvent *event);


private:

  QString m_FindPattern;
  bool m_MiddleClick;

};

#endif // NEXUSVIEW_H
