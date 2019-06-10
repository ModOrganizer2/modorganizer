#ifndef MO_STATUSBAR_H
#define MO_STATUSBAR_H

#include <QStatusBar>
#include <QProgressBar>

class StatusBar
{
public:
  StatusBar(QStatusBar* bar);

  void setProgress(int percent);

private:
  QStatusBar* m_bar;
  QLabel* m_api;
  QProgressBar* m_progress;
};

#endif // MO_STATUSBAR_H
