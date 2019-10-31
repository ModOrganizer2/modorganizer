#pragma once

#include <QMainWindow>

class LockWidget
{
public:
  enum Reasons
  {
    NoReason = 0,
    LockUI,
    OutputRequired,
    PreventExit
  };

  enum Results
  {
    NoResult = 0,
    StillLocked,
    ForceUnlocked,
    Cancelled
  };

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

  protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
      if (e->type() == QEvent::Resize) {
        if (resized) {
          resized();
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
  std::vector<QWidget*> m_disabled;

  void createUi(Reasons reason);

  void onForceUnlock();
  void onCancel();

  void disableAll();
  void enableAll();
  void disable(QWidget* w);
};
