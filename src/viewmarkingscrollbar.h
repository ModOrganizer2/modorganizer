#ifndef VIEWMARKINGSCROLLBAR_H
#define VIEWMARKINGSCROLLBAR_H

#include <QScrollBar>
#include <QTreeView>

class ViewMarkingScrollBar : public QScrollBar
{
public:
  ViewMarkingScrollBar(QTreeView* view, int role);

protected:
  void paintEvent(QPaintEvent* event) override;

  // retrieve the color of the marker for the given index
  //
  virtual QColor color(const QModelIndex& index) const;

private:
  QTreeView* m_view;
  int m_role;
};

#endif  // VIEWMARKINGSCROLLBAR_H
