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

#ifndef BROWSERDIALOG_H
#define BROWSERDIALOG_H

#include <QAtomicInt>
#include <QDialog>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQueue>
#include <QTabWidget>
#include <QTimer>
#include <QWebEngineView>

namespace Ui
{
class BrowserDialog;
}

class BrowserView;

/**
 * @brief a dialog containing a webbrowser that is intended to browse the nexus network
 **/
class BrowserDialog : public QDialog
{
  Q_OBJECT

public:
  /**
   * @brief constructor
   *
   * @param accessManager the access manager to use for network requests
   * @param parent parent widget
   **/
  explicit BrowserDialog(QWidget* parent = 0);
  ~BrowserDialog();

  /**
   * @brief set the url to open. If automatic login is enabled, the url is opened after
   *login
   *
   * @param url the url to open
   **/
  void openUrl(const QUrl& url);

  virtual bool eventFilter(QObject* object, QEvent* event);
signals:

  /**
   * @brief emitted when the user starts a download
   * @param pageUrl url of the current web site from which the download was started
   * @param reply network reply of the started download
   */
  void requestDownload(const QUrl& pageUrl, QNetworkReply* reply);

protected:
  virtual void closeEvent(QCloseEvent*);

private slots:

  void initTab(BrowserView* newView);
  void openInNewTab(const QUrl& url);

  void progress(int value);

  void titleChanged(const QString& title);
  void unsupportedContent(QNetworkReply* reply);
  void downloadRequested(const QNetworkRequest& request);

  void tabCloseRequested(int index);

  void urlChanged(const QUrl& url);

  void on_backBtn_clicked();

  void on_fwdBtn_clicked();

  void on_searchEdit_returnPressed();

  void startSearch();

  void on_refreshBtn_clicked();

  void on_browserTabWidget_currentChanged(int index);

  void on_urlEdit_returnPressed();

private:
  QString guessFileName(const QString& url);

  BrowserView* getCurrentView();

  void maximizeWidth();

private:
  Ui::BrowserDialog* ui;

  QNetworkAccessManager* m_AccessManager;

  QTabWidget* m_Tabs;
};

#endif  // BROWSERDIALOG_H
