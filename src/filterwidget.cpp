#include "filterwidget.h"
#include "eventfilter.h"

FilterWidget::FilterWidget()
  : m_edit(nullptr), m_eventFilter(nullptr), m_clear(nullptr)
{
}

void FilterWidget::set(QLineEdit* edit)
{
  unhook();

  m_edit = edit;

  if (!m_edit) {
    return;
  }

  createClear();
  hookEvents();
  clear();
}

void FilterWidget::clear()
{
  if (!m_edit) {
    return;
  }

  m_edit->clear();
}

bool FilterWidget::matches(const QString& s) const
{
  return s.contains(m_text);
}

void FilterWidget::unhook()
{
  if (m_clear) {
    delete m_clear;
    m_clear = nullptr;
  }

  if (m_edit) {
    m_edit->removeEventFilter(m_eventFilter);
  }
}

void FilterWidget::createClear()
{
  m_clear = new QToolButton(m_edit);

  QPixmap pixmap(":/MO/gui/edit_clear");
  m_clear->setIcon(QIcon(pixmap));
  m_clear->setIconSize(pixmap.size());
  m_clear->setCursor(Qt::ArrowCursor);
  m_clear->setStyleSheet("QToolButton { border: none; padding: 0px; }");
  m_clear->hide();

  QObject::connect(m_clear, &QToolButton::clicked, [&]{ clear(); });
  QObject::connect(m_edit, &QLineEdit::textChanged, [&]{ onTextChanged(); });

  repositionClearButton();
}

void FilterWidget::hookEvents()
{
  m_eventFilter = new EventFilter(m_edit, [&](auto* w, auto* e) {
    if (e->type() == QEvent::Resize) {
      onResized();
    }

    return false;
  });

  m_edit->installEventFilter(m_eventFilter);
}

void FilterWidget::onTextChanged()
{
  m_clear->setVisible(!m_edit->text().isEmpty());

  const auto text = m_edit->text();

  if (text != m_text) {
    m_text = text;

    if (changed) {
      changed();
    }
  }
}

void FilterWidget::onResized()
{
  repositionClearButton();
}

void FilterWidget::repositionClearButton()
{
  if (!m_clear) {
    return;
  }

  const QSize sz = m_clear->sizeHint();
  const int frame = m_edit->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  const auto r = m_edit->rect();

  const auto x = r.right() - frame - sz.width();
  const auto y = (r.bottom() + 1 - sz.height()) / 2;

  m_clear->move(x, y);
}
