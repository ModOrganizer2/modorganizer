#include "viewmarkingscrollbar.h"
#include <QStyle>
#include <QStyleOptionSlider>
#include <QPainter>

ViewMarkingScrollBar::ViewMarkingScrollBar(QAbstractItemModel *model, QWidget *parent, int role)
  : QScrollBar(parent)
  , m_Model(model)
  , m_Role(role)
{
  // not implemented for horizontal sliders
  Q_ASSERT(this->orientation() == Qt::Vertical);
}

void ViewMarkingScrollBar::paintEvent(QPaintEvent *event)
{
  if (m_Model == nullptr) {
    return;
  }
  QScrollBar::paintEvent(event);

  QStyleOptionSlider styleOption;
  initStyleOption(&styleOption);

  QPainter painter(this);

  QRect handleRect = style()->subControlRect(QStyle::CC_ScrollBar, &styleOption, QStyle::SC_ScrollBarSlider, this);
  QRect innerRect = style()->subControlRect(QStyle::CC_ScrollBar, &styleOption, QStyle::SC_ScrollBarGroove, this);

  painter.translate(innerRect.topLeft() + QPoint(0, 3));
  qreal scale = static_cast<qreal>(innerRect.height() - 3) / static_cast<qreal>(m_Model->rowCount());

  for (int i = 0; i < m_Model->rowCount(); ++i) {
    QVariant data = m_Model->data(m_Model->index(i, 0), m_Role);
    if (data.isValid()) {
      QColor col = data.value<QColor>();
      painter.setPen(col);
      painter.setBrush(col);
      painter.drawRect(QRect(2, i * scale - 2, handleRect.width() - 5, 3));
    }
  }
}
