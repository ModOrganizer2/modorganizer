#include "createinstancedialog.h"
#include "createinstancedialogpages.h"
#include "instancemanager.h"
#include "settings.h"
#include "shared/appconfig.h"
#include "shared/util.h"
#include "ui_createinstancedialog.h"
#include <iplugingame.h>
#include <utility.h>

using namespace MOBase;

class Failed
{};

// create() will create all the directories in `target`; if any path component
// fails to create, it will throw Failed
//
// unless commit() is called, all the created directories will be deleted in
// the destructor
//
class DirectoryCreator
{
public:
  DirectoryCreator(const DirectoryCreator&)            = delete;
  DirectoryCreator& operator=(const DirectoryCreator&) = delete;

  static std::unique_ptr<DirectoryCreator> create(const QDir& target,
                                                  std::function<void(QString)> log)
  {
    return std::unique_ptr<DirectoryCreator>(new DirectoryCreator(target, log));
  }

  ~DirectoryCreator() { rollback(); }

  void commit() { m_created.clear(); }

  void rollback() noexcept
  {
    try {
      // delete each directory starting from the end
      for (auto itor = m_created.rbegin(); itor != m_created.rend(); ++itor) {
        const auto r = shell::DeleteDirectoryRecursive(*itor);
        if (!r) {
          m_logger(r.toString());
        }
      }

      m_created.clear();
    } catch (...) {
      // eat it
    }
  }

private:
  std::function<void(QString)> m_logger;

  DirectoryCreator(const QDir& target, std::function<void(QString)> log) : m_logger(log)
  {
    try {
      // split on separators
      const QString s      = QDir::toNativeSeparators(target.absolutePath());
      const QStringList cs = s.split("\\");

      if (cs.empty()) {
        return;
      }

      // root directory
      QDir d(cs[0]);

      // for each directory after the root
      for (int i = 1; i < cs.size(); ++i) {
        d = d.filePath(cs[i]);

        if (!d.exists()) {
          m_logger(QObject::tr("Creating %1").arg(d.path()));
          const auto r = shell::CreateDirectories(d);

          if (!r) {
            m_logger(r.toString());
            throw Failed();
          }

          m_created.push_back(d);
        }
      }
    } catch (...) {
      rollback();
      throw;
    }
  }

private:
  std::vector<QDir> m_created;
};

CreateInstanceDialog::CreateInstanceDialog(const PluginContainer& pc, Settings* s,
                                           QWidget* parent)
    : QDialog(parent), ui(new Ui::CreateInstanceDialog), m_pc(pc), m_settings(s),
      m_switching(false), m_singlePage(false)
{
  using namespace cid;

  ui->setupUi(this);
  m_originalNext = ui->next->text();

  m_pages.push_back(std::make_unique<IntroPage>(*this));
  m_pages.push_back(std::make_unique<TypePage>(*this));
  m_pages.push_back(std::make_unique<GamePage>(*this));
  m_pages.push_back(std::make_unique<VariantsPage>(*this));
  m_pages.push_back(std::make_unique<NamePage>(*this));
  m_pages.push_back(std::make_unique<ProfilePage>(*this));
  m_pages.push_back(std::make_unique<PathsPage>(*this));
  m_pages.push_back(std::make_unique<NexusPage>(*this));
  m_pages.push_back(std::make_unique<ConfirmationPage>(*this));

  ui->pages->setCurrentIndex(0);
  ui->launch->setChecked(true);

  if (!InstanceManager::singleton().hasAnyInstances()) {
    // first run of MO, there are no instances yet, force launch
    ui->launch->setEnabled(false);
  }

  if (m_pages[0]->skip()) {
    next();
  }

  ui->next->setFocus();

  updateNavigation();

  addShortcutAction(QKeySequence::Find, Actions::Find);

  addShortcut(Qt::ALT | Qt::Key_Left, [&] {
    back();
  });
  addShortcut(Qt::ALT | Qt::Key_Right, [&] {
    next(false);
  });
  addShortcut(Qt::CTRL | Qt::Key_Return, [&] {
    next();
  });

  connect(ui->next, &QPushButton::clicked, [&] {
    next();
  });
  connect(ui->back, &QPushButton::clicked, [&] {
    back();
  });
  connect(ui->cancel, &QPushButton::clicked, [&] {
    reject();
  });
}

CreateInstanceDialog::~CreateInstanceDialog() = default;

Ui::CreateInstanceDialog* CreateInstanceDialog::getUI()
{
  return ui.get();
}

const PluginContainer& CreateInstanceDialog::pluginContainer()
{
  return m_pc;
}

Settings* CreateInstanceDialog::settings()
{
  return m_settings;
}

bool CreateInstanceDialog::isOnLastPage() const
{
  for (int i = ui->pages->currentIndex() + 1; i < ui->pages->count(); ++i) {
    if (!m_pages[i]->skip()) {
      return false;
    }
  }

  return true;
}

