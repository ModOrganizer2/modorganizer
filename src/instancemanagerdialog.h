#ifndef MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED
#define MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED

#include <filterwidget.h>
#include <QDialog>

namespace Ui { class InstanceManagerDialog; };

class InstanceInfo;
class PluginContainer;

class InstanceManagerDialog : public QDialog
{
  Q_OBJECT

public:
  explicit InstanceManagerDialog(
    const PluginContainer& pc, QWidget *parent = nullptr);

  ~InstanceManagerDialog();

  void select(std::size_t i);
  void select(const QString& name);
  void selectActiveInstance();
  void openSelectedInstance();

  void rename();
  void exploreLocation();
  void exploreBaseDirectory();
  void exploreGame();

  void convertToGlobal();
  void convertToPortable();
  void openINI();
  void deleteInstance();

  void setRestartOnSelect(bool b);

private:
  static const std::size_t NoSelection = -1;

  std::unique_ptr<Ui::InstanceManagerDialog> ui;
  const PluginContainer& m_pc;
  std::vector<std::unique_ptr<InstanceInfo>> m_instances;
  MOBase::FilterWidget m_filter;
  QStandardItemModel* m_model;
  bool m_restartOnSelect;

  void updateInstances();

  void onSelection();
  void createNew();

  std::size_t singleSelectionIndex() const;
  InstanceInfo* singleSelection();
  const InstanceInfo* singleSelection() const;

  void updateList();
  void fillData(const InstanceInfo& ii);
  void clearData();
  void setButtonsEnabled(bool b);

  bool deletePortable(const InstanceInfo& ii);
  bool deleteGlobal(const InstanceInfo& ii);
  bool doDelete(const QStringList& files, bool recycle);
};

#endif // MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED
