#ifndef VIEWMARKINGSCROLLBAR_H
#define VIEWMARKINGSCROLLBAR_H

#include <QAbstractItemModel>
#include <QScrollBar>


class ViewMarkingScrollBar : public QScrollBar
{
public:
  ViewMarkingScrollBar(QAbstractItemModel *model, int role, QWidget* parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QAbstractItemModel* m_model;
  int m_role;
};


#endif // VIEWMARKINGSCROLLBAR_H
