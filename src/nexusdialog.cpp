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

#include "nexusdialog.h"
#include "ui_nexusdialog.h"

#include "messagedialog.h"
#include "report.h"
#include "json.h"

#include <utility.h>
#include <gameinfo.h>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QMenu>
#include <QInputDialog>
#include <QWebHistory>


using namespace MOBase;
using namespace MOShared;

NexusDialog::NexusDialog(NXMAccessManager *accessManager, QWidget *parent)
  : QDialog(parent), ui(new Ui::NexusDialog),
    m_ModUrlExp(QString("http://([a-zA-Z0-9]*).nexusmods.com/mods/(\\d+)"), Qt::CaseInsensitive),
    m_AccessManager(accessManager),
    m_Tutorial(this, "NexusDialog")
{
  ui->setupUi(this);

  Qt::WindowFlags flags = windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint;
  Qt::WindowFlags helpFlag = Qt::WindowContextHelpButtonHint;
  flags = flags & (~helpFlag);
  setWindowFlags(flags);

  m_Tabs = this->findChild<QTabWidget*>("browserTabWidget");

  m_View = new NexusView(this);

  initTab(m_View);

  m_LoadProgress = this->findChild<QProgressBar*>("loadProgress");

  connect(m_Tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequested(int)));
  m_Tutorial.registerControl();
}


NexusDialog::~NexusDialog()
{

  delete ui;
}

void NexusDialog::closeEvent(QCloseEvent *event)
{
//  m_AccessManager->showCookies();
  QDialog::closeEvent(event);
}

void NexusDialog::initTab(NexusView *newView)
{
  newView->page()->setNetworkAccessManager(m_AccessManager);
  newView->page()->setForwardUnsupportedContent(true);

  connect(newView, SIGNAL(loadProgress(int)), this, SLOT(progress(int)));
  connect(newView, SIGNAL(titleChanged(QString)), this, SLOT(titleChanged(QString)));
  connect(newView, SIGNAL(initTab(NexusView*)), this, SLOT(initTab(NexusView*)));
  connect(newView, SIGNAL(startFind()), this, SLOT(startSearch()));
  connect(newView, SIGNAL(urlChanged(QUrl)), this, SLOT(urlChanged(QUrl)));
  connect(newView, SIGNAL(openUrlInNewTab(QUrl)), this, SLOT(openInNewTab(QUrl)));
  connect(newView->page(), SIGNAL(downloadRequested(QNetworkRequest)), this, SLOT(downloadRequested(QNetworkRequest)));

  connect(newView->page(), SIGNAL(unsupportedContent(QNetworkReply*)), this, SLOT(unsupportedContent(QNetworkReply*)));
  connect(m_AccessManager, SIGNAL(requestNXMDownload(QString)), this, SLOT(requestNXMDownload(QString)));

  ui->backBtn->setEnabled(false);
  ui->fwdBtn->setEnabled(false);
  m_Tabs->addTab(newView, tr("new"));

  m_View->settings()->setAttribute(QWebSettings::PluginsEnabled, true);
  m_View->settings()->setAttribute(QWebSettings::AutoLoadImages, true);
}


void NexusDialog::openInNewTab(const QUrl &url)
{
  NexusView *newView = new NexusView(this);

  initTab(newView);
  newView->setUrl(url);
}


NexusView *NexusDialog::getCurrentView()
{
  return qobject_cast<NexusView*>(m_Tabs->currentWidget());
}


void NexusDialog::urlChanged(const QUrl &url)
{
  NexusView *sendingView = qobject_cast<NexusView*>(sender());
  NexusView *currentView = getCurrentView();
  if ((m_ModUrlExp.indexIn(url.toString()) != -1) &&
      (sendingView == currentView)) {
    ui->modIDEdit->setText(m_ModUrlExp.cap(2));
  }

  if (currentView != NULL) {
    ui->backBtn->setEnabled(currentView->history()->canGoBack());
    ui->fwdBtn->setEnabled(currentView->history()->canGoForward());
  }
}


void NexusDialog::openUrl(const QString &url)
{
  m_Url = url;
  if (m_Url.startsWith("www")) {
    m_Url.prepend("http://");
  }
}

void NexusDialog::login(const QString &username, const QString &password)
{
  m_AccessManager->login(username, password);

  connect(m_AccessManager, SIGNAL(loginSuccessful(bool)), this, SLOT(loginFinished(bool)), Qt::UniqueConnection);
  connect(m_AccessManager, SIGNAL(loginFailed(QString)), this, SLOT(loginFailed(QString)), Qt::UniqueConnection);
}


void NexusDialog::loginFailed(const QString &message)
{
  if (this->isVisible()) {
    MessageDialog::showMessage(tr("login failed: %1").arg(message), this);
  }
}


void NexusDialog::loginFinished(bool necessary)
{
  if (necessary && this->isVisible()) {
    MessageDialog::showMessage(tr("login successful"), this);
  }

  loadNexus();
  emit loginSuccessful(necessary);
}


void NexusDialog::loadNexus()
{
  if (m_View != NULL) {
    m_View->load(QUrl(m_Url));
  }
}


