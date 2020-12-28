#include "modlistview.h"
#include <widgetutility.h>
#include <QUrl>
#include <QMimeData>

ModListView::ModListView(QWidget *parent)
  : QTreeView(parent)
  , m_Scrollbar(new ViewMarkingScrollBar(this->model(), this))
{
  setVerticalScrollBar(m_Scrollbar);
  MOBase::setCustomizableColumns(this);
  setAutoExpandDelay(500);
}

void ModListView::setModel(QAbstractItemModel *model)
{
  QTreeView::setModel(model);
  setVerticalScrollBar(new ViewMarkingScrollBar(model, this));
}

void ModListView::dragEnterEvent(QDragEnterEvent* event)
{
  emit dragEntered(event->mimeData());
  QTreeView::dragEnterEvent(event);
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
