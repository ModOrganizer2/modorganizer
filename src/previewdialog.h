#ifndef PREVIEWDIALOG_H
#define PREVIEWDIALOG_H

#include <QDialog>

namespace Ui
{
class PreviewDialog;
}

class PreviewDialog : public QDialog
{
  Q_OBJECT

public:
  explicit PreviewDialog(const QString& fileName, QWidget* parent = 0);
  ~PreviewDialog();

  // also saves and restores geometry
  //
  int exec() override;

  void addVariant(const QString& modName, QWidget* widget);
  int numVariants() const;

private slots:

  void on_variantsStack_currentChanged(int arg1);

  void on_closeButton_clicked();

  void on_previousButton_clicked();

  void on_nextButton_clicked();

private:
  Ui::PreviewDialog* ui;
};

#endif  // PREVIEWDIALOG_H
