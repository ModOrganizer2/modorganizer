#ifndef MODINFODIALOGTEXTFILES_H
#define MODINFODIALOGTEXTFILES_H

#include "modinfodialog.h"

class TextFilesTab : public ModInfoDialogTab
{
public:
  TextFilesTab(Ui::ModInfoDialog* ui);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;
  bool canClose() override;

private:
  Ui::ModInfoDialog* ui;

  void onSelection(QListWidgetItem* current, QListWidgetItem* previous);
};

#endif // MODINFODIALOGTEXTFILES_H
