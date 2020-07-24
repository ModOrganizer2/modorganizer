#include "instancemanagerdialog.h"
#include "ui_instancemanagerdialog.h"
#include "instancemanager.h"
#include "createinstancedialog.h"
#include "settings.h"
#include "selectiondialog.h"
#include "plugincontainer.h"
#include "shared/appconfig.h"
#include <iplugingame.h>

void openInstanceManager(PluginContainer& pc, QWidget* parent)
{
  InstanceManagerDialog dlg(pc, parent);
  dlg.exec();
}

class InstanceInfo
{
public:
  InstanceInfo(QDir dir) :
    m_dir(std::move(dir)),
    m_settings(dir.filePath(QString::fromStdWString(AppConfig::iniFileName())))
  {
  }

  QString name() const
  {
    return m_dir.dirName();
  }

  QString gameName() const
  {
    if (auto n=m_settings.game().name()) {
      if (auto e=m_settings.game().edition()) {
        if (!e->isEmpty()) {
          return *n + " (" + *e + ")";
        }
      }

      return *n;
    } else {
      return {};
    }
  }

  QString gamePath() const
  {
    if (auto n=m_settings.game().directory()) {
      return *n;
    } else {
      return {};
    }
  }

  QString location() const
  {
    return m_dir.path();
  }

  QString baseDirectory() const
  {
    return m_settings.paths().base();
  }

private:
  QDir m_dir;
  Settings m_settings;
};


InstanceManagerDialog::InstanceManagerDialog(
  const PluginContainer& pc, QWidget *parent)
    : QDialog(parent), ui(new Ui::InstanceManagerDialog), m_pc(pc)
{
  ui->setupUi(this);
  ui->splitter->setSizes({200, 1});
  ui->splitter->setStretchFactor(0, 0);
  ui->splitter->setStretchFactor(1, 1);

  auto& m = InstanceManager::instance();

  for (auto&& d : m.instancePaths()) {
    auto ii = std::make_unique<InstanceInfo>(d);

    ui->list->addItem(ii->name());
    m_instances.push_back(std::move(ii));
  }

  if (!m_instances.empty()) {
    select(0);
  }

  connect(ui->createNew, &QPushButton::clicked, [&]{ createNew(); });
  connect(ui->list, &QListWidget::itemSelectionChanged, [&]{ onSelection(); });
}

InstanceManagerDialog::~InstanceManagerDialog() = default;

void InstanceManagerDialog::select(std::size_t i)
{
  if (i >= m_instances.size()) {
    return;
  }

  const auto& ii = m_instances[i];
  fill(*ii);
}

void InstanceManagerDialog::onSelection()
{
  const auto sel = ui->list->selectionModel()->selectedIndexes();
  if (sel.size() != 1) {
    return;
  }

  select(static_cast<std::size_t>(sel[0].row()));
}

void InstanceManagerDialog::createNew()
{
  CreateInstanceDialog dlg(m_pc, this);
  dlg.exec();
}

void InstanceManagerDialog::fill(const InstanceInfo& ii)
{
  ui->name->setText(ii.name());
  ui->location->setText(ii.location());
  ui->baseDirectory->setText(ii.baseDirectory());
  ui->gameName->setText(ii.gameName());
  ui->gameDir->setText(ii.gamePath());
}
