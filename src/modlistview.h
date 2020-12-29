#ifndef MODLISTVIEW_H
#define MODLISTVIEW_H

#include <QTreeView>
#include <QDragEnterEvent>
#include "viewmarkingscrollbar.h"

class ModListView : public QTreeView
{
  Q_OBJECT

public:

  // this is a public version of DropIndicatorPosition
  enum DropPosition {
    OnItem = DropIndicatorPosition::OnItem,
    AboveItem = DropIndicatorPosition::AboveItem,
    BelowItem = DropIndicatorPosition::BelowItem,
    OnViewport = DropIndicatorPosition::OnViewport
  };

public:
  explicit ModListView(QWidget* parent = 0);
  void setModel(QAbstractItemModel* model) override;

signals:

  void dragEntered(const QMimeData* mimeData);
  void dropEntered(const QMimeData* mimeData, DropPosition position);

protected:

  // re-implemented to fake the return value to allow drag-and-drop on
  // itself for separators
  //
  QModelIndexList selectedIndexes() const;

  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

private:

  ViewMarkingScrollBar* m_scrollbar;
  bool m_inDragMoveEvent = false;

};

#endif // MODLISTVIEW_H
