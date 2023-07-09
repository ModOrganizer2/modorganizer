#ifndef MODINFODIALOGESPS_H
#define MODINFODIALOGESPS_H

#include "modinfodialogtab.h"

class ESPItem;
class ESPListModel;

class ESPsTab : public ModInfoDialogTab
{
  Q_OBJECT;

public:
  ESPsTab(ModInfoDialogTabContext cx);

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;
  void update();
  void saveState(Settings& s) override;
  void restoreState(const Settings& s) override;

private:
  ESPListModel* m_inactiveModel;
  ESPListModel* m_activeModel;

  void onActivate();
  void onDeactivate();
  void selectRow(QListView* list, int row);
};

#endif  // MODINFODIALOGESPS_H
