#include "instancemanagerdialog.h"
#include "ui_instancemanagerdialog.h"
#include "instancemanager.h"
#include "createinstancedialog.h"
#include "settings.h"
#include "selectiondialog.h"
#include "plugincontainer.h"
#include "shared/appconfig.h"
#include <utility.h>
#include <report.h>
#include <iplugingame.h>

using namespace MOBase;

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
      return QDir::toNativeSeparators(*n);
    } else {
      return {};
    }
  }

  QString location() const
  {
    return QDir::toNativeSeparators(m_dir.path());
  }

  QString baseDirectory() const
  {
    return QDir::toNativeSeparators(m_settings.paths().base());
  }

  bool isPortable() const
  {
    return m_portable;
  }

  bool isActive() const
  {
    auto& m = InstanceManager::instance();

    if (m_portable && m.currentInstance() == "") {
      return true;
    } else if (m.currentInstance() == name()) {
      return true;
    }

    return false;
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
  connect(ui->deleteInstance, &QPushButton::clicked, [&]{ deleteInstance(); });

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
  const auto prevSelIndex = singleSelectionIndex();
  const auto* prevSel = singleSelection();

  m_model->clear();

  const std::size_t NoSel = -1;
  std::size_t sel = NoSel;

  for (std::size_t i=0; i<m_instances.size(); ++i) {
    const auto& ii = *m_instances[i];
    m_model->appendRow(new QStandardItem(ii.name()));

    if (&ii == prevSel) {
      sel = i;
    }
  }


  if (m_instances.empty()) {
    select(-1);
  } else {
    if (sel == NoSel) {
      if (prevSelIndex >= m_instances.size()) {
        sel = m_instances.size() - 1;
      } else {
        sel = prevSelIndex;
      }
    }

    select(sel);
  }
}

void InstanceManagerDialog::select(std::size_t i)
{
  if (i < m_instances.size()) {
    const auto& ii = m_instances[i];
    fillData(*ii);

    ui->list->selectionModel()->select(
      m_filter.mapFromSource(m_filter.sourceModel()->index(i, 0)),
      QItemSelectionModel::ClearAndSelect);
  } else {
    clearData();
  }
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
  if (i->isActive()) {
    QMessageBox::information(this,
      tr("Rename instance"), tr("The active instance cannot be renamed."));
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
  const QString src = i->location();
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

void InstanceManagerDialog::deleteInstance()
{
  const auto* i = singleSelection();
  if (!i) {
    return;
  }

  auto& m = InstanceManager::instance();
  if (i->isActive()) {
    QMessageBox::information(this,
      tr("Deleting instance"), tr("The active instance cannot be deleted."));
    return;
  }

  if (i->isPortable()) {
    deletePortable(*i);
  } else {
    deleteGlobal(*i);
  }

  updateInstances();
  updateList();
}

bool InstanceManagerDialog::deletePortable(const InstanceInfo& i)
{
  const auto Recycle = QMessageBox::Save;
  const auto Delete = QMessageBox::Yes;
  const auto Cancel = QMessageBox::Cancel;

  const std::vector<std::wstring> fileNames = {
    AppConfig::iniFileName(),
  };

  const std::vector<std::wstring> dirNames = {
    AppConfig::dumpsDir(),
    AppConfig::downloadPath(),
    AppConfig::logPath(),
    AppConfig::modsPath(),
    AppConfig::overwritePath(),
    AppConfig::profilesPath(),
    AppConfig::cachePath()
  };

  QStringList files;
  for (const auto& n : fileNames) {
    files.push_back(QDir::toNativeSeparators(
      i.location() + "/" + QString::fromStdWString(n)));
  }

  QStringList dirs;
  for (const auto& n : dirNames) {
    dirs.push_back(QDir::toNativeSeparators(
      i.location() + "/" + QString::fromStdWString(n)));
  }

  QString details = QObject::tr("These files will be deleted:");
  for (const auto& f : files) {
    details += "\n  - " + f;
  }

  details += "\n\n" + QObject::tr("These folders will be deleted:");
  for (const auto& d : dirs) {
    details += "\n  - " + d;
  }


  QStringList all;
  all.append(files);
  all.append(dirs);


  const auto r = MOBase::TaskDialog(this)
    .title(("Deleting portable instance"))
    .main(tr("This will delete the data of the portable instance."))
    .content(tr(
      "The data is in %1. Only the relevant files and folders will be "
      "deleted. The Mod Organizer installation itself will be untouched.")
        .arg(i.location()))
    .details(details)
    .icon(QMessageBox::Warning)
    .button({tr("Move the data to the recycle bin"), Recycle})
    .button({tr("Delete the data permanently"), Delete})
    .button({tr("Cancel"), Cancel})
    .exec();

  switch (r)
  {
    case Recycle:
      return doDelete(all, true);

    case Delete:
      return doDelete(all, false);

    case Cancel:  // fall-through
    default:
    {
      return false;
    }
  }

  return true;
}

bool InstanceManagerDialog::deleteGlobal(const InstanceInfo& i)
{
  const auto Recycle = QMessageBox::Save;
  const auto Delete = QMessageBox::Yes;
  const auto Cancel = QMessageBox::Cancel;

  const auto r = MOBase::TaskDialog(this)
    .title(tr("Deleting instance"))
    .main(tr("The instance folder will be deleted."))
    .content(i.location())
    .icon(QMessageBox::Warning)
    .button({tr("Move the folder to the recycle bin"), Recycle})
    .button({tr("Delete the folder permanently"), Delete})
    .button({tr("Cancel"), Cancel})
    .exec();

  switch (r)
  {
    case Recycle:
      return doDelete(QStringList(i.location()), true);

    case Delete:
      return doDelete(QStringList(i.location()), false);

    case Cancel:  // fall-through
    default:
    {
      return false;
    }
  }

  return true;
}

bool InstanceManagerDialog::doDelete(const QStringList& files, bool recycle)
{
  if (MOBase::shellDelete(files, recycle, this)) {
    return true;
  }

  const auto e = GetLastError();
  if (e == ERROR_CANCELLED) {
    log::debug("deletion cancelled by user");
  } else {
    log::error("failed to delete, {}", formatSystemMessage(e));
  }

  return false;
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
  setButtonsEnabled(true);

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


  // these are not currently implemented; the ui sets them correctly above,
  // but force them hidden for now
  ui->convertToPortable->setVisible(false);
  ui->convertToGlobal->setVisible(false);
}

void InstanceManagerDialog::clearData()
{
  ui->name->clear();
  ui->location->clear();
  ui->baseDirectory->clear();
  ui->gameName->clear();
  ui->gameDir->clear();

  setButtonsEnabled(false);

  ui->convertToPortable->setVisible(false);
  ui->convertToGlobal->setVisible(false);
}

void InstanceManagerDialog::setButtonsEnabled(bool b)
{
  ui->rename->setEnabled(b);
  ui->exploreLocation->setEnabled(b);
  ui->exploreBaseDirectory->setEnabled(b);
  ui->exploreGame->setEnabled(b);
  ui->convertToPortable->setEnabled(b);
  ui->convertToGlobal->setEnabled(b);
  ui->deleteInstance->setEnabled(b);
  ui->switchToInstance->setEnabled(b);
}
