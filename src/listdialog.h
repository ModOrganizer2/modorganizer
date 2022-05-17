#ifndef LISTDIALOG_H
#define LISTDIALOG_H

#include <QDialog>

namespace Ui
{
class ListDialog;
}

class ListDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ListDialog(QWidget* parent = nullptr);
  ~ListDialog();

  // also saves and restores geometry
  //
  int exec() override;

  void setChoices(QStringList choices);
  QString getChoice() const;

public slots:
  void on_filterEdit_textChanged(QString filter);

private:
  Ui::ListDialog* ui;
  QStringList m_Choices;
};

#endif  // LISTDIALOG_H
