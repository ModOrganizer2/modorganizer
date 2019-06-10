#include "statusbar.h"

StatusBar::StatusBar(QStatusBar* bar)
  : m_bar(bar), m_nexusAPI(new QLabel), m_progress(new QProgressBar)
{
  m_progress->setTextVisible(true);
  m_progress->setRange(0, 100);
  m_progress->setValue(0);
  m_progress->setVisible(false);

  m_bar->addPermanentWidget(m_nexusAPI);
  m_bar->addPermanentWidget(m_progress);

  m_bar->clearMessage();
}

void StatusBar::setProgress(int percent)
{
  qDebug().nospace() << "progress: " << percent;

  if (percent < 0 || percent >= 100) {
    m_progress->setVisible(false);
  } else if (!m_progress->isVisible()) {
    m_progress->setVisible(true);
    m_progress->setValue(percent);
  }
}
