#ifndef MO_STATUSBAR_H
#define MO_STATUSBAR_H

#include <QStatusBar>
#include <QProgressBar>

struct APIStats;
class APIUserAccount;
class Settings;


class StatusBarNotifications : public QWidget
{
public:
  StatusBarNotifications(QAction* action);

  void update(bool hasNotifications);

protected:
  void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
  QAction* m_action;
  QLabel* m_icon;
  QLabel* m_text;
};


class StatusBar
{
public:
  StatusBar(QStatusBar* bar, QAction* actionNotifications);

  void setProgress(int percent);
  void updateNotifications(bool hasNotifications);
  void updateAPI(const APIStats& stats, const APIUserAccount& user);
  void checkSettings(const Settings& settings);

private:
  QStatusBar* m_bar;
  StatusBarNotifications* m_notifications;
  QProgressBar* m_progress;
  QLabel* m_api;
};

#endif // MO_STATUSBAR_H
