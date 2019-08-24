#include "expanderwidget.h"

ExpanderWidget::ExpanderWidget()
  : m_button(nullptr), m_content(nullptr), opened_(false)
{
}

ExpanderWidget::ExpanderWidget(QToolButton* button, QWidget* content)
  : ExpanderWidget()
{
  set(button, content);
}

void ExpanderWidget::set(QToolButton* button, QWidget* content, bool o)
{
  m_button = button;
  m_content = content;

  m_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  QObject::connect(m_button, &QToolButton::clicked, [&]{ toggle(); });

  toggle(o);
}

void ExpanderWidget::toggle()
{
  if (opened()) {
    toggle(false);
  }
  else {
    toggle(true);
  }
}

void ExpanderWidget::toggle(bool b)
{
  if (b) {
    m_button->setArrowType(Qt::DownArrow);
    m_content->show();
  } else {
    m_button->setArrowType(Qt::RightArrow);
    m_content->hide();
  }

  // the state has to be remembered instead of using m_content's visibility
  // because saving the state in saveConflictExpandersState() happens after the
  // dialog is closed, which marks all the widgets hidden
  opened_ = b;
}

bool ExpanderWidget::opened() const
{
  return opened_;
}

QByteArray ExpanderWidget::saveState() const
{
  QByteArray result;
  QDataStream stream(&result, QIODevice::WriteOnly);

  stream << opened();

  return result;
}

void ExpanderWidget::restoreState(const QByteArray& a)
{
  QDataStream stream(a);

  bool opened = false;
  stream >> opened;

  if (stream.status() == QDataStream::Ok) {
    toggle(opened);
  }
}

QToolButton* ExpanderWidget::button() const
{
  return m_button;
}
