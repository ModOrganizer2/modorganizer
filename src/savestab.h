#ifndef MODORGANIZER_SAVESTAB_INCLUDED
#define MODORGANIZER_SAVESTAB_INCLUDED

#include "savegameinfo.h"
#include <filterwidget.h>

namespace Ui
{
class MainWindow;
}

namespace MOBase
{
class ISaveGame;
class ISaveGameInfoWidget;
}  // namespace MOBase

class MainWindow;
class OrganizerCore;

class SavesTab : public QObject
{
  Q_OBJECT;

public:
  SavesTab(QWidget* window, OrganizerCore& core, Ui::MainWindow* ui);

  void refreshSaveList();
  void displaySaveGameInfo(QTreeWidgetItem* newItem);

  QDir currentSavesDir() const;

  void startMonitorSaves();
  void stopMonitorSaves();
  void hideSaveGameInfo();

protected:
  bool eventFilter(QObject* object, QEvent* e) override;

private:
  struct SavesTabUi
  {
    QTabWidget* mainTabs;
    QWidget* tab;
    QTreeWidget* list;
  };

  QWidget* m_window;
  OrganizerCore& m_core;
  SavesTabUi ui;
  MOBase::FilterWidget m_filter;
  std::vector<std::shared_ptr<const MOBase::ISaveGame>> m_SaveGames;
  MOBase::ISaveGameInfoWidget* m_CurrentSaveView;

  QTimer m_SavesWatcherTimer;
  QFileSystemWatcher m_SavesWatcher;

  void onContextMenu(const QPoint& pos);
  void deleteSavegame();
  void saveSelectionChanged(QTreeWidgetItem* newItem);
  void fixMods(SaveGameInfo::MissingAssets const& missingAssets);
  void refreshSavesIfOpen();
  void openInExplorer();
};

#endif  // MODORGANIZER_SAVESTAB_INCLUDED
