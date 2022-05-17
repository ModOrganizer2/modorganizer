#ifndef SAVETEXTASDIALOG_H
#define SAVETEXTASDIALOG_H

#include <QDialog>

namespace Ui {
class SaveTextAsDialog;
}

class SaveTextAsDialog : public QDialog
{
  Q_OBJECT
  
public:
  explicit SaveTextAsDialog(QWidget *parent = 0);
  ~SaveTextAsDialog();

  void setText(const QString &text);

private slots:
  void on_closeBtn_clicked();

  void on_clipboardBtn_clicked();

  void on_saveAsBtn_clicked();

private:
  Ui::SaveTextAsDialog *ui;
};

#endif // SAVETEXTASDIALOG_H
