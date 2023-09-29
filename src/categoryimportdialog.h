#ifndef CATEGORYIMPORTDIALOG_H
#define CATEGORYIMPORTDIALOG_H

#include <QDialog>

namespace Ui
{
class CategoryImportDialog;
}

/**
 * @brief Dialog that allows users to configure mod categories
 **/
class CategoryImportDialog : public QDialog
{
  Q_OBJECT

public:
  enum ImportStrategy
  {
    None,
    Overwrite,
    Merge
  };

public:
  explicit CategoryImportDialog(QWidget* parent = 0);
  ~CategoryImportDialog();

  ImportStrategy strategy();
  bool assign();
  bool remap();

public slots:
  void accepted();
  void rejected();
  void on_strategyClicked(QAbstractButton* button);
  void on_assignOptionClicked(bool clicked);

private:
  Ui::CategoryImportDialog* ui;
};

#endif  // CATEGORYIMPORTDIALOG_H
