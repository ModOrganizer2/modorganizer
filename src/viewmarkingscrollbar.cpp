#include "viewmarkingscrollbar.h"
#include "modelutils.h"
#include <QStyle>
#include <QStyleOptionSlider>
#include <QPainter>

ViewMarkingScrollBar::ViewMarkingScrollBar(QAbstractItemModel* model, int role, QWidget *parent)
  : QScrollBar(parent)
  , m_model(model)
  , m_role(role)
{
  // not implemented for horizontal sliders
  Q_ASSERT(this->orientation() == Qt::Vertical);
}

void ViewMarkingScrollBar::paintEvent(QPaintEvent* event)
{
  if (m_model == nullptr) {
    return;
  }
  QScrollBar::paintEvent(event);

  QStyleOptionSlider styleOption;
  initStyleOption(&styleOption);

  QPainter painter(this);

  QRect handleRect = style()->subControlRect(QStyle::CC_ScrollBar, &styleOption, QStyle::SC_ScrollBarSlider, this);
  QRect innerRect = style()->subControlRect(QStyle::CC_ScrollBar, &styleOption, QStyle::SC_ScrollBarGroove, this);

  auto indices = flatIndex(m_model, 0, QModelIndex());

  painter.translate(innerRect.topLeft() + QPoint(0, 3));
  qreal scale = static_cast<qreal>(innerRect.height() - 3) / static_cast<qreal>(indices.size());

  for (int i = 0; i < indices.size(); ++i) {
    QVariant data = indices[i].data(m_role);
    if (data.isValid()) {
      QColor col = data.value<QColor>();
      painter.setPen(col);
      painter.setBrush(col);
      painter.drawRect(QRect(2, i * scale - 2, handleRect.width() - 5, 3));
    }
  }
}
