#ifndef MO_STATUSBAR_H
#define MO_STATUSBAR_H

#include <QProgressBar>
#include <QStatusBar>

struct APIStats;
class APIUserAccount;
class Settings;
class OrganizerCore;

namespace Ui
{
class MainWindow;
}

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
  Q_OBJECT;

public:
  StatusBar(QWidget* parent = nullptr);

  void setup(Ui::MainWindow* ui, const Settings& settings);

  void setProgress(int percent);
  void setNotifications(bool hasNotifications);
  void setAPI(const APIStats& stats, const APIUserAccount& user);
  void setUpdateAvailable(bool b);
  void checkSettings(const Settings& settings);
  void updateNormalMessage(OrganizerCore& core);

protected:
  void showEvent(QShowEvent* e);
  void hideEvent(QHideEvent* e);

private:
  Ui::MainWindow* ui;
  QLabel* m_normal;
  QProgressBar* m_progress;
  QWidget* m_progressSpacer1;
  QWidget* m_progressSpacer2;
  StatusBarAction* m_notifications;
  StatusBarAction* m_update;
  QLabel* m_api;

  void visibilityChanged(bool visible);
};

#endif  // MO_STATUSBAR_H
