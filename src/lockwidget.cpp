#include "lockwidget.h"
#include "mainwindow.h"
#include <QGraphicsDropShadowEffect>
#include <QMenuBar>
#include <QStatusBar>

class UILockerInterface
{
public:
  UILockerInterface(QWidget* mainUI) :
    m_mainUI(mainUI), m_target(nullptr), m_message(nullptr), m_info(nullptr),
    m_buttons(nullptr), m_reason(UILocker::NoReason)
  {
    m_timer.reset(new QTimer);
    QObject::connect(m_timer.get(), &QTimer::timeout, [&]{ checkTarget(); });
    m_timer->start(200);

    set();
  }

  ~UILockerInterface()
  {
  }

  void checkTarget()
  {
    if (set()) {
      update(m_reason);
    }
  }

  bool set()
  {
    QWidget* newTarget = nullptr;

    newTarget = m_mainUI;
    if (auto* w = QApplication::activeModalWidget()) {
      newTarget = w;
    }

    if (newTarget == m_target) {
      return false;
    }

    m_target = newTarget;

    QFrame* center = nullptr;

    if (m_target) {
      center = createOverlay(m_target);
    } else {
      center = createDialog();
    }

    createMessageLabel();
    createInfoLabel();
    createButtonsPanel();

    center->layout()->addWidget(m_message);
    center->layout()->addWidget(m_info);
    center->layout()->addWidget(m_buttons);

    m_topLevel->setFocusPolicy(Qt::TabFocus);
    m_topLevel->setFocus();
    m_topLevel->show();
    m_topLevel->setEnabled(true);

    m_topLevel->raise();
    m_topLevel->activateWindow();

    return true;
  }

  void update(UILocker::Reasons reason)
  {
    m_reason = reason;
    updateMessage(reason);
    updateButtons(reason);
    setInfo(m_labels);
  }

  void setInfo(const QStringList& labels)
  {
    const int MaxLabels = 2;

    m_labels = labels;

    QString s;

    if (labels.size() > MaxLabels) {
      s = labels.mid(0, MaxLabels).join(", ") + "...";
    } else {
      s = labels.join(", ");
    }

    m_info->setText(s);
  }

  QWidget* topLevel()
  {
    return m_topLevel.get();
  }

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


  std::unique_ptr<QTimer> m_timer;
  QWidget* m_mainUI;
  QWidget* m_target;
  std::unique_ptr<QWidget> m_topLevel;
  QLabel* m_message;
  QLabel* m_info;
  QStringList m_labels;
  QWidget* m_buttons;
  std::unique_ptr<Filter> m_filter;
  UILocker::Reasons m_reason;


  bool hasMainUI() const
  {
    return (m_target != nullptr);
  }

  QWidget* createTransparentWidget(QWidget* parent=nullptr)
  {
    auto* w = new QWidget(parent);

    w->setWindowOpacity(0);
    w->setAttribute(Qt::WA_NoSystemBackground);
    w->setAttribute(Qt::WA_TranslucentBackground);

    return w;
  }

  QFrame* createOverlay(QWidget* mainUI)
  {
    m_topLevel.reset(createTransparentWidget(mainUI));
    m_topLevel->setWindowFlags(m_topLevel->windowFlags() & Qt::FramelessWindowHint);
    m_topLevel->setGeometry(mainUI->rect());

    m_filter.reset(new Filter);
    m_filter->resized = [=]{ m_topLevel->setGeometry(mainUI->rect()); };
    m_filter->closed = [=]{ checkTarget(); };

    mainUI->installEventFilter(m_filter.get());

    return createFrame();
  }

  QFrame* createDialog()
  {
    m_topLevel.reset(new QDialog);

    return createFrame();
  }

