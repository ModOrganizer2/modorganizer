#include "lockwidget.h"
#include "mainwindow.h"
#include <QGraphicsDropShadowEffect>
#include <QMenuBar>
#include <QStatusBar>

class LockInterface
{
public:
  LockInterface() :
    m_hasMainUI(false), m_message(nullptr), m_info(nullptr), m_buttons(nullptr)
  {
  }

  ~LockInterface()
  {
  }

  void set(QWidget* target)
  {
    QFrame* center = nullptr;

    if (target) {
      center = createOverlay(target);
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
  }

  void update(LockWidget::Reasons reason)
  {
    updateMessage(reason);
    updateButtons(reason);
  }

  void setInfo(const QString& s)
  {
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


  bool m_hasMainUI;
  std::unique_ptr<QWidget> m_topLevel;
  QLabel* m_message;
  QLabel* m_info;
  QWidget* m_buttons;
  std::unique_ptr<Filter> m_filter;

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
    m_hasMainUI = true;

    m_topLevel.reset(createTransparentWidget(mainUI));
    m_topLevel->setWindowFlags(m_topLevel->windowFlags() & Qt::FramelessWindowHint);
    m_topLevel->setGeometry(mainUI->rect());

    m_filter.reset(new Filter);
    m_filter->resized = [=]{ m_topLevel->setGeometry(mainUI->rect()); };
    m_filter->closed = [=]{ LockWidget::instance().onForceUnlock(); };

    mainUI->installEventFilter(m_filter.get());

    return createFrame();
  }

  QFrame* createDialog()
  {
    m_hasMainUI = false;
    m_topLevel.reset(new QDialog);

    return createFrame();
  }

  QFrame* createFrame()
  {
    auto* frame = new QFrame;
    auto* ly = new QVBoxLayout(frame);

    if (m_hasMainUI) {
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

    if (!m_hasMainUI) {
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


  void updateMessage(LockWidget::Reasons reason)
  {
    switch (reason)
    {
      case LockWidget::LockUI:
      {
        QString s;

        if (m_hasMainUI) {
          s = QObject::tr(
            "Mod Organizer is locked while the application is running.");
        } else {
          s = QObject::tr("Mod Organizer is currently running an application.");
        }

        m_message->setText(s);

        break;
      }

      case LockWidget::OutputRequired:
      {
        m_message->setText(QObject::tr(
          "The application must run to completion because its output is "
          "required."));

        break;
      }

      case LockWidget::PreventExit:
      {
        m_message->setText(QObject::tr(
          "Mod Organizer is waiting on application to close before exiting."));

        break;
      }
    }
  }

  void updateButtons(LockWidget::Reasons reason)
  {
    MOBase::deleteChildWidgets(m_buttons);
    auto* ly = m_buttons->layout();

    switch (reason)
    {
      case LockWidget::LockUI: // fall-through
      case LockWidget::OutputRequired:
      {
        auto* unlock = new QPushButton(QObject::tr("Unlock"));

        QObject::connect(unlock, &QPushButton::clicked, [&]{
          LockWidget::instance().onForceUnlock();
        });

        ly->addWidget(unlock);

        break;
      }

      case LockWidget::PreventExit:
      {
        auto* exit = new QPushButton(QObject::tr("Exit Now"));
        QObject::connect(exit, &QPushButton::clicked, [&]{
          LockWidget::instance().onForceUnlock();
        });

        ly->addWidget(exit);

        auto* cancel = new QPushButton(QObject::tr("Cancel"));
        QObject::connect(cancel, &QPushButton::clicked, [&]{
          LockWidget::instance().onCancel();
        });

        ly->addWidget(cancel);

        break;
      }
    }
  }
};

LockWidget::Session::~Session()
{
  LockWidget::instance().unlock(this);
}

void LockWidget::Session::setInfo(DWORD pid, const QString& name)
{
  m_pid = pid;
  m_name = name;

  LockWidget::instance().updateLabel();
}

DWORD LockWidget::Session::pid() const
{
  return m_pid;
}

const QString& LockWidget::Session::name() const
{
  return m_name;
}

LockWidget::Results LockWidget::Session::result() const
{
  return LockWidget::instance().result();
}


static LockWidget* g_instance = nullptr;


LockWidget::LockWidget()
  : m_parent(nullptr), m_result(NoResult)
{
  Q_ASSERT(!g_instance);
  g_instance = this;
}

LockWidget::~LockWidget()
{
  const auto v = m_sessions;

  for (auto& wp : v) {
    if (auto s=wp.lock()) {
      unlock(s.get());
    }
  }
}

LockWidget& LockWidget::instance()
{
  Q_ASSERT(g_instance);
  return *g_instance;
}

void LockWidget::setUserInterface(QWidget* parent)
{
  m_parent = parent;
}

std::shared_ptr<LockWidget::Session> LockWidget::lock(Reasons reason)
{
  m_result = StillLocked;
  createUi(reason);

  auto ls = std::make_shared<Session>();
  m_sessions.push_back(ls);

  updateLabel();

  return ls;
}

void LockWidget::unlock(Session* s)
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

void LockWidget::unlockCurrent()
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

void LockWidget::updateLabel()
{
  QString label;

  for (auto itor=m_sessions.rbegin(); itor!=m_sessions.rend(); ++itor) {
    if (auto ss=itor->lock()) {
      label += QString("%1 (%2)").arg(ss->name()).arg(ss->pid());
      break;
    }
  }

  m_ui->setInfo(label);
}

LockWidget::Results LockWidget::result() const
{
  return m_result;
}

void LockWidget::createUi(Reasons reason)
{
  QWidget* target = m_parent;
  if (auto* w = qApp->activeWindow()) {
    target = w;
  }

  if (!m_ui) {
    m_ui.reset(new LockInterface);
  }

  m_ui->set(target);
  m_ui->update(reason);

  disableAll();
}

void LockWidget::onForceUnlock()
{
  m_result = ForceUnlocked;
  unlockCurrent();
}

void LockWidget::onCancel()
{
  m_result = Cancelled;
  unlockCurrent();
}

template <class T>
QList<T> findChildrenImmediate(QWidget* parent)
{
  return parent->findChildren<T>(QString(), Qt::FindDirectChildrenOnly);
}

void LockWidget::disableAll()
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

void LockWidget::enableAll()
{
  for (auto w : m_disabled) {
    if (w) {
      w->setEnabled(true);
    }
  }

  m_disabled.clear();
}

void LockWidget::disable(QWidget* w)
{
  if (w->isEnabled()) {
    w->setEnabled(false);
    m_disabled.push_back(w);
  }
}
