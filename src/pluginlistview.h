#ifndef PLUGINLISTVIEW_H
#define PLUGINLISTVIEW_H

#include "viewmarkingscrollbar.h"
#include <QDragEnterEvent>
#include <QTreeView>

class PluginListView : public QTreeView {
    Q_OBJECT
public:
    explicit PluginListView(QWidget* parent = 0);
    virtual void dragEnterEvent(QDragEnterEvent* event);
    virtual void setModel(QAbstractItemModel* model);
signals:
    void dropModeUpdate(bool dropOnRows);

public slots:
private:
    ViewMarkingScrollBar* m_Scrollbar;
};

#endif // PLUGINLISTVIEW_H
