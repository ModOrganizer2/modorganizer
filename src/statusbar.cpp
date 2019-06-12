#include "statusbar.h"
#include "nexusinterface.h"
#include "settings.h"

StatusBar::StatusBar(QStatusBar* bar, QAction* actionNotifications) :
  m_bar(bar), m_notifications(new StatusBarNotifications(actionNotifications)),
  m_progress(new QProgressBar), m_api(new QLabel)
{
  m_bar->addPermanentWidget(m_progress);
  m_bar->addPermanentWidget(m_notifications);
  m_bar->addPermanentWidget(m_api);

  m_progress->setTextVisible(true);
  m_progress->setRange(0, 100);
  m_progress->setMaximumWidth(150);
  m_progress->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  m_api->setObjectName("apistats");
  m_api->setStyleSheet("QLabel{ padding: 0.1em 0 0.1em 0; }");

  m_bar->clearMessage();
  setProgress(-1);
  updateAPI({}, {});
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

void StatusBar::updateNotifications(bool hasNotifications)
{
  m_notifications->update(hasNotifications);
}

void StatusBar::updateAPI(const APIStats& stats, const APIUserAccount& user)
{
  m_api->setText(
    QString("API: Q: %1 | D: %2 | H: %3")
    .arg(stats.requestsQueued)
    .arg(user.limits().remainingDailyRequests)
    .arg(user.limits().remainingHourlyRequests));

  QColor textColor;
  QColor backgroundColor;

  if (user.type() == APIUserAccountTypes::None) {
    backgroundColor = Qt::transparent;
  } else if (user.remainingRequests() > 300) {
    textColor = "white";
    backgroundColor = Qt::darkGreen;
  } else if (user.remainingRequests() < 150) {
    textColor = "white";
    backgroundColor = Qt::darkRed;
  } else {
    textColor = "black";
    backgroundColor = Qt::darkYellow;
  }

  QPalette palette = m_api->palette();

  palette.setColor(QPalette::WindowText, textColor);
  palette.setColor(QPalette::Background, backgroundColor);

  m_api->setPalette(palette);
  m_api->setAutoFillBackground(true);
}

void StatusBar::checkSettings(const Settings& settings)
{
  m_api->setVisible(!settings.hideAPICounter());
}


StatusBarNotifications::StatusBarNotifications(QAction* action)
  : m_action(action), m_icon(new QLabel), m_text(new QLabel)
{
  setLayout(new QHBoxLayout);
  layout()->setContentsMargins(0, 0, 0, 0);
  layout()->addWidget(m_icon);
  layout()->addWidget(m_text);
}

void StatusBarNotifications::update(bool hasNotifications)
{
  if (hasNotifications) {
    m_icon->setPixmap(m_action->icon().pixmap(16, 16));
    m_text->setText(QObject::tr("Notifications"));
  }

  setVisible(hasNotifications);
}

void StatusBarNotifications::mouseDoubleClickEvent(QMouseEvent* e)
{
  if (m_action->isEnabled()) {
    m_action->trigger();
  }
}
