#ifndef MO_STATUSBAR_H
#define MO_STATUSBAR_H

#include <QStatusBar>
#include <QProgressBar>

struct APIStats;
class APIUserAccount;
class Settings;
namespace Ui { class MainWindow; }


class StatusBarAction : public QWidget
{
public:
  StatusBarAction(QAction* action);

  void set(bool visible);

protected:
  void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
  QAction* m_action;
  QLabel* m_icon;
  QLabel* m_text;

  QString cleanupActionText(const QString& s) const;
};


class StatusBar
{
public:
  StatusBar(QStatusBar* bar, Ui::MainWindow* ui);

  void setProgress(int percent);
  void setNotifications(bool hasNotifications);
  void setAPI(const APIStats& stats, const APIUserAccount& user);
  void setUpdateAvailable(bool b);
  void checkSettings(const Settings& settings);

private:
  QStatusBar* m_bar;
  QProgressBar* m_progress;
  StatusBarAction* m_notifications;
  StatusBarAction* m_update;
  QLabel* m_api;
};

#endif // MO_STATUSBAR_H