  QFrame* createFrame()
  {
    auto* frame = new QFrame;
    auto* ly = new QVBoxLayout(frame);

    if (hasMainUI()) {
      frame->setFrameStyle(QFrame::StyledPanel);
      frame->setLineWidth(1);
      frame->setAutoFillBackground(true);

      auto* shadow = new QGraphicsDropShadowEffect;
      shadow->setBlurRadius(50);
      shadow->setOffset(0);
      shadow->setColor(QColor(0, 0, 0, 100));
      frame->setGraphicsEffect(shadow);
    } else {
      ly->setContentsMargins(0, 0, 0, 0);
    }

    auto* grid = new QGridLayout(m_topLevel.get());
    grid->addWidget(createTransparentWidget(), 0, 1);
    grid->addWidget(createTransparentWidget(), 2, 1);
    grid->addWidget(createTransparentWidget(), 1, 0);
    grid->addWidget(createTransparentWidget(), 1, 2);
    grid->addWidget(frame, 1, 1);

    if (!hasMainUI()) {
      grid->setContentsMargins(0, 0, 0, 0);
    }

    grid->setRowStretch(0, 1);
    grid->setRowStretch(2, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(2, 1);

    return frame;
  }

  void createMessageLabel()
  {
    m_message = new QLabel;
    m_message->setAlignment(Qt::AlignCenter | Qt::AlignHCenter);
  }

  void createInfoLabel()
  {
    m_info = new QLabel(" ");
    m_info->setAlignment(Qt::AlignCenter | Qt::AlignHCenter);
  }

  void createButtonsPanel()
  {
    m_buttons = new QWidget;
    m_buttons->setLayout(new QHBoxLayout);
  }


  void updateMessage(UILocker::Reasons reason)
  {
    switch (reason)
    {
      case UILocker::LockUI:
      {
        QString s;

        if (hasMainUI()) {
          s = QObject::tr(
            "Mod Organizer is locked while the application is running.");
        } else {
          s = QObject::tr("Mod Organizer is currently running an application.");
        }

        m_message->setText(s);

        break;
      }

      case UILocker::OutputRequired:
      {
        m_message->setText(QObject::tr(
          "The application must run to completion because its output is "
          "required."));

        break;
      }

      case UILocker::PreventExit:
      {
        m_message->setText(QObject::tr(
          "Mod Organizer is waiting on application to close before exiting."));

        break;
      }
    }
  }

  void updateButtons(UILocker::Reasons reason)
  {
    MOBase::deleteChildWidgets(m_buttons);
    auto* ly = m_buttons->layout();

    switch (reason)
    {
      case UILocker::LockUI: // fall-through
      case UILocker::OutputRequired:
      {
        auto* unlock = new QPushButton(QObject::tr("Unlock"));

        QObject::connect(unlock, &QPushButton::clicked, [&]{
          UILocker::instance().onForceUnlock();
        });

        ly->addWidget(unlock);

        break;
      }

      case UILocker::PreventExit:
      {
        auto* exit = new QPushButton(QObject::tr("Exit Now"));
        QObject::connect(exit, &QPushButton::clicked, [&]{
          UILocker::instance().onForceUnlock();
        });

        ly->addWidget(exit);

        auto* cancel = new QPushButton(QObject::tr("Cancel"));
        QObject::connect(cancel, &QPushButton::clicked, [&]{
          UILocker::instance().onCancel();
        });

        ly->addWidget(cancel);

        break;
      }
    }
  }
};

UILocker::Session::~Session()
{
  unlock();
}

void UILocker::Session::unlock()
{
  QMetaObject::invokeMethod(qApp, [this]{
    UILocker::instance().unlock(this);
  });
}

void UILocker::Session::setInfo(DWORD pid, const QString& name)
{
  {
    std::scoped_lock lock(m_mutex);
    m_pid = pid;
    m_name = name;
  }

  QMetaObject::invokeMethod(qApp, [this]{
    UILocker::instance().updateLabel();
  });
}

DWORD UILocker::Session::pid() const
{
  std::scoped_lock lock(m_mutex);
  return m_pid;
}

const QString& UILocker::Session::name() const
{
  std::scoped_lock lock(m_mutex);
  return m_name;
}

UILocker::Results UILocker::Session::result() const
{
  return UILocker::instance().result();
}


static UILocker* g_instance = nullptr;


UILocker::UILocker()
  : m_parent(nullptr), m_result(NoResult)
{
  Q_ASSERT(!g_instance);
  g_instance = this;
}

UILocker::~UILocker()
{
  const auto v = m_sessions;

  for (auto& wp : v) {
    if (auto s=wp.lock()) {
      unlock(s.get());
    }
  }
}

UILocker& UILocker::instance()
{
  Q_ASSERT(g_instance);
  return *g_instance;
}

void UILocker::setUserInterface(QWidget* parent)
{
  m_parent = parent;
}

std::shared_ptr<UILocker::Session> UILocker::lock(Reasons reason)
{
  m_result = StillLocked;
  createUi(reason);

  auto ls = std::make_shared<Session>();
  m_sessions.push_back(ls);

  updateLabel();

  return ls;
}

void UILocker::unlock(Session* s)
{
  auto itor = m_sessions.begin();
  for (;;) {
    if (itor == m_sessions.end()) {
      break;
    }

    if (auto ss=itor->lock()) {
      if (ss.get() == s) {
        itor = m_sessions.erase(itor);
        continue;
      }
    } else {
      itor = m_sessions.erase(itor);
      continue;
    }

    ++itor;
  }

  if (m_sessions.empty()) {
    m_ui.reset();
    enableAll();
  } else {
    updateLabel();
  }
}

void UILocker::unlockCurrent()
{
  if (m_sessions.empty()) {
    return;
  }

  auto s = m_sessions.back().lock();
  if (!s) {
    m_sessions.pop_back();
    return;
  }

  unlock(s.get());
}

void UILocker::updateLabel()
{
  QStringList labels;

  for (auto itor=m_sessions.rbegin(); itor!=m_sessions.rend(); ++itor) {
    if (auto ss=itor->lock()) {
      labels.push_back(QString("%1 (%2)").arg(ss->name()).arg(ss->pid()));
    }
  }

  m_ui->setInfo(labels);
}

UILocker::Results UILocker::result() const
{
  return m_result;
}

void UILocker::createUi(Reasons reason)
{
  if (!m_ui) {
    m_ui.reset(new UILockerInterface(m_parent));
  }

  m_ui->update(reason);

  disableAll();
}

void UILocker::onForceUnlock()
{
  m_result = ForceUnlocked;
  unlockCurrent();
}

void UILocker::onCancel()
{
  m_result = Cancelled;
  unlockCurrent();
}

template <class T>
QList<T> findChildrenImmediate(QWidget* parent)
{
  return parent->findChildren<T>(QString(), Qt::FindDirectChildrenOnly);
}

void UILocker::disableAll()
{
  const auto topLevels = QApplication::topLevelWidgets();

  for (auto* w : topLevels) {
    if (auto* mw=dynamic_cast<QMainWindow*>(w)) {
      disable(mw->centralWidget());
      disable(mw->menuBar());
      disable(mw->statusBar());

      for (auto* tb : findChildrenImmediate<QToolBar*>(w)) {
        disable(tb);
      }

      for (auto* d : findChildrenImmediate<QDockWidget*>(w)) {
        disable(d);
      }
    }

    if (auto* d=dynamic_cast<QDialog*>(w)) {
      // don't disable stuff if this dialog is the overlay, which happens when
      // there's no ui
      if (d != m_ui->topLevel()) {
        // no central widget, just disable the children, except for the overlay
        for (auto* child : findChildrenImmediate<QWidget*>(d)) {
          if (child != m_ui->topLevel()) {
            disable(child);
          }
        }
      }
    }
  }
}

void UILocker::enableAll()
{
  for (auto w : m_disabled) {
    if (w) {
      w->setEnabled(true);
    }
  }

  m_disabled.clear();
}

void UILocker::disable(QWidget* w)
{
  if (w->isEnabled()) {
    w->setEnabled(false);
    m_disabled.push_back(w);
  }
}
