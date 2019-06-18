#ifndef MODINFODIALOGESPS_H
#define MODINFODIALOGESPS_H

#include "modinfodialog.h"

class ESPItem;

class ESPsTab : public ModInfoDialogTab
{
public:
  ESPsTab(Ui::ModInfoDialog* ui);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;

private:
  Ui::ModInfoDialog* ui;

  void onActivate();
  void onDeactivate();

  ESPItem* selectedInactive();
  ESPItem* selectedActive();
};

#endif // MODINFODIALOGESPS_H
