#ifndef LISTDIALOG_H
#define LISTDIALOG_H

#include <QDialog>

namespace Ui {
class ListDialog;
}

class ListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ListDialog(QWidget *parent = nullptr);
    ~ListDialog();

    void setChoices(QStringList choices);
    QString getChoice() const;

private:
    Ui::ListDialog *ui;
};

#endif // LISTDIALOG_H
