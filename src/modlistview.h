#ifndef MODLISTVIEW_H
#define MODLISTVIEW_H

#include <QTreeView>
#include <QDragEnterEvent>

class ModListView : public QTreeView
{
  Q_OBJECT
public:
  explicit ModListView(QWidget *parent = 0);
  virtual void dragEnterEvent(QDragEnterEvent *event);
signals:
  void dropModeUpdate(bool dropOnRows);
  
public slots:
  
};

#endif // MODLISTVIEW_H
