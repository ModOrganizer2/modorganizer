#include "modinfodialogfwd.h"
#include "modinfo.h"
#include <QPushButton>
#include <QTreeWidget>
#include <QCheckBox>

namespace Ui { class MainWindow; }
class OrganizerCore;
class Settings;
class PluginContainer;

namespace MOShared { class DirectoryEntry; }

class DataTab : public QObject
{
  Q_OBJECT;

public:
  DataTab(
    OrganizerCore& core, PluginContainer& pc,
    QWidget* parent, Ui::MainWindow* ui);

  void saveState(Settings& s) const;
  void restoreState(const Settings& s);
  void activated();

  void refreshDataTreeKeepExpandedNodes();
  void refreshDataTree();

signals:
  void executablesChanged();
  void originModified(int originID);
  void displayModInformation(ModInfo::Ptr m, unsigned int i, ModInfoTabIDs tab);

private:
  struct DataTabUi
  {
    QPushButton* refresh;
    QTreeWidget* tree;
    QCheckBox* conflicts;
    QCheckBox* archives;
  };

  OrganizerCore& m_core;
  PluginContainer& m_pluginContainer;
  bool m_archives;
  QWidget* m_parent;
  DataTabUi ui;
  std::vector<QTreeWidgetItem*> m_removeLater;

  void onRefresh();
  void onItemExpanded(QTreeWidgetItem* item);
  void onItemActivated(QTreeWidgetItem* item, int col);
  void onContextMenu(const QPoint &pos);
  void onConflicts();
  void onArchives();

  void updateTo(
    QTreeWidgetItem *subTree, const std::wstring &directorySoFar,
    const MOShared::DirectoryEntry &directoryEntry, bool conflictsOnly,
    QIcon *fileIcon, QIcon *folderIcon);

  QTreeWidgetItem* singleSelection();

  void openSelection();
  void open(QTreeWidgetItem* item);

  void runSelectionHooked();
  void runHooked(QTreeWidgetItem* item);

  void previewSelection();
  void preview(QTreeWidgetItem* item);

  void addAsExecutable();
  void openOriginInExplorer();
  void openModInfo();
  void hideFile();
  void unhideFile();
  void writeDataToFile();
  void writeDataToFile(
    QFile &file, const QString &directory,
    const MOShared::DirectoryEntry &directoryEntry);
};
