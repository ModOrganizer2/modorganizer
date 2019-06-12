#ifndef MO_STATUSBAR_H
#define MO_STATUSBAR_H

#include <QStatusBar>
#include <QProgressBar>

struct APIStats;
class APIUserAccount;
class Settings;

class StatusBar
{
public:
  StatusBar(QStatusBar* bar, QAction* notifications);

  void setProgress(int percent);
  void setHasNotifications(bool b);
  void updateAPI(const APIStats& stats, const APIUserAccount& user);
  void checkSettings(const Settings& settings);

private:
  QStatusBar* m_bar;
  QProgressBar* m_progress;
  QToolButton* m_notifications;
  QLabel* m_api;
};

#endif // MO_STATUSBAR_H
