#ifndef MODORGANIZER_UILOCKER_INCLUDED
#define MODORGANIZER_UILOCKER_INCLUDED

#include <QMainWindow>
#include <mutex>

class UILockerInterface;

class UILocker
{
  friend class UILockerInterface;

public:
  // reason to show the widget
  //
  enum Reasons
  {
    NoReason = 0,

    // lock the ui
    LockUI,

    // because the output is required
    OutputRequired,

    // to prevent exiting until all processes are completed
    PreventExit
  };

  // returned by result()
  //
  enum Results
  {
    NoResult = 0,

    // the widget is still up
    StillLocked,

    // force unlock was clicked
    ForceUnlocked,

    // cancel was clicked
    Cancelled
  };

  class Session
  {
  public:
    ~Session();

    void unlock();
    void setInfo(DWORD pid, const QString& name);
    Results result() const;

    DWORD pid() const;
    const QString& name() const;

  private:
    mutable std::mutex m_mutex;
    DWORD m_pid;
    QString m_name;
  };

  UILocker();
  ~UILocker();

  static UILocker& instance();

  void setUserInterface(QWidget* parent);

  std::shared_ptr<Session> lock(Reasons reason);
  bool locked() const;

  Results result() const;

private:
  QWidget* m_parent;
  std::unique_ptr<UILockerInterface> m_ui;
  std::vector<std::weak_ptr<Session>> m_sessions;
  std::atomic<Results> m_result;
  std::vector<QPointer<QWidget>> m_disabled;

  void createUi(Reasons reason);

  void unlockCurrent();
  void unlock(Session* s);
  void updateLabel();

  void onForceUnlock();
  void onCancel();

  void disableAll();
  void enableAll();
  void disable(QWidget* w);
};

#endif  // MODORGANIZER_UILOCKER_INCLUDED
