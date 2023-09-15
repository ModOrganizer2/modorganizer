#include "viewmarkingscrollbar.h"
#include "modelutils.h"
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>

using namespace MOShared;

ViewMarkingScrollBar::ViewMarkingScrollBar(QTreeView* view, int role)
    : QScrollBar(view), m_view(view), m_role(role)
{
  // not implemented for horizontal sliders
  Q_ASSERT(this->orientation() == Qt::Vertical);
}

QColor ViewMarkingScrollBar::color(const QModelIndex& index) const
{
  auto data = index.data(m_role);
  if (data.canConvert<QColor>()) {
    return data.value<QColor>();
  }
  return QColor();
}

void ViewMarkingScrollBar::paintEvent(QPaintEvent* event)
{
  if (m_view->model() == nullptr) {
    return;
  }
  QScrollBar::paintEvent(event);

  QStyleOptionSlider styleOption;
  initStyleOption(&styleOption);

  QPainter painter(this);

  QRect handleRect = style()->subControlRect(QStyle::CC_ScrollBar, &styleOption,
                                             QStyle::SC_ScrollBarSlider, this);
  QRect innerRect  = style()->subControlRect(QStyle::CC_ScrollBar, &styleOption,
                                             QStyle::SC_ScrollBarGroove, this);

  auto indices = visibleIndex(m_view, 0);

  painter.translate(innerRect.topLeft() + QPoint(0, 3));
  qreal scale =
      static_cast<qreal>(innerRect.height() - 3) / static_cast<qreal>(indices.size());

  for (int i = 0; i < indices.size(); ++i) {
    QColor color = this->color(indices[i]);
    if (color.isValid()) {
      painter.setPen(color);
      painter.setBrush(color);
      painter.drawRect(QRect(2, i * scale - 2, handleRect.width() - 5, 3));
    }
  }
}
