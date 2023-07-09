#ifndef FORCEDLOADDIALOG_H
#define FORCEDLOADDIALOG_H

#include <QDialog>
#include <QPushButton>

#include "executableinfo.h"
#include "iplugingame.h"

namespace Ui
{
class ForcedLoadDialog;
}

class ForcedLoadDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ForcedLoadDialog(const MOBase::IPluginGame* game, QWidget* parent = nullptr);
  ~ForcedLoadDialog();

  void setValues(QList<MOBase::ExecutableForcedLoadSetting>& values);
  QList<MOBase::ExecutableForcedLoadSetting> values();

private slots:
  void on_addRowButton_clicked();
  void on_deleteRowButton_clicked();

private:
  Ui::ForcedLoadDialog* ui;
  const MOBase::IPluginGame* m_GamePlugin;
};

#endif  // FORCEDLOADDIALOG_H
