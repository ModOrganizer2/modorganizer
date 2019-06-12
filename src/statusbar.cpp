#include "statusbar.h"
#include "nexusinterface.h"
#include "settings.h"

StatusBar::StatusBar(QStatusBar* bar)
  : m_bar(bar), m_api(new QLabel), m_progress(new QProgressBar)
{
  m_progress->setTextVisible(true);
  m_progress->setRange(0, 100);

  m_bar->addPermanentWidget(m_api);
  m_bar->addPermanentWidget(m_progress);

  m_api->setObjectName("apistats");
  m_api->setStyleSheet("QLabel{ padding-left: 0.1em; padding-right: 0.1em; }");

  m_bar->clearMessage();
  setProgress(-1);
  updateAPI({}, {});
}

void StatusBar::setProgress(int percent)
{
  if (percent < 0 || percent >= 100) {
    m_progress->setVisible(false);
  } else if (!m_progress->isVisible()) {
    m_progress->setVisible(true);
    m_progress->setValue(percent);
  }
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
