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

#ifndef NEXUSDIALOG_H
#define NEXUSDIALOG_H

#include "nexusview.h"
#include "nxmaccessmanager.h"
#include "tutorialcontrol.h"
#include <QDialog>
#include <QProgressBar>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QWebView>
#include <QQueue>
#include <QTabWidget>
#include <QAtomicInt>


namespace Ui {
    class NexusDialog;
}


/**
 * @brief a dialog containing a webbrowser that is intended to browse the nexus network
 **/
class NexusDialog : public QDialog
{
    Q_OBJECT

public:

 /**
  * @brief constructor
  *
  * @param accessManager the access manager to use for network requests
  * @param parent parent widget
  **/
  explicit NexusDialog(NXMAccessManager *accessManager, QWidget *parent = 0);
  ~NexusDialog();

  /**
   * @brief set the url to open. If automatic login is enabled, the url is opened after login
   *
   * @param url the url to open
   **/
  void openUrl(const QString &url);

  /**
   * @brief log-in to the nexus page.
   * 
   * After successful login, loadNexus() is automatically called. If the user is already
   * logged in, this happens immediately
   *
   * @param username the user name to log in as
   * @param password the user password
   **/
  void login(const QString &username, const QString &password);

  /**
   * @brief load the page set with openUrl()
   **/
  void loadNexus();

signals:

 /**
  * @brief emitted when the user caused a download
  *
  * @param reply the network-reply transmitting the file
  * @param modID mod id of the file requested
  * @param fileName suggested filename
  **/
 void requestDownload(QNetworkReply *reply, int modID, const QString &fileName);

 void loginSuccessful(bool necessary);

protected:

 virtual void closeEvent(QCloseEvent *);

private slots:

  void loginFailed(const QString &message);
  void loginFinished(bool necessary);

  void initTab(NexusView *newView);
  void openInNewTab(const QUrl &url);

  void progress(int value);

  void titleChanged(const QString &title);
  void requestNXMDownload(const QString &url);
  void unsupportedContent(QNetworkReply *reply);
  void downloadRequested(const QNetworkRequest &request);


  void tabCloseRequested(int index);

  void on_browserTabWidget_customContextMenuRequested(const QPoint &pos);

  void urlChanged(const QUrl &url);

  void on_backBtn_clicked();

  void on_fwdBtn_clicked();

  void on_modIDEdit_returnPressed();

  void on_searchEdit_returnPressed();

  void startSearch();

  void on_browserTabWidget_currentChanged(QWidget *arg1);

  void on_refreshBtn_clicked();

private:

  QString guessFileName(const QString &url);

  NexusView *getCurrentView();

private:

  Ui::NexusDialog *ui;

  MOBase::TutorialControl m_Tutorial;

  QString m_Url;
  QRegExp m_ModUrlExp;

  QTabWidget *m_Tabs;
  NexusView *m_View;
  QProgressBar *m_LoadProgress;
  NXMAccessManager *m_AccessManager;

};

#endif // NEXUSDIALOG_H
