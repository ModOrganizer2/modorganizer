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

#include "browserdialog.h"

#include "browserview.h"
#include "messagedialog.h"
#include "persistentcookiejar.h"
#include "report.h"
#include "settings.h"
#include "ui_browserdialog.h"

#include <log.h>
#include <utility.h>

#include <QDir>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QWebEngineHistory>
#include <QWebEngineSettings>

using namespace MOBase;

BrowserDialog::BrowserDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::BrowserDialog),
      m_AccessManager(new QNetworkAccessManager(this))
{
  ui->setupUi(this);

  m_AccessManager->setCookieJar(new PersistentCookieJar(QDir::fromNativeSeparators(
      Settings::instance().paths().cache() + "/cookies.dat")));

  Qt::WindowFlags flags =
      windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint;
  Qt::WindowFlags helpFlag = Qt::WindowContextHelpButtonHint;
  flags                    = flags & (~helpFlag);
  setWindowFlags(flags);

  m_Tabs = this->findChild<QTabWidget*>("browserTabWidget");

  installEventFilter(this);

  connect(m_Tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequested(int)));

  ui->urlEdit->setVisible(false);
}

BrowserDialog::~BrowserDialog()
{
  delete ui;
}

void BrowserDialog::closeEvent(QCloseEvent* event)
{
  Settings::instance().geometry().saveGeometry(this);
  QDialog::closeEvent(event);
}

void BrowserDialog::initTab(BrowserView* newView)
{
  // newView->page()->setNetworkAccessManager(m_AccessManager);
  // newView->page()->setForwardUnsupportedContent(true);

  connect(newView, SIGNAL(loadProgress(int)), this, SLOT(progress(int)));
  connect(newView, SIGNAL(titleChanged(QString)), this, SLOT(titleChanged(QString)));
  connect(newView, SIGNAL(initTab(BrowserView*)), this, SLOT(initTab(BrowserView*)));
  connect(newView, SIGNAL(startFind()), this, SLOT(startSearch()));
  connect(newView, SIGNAL(urlChanged(QUrl)), this, SLOT(urlChanged(QUrl)));
  connect(newView, SIGNAL(openUrlInNewTab(QUrl)), this, SLOT(openInNewTab(QUrl)));
  connect(newView, SIGNAL(downloadRequested(QNetworkRequest)), this,
          SLOT(downloadRequested(QNetworkRequest)));
  connect(newView, SIGNAL(unsupportedContent(QNetworkReply*)), this,
          SLOT(unsupportedContent(QNetworkReply*)));

  ui->backBtn->setEnabled(false);
  ui->fwdBtn->setEnabled(false);
  m_Tabs->addTab(newView, tr("new"));
  newView->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
  newView->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, true);
}

void BrowserDialog::openInNewTab(const QUrl& url)
{
  BrowserView* newView = new BrowserView(this);
  initTab(newView);
  newView->setUrl(url);
}

BrowserView* BrowserDialog::getCurrentView()
{
  return qobject_cast<BrowserView*>(m_Tabs->currentWidget());
}

void BrowserDialog::urlChanged(const QUrl& url)
{
  BrowserView* currentView = getCurrentView();
  if (currentView != nullptr) {
    ui->backBtn->setEnabled(currentView->history()->canGoBack());
    ui->fwdBtn->setEnabled(currentView->history()->canGoForward());
  }
  ui->urlEdit->setText(url.toString());
}

void BrowserDialog::openUrl(const QUrl& url)
{
  if (isHidden()) {
    Settings::instance().geometry().restoreGeometry(this);
    show();
  }
  openInNewTab(url);
}

void BrowserDialog::maximizeWidth()
{
  int viewportWidth = getCurrentView()->page()->contentsSize().width();
  int frameWidth    = width() - viewportWidth;

  int contentWidth = getCurrentView()->page()->contentsSize().width();

  QScreen* screen = this->window()->windowHandle()->screen();
  int screenWidth = screen->geometry().size().width();

  int targetWidth = std::min<int>(
      std::max<int>(viewportWidth, contentWidth) + frameWidth, screenWidth);
  this->resize(targetWidth, height());
}

