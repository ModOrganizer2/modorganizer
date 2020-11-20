#ifndef MODLISTVIEW_H
#define MODLISTVIEW_H

#include "viewmarkingscrollbar.h"

class ModListView : public QTreeView
{
  Q_OBJECT
public:
  explicit ModListView(QWidget *parent = 0);
  virtual void dragEnterEvent(QDragEnterEvent *event);
  virtual void setModel(QAbstractItemModel *model);
signals:
  void dropModeUpdate(bool dropOnRows);
  
public slots:
private:

  ViewMarkingScrollBar *m_Scrollbar;
};

#endif // MODLISTVIEW_H
