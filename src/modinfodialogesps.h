#ifndef MODINFODIALOGESPS_H
#define MODINFODIALOGESPS_H

#include "modinfodialogtab.h"

class ESPItem;

class ESPsTab : public ModInfoDialogTab
{
  Q_OBJECT;

public:
  ESPsTab(
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;

private:
  void onActivate();
  void onDeactivate();

  ESPItem* selectedInactive();
  ESPItem* selectedActive();
};

#endif // MODINFODIALOGESPS_H
