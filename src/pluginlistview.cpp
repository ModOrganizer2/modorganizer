#include "pluginlistview.h"
#include <QUrl>
#include <QMimeData>
#include <QProxyStyle>


class PluginListViewStyle : public QProxyStyle {
public:
  PluginListViewStyle(QStyle *style, int indentation);

  void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
    QPainter *painter, const QWidget *widget = 0) const;
private:
  int m_Indentation;
};

PluginListViewStyle::PluginListViewStyle(QStyle *style, int indentation)
  : QProxyStyle(style), m_Indentation(indentation)
{
}

void PluginListViewStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
  QPainter *painter, const QWidget *widget) const
{
  if (element == QStyle::PE_IndicatorItemViewItemDrop && !option->rect.isNull()) {
    QStyleOption opt(*option);
    opt.rect.setLeft(m_Indentation);
    if (widget) {
      opt.rect.setRight(widget->width() - 5); // 5 is an arbitrary value that seems to work ok
    }
    QProxyStyle::drawPrimitive(element, &opt, painter, widget);
  }
  else {
    QProxyStyle::drawPrimitive(element, option, painter, widget);
  }
}

PluginListView::PluginListView(QWidget *parent)
  : QTreeView(parent)
  , m_Scrollbar(new ViewMarkingScrollBar(this->model(), this))
{
  setVerticalScrollBar(m_Scrollbar);
}

void PluginListView::dragEnterEvent(QDragEnterEvent *event)
{
  emit dropModeUpdate(event->mimeData()->hasUrls());

  QTreeView::dragEnterEvent(event);
}

void PluginListView::setModel(QAbstractItemModel *model)
{
  QTreeView::setModel(model);
  setVerticalScrollBar(new ViewMarkingScrollBar(model, this));
}

#pragma once
