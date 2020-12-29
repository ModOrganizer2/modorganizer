#include "modlistview.h"
#include <widgetutility.h>
#include <QUrl>
#include <QMimeData>
#include <QProxyStyle>

class ModListProxyStyle : public QProxyStyle {
public:

  using QProxyStyle::QProxyStyle;

  void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const
  {
    if (element == QStyle::PE_IndicatorItemViewItemDrop)
    {
      QStyleOption opt(*option);
      opt.rect.setLeft(20);
      if (auto* view = qobject_cast<const QTreeView*>(widget)) {
        opt.rect.setRight(widget->width());
      }
      QProxyStyle::drawPrimitive(element, &opt, painter, widget);
    }
    else {
      QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
  }
};

class ModListStyledItemDelegated : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    QStyleOptionViewItem opt(option);
    if (index.column() == 0)
      opt.rect.adjust(opt.rect.height(), 0, 0, 0);
    QStyledItemDelegate::paint(painter, opt, index);
    if (index.column() == 0) {
      QStyleOptionViewItem branch;
      branch.rect = QRect(0, opt.rect.y(), opt.rect.height(), opt.rect.height());
      branch.state = option.state;
      const QWidget* widget = option.widget;
      QStyle* style = widget ? widget->style() : QApplication::style();
      style->drawPrimitive(QStyle::PE_IndicatorBranch, &branch, painter, widget);
    }
  }
};

ModListView::ModListView(QWidget* parent)
  : QTreeView(parent)
  , m_scrollbar(new ViewMarkingScrollBar(this->model(), this))
{
  setVerticalScrollBar(m_scrollbar);
  MOBase::setCustomizableColumns(this);
  setAutoExpandDelay(1000);

  setIndentation(0);
  setStyle(new ModListProxyStyle(style()));
  setItemDelegate(new ModListStyledItemDelegated(this));
}

void ModListView::setModel(QAbstractItemModel* model)
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
  if (autoExpandDelay() >= 0) {
    m_openTimer.start(autoExpandDelay(), this);
  }

  // bypass QTreeView
  m_inDragMoveEvent = true;
  QAbstractItemView::dragMoveEvent(event);
  m_inDragMoveEvent = false;
}

void ModListView::dropEvent(QDropEvent* event)
{
  emit dropEntered(event->mimeData(), static_cast<DropPosition>(dropIndicatorPosition()));

  m_inDragMoveEvent = true;
  QTreeView::dropEvent(event);
  m_inDragMoveEvent = false;
}

void ModListView::timerEvent(QTimerEvent* event)
{
  if (event->timerId() == m_openTimer.timerId()) {
    QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
    if (state() == QAbstractItemView::DraggingState
      && viewport()->rect().contains(pos)) {
      QModelIndex index = indexAt(pos);
      setExpanded(index, true);
    }
    m_openTimer.stop();
  }
  else {
    QTreeView::timerEvent(event);
  }
}
