#include "instancemanagerdialog.h"
#include "ui_instancemanagerdialog.h"
#include "instancemanager.h"

class InstanceInfo
{
public:
  InstanceInfo(QDir dir)
    : m_dir(std::move(dir))
  {
  }

  QString name() const
  {
    return m_dir.dirName();
  }

private:
  QDir m_dir;
};


InstanceManagerDialog::InstanceManagerDialog(QWidget *parent)
  : QDialog(parent), ui(new Ui::InstanceManagerDialog)
{
  ui->setupUi(this);

  auto& m = InstanceManager::instance();

  for (auto&& d : m.instancePaths()) {
    InstanceInfo i(d);
    ui->list->addItem(i.name());
  }
}

InstanceManagerDialog::~InstanceManagerDialog() = default;
