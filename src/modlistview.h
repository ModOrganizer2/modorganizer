#ifndef MODLISTVIEW_H
#define MODLISTVIEW_H

#include <QTreeView>
#include <QDragEnterEvent>
#include "viewmarkingscrollbar.h"

class ModListView : public QTreeView
{
  Q_OBJECT
public:
  explicit ModListView(QWidget *parent = 0);
  void setModel(QAbstractItemModel *model) override;

  QModelIndexList selectedIndexes() const;

signals:

  void dragEntered(const QMimeData* mimeData);

protected:

  bool m_inDragMoveEvent = false;

  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

private:

  ViewMarkingScrollBar *m_Scrollbar;

};

#endif // MODLISTVIEW_H
