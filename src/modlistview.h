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

protected:

  // replace the auto-expand timer from QTreeView to avoid
  // auto-collapsing
  QBasicTimer openTimer;

  void timerEvent(QTimerEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;

private:

  ViewMarkingScrollBar *m_Scrollbar;

};

#endif // MODLISTVIEW_H
