#ifndef VIEWMARKINGSCROLLBAR_H
#define VIEWMARKINGSCROLLBAR_H

#include <QTreeView>
#include <QScrollBar>


class ViewMarkingScrollBar : public QScrollBar
{
public:
  ViewMarkingScrollBar(QTreeView* view, int role);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QTreeView* m_view;
  int m_role;
};


#endif // VIEWMARKINGSCROLLBAR_H