void BrowserDialog::progress(int value)
{
  ui->loadProgress->setValue(value);
  if (value == 100) {
    maximizeWidth();
    ui->loadProgress->setVisible(false);
  } else {
    ui->loadProgress->setVisible(true);
  }
}

void BrowserDialog::titleChanged(const QString& title)
{
  BrowserView* view = qobject_cast<BrowserView*>(sender());
  for (int i = 0; i < m_Tabs->count(); ++i) {
    if (m_Tabs->widget(i) == view) {
      m_Tabs->setTabText(i, title.mid(0, 15));
      m_Tabs->setTabToolTip(i, title);
    }
  }
}

QString BrowserDialog::guessFileName(const QString& url)
{
  QRegularExpression uploadsExp(QString("https://.+/uploads/([^/]+)$"));
  auto match = uploadsExp.match(url);
  if (match.hasMatch()) {
    // these seem to be premium downloads
    return match.captured(1);
  }

  QRegularExpression filesExp(QString("https://.+\\?file=([^&]+)"));
  match = filesExp.match(url);
  if (match.hasMatch()) {
    // a regular manual download?
    return match.captured(1);
  }
  return "unknown";
}

void BrowserDialog::unsupportedContent(QNetworkReply* reply)
{
  try {
    QWebEnginePage* page = qobject_cast<QWebEnginePage*>(sender());
    if (page == nullptr) {
      log::error("sender not a page");
      return;
    }
    /*browserview *view = qobject_cast<browserview*>(page->view());
    if (view == nullptr) {
      log::error("no view?");
      return;
    }*/

    emit requestDownload(page->url(), reply);
  } catch (const std::exception& e) {
    if (isVisible()) {
      MessageDialog::showMessage(tr("failed to start download"), this);
    }
    log::error("exception downloading unsupported content: {}", e.what());
  }
}

void BrowserDialog::downloadRequested(const QNetworkRequest& request)
{
  log::error("download request {} ignored", request.url().toString());
}

void BrowserDialog::tabCloseRequested(int index)
{
  if (m_Tabs->count() == 1) {
    this->close();
  } else {
    m_Tabs->widget(index)->deleteLater();
    m_Tabs->removeTab(index);
  }
}

void BrowserDialog::on_backBtn_clicked()
{
  BrowserView* currentView = getCurrentView();
  if (currentView != nullptr) {
    currentView->back();
  }
}

void BrowserDialog::on_fwdBtn_clicked()
{
  BrowserView* currentView = getCurrentView();
  if (currentView != nullptr) {
    currentView->forward();
  }
}

void BrowserDialog::startSearch()
{
  ui->searchEdit->setFocus();
}

void BrowserDialog::on_searchEdit_returnPressed()
{
  //  BrowserView *currentView = getCurrentView();
  //  if (currentView != nullptr) {
  //    currentView->findText(ui->searchEdit->text(),
  //    QWebEnginePage::FindWrapsAroundDocument);
  //  }
}

void BrowserDialog::on_refreshBtn_clicked()
{
  getCurrentView()->reload();
}

void BrowserDialog::on_browserTabWidget_currentChanged(int index)
{
  BrowserView* currentView =
      qobject_cast<BrowserView*>(ui->browserTabWidget->widget(index));
  if (currentView != nullptr) {
    ui->backBtn->setEnabled(currentView->history()->canGoBack());
    ui->fwdBtn->setEnabled(currentView->history()->canGoForward());
  }
}

void BrowserDialog::on_urlEdit_returnPressed()
{
  QWebEngineView* currentView = getCurrentView();
  if (currentView != nullptr) {
    currentView->setUrl(QUrl(ui->urlEdit->text()));
  }
}

bool BrowserDialog::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent* keyEvent = reinterpret_cast<QKeyEvent*>(event);
    if ((keyEvent->modifiers() & Qt::ControlModifier) &&
        (keyEvent->key() == Qt::Key_U)) {
      ui->urlEdit->setVisible(!ui->urlEdit->isVisible());
      return true;
    }
  }
  return QDialog::eventFilter(object, event);
}
