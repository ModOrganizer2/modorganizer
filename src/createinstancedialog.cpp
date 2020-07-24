#include "createinstancedialog.h"
#include "ui_createinstancedialog.h"
#include "createinstancedialogpages.h"
#include "instancemanager.h"
//#include "plugincontainer.h"
#include "settings.h"
#include "shared/appconfig.h"
//#include <report.h>
#include <iplugingame.h>
#include <utility.h>

using namespace MOBase;

CreateInstanceDialog::CreateInstanceDialog(
  const PluginContainer& pc, QWidget *parent)
    : QDialog(parent), ui(new Ui::CreateInstanceDialog), m_pc(pc)
{
  using namespace cid;

  ui->setupUi(this);
  m_originalNext = ui->next->text();

  m_pages.push_back(std::make_unique<InfoPage>(*this));
  m_pages.push_back(std::make_unique<TypePage>(*this));
  m_pages.push_back(std::make_unique<GamePage>(*this));
  m_pages.push_back(std::make_unique<EditionsPage>(*this));
  m_pages.push_back(std::make_unique<NamePage>(*this));
  m_pages.push_back(std::make_unique<PathsPage>(*this));
  m_pages.push_back(std::make_unique<ConfirmationPage>(*this));

  ui->pages->setCurrentIndex(0);

  updateNavigation();

  connect(ui->next, &QPushButton::clicked, [&]{ next(); });
  connect(ui->back, &QPushButton::clicked, [&]{ back(); });
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

bool CreateInstanceDialog::isOnLastPage() const
{
  for (int i=ui->pages->currentIndex() + 1; i < ui->pages->count(); ++i) {
    if (!m_pages[i]->skip()) {
      return false;
    }
  }

  return true;
}

void CreateInstanceDialog::next()
{
  const auto i = ui->pages->currentIndex();
  const auto last = isOnLastPage();

  if (last) {
    finish();
  } else {
    changePage(+1);
  }
}

void CreateInstanceDialog::back()
{
  changePage(-1);
}

void CreateInstanceDialog::changePage(int d)
{
  std::size_t i = static_cast<std::size_t>(ui->pages->currentIndex());

  if (d > 0) {
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


class Failed {};

class DirectoryCreator
{
public:
  DirectoryCreator(const DirectoryCreator&) = delete;
  DirectoryCreator& operator=(const DirectoryCreator&) = delete;

  static std::unique_ptr<DirectoryCreator> create(
    const QDir& target, std::function<void (QString)> log)
  {
    return std::unique_ptr<DirectoryCreator>(new DirectoryCreator(target, log));
  }

  ~DirectoryCreator()
  {
    rollback();
  }

  void commit()
  {
    m_created.clear();
  }

  void rollback() noexcept
  {
    try
    {
      for (auto itor=m_created.rbegin(); itor!=m_created.rend(); ++itor) {
        const auto r = shell::DeleteDirectoryRecursive(*itor);
        if (!r) {
          m_logger(r.toString());
        }
      }

      m_created.clear();
    }
    catch(...)
    {
      // eat it
    }
  }

private:
  std::function<void (QString)> m_logger;

  DirectoryCreator(const QDir& target, std::function<void (QString)> log)
    : m_logger(log)
  {
    try
    {
      const QString s = QDir::toNativeSeparators(target.absolutePath());
      const QStringList cs = s.split("\\");

      if (cs.empty()) {
        return;
      }

      QDir d(cs[0]);

      for (int i=1; i<cs.size(); ++i) {
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
    }
    catch(...)
    {
      rollback();
      throw;
    }
  }

private:
  std::vector<QDir> m_created;
};

void CreateInstanceDialog::finish()
{
  ui->creationLog->clear();
  logCreation(tr("Creating instance..."));

  const auto& m = InstanceManager::instance();
  const auto ci = creationInfo();

  auto logger = [&](QString s) {
    logCreation(s);
  };

  auto createDir = [&](QString path) {
    return DirectoryCreator::create(path, logger);
  };


  try
  {
    std::vector<std::unique_ptr<DirectoryCreator>> dirs;

    dirs.push_back(createDir(ci.dataPath));
    dirs.push_back(createDir(ci.paths.base));
    dirs.push_back(createDir(PathSettings::resolve(ci.paths.downloads, ci.paths.base)));
    dirs.push_back(createDir(PathSettings::resolve(ci.paths.mods, ci.paths.base)));
    dirs.push_back(createDir(PathSettings::resolve(ci.paths.profiles, ci.paths.base)));
    dirs.push_back(createDir(PathSettings::resolve(ci.paths.overwrite, ci.paths.base)));


    Settings s(ci.iniPath);
    s.game().setName(ci.game->gameName());
    s.game().setDirectory(ci.gameLocation);

    if (!ci.gameEdition.isEmpty()) {
      s.game().setEdition(ci.gameEdition);
    }

    if (ci.type == Global) {
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
    }


    logCreation(tr("Writing %1...").arg(ci.iniPath));

    const auto r = s.sync();
    if (r != QSettings::NoError) {
      switch (r)
      {
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

    for (auto& d : dirs) {
      d->commit();
    }

    logCreation(tr("Done."));
  }
  catch(Failed&)
  {
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
  const auto i = ui->pages->currentIndex();
  const auto last = isOnLastPage();

  ui->next->setEnabled(m_pages[i]->ready());
  ui->back->setEnabled(i > 0);

  if (last) {
    ui->next->setText(tr("Finish"));
  } else {
    ui->next->setText(m_originalNext);
  }
}

CreateInstanceDialog::Types CreateInstanceDialog::instanceType() const
{
  return getSelected(&cid::Page::selectedInstanceType);
}

MOBase::IPluginGame* CreateInstanceDialog::game() const
{
  return getSelected(&cid::Page::selectedGame);
}

QString CreateInstanceDialog::gameLocation() const
{
  return getSelected(&cid::Page::selectedGameLocation);
}

QString CreateInstanceDialog::gameEdition() const
{
  return getSelected(&cid::Page::selectedGameEdition);
}

QString CreateInstanceDialog::instanceName() const
{
  return getSelected(&cid::Page::selectedInstanceName);
}

QString CreateInstanceDialog::dataPath() const
{
  QString s;

  if (instanceType() == Portable) {
    s = QDir(qApp->applicationDirPath()).absolutePath();
  } else {
    s = InstanceManager::instance().instancePath(instanceName());
  }

  return QDir::toNativeSeparators(s);
}

CreateInstanceDialog::Paths CreateInstanceDialog::paths() const
{
  return getSelected(&cid::Page::selectedPaths);
}

CreateInstanceDialog::CreationInfo CreateInstanceDialog::creationInfo() const
{
  CreationInfo ci;

  ci.type         = getSelected(&cid::Page::selectedInstanceType);
  ci.game         = getSelected(&cid::Page::selectedGame);
  ci.gameLocation = getSelected(&cid::Page::selectedGameLocation);
  ci.gameEdition  = getSelected(&cid::Page::selectedGameEdition);
  ci.instanceName = getSelected(&cid::Page::selectedInstanceName);
  ci.paths        = getSelected(&cid::Page::selectedPaths);
  ci.dataPath     = dataPath();

  ci.paths.base = QDir(ci.paths.base).absolutePath();

  if (ci.paths.downloads.isEmpty()) {
    ci.paths.downloads = cid::makeDefaultPath(AppConfig::downloadPath());
  } else if (!ci.paths.downloads.contains(PathSettings::BaseDirVariable)) {
    ci.paths.downloads = QDir(ci.paths.downloads).absolutePath();
  }

  if (ci.paths.mods.isEmpty()) {
    ci.paths.mods = cid::makeDefaultPath(AppConfig::modsPath());
  } else if (!ci.paths.mods.contains(PathSettings::BaseDirVariable)) {
    ci.paths.mods = QDir(ci.paths.mods).absolutePath();
  }

  if (ci.paths.profiles.isEmpty()) {
    ci.paths.profiles = cid::makeDefaultPath(AppConfig::profilesPath());
  } else if (!ci.paths.profiles.contains(PathSettings::BaseDirVariable)) {
    ci.paths.profiles = QDir(ci.paths.profiles).absolutePath();
  }

  if (ci.paths.overwrite.isEmpty()) {
    ci.paths.overwrite = cid::makeDefaultPath(AppConfig::overwritePath());
  } else if (!ci.paths.overwrite.contains(PathSettings::BaseDirVariable)) {
    ci.paths.overwrite = QDir(ci.paths.overwrite).absolutePath();
  }

  ci.iniPath = QFileInfo(
    ci.dataPath + "/" + QString::fromStdWString(AppConfig::iniFileName()))
      .absoluteFilePath();

  ci.paths.base = QDir::toNativeSeparators(ci.paths.base);
  ci.paths.downloads = QDir::toNativeSeparators(ci.paths.downloads);
  ci.paths.mods = QDir::toNativeSeparators(ci.paths.mods);
  ci.paths.profiles = QDir::toNativeSeparators(ci.paths.profiles);
  ci.paths.overwrite = QDir::toNativeSeparators(ci.paths.overwrite);
  ci.paths.ini = QDir::toNativeSeparators(ci.paths.ini);

  return ci;
}
