#ifndef VIEWMARKINGSCROLLBAR_H
#define VIEWMARKINGSCROLLBAR_H

#include <QAbstractItemModel>
#include <QScrollBar>

class ViewMarkingScrollBar : public QScrollBar {
public:
    static const int DEFAULT_ROLE = Qt::UserRole + 42;

public:
    ViewMarkingScrollBar(QAbstractItemModel* model, QWidget* parent = 0, int role = DEFAULT_ROLE);

protected:
    virtual void paintEvent(QPaintEvent* event);

private:
    QAbstractItemModel* m_Model;
    int m_Role;
};

#endif // VIEWMARKINGSCROLLBAR_H
