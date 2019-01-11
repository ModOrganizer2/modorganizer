#ifndef FORCEDLOADDIALOGWIDGET_H
#define FORCEDLOADDIALOGWIDGET_H

#include <QWidget>

namespace Ui {
class ForcedLoadDialogWidget;
}

class ForcedLoadDialogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ForcedLoadDialogWidget(QWidget *parent = nullptr);
    ~ForcedLoadDialogWidget();

    bool getEnabled();
    bool getForced();
    QString getLibraryPath();
    QString getProcess();

    void setEnabled(bool enabled);
    void setForced(bool forced);
    void setLibraryPath(QString &path);
    void setProcess(QString &name);

private slots:
    void on_enabledBox_toggled();
    void on_libraryPathBrowseButton_clicked();
    void on_processBrowseButton_clicked();

private:
    Ui::ForcedLoadDialogWidget *ui;
    bool m_Forced;
};

#endif // FORCEDLOADDIALOGWIDGET_H
