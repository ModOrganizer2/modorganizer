#include "statusbar.h"
#include "nexusinterface.h"
#include "settings.h"
#include "ui_mainwindow.h"

StatusBar::StatusBar(QStatusBar* bar, Ui::MainWindow* ui) :
  m_bar(bar), m_progress(new QProgressBar),
  m_notifications(new StatusBarAction(ui->actionNotifications)),
  m_update(new StatusBarAction(ui->actionUpdate)),
  m_api(new QLabel)
{
  m_bar->addPermanentWidget(m_progress);
  m_bar->addPermanentWidget(m_notifications);
  m_bar->addPermanentWidget(m_update);
  m_bar->addPermanentWidget(m_api);

  m_progress->setTextVisible(true);
  m_progress->setRange(0, 100);
  m_progress->setMaximumWidth(150);
  m_progress->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  m_update->set(false);
  m_notifications->set(false);

  m_api->setObjectName("apistats");

  m_bar->clearMessage();
  setProgress(-1);
  setAPI({}, {});
}

void StatusBar::setProgress(int percent)
{
  if (percent < 0 || percent >= 100) {
    m_bar->clearMessage();
    m_progress->setVisible(false);
  } else {
    m_bar->showMessage(QObject::tr("Loading..."));
    m_progress->setVisible(true);
    m_progress->setValue(percent);
  }
}

void StatusBar::setNotifications(bool hasNotifications)
{
  m_notifications->set(hasNotifications);
}

void StatusBar::setAPI(const APIStats& stats, const APIUserAccount& user)
{
  QString text;
  QString textColor;
  QString backgroundColor;

  if (user.type() == APIUserAccountTypes::None) {
    text = "API: not logged in";
    textColor = "white";
    backgroundColor = "transparent";
  } else {
    text = QString("API: Queued: %1 | Daily: %2 | Hourly: %3")
      .arg(stats.requestsQueued)
      .arg(user.limits().remainingDailyRequests)
      .arg(user.limits().remainingHourlyRequests);

    if (user.remainingRequests() > 500) {
      textColor = "white";
      backgroundColor = "darkgreen";
    } else if (user.remainingRequests() > 200) {
      textColor = "black";
      backgroundColor = "rgb(226, 192, 0)";  // yellow
    } else {
      textColor = "white";
      backgroundColor = "darkred";
    }
  }

  m_api->setText(text);

  m_api->setStyleSheet(QString(R"(
    QLabel
    {
      padding-left: 0.1em;
      padding-right: 0.1em;
      padding-top: 0;
      padding-bottom: 0;
      color: %1;
      background-color: %2;
    }
    )")
    .arg(textColor)
    .arg(backgroundColor));

  m_api->setAutoFillBackground(true);
}

void StatusBar::setUpdateAvailable(bool b)
{
  m_update->set(b);
}

void StatusBar::checkSettings(const Settings& settings)
{
  m_api->setVisible(!settings.hideAPICounter());
}


StatusBarAction::StatusBarAction(QAction* action)
  : m_action(action), m_icon(new QLabel), m_text(new QLabel)
{
  setLayout(new QHBoxLayout);
  layout()->setContentsMargins(0, 0, 0, 0);
  layout()->addWidget(m_icon);
  layout()->addWidget(m_text);
}

void StatusBarAction::set(bool visible)
{
  if (visible) {
    m_icon->setPixmap(m_action->icon().pixmap(16, 16));
    m_text->setText(cleanupActionText(m_action->text()));
  }

  setVisible(visible);
}

void StatusBarAction::mouseDoubleClickEvent(QMouseEvent* e)
{
  if (m_action->isEnabled()) {
    m_action->trigger();
  }
}

QString StatusBarAction::cleanupActionText(const QString& original) const
{
  QString s = original;

  s.replace(QRegExp("\\&([^&])"), "\\1");  // &Item -> Item
  s.replace("&&", "&"); // &&Item -> &Item

  if (s.endsWith("...")) {
    s = s.left(s.size() - 3);
  }

  return s;
}
