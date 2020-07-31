#include "instancemanagerdialog.h"
#include "ui_instancemanagerdialog.h"
#include "instancemanager.h"
#include "createinstancedialog.h"
#include "settings.h"
#include "selectiondialog.h"
#include "plugincontainer.h"
#include "shared/appconfig.h"
#include <iplugingame.h>

namespace shell = MOBase::shell;

void openInstanceManager(PluginContainer& pc, QWidget* parent)
{
  //CreateInstanceDialog dlg(pc, parent);
  //dlg.exec();
  InstanceManagerDialog dlg(pc, parent);
  dlg.exec();
}

class InstanceInfo
{
public:
  InstanceInfo(QDir dir, bool isPortable) :
    m_dir(std::move(dir)), m_portable(isPortable),
    m_settings(dir.filePath(QString::fromStdWString(AppConfig::iniFileName())))
  {
  }

  QString name() const
  {
    if (m_portable) {
      return QObject::tr("Portable");
    } else {
      return m_dir.dirName();
    }
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

  bool isPortable() const
  {
    return m_portable;
  }

private:
  QDir m_dir;
  bool m_portable;
  Settings m_settings;
};


InstanceManagerDialog::~InstanceManagerDialog() = default;

InstanceManagerDialog::InstanceManagerDialog(
  const PluginContainer& pc, QWidget *parent) :
    QDialog(parent), ui(new Ui::InstanceManagerDialog), m_pc(pc),
    m_model(nullptr)
{
  ui->setupUi(this);
  ui->splitter->setSizes({200, 1});
  ui->splitter->setStretchFactor(0, 0);
  ui->splitter->setStretchFactor(1, 1);

  m_model = new QStandardItemModel;
  ui->list->setModel(m_model);

  m_filter.setEdit(ui->filter);
  m_filter.setList(ui->list);
  m_filter.setUpdateDelay(false);
  m_filter.setFilteredBorder(false);

  updateInstances();
  updateList();

  connect(ui->createNew, &QPushButton::clicked, [&]{ createNew(); });

  connect(ui->list->selectionModel(), &QItemSelectionModel::selectionChanged, [&]{ onSelection(); });
  //connect(ui->list, &QListWidget::itemActivated, [&]{ openSelectedInstance(); });

  connect(ui->rename, &QPushButton::clicked, [&]{ rename(); });
  connect(ui->exploreLocation, &QPushButton::clicked, [&]{ exploreLocation(); });
  connect(ui->exploreBaseDirectory, &QPushButton::clicked, [&]{ exploreBaseDirectory(); });
  connect(ui->exploreGame, &QPushButton::clicked, [&]{ exploreGame(); });

  connect(ui->switchToInstance, &QPushButton::clicked, [&]{ openSelectedInstance(); });
  connect(ui->close, &QPushButton::clicked, [&]{ close(); });
}

void InstanceManagerDialog::updateInstances()
{
  auto& m = InstanceManager::instance();

  m_instances.clear();

  if (m.portableInstanceExists()) {
    m_instances.push_back(std::make_unique<InstanceInfo>(
      m.portablePath(), true));
  }

  for (auto&& d : m.instancePaths()) {
    m_instances.push_back(std::make_unique<InstanceInfo>(d, false));
  }
}

void InstanceManagerDialog::updateList()
{
  m_model->clear();

  for (auto&& ii : m_instances) {
    m_model->appendRow(new QStandardItem(ii->name()));
  }

  if (!m_instances.empty()) {
    select(0);
  }
}

void InstanceManagerDialog::select(std::size_t i)
{
  if (i >= m_instances.size()) {
    return;
  }

  const auto& ii = m_instances[i];
  fillData(*ii);

  ui->list->selectionModel()->select(
    m_filter.mapFromSource(m_filter.sourceModel()->index(i, 0)),
    QItemSelectionModel::ClearAndSelect);
}

void InstanceManagerDialog::openSelectedInstance()
{
  const auto i = singleSelectionIndex();
  if (i == NoSelection) {
    return;
  }

  InstanceManager::instance().switchToInstance(m_instances[i]->name());
}

void InstanceManagerDialog::rename()
{
  auto* i = singleSelection();
  if (!i) {
    return;
  }

  auto& m = InstanceManager::instance();
  if (m.currentInstance() == i->name()) {
    QMessageBox::information(this,
      tr("Rename instance"), tr("The active instance cannot be renamed"));
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Rename instance"));

  auto* ly = new QVBoxLayout(&dlg);

  auto* bb = new QDialogButtonBox(
    QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

  auto* text = new QLineEdit(i->name());
  text->selectAll();

  auto* error = new QLabel;

  ly->addWidget(new QLabel(tr("Instance name")));
  ly->addWidget(text);
  ly->addWidget(error);
  ly->addStretch();
  ly->addWidget(bb);

  connect(text, &QLineEdit::textChanged, [&] {
    bool okay = false;

    if (!m.validInstanceName(text->text())) {
      error->setText(tr("The instance name must be a valid folder name."));
    } else {
      const auto name = m.sanitizeInstanceName(text->text());

      if ((name != i->name()) && m.instanceExists(text->text())) {
        error->setText(tr("An instance with this name already exists."));
      } else {
        okay = true;
      }
    }

    error->setVisible(!okay);
    bb->button(QDialogButtonBox::Ok)->setEnabled(okay);
  });

  connect(bb, &QDialogButtonBox::accepted, [&]{ dlg.accept(); });
  connect(bb, &QDialogButtonBox::rejected, [&]{ dlg.reject(); });

  dlg.resize({400, 120});
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }


  const QString newName = m.sanitizeInstanceName(text->text());
  const QString src = QDir::toNativeSeparators(i->location());
  const QString dest = QDir::toNativeSeparators(
    QFileInfo(i->location()).dir().path() + "/" + newName);

  const auto r = shell::Rename(src, dest, false);
  if (!r) {
    QMessageBox::critical(
      this, tr("Error"),
      tr("Failed to rename \"%1\" to \"%2\": %3")
        .arg(src).arg(dest).arg(r.toString()));

    return;
  }


}

void InstanceManagerDialog::exploreLocation()
{
  if (const auto* i=singleSelection()) {
    shell::Explore(i->location());
  }
}

void InstanceManagerDialog::exploreBaseDirectory()
{
  if (const auto* i=singleSelection()) {
    shell::Explore(i->baseDirectory());
  }
}

void InstanceManagerDialog::exploreGame()
{
  if (const auto* i=singleSelection()) {
    shell::Explore(i->gamePath());
  }
}

void InstanceManagerDialog::onSelection()
{
  const auto i = singleSelectionIndex();
  if (i == NoSelection) {
    return;
  }

  select(i);
}

void InstanceManagerDialog::createNew()
{
  CreateInstanceDialog dlg(m_pc, this);
  dlg.exec();
}

std::size_t InstanceManagerDialog::singleSelectionIndex() const
{
  const auto sel = m_filter.mapSelectionToSource(
    ui->list->selectionModel()->selection());

  if (sel.size() != 1) {
    return NoSelection;
  }

  return static_cast<std::size_t>(sel.indexes()[0].row());
}

InstanceInfo* InstanceManagerDialog::singleSelection()
{
  const auto i = singleSelectionIndex();
  if (i == NoSelection) {
    return nullptr;
  }

  return m_instances[i].get();
}

const InstanceInfo* InstanceManagerDialog::singleSelection() const
{
  const auto i = singleSelectionIndex();
  if (i == NoSelection) {
    return nullptr;
  }

  return m_instances[i].get();
}

void InstanceManagerDialog::fillData(const InstanceInfo& ii)
{
  ui->name->setText(ii.name());
  ui->location->setText(ii.location());
  ui->baseDirectory->setText(ii.baseDirectory());
  ui->gameName->setText(ii.gameName());
  ui->gameDir->setText(ii.gamePath());

  const auto& m = InstanceManager::instance();

  ui->rename->setEnabled(!ii.isPortable());

  if (ii.isPortable()) {
    ui->convertToPortable->setVisible(false);
    ui->convertToGlobal->setVisible(true);
    ui->convertToGlobal->setEnabled(true);
  } else {
    ui->convertToPortable->setVisible(true);
    ui->convertToGlobal->setVisible(false);

    if (m.portableInstanceExists()) {
      ui->convertToPortable->setEnabled(false);
      ui->convertToPortable->setToolTip(tr("A portable instance already exists."));
    } else {
      ui->convertToPortable->setEnabled(false);
      ui->convertToPortable->setToolTip("");
    }
  }
}
