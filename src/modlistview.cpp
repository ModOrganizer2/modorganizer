#include "modlistview.h"
#include <widgetutility.h>
#include <QUrl>
#include <QMimeData>
#include <QProxyStyle>

class ModListViewStyle: public QProxyStyle {
public:
  ModListViewStyle(QStyle *style, int indentation);

  void drawPrimitive (PrimitiveElement element, const QStyleOption *option,
                      QPainter *painter, const QWidget *widget = 0) const;
private:
  int m_Indentation;
};

ModListViewStyle::ModListViewStyle(QStyle *style, int indentation)
  : QProxyStyle(style), m_Indentation(indentation)
{
}

void ModListViewStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                                     QPainter *painter, const QWidget *widget) const
{
  if (element == QStyle::PE_IndicatorItemViewItemDrop && !option->rect.isNull()) {
    QStyleOption opt(*option);
    opt.rect.setLeft(m_Indentation);
    if (widget) {
      opt.rect.setRight(widget->width() - 5); // 5 is an arbitrary value that seems to work ok
    }
    QProxyStyle::drawPrimitive(element, &opt, painter, widget);
  } else {
    QProxyStyle::drawPrimitive(element, option, painter, widget);
  }
}

ModListView::ModListView(QWidget *parent)
  : QTreeView(parent)
  , m_Scrollbar(new ViewMarkingScrollBar(this->model(), this))
{
  setVerticalScrollBar(m_Scrollbar);
  MOBase::setCustomizableColumns(this);
  setAutoExpandDelay(500);
}

void ModListView::dragEnterEvent(QDragEnterEvent *event)
{
  emit dropModeUpdate(event->mimeData()->hasUrls());

  QTreeView::dragEnterEvent(event);
}

void ModListView::setModel(QAbstractItemModel *model)
{
  QTreeView::setModel(model);
  setVerticalScrollBar(new ViewMarkingScrollBar(model, this));
}

void ModListView::dragMoveEvent(QDragMoveEvent* event)
{
  if (autoExpandDelay() >= 0) {
    openTimer.start(autoExpandDelay(), this);
  }
  QAbstractItemView::dragMoveEvent(event);
}

void ModListView::timerEvent(QTimerEvent* event)
{
  if (event->timerId() == openTimer.timerId()) {
    QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
    if (state() == QAbstractItemView::DraggingState
      && viewport()->rect().contains(pos)) {
      QModelIndex index = indexAt(pos);
      setExpanded(index, true);
    }
    openTimer.stop();
  }
  else {
    QTreeView::timerEvent(event);
  }
}
