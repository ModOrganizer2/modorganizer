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
  StatusBar(QStatusBar* bar);

  void setProgress(int percent);
  void updateAPI(const APIStats& stats, const APIUserAccount& user);
  void checkSettings(const Settings& settings);

private:
  QStatusBar* m_bar;
  QLabel* m_api;
  QProgressBar* m_progress;
};

#endif // MO_STATUSBAR_H
