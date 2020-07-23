#include "createinstancedialog.h"
#include "ui_createinstancedialog.h"
#include "instancemanager.h"
#include "plugincontainer.h"
#include <report.h>
#include <iplugingame.h>

namespace cid
{

class Page
{
public:
  Page(CreateInstanceDialog& dlg, std::size_t i)
    : ui(dlg.getUI()), m_dlg(dlg), m_pc(dlg.pluginContainer()), m_index(i)
  {
  }

  virtual bool ready() const
  {
    return true;
  }

  void next()
  {
    m_dlg.next();
  }

protected:
  Ui::CreateInstanceDialog* ui;
  CreateInstanceDialog& m_dlg;
  const PluginContainer& m_pc;

private:
  std::size_t m_index;
};


class TypePage : public Page
{
public:
  TypePage(CreateInstanceDialog& dlg, std::size_t i)
    : Page(dlg, i)
  {
    ui->createGlobal->setDescription(
      ui->createGlobal->description()
        .arg(InstanceManager::instance().instancesPath()));

    ui->createPortable->setDescription(
      ui->createPortable->description()
        .arg(qApp->applicationDirPath()));

    QObject::connect(
      ui->createGlobal, &QAbstractButton::clicked, [&]{ global(); });

    QObject::connect(
      ui->createPortable, &QAbstractButton::clicked, [&]{ portable(); });
  }

  bool ready() const override
  {
    return m_global.has_value();
  }

  void global()
  {
    m_global = true;

    ui->createGlobal->setChecked(true);
    ui->createPortable->setChecked(false);

    next();
  }

  void portable()
  {
    m_global = false;

    ui->createGlobal->setChecked(false);
    ui->createPortable->setChecked(true);

    next();
  }

private:
  std::optional<bool> m_global;
};


class GamePage : public Page
{
public:
  GamePage(CreateInstanceDialog& dlg, std::size_t i)
    : Page(dlg, i)
  {
    createGames();
    fillList();

    QObject::connect(ui->showAllGames, &QCheckBox::clicked, [&]{ fillList(); });
  }

  void select(MOBase::IPluginGame* game)
  {
    Game* checked = findGame(game);
    if (!checked) {
      return;
    }

    if (!checked->installed) {
      const auto path = QFileDialog::getExistingDirectory(
        &m_dlg, QObject::tr("Find game installation"));

      if (path.isEmpty()) {
        checked = nullptr;
      } else {
        checked = checkInstallation(path, checked);
      }
    }

    selectButton(checked);
  }

private:
  struct Game
  {
    MOBase::IPluginGame* game = nullptr;
    QCommandLinkButton* button = nullptr;
    QString dir;
    bool installed = false;

    Game(MOBase::IPluginGame* g)
      : game(g), installed(g->isInstalled())
    {
      if (installed) {
        dir = game->gameDirectory().path();
      }
    }

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
  };

  std::vector<std::unique_ptr<Game>> m_games;


  Game* findGame(MOBase::IPluginGame* game)
  {
    for (auto& g : m_games) {
      if (g->game == game) {
        return g.get();
      }
    }

    return nullptr;
  }

  void createGames()
  {
    m_games.clear();

    for (auto* game : m_pc.plugins<MOBase::IPluginGame>()) {
      m_games.push_back(std::make_unique<Game>(game));
    }
  }

  void createButton(Game* g)
  {
    g->button = new QCommandLinkButton;
    g->button->setCheckable(true);

    updateButton(g);

    QObject::connect(g->button, &QAbstractButton::clicked, [g, this] {
      select(g->game);
    });
  }

  void updateButton(Game* g)
  {
    if (!g->button) {
      return;
    }

    g->button->setText(g->game->gameName());

    if (g->installed) {
      g->button->setDescription(g->dir);
    } else {
      g->button->setDescription(QObject::tr("No installation found"));
    }
  }

  void selectButton(Game* g)
  {
    for (const auto& gg : m_games) {
      if (!gg->button) {
        continue;
      }

      if (g) {
        gg->button->setChecked(gg->game == g->game);
      } else {
        gg->button->setChecked(false);
      }
    }
  }

  void fillList()
  {
    const bool showAll = ui->showAllGames->isChecked();

    ui->games->clear();

    for (auto& g : m_games) {
      g->button = nullptr;

      if (!showAll && !g->installed) {
        // not installed
        continue;
      }

      createButton(g.get());
      ui->games->addButton(g->button, QDialogButtonBox::AcceptRole);
    }
  }

