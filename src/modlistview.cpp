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

QModelIndexList ModListView::selectedIndexes() const
{
  return m_inDragMoveEvent ? QModelIndexList() : QTreeView::selectedIndexes();
}

void ModListView::dragEnterEvent(QDragEnterEvent* event)
{
  emit dragEntered(event->mimeData());
  QTreeView::dragEnterEvent(event);
}

void ModListView::dragMoveEvent(QDragMoveEvent* event)
{
  m_inDragMoveEvent = true;
  QTreeView::dragMoveEvent(event);
  m_inDragMoveEvent = false;
}

void ModListView::dropEvent(QDropEvent* event)
{
  m_inDragMoveEvent = true;
  QTreeView::dropEvent(event);
  m_inDragMoveEvent = false;
}