void NexusDialog::progress(int value)
{
  m_LoadProgress->setValue(value);
  m_LoadProgress->setVisible(value != 100);
}


void NexusDialog::titleChanged(const QString &title)
{
  NexusView *view = qobject_cast<NexusView*>(sender());
  for (int i = 0; i < m_Tabs->count(); ++i) {
    if (m_Tabs->widget(i) == view) {
      m_Tabs->setTabText(i, title.mid(0, 15));
      m_Tabs->setTabToolTip(i, title);
    }
  }
}


QString NexusDialog::guessFileName(const QString &url)
{
  QRegExp uploadsExp(QString("http://.+/uploads/([^/]+)$"));
  if (uploadsExp.indexIn(url) != -1) {
    // these seem to be premium downloads
    return uploadsExp.cap(1);
  }

  QRegExp filesExp(QString("http://.+\\?file=([^&]+)"));
  if (filesExp.indexIn(url) != -1) {
    // a regular manual download?
    return filesExp.cap(1);
  }
  return "unknown";
}

void NexusDialog::unsupportedContent(QNetworkReply *reply)
{
  try {
    QWebPage *page = qobject_cast<QWebPage*>(sender());
    if (page == NULL) {
      qCritical("sender not a page");
      return;
    }
    NexusView *view = qobject_cast<NexusView*>(page->view());
    if (view == NULL) {
      qCritical("no view?");
      return;
    }

    int modID = 0;
    QString fileName = guessFileName(reply->url().toString());
    qDebug("unsupported: %s - %s", view->url().toString().toUtf8().constData(), reply->url().toString().toUtf8().constData());
    QRegExp sourceExp(QString("http://[a-zA-Z0-9.]*.nexusmods.com/.*"), Qt::CaseInsensitive);
    QRegExp modidExp(QString("http://([a-zA-Z0-9]*).nexusmods.com/downloads/file.php\\?id=(\\d+)"), Qt::CaseInsensitive);
    QRegExp modidAltExp(QString("http://([a-zA-Z0-9]*).nexusmods.com/mods/(\\d+)"), Qt::CaseInsensitive);
    if (sourceExp.indexIn(reply->url().toString()) != -1) {
      if (modidExp.indexIn(view->url().toString()) != -1) {
        modID = modidExp.cap(2).toInt();
      } else if (modidAltExp.indexIn(view->url().toString()) != -1) {
        modID = modidAltExp.cap(2).toInt();
      } else {
        modID = view->getLastSeenModID();
      }
    } else {
      qDebug("not a nexus download: %s", reply->url().toString().toUtf8().constData());
      return;
    }
    emit requestDownload(reply, modID, fileName);
  } catch (const std::exception &e) {
    if (isVisible()) {
      MessageDialog::showMessage(tr("failed to start download"), this);
    }
    qCritical("exception downloading unsupported content: %s", e.what());
  }
}


void NexusDialog::downloadRequested(const QNetworkRequest &request)
{
  qCritical("download request %s ignored", request.url().toString().toUtf8().constData());
}


void NexusDialog::requestNXMDownload(const QString&)
{
  if (isVisible()) {
    MessageDialog::showMessage(tr("Download started"), this);
  }
}


void NexusDialog::tabCloseRequested(int index)
{
  if (m_Tabs->count() == 1) {
    this->close();
  } else {
    m_Tabs->widget(index)->deleteLater();
    m_Tabs->removeTab(index);
  }
}


void NexusDialog::on_browserTabWidget_customContextMenuRequested(const QPoint&)
{
}

void NexusDialog::on_backBtn_clicked()
{
  NexusView *currentView = getCurrentView();
  if (currentView != NULL) {
    currentView->back();
  }
}

void NexusDialog::on_fwdBtn_clicked()
{
  NexusView *currentView = getCurrentView();
  if (currentView != NULL) {
    currentView->forward();
  }
}


void NexusDialog::startSearch()
{
  ui->searchEdit->setFocus();
}


void NexusDialog::on_modIDEdit_returnPressed()
{
  QString url = ToQString(GameInfo::instance().getNexusPage()).append("/downloads/file.php?id=%1").arg(ui->modIDEdit->text());
  NexusView *currentView = getCurrentView();
  if (currentView != NULL) {
    currentView->load(QUrl(url));
  }
}

void NexusDialog::on_searchEdit_returnPressed()
{
  NexusView *currentView = getCurrentView();
  if (currentView != NULL) {
    currentView->findText(ui->searchEdit->text(), QWebPage::FindWrapsAroundDocument);
  }
}

void NexusDialog::on_browserTabWidget_currentChanged(QWidget *current)
{
  NexusView *currentView = qobject_cast<NexusView*>(current);
  if (currentView != NULL) {
    ui->backBtn->setEnabled(currentView->history()->canGoBack());
    ui->fwdBtn->setEnabled(currentView->history()->canGoForward());
  }

  if (m_ModUrlExp.indexIn(currentView->url().toString()) != -1) {
    ui->modIDEdit->setText(m_ModUrlExp.cap(2));
  }
}

void NexusDialog::on_refreshBtn_clicked()
{
  getCurrentView()->reload();
}