void CreateInstanceDialog::next(bool allowFinish)
{
  if (!canNext()) {
    return;
  }

  const auto i    = ui->pages->currentIndex();
  const auto last = isOnLastPage();

  if (last) {
    if (allowFinish) {
      if (m_singlePage) {
        // just close the dialog
        accept();
      } else {
        finish();
      }
    }
  } else {
    changePage(+1);
  }
}

void CreateInstanceDialog::back()
{
  if (!canBack()) {
    return;
  }

  changePage(-1);
}

void CreateInstanceDialog::addShortcut(QKeySequence seq, std::function<void()> f)
{
  auto* sc = new QShortcut(seq, this);

  sc->setAutoRepeat(false);
  sc->setContext(Qt::WidgetWithChildrenShortcut);

  QObject::connect(sc, &QShortcut::activated, f);
}

void CreateInstanceDialog::addShortcutAction(QKeySequence seq, Actions a)
{
  addShortcut(seq, [this, a] {
    doAction(a);
  });
}

void CreateInstanceDialog::doAction(Actions a)
{
  std::size_t i = static_cast<std::size_t>(ui->pages->currentIndex());

  if (i >= m_pages.size()) {
    return;
  }

  m_pages[i]->action(a);
}

void CreateInstanceDialog::setSinglePageImpl(const QString& instanceName)
{
  m_singlePage = true;

  if (m_pages[ui->pages->currentIndex()]->skip()) {
    next();
  }

  // don't show the "create a new instance" title for single pages, this is
  // when the instance already exists but some info is missing
  ui->title->setText(tr("Setting up instance %1").arg(instanceName));
  setWindowTitle(tr("Setting up an instance %1").arg(instanceName));
}

void CreateInstanceDialog::changePage(int d)
{
  std::size_t i = static_cast<std::size_t>(ui->pages->currentIndex());

  // goes back or forwards until an unskippable page is reached, or the
  // first/last page

  if (d > 0) {
    // forwards
    for (;;) {
      ++i;

      if (i >= m_pages.size()) {
        break;
      }

      if (!m_pages[i]->skip()) {
        break;
      }
    }
  } else {
    // backwards
    for (;;) {
      if (i == 0) {
        break;
      }

      --i;

      if (!m_pages[i]->skip()) {
        break;
      }
    }
  }

  if (i < m_pages.size()) {
    selectPage(i);
  }
}

void CreateInstanceDialog::finish()
{
  ui->creationLog->clear();
  logCreation(tr("Creating instance..."));

  const auto& m = InstanceManager::singleton();
  const auto ci = creationInfo();

  auto logger = [&](QString s) {
    logCreation(s);
  };

  auto createDir = [&](QString path) {
    return DirectoryCreator::create(path, logger);
  };

  // don't restart if this is the first instance, it'll be selected and opened
  const bool mustRestart = InstanceManager::singleton().hasAnyInstances();

  try {
    std::vector<std::unique_ptr<DirectoryCreator>> dirs;

    // creating all these directories; if any of them fail, this throws and
    // any newly created directory will be deleted in DirectoryCreator's dtor
    dirs.push_back(createDir(ci.dataPath));
    dirs.push_back(createDir(ci.paths.base));
    dirs.push_back(createDir(PathSettings::resolve(ci.paths.downloads, ci.paths.base)));
    dirs.push_back(createDir(PathSettings::resolve(ci.paths.mods, ci.paths.base)));
    dirs.push_back(createDir(PathSettings::resolve(ci.paths.profiles, ci.paths.base)));
    dirs.push_back(createDir(PathSettings::resolve(ci.paths.overwrite, ci.paths.base)));

    // creating ini
    Settings s(ci.iniPath);
    s.game().setName(ci.game->gameName());
    s.game().setDirectory(ci.gameLocation);

    if (!ci.gameVariant.isEmpty()) {
      s.game().setEdition(ci.gameVariant);
    }

    if (ci.paths.base != ci.dataPath) {
      s.paths().setBase(ci.paths.base);
    }

    if (ci.paths.downloads != cid::makeDefaultPath(AppConfig::downloadPath())) {
      s.paths().setDownloads(ci.paths.downloads);
    }

    if (ci.paths.mods != cid::makeDefaultPath(AppConfig::modsPath())) {
      s.paths().setMods(ci.paths.mods);
    }

    if (ci.paths.profiles != cid::makeDefaultPath(AppConfig::profilesPath())) {
      s.paths().setProfiles(ci.paths.profiles);
    }

    if (ci.paths.overwrite != cid::makeDefaultPath(AppConfig::overwritePath())) {
      s.paths().setOverwrite(ci.paths.overwrite);
    }

    s.setProfileLocalInis(ci.profileSettings.localInis);
    s.setProfileLocalSaves(ci.profileSettings.localSaves);
    s.setProfileArchiveInvalidation(ci.profileSettings.archiveInvalidation);

    logCreation(tr("Writing %1...").arg(ci.iniPath));

    // writing ini
    const auto r = s.sync();

    if (r != QSettings::NoError) {
      switch (r) {
      case QSettings::AccessError:
        logCreation(formatSystemMessage(ERROR_ACCESS_DENIED));
        break;

      case QSettings::FormatError:
        logCreation(tr("Format error."));
        break;

      default:
        logCreation(tr("Error %1.").arg(static_cast<int>(r)));
        break;
      }

      throw Failed();
    }

    // committing all the directories so they don't get deleted
    for (auto& d : dirs) {
      d->commit();
    }

    logCreation(tr("Done."));

    // launch the new instance
    if (ui->launch->isChecked()) {
      InstanceManager::singleton().setCurrentInstance(ci.instanceName);

      if (mustRestart) {
        ExitModOrganizer(Exit::Restart);
        m_switching = true;
      }
    }

    // close the dialog
    accept();
  } catch (Failed&) {
    // if Failed was thrown, all the directories have been deleted
  }
}

