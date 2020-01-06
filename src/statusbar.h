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


class StatusBar : public QStatusBar
{
public:
  StatusBar(QWidget* parent=nullptr);

  void setup(Ui::MainWindow* ui, const Settings& settings);

  void setProgress(int percent);
  void setNotifications(bool hasNotifications);
  void setAPI(const APIStats& stats, const APIUserAccount& user);
  void setUpdateAvailable(bool b);
  void checkSettings(const Settings& settings);

protected:
  void showEvent(QShowEvent* e);
  void hideEvent(QHideEvent* e);

private:
  Ui::MainWindow* ui;
  QProgressBar* m_progress;
  StatusBarAction* m_notifications;
  StatusBarAction* m_update;
  QLabel* m_api;

  void visibilityChanged(bool visible);
};

#endif // MO_STATUSBAR_H
