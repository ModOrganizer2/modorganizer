#pragma once

#include <QMainWindow>

class LockWidget
{
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

  // if `reason` is not NoReason, lock() is called with it
  //
  LockWidget(QWidget* parent, Reasons reason=NoReason);
  ~LockWidget();

  void lock(Reasons reason);
  void unlock();

  void setInfo(DWORD pid, const QString& name);
  Results result() const;

private:
  class Filter : public QObject
  {
  public:
    std::function<void ()> resized;
    std::function<void ()> closed;

  protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
      if (e->type() == QEvent::Resize) {
        if (resized) {
          resized();
        }
      } else if (e->type() == QEvent::Close) {
        if (closed) {
          closed();
        }
      }

      return QObject::eventFilter(o, e);
    }
  };


  QWidget* m_parent;
  std::unique_ptr<QWidget> m_overlay;
  QLabel* m_info;
  Results m_result;
  std::unique_ptr<Filter> m_filter;
  std::vector<QPointer<QWidget>> m_disabled;

  void createUi(Reasons reason);

  void onForceUnlock();
  void onCancel();

  void disableAll();
  void enableAll();
  void disable(QWidget* w);
};