void CreateInstanceDialog::logCreation(const QString& s)
{
  ui->creationLog->insertPlainText(s + "\n");
}

void CreateInstanceDialog::logCreation(const std::wstring& s)
{
  logCreation(QString::fromStdWString(s));
}

void CreateInstanceDialog::selectPage(std::size_t i)
{
  if (i >= m_pages.size()) {
    return;
  }

  ui->pages->setCurrentIndex(static_cast<int>(i));
  m_pages[i]->activated();

  updateNavigation();
}

void CreateInstanceDialog::updateNavigation()
{
  const auto i    = ui->pages->currentIndex();
  const auto last = isOnLastPage();

  ui->next->setEnabled(canNext());
  ui->back->setEnabled(canBack());

  if (last) {
    ui->next->setText(tr("Finish"));
  } else {
    ui->next->setText(m_originalNext);
  }
}

bool CreateInstanceDialog::canNext() const
{
  const auto i = ui->pages->currentIndex();
  return m_pages[i]->skip() || m_pages[i]->ready();
}

bool CreateInstanceDialog::canBack() const
{
  auto i = ui->pages->currentIndex();

  for (;;) {
    if (i == 0) {
      break;
    }

    --i;

    if (!m_pages[i]->skip()) {
      return true;
    }
  }

  return false;
}

bool CreateInstanceDialog::switching() const
{
  return m_switching;
}

CreateInstanceDialog::CreationInfo CreateInstanceDialog::rawCreationInfo() const
{
  const auto iniFilename = QString::fromStdWString(AppConfig::iniFileName());

  CreationInfo ci;

  ci.type            = getSelected(&cid::Page::selectedInstanceType);
  ci.game            = getSelected(&cid::Page::selectedGame);
  ci.gameLocation    = getSelected(&cid::Page::selectedGameLocation);
  ci.gameVariant     = getSelected(&cid::Page::selectedGameVariant, ci.game);
  ci.instanceName    = getSelected(&cid::Page::selectedInstanceName);
  ci.profileSettings = getSelected(&cid::Page::profileSettings);
  ci.paths           = getSelected(&cid::Page::selectedPaths);

  if (ci.type == Portable) {
    ci.dataPath = QDir(InstanceManager::singleton().portablePath()).absolutePath();
  } else {
    ci.dataPath = InstanceManager::singleton().instancePath(ci.instanceName);
  }

  ci.dataPath = QDir::toNativeSeparators(ci.dataPath);
  ci.iniPath  = ci.dataPath + "/" + iniFilename;

  return ci;
}

CreateInstanceDialog::CreationInfo CreateInstanceDialog::creationInfo() const
{
  auto fixVarDir = [](QString& path, const std::wstring& defaultDir) {
    // if the path is empty, it wasn't filled by the user, probably because
    // the "Advanced" checkbox wasn't checked, so use the base dir variable
    // with the default dir

    if (path.isEmpty()) {
      path = cid::makeDefaultPath(defaultDir);
    } else if (!path.contains(PathSettings::BaseDirVariable)) {
      path = QDir(path).absolutePath();
    }

    path = QDir::toNativeSeparators(path);
  };

  auto fixDirPath = [](QString& path) {
    path = QDir::toNativeSeparators(QDir(path).absolutePath());
  };

  auto fixFilePath = [](QString& path) {
    path = QDir::toNativeSeparators(QFileInfo(path).absolutePath());
  };

  auto ci = rawCreationInfo();

  fixDirPath(ci.paths.base);
  fixFilePath(ci.paths.ini);

  fixVarDir(ci.paths.downloads, AppConfig::downloadPath());
  fixVarDir(ci.paths.mods, AppConfig::modsPath());
  fixVarDir(ci.paths.profiles, AppConfig::profilesPath());
  fixVarDir(ci.paths.overwrite, AppConfig::overwritePath());

  return ci;
}