  Game* checkInstallation(const QString& path, Game* g)
  {
    if (g->game->looksValid(path)) {
      // okay
      return g;
    }

    // the selected game can't use that folder, find another one
    auto* otherGame = findAnotherGame(path);
    if (otherGame == g->game) {
      // shouldn't happen, but okay
      return g;
    }

    if (otherGame) {
      auto* confirmedGame = confirmOtherGame(path, g->game, otherGame);

      if (!confirmedGame) {
        // cancelled
        return nullptr;
      }

      // make it look like the user clicked that button instead
      g = findGame(confirmedGame);
      if (!g) {
        return nullptr;
      }
    } else {
      // nothing can manage this, but the user can override
      if (!confirmUnknown(path, g->game)) {
        // cancelled
        return nullptr;
      }
    }

    // remember this path
    g->dir = path;
    g->installed = true;

    updateButton(g);

    return g;
  }

  MOBase::IPluginGame* findAnotherGame(const QString& path)
  {
    for (auto* otherGame : m_pc.plugins<MOBase::IPluginGame>()) {
      if (otherGame->looksValid(path)) {
        return otherGame;
      }
    }

    return nullptr;
  }

  bool confirmUnknown(const QString& path, MOBase::IPluginGame* game)
  {
    const auto r = MOBase::TaskDialog(&m_dlg)
      .title(QObject::tr("Unrecognized game"))
      .main(QObject::tr("Unrecognized game"))
      .content(QObject::tr(
        "The folder %1 does not seem to contain installation for "
        "<span style=\"white-space: nowrap; font-weight: bold;\">%2</span> or "
        "any other game Mod Organizer can manage.")
          .arg(path)
          .arg(game->gameName()))
      .button({
        QObject::tr("Use this folder for %1").arg(game->gameName()),
        QObject::tr("I know what I'm doing"),
        QMessageBox::Ignore})
      .button({
        QObject::tr("Cancel"),
        QMessageBox::Cancel})
      .exec();

    return (r == QMessageBox::Ignore);
  }

  MOBase::IPluginGame* confirmOtherGame(
    const QString& path,
    MOBase::IPluginGame* selectedGame, MOBase::IPluginGame* guessedGame)
  {
    const auto r = MOBase::TaskDialog(&m_dlg)
      .title(QObject::tr("Incorrect game"))
      .main(QObject::tr("Incorrect game"))
      .content(QObject::tr(
        "The folder %1 seems to contain an installation for "
        "<span style=\"white-space: nowrap; font-weight: bold;\">%2</span>, "
        "not "
        "<span style=\"white-space: nowrap; font-weight: bold;\">%3</span>.")
          .arg(path)
          .arg(guessedGame->gameName())
          .arg(selectedGame->gameName()))
      .button({
        QObject::tr("Manage %1 instead").arg(guessedGame->gameName()),
        QMessageBox::Ok})
      .button({
        QObject::tr("Use this folder for %1").arg(selectedGame->gameName()),
        QObject::tr("I know what I'm doing"),
        QMessageBox::Ignore})
      .button({
        QObject::tr("Cancel"),
        QMessageBox::Cancel})
      .exec();

    switch (r)
    {
      case QMessageBox::Ok:
        return guessedGame;

      case QMessageBox::Ignore:
        return selectedGame;

      case QMessageBox::Cancel:
      default:
        return nullptr;
    }
  }
};


class NamePage : public Page
{
public:
  NamePage(CreateInstanceDialog& dlg, std::size_t i)
    : Page(dlg, i)
  {
  }
};


class PathsPage : public Page
{
public:
  PathsPage(CreateInstanceDialog& dlg, std::size_t i)
    : Page(dlg, i)
  {
  }
};

} // namespace


CreateInstanceDialog::CreateInstanceDialog(
  const PluginContainer& pc, QWidget *parent)
    : QDialog(parent), ui(new Ui::CreateInstanceDialog), m_pc(pc)
{
  using namespace cid;

  ui->setupUi(this);

  m_pages.push_back(std::make_unique<TypePage>(*this, 0));
  m_pages.push_back(std::make_unique<GamePage>(*this, 1));
  m_pages.push_back(std::make_unique<NamePage>(*this, 2));
  m_pages.push_back(std::make_unique<PathsPage>(*this, 3));

  ui->pages->setCurrentIndex(0);

  updateNavigationButtons();

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

void CreateInstanceDialog::next()
{
  ui->pages->setCurrentIndex(ui->pages->currentIndex() + 1);
  updateNavigationButtons();
}

void CreateInstanceDialog::back()
{
  ui->pages->setCurrentIndex(ui->pages->currentIndex() - 1);
  updateNavigationButtons();
}

void CreateInstanceDialog::updateNavigationButtons()
{
  const auto i = ui->pages->currentIndex();
  const auto last = (i == (ui->pages->count() - 1));

  ui->next->setEnabled(m_pages[i]->ready() && !last);
  ui->back->setEnabled(i > 0);
}
