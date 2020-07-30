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
  void openSelectedInstance();

  void rename();
  void exploreLocation();
  void exploreBaseDirectory();
  void exploreGame();

private:
  static const std::size_t NoSelection = -1;

  std::unique_ptr<Ui::InstanceManagerDialog> ui;
  const PluginContainer& m_pc;
  std::vector<std::unique_ptr<InstanceInfo>> m_instances;
  MOBase::FilterWidget m_filter;

  void onSelection();
  void createNew();

  std::size_t singleSelectionIndex() const;
  InstanceInfo* singleSelection();
  const InstanceInfo* singleSelection() const;

  void fill(const InstanceInfo& ii);
};

#endif // MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED
