#ifndef MODINFODIALOGESPS_H
#define MODINFODIALOGESPS_H

#include "modinfodialog.h"

class ESPItem;

class ESPsTab : public ModInfoDialogTab
{
  Q_OBJECT;

public:
  ESPsTab(QWidget* parent, Ui::ModInfoDialog* ui);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;

private:
  QWidget* m_parent;
  Ui::ModInfoDialog* ui;

  void onActivate();
  void onDeactivate();

  ESPItem* selectedInactive();
  ESPItem* selectedActive();
};

#endif // MODINFODIALOGESPS_H
