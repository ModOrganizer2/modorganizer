#ifndef MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED
#define MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED

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

private:
  static const std::size_t NoSelection = -1;

  std::unique_ptr<Ui::InstanceManagerDialog> ui;
  const PluginContainer& m_pc;
  std::vector<std::unique_ptr<InstanceInfo>> m_instances;

  void onSelection();
  void createNew();

  std::size_t singleSelection() const;
  void fill(const InstanceInfo& ii);
};

#endif // MODORGANIZER_INSTANCEMANAGERDIALOG_INCLUDED
