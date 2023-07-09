#ifndef FORCEDLOADDIALOGWIDGET_H
#define FORCEDLOADDIALOGWIDGET_H

#include "iplugingame.h"
#include <QWidget>

namespace Ui
{
class ForcedLoadDialogWidget;
}

class ForcedLoadDialogWidget : public QWidget
{
  Q_OBJECT

public:
  explicit ForcedLoadDialogWidget(const MOBase::IPluginGame* game,
                                  QWidget* parent = nullptr);
  ~ForcedLoadDialogWidget();

  bool getEnabled();
  bool getForced();
  QString getLibraryPath();
  QString getProcess();

  void setEnabled(bool enabled);
  void setForced(bool forced);
  void setLibraryPath(const QString& path);
  void setProcess(const QString& name);

private slots:
  void on_enabledBox_toggled();
  void on_libraryPathBrowseButton_clicked();
  void on_processBrowseButton_clicked();

private:
  Ui::ForcedLoadDialogWidget* ui;
  bool m_Forced;
  const MOBase::IPluginGame* m_GamePlugin;
};

#endif  // FORCEDLOADDIALOGWIDGET_H
