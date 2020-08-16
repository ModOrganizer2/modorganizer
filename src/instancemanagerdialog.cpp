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

QString iniFile(const QDir& dir)
{
  return dir.filePath(QString::fromStdWString(AppConfig::iniFileName()));
}


class InstanceInfo
{
public:
  InstanceInfo(QDir dir, bool isPortable) :
    m_dir(std::move(dir)), m_portable(isPortable), m_settings(iniFile(dir))
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

  // returns a list of files and folders that must be deleted when deleting
  // this instance
  //
  QStringList filesForDeletion() const
  {
    // native separators and ending slash
    auto prettyDir = [](auto s) {
      if (!s.endsWith("/") || !s.endsWith("\\")) {
        s += "/";
      }

      return QDir::toNativeSeparators(s);
    };

    // native separators
    auto prettyFile = [](auto s) {
      return QDir::toNativeSeparators(s);
    };


    // lowercase, native separators and ending slash
    auto canonicalDir = [](auto s) {
      s = s.toLower();
      if (!s.endsWith("/") || !s.endsWith("\\")) {
        s += "/";
      }

      return QDir::toNativeSeparators(s);
    };

    // lower and native separators
    auto canonicalFile = [](auto s) {
      return QDir::toNativeSeparators(s.toLower());
    };



    // whether the given directory is contained in the root
    auto dirInRoot = [&](auto root, auto dir) {
      return canonicalDir(dir).startsWith(canonicalDir(root));
    };

    // whether the given file is contained in the root
    auto fileInRoot = [&](auto root, auto file) {
      return canonicalFile(file).startsWith(canonicalDir(root));
    };




    const auto loc = location();
    const auto base = m_settings.paths().base();


    // directories that might contain the individual files and directories
    // set in the path settings
    QStringList roots;

    // a portable instance has its location in the installation directory,
    // don't delete that
    if (!isPortable()) {
      roots.append(loc);
    }

    // the base directory is the location directory by default, don't add it
    // if it's the same
    if (canonicalDir(base) != canonicalDir(loc)) {
      roots.append(base);
    }


    // all the directories that are part of an instance
    const QStringList dirs = {
      m_settings.paths().downloads(),
      m_settings.paths().mods(),
      m_settings.paths().overwrite(),
      m_settings.paths().profiles(),
      m_settings.paths().cache(),
      m_dir.filePath(QString::fromStdWString(AppConfig::dumpsDir())),
      m_dir.filePath(QString::fromStdWString(AppConfig::logPath())),
    };

    // all the files that are part of an instance
    const QStringList files = {
      iniFile(m_dir),
    };


    // this will contain the root directories, plus all the individual
    // directories that are not inside these roots
    QStringList cleanDirs;

    for (const auto& f : dirs) {
      bool inRoots = false;

      for (const auto& root : roots) {
        if (dirInRoot(root, f)) {
          inRoots = true;
          break;
        }
      }

      if (!inRoots) {
        // not in roots, this is a path that was changed by the user
        cleanDirs.append(prettyDir(f));
      }
    }

    // adding the roots
    for (const auto& root : roots) {
      cleanDirs.append(prettyDir(root));
    }

    cleanDirs.sort(Qt::CaseInsensitive);


    // this will contain the individual files that are not inside the roots;
    // not that this only contains the INI file for now, so most of this is
    // useless
    QStringList cleanFiles;

    for (const auto& f : files) {
      bool inRoots = false;

      for (const auto& root : roots) {
        if (fileInRoot(root, f)) {
          inRoots = true;
          break;
        }
      }

      if (!inRoots) {
        // not in roots, this is a path that was changed by the user
        cleanFiles.append(prettyFile(f));
      }
    }

    cleanFiles.sort(Qt::CaseInsensitive);


    // contains all the directories and files to be deleted
    QStringList all;
    all.append(cleanDirs);
    all.append(cleanFiles);

    return all;
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
  selectActiveInstance();

  connect(ui->createNew, &QPushButton::clicked, [&]{ createNew(); });

  connect(ui->list->selectionModel(), &QItemSelectionModel::selectionChanged, [&]{ onSelection(); });
  connect(ui->list, &QListView::activated, [&]{ openSelectedInstance(); });

  connect(ui->rename, &QPushButton::clicked, [&]{ rename(); });
  connect(ui->exploreLocation, &QPushButton::clicked, [&]{ exploreLocation(); });
  connect(ui->exploreBaseDirectory, &QPushButton::clicked, [&]{ exploreBaseDirectory(); });
  connect(ui->exploreGame, &QPushButton::clicked, [&]{ exploreGame(); });
  connect(ui->deleteInstance, &QPushButton::clicked, [&]{ deleteInstance(); });

  connect(ui->convertToGlobal, &QPushButton::clicked, [&]{ convertToGlobal(); });
  connect(ui->convertToPortable, &QPushButton::clicked, [&]{ convertToPortable(); });

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

  // keep current selection or select the next one if there was a selection;
  // there's no selection when opening the dialog, that's handled in the ctor
  if (prevSel) {
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

void InstanceManagerDialog::selectActiveInstance()
{
  const auto active = InstanceManager::instance().currentInstance();

  for (std::size_t i=0; i<m_instances.size(); ++i) {
    if (m_instances[i]->name() == active) {
      select(i);

      ui->list->scrollTo(
        m_filter.mapFromSource(m_filter.sourceModel()->index(i, 0)));

      return;
    }
  }

  select(0);
}

void InstanceManagerDialog::openSelectedInstance()
{
  const auto i = singleSelectionIndex();
  if (i == NoSelection) {
    return;
  }

  InstanceManager::instance().switchToInstance(m_instances[i]->name());
}

QString getInstanceName(
  QWidget* parent, const QString& title, const QString& moreText,
  const QString& label, const QString& oldName={})
{
  auto& m = InstanceManager::instance();

  QDialog dlg(parent);
  dlg.setWindowTitle(title);

  auto* ly = new QVBoxLayout(&dlg);

  auto* bb = new QDialogButtonBox(
    QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

  auto* text = new QLineEdit(oldName);
  text->selectAll();

  auto* error = new QLabel;

  if (!moreText.isEmpty()) {
    auto* lb = new QLabel(moreText);
    lb->setWordWrap(true);
    ly->addWidget(lb);
    ly->addSpacing(10);
  }

  auto* lb = new QLabel(label);
  lb->setWordWrap(true);
  ly->addWidget(lb);

  ly->addWidget(text);
  ly->addWidget(error);
  ly->addStretch();
  ly->addWidget(bb);

  auto check = [&] {
    bool okay = false;

    if (text->text().isEmpty()) {
      error->setText("");
    } else if (!m.validInstanceName(text->text())) {
      error->setText(QObject::tr("The instance name must be a valid folder name."));
    } else {
      const auto name = m.sanitizeInstanceName(text->text());

      if ((name != oldName) && m.instanceExists(text->text())) {
        error->setText(QObject::tr("An instance with this name already exists."));
      } else {
        okay = true;
      }
    }

    error->setVisible(!okay);
    bb->button(QDialogButtonBox::Ok)->setEnabled(okay);
  };

  QObject::connect(text, &QLineEdit::textChanged, [&] { check(); });
  QObject::connect(bb, &QDialogButtonBox::accepted, [&]{ dlg.accept(); });
  QObject::connect(bb, &QDialogButtonBox::rejected, [&]{ dlg.reject(); });

  check();

  dlg.resize({400, 120});
  if (dlg.exec() != QDialog::Accepted) {
    return {};
  }

  return m.sanitizeInstanceName(text->text());
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

  const auto newName = getInstanceName(
    this, tr("Rename instance"), "", tr("Instance name"), i->name());

  if (newName.isEmpty()) {
    return;
  }

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

  const auto Recycle = QMessageBox::Save;
  const auto Delete = QMessageBox::Yes;
  const auto Cancel = QMessageBox::Cancel;

  const auto files = i->filesForDeletion();

  MOBase::TaskDialog dlg(this);

  dlg
    .title(("Deleting instance"))
    .main(QObject::tr("These files and folders will be deleted"))
    .icon(QMessageBox::Warning)
    .button({tr("Move to the recycle bin"), Recycle})
    .button({tr("Delete permanently"), Delete})
    .button({tr("Cancel"), Cancel});

  auto* list = new QPlainTextEdit();
  list->setReadOnly(true);
  list->setWordWrapMode(QTextOption::NoWrap);
  list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  list->setMaximumHeight(150);

  for (const auto& f : files) {
    list->appendPlainText(f);
  }

  list->moveCursor(QTextCursor::MoveOperation::Start);

  dlg.addContent(list);

  const auto r = dlg.exec();

  switch (r)
  {
    case Recycle:
    {
      if (!doDelete(files, true)) {
        return;
      }

      break;
    }

    case Delete:
    {
      if (!doDelete(files, false)) {
        return;
      }

      break;
    }

    case Cancel:  // fall-through
    default:
    {
      return;
    }
  }

  updateInstances();
  updateList();
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

void InstanceManagerDialog::convertToGlobal()
{
  const auto* i = singleSelection();
  if (!i) {
    return;
  }

  if (!i->isPortable()) {
    log::error("can't convert to global, this is not a portable instance");
    return;
  }

  const auto& m = InstanceManager::instance();

  const auto name = getInstanceName(
    this,
    tr("Convert to global instance"),
    tr(
      "This will move all the instance data currently in Mod Organizer's "
      "installation folder into a global instance. If the operation fails or "
      "is cancelled, no data should be lost, but the move will need to be "
      "completed or cleaned up manually.<br><br>"
      "Source: %1<br>"
      "Destination: %2")
        .arg(i->location())
        .arg(QDir::toNativeSeparators(m.instancesPath())),
    tr("Name of the new instance"));

  if (name.isEmpty()) {
    return;
  }
}

void InstanceManagerDialog::convertToPortable()
{
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
