#include "modinfodialogfwd.h"
#include "modinfo.h"
#include <QPushButton>
#include <QTreeWidget>
#include <QCheckBox>

namespace Ui { class MainWindow; }
class OrganizerCore;
class Settings;
class PluginContainer;
class FileTree;

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

  void updateTree();

signals:
  void executablesChanged();
  void originModified(int originID);
  void displayModInformation(ModInfo::Ptr m, unsigned int i, ModInfoTabIDs tab);

private:
  struct DataTabUi
  {
    QPushButton* refresh;
    QTreeView* tree;
    QCheckBox* conflicts;
    QCheckBox* archives;
  };

  OrganizerCore& m_core;
  PluginContainer& m_pluginContainer;
  QWidget* m_parent;
  DataTabUi ui;
  std::unique_ptr<FileTree> m_filetree;
  std::vector<QTreeWidgetItem*> m_removeLater;

  void onRefresh();
  void onItemExpanded(QTreeWidgetItem* item);
  void onConflicts();
  void onArchives();
  void updateOptions();
};
