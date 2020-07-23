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
  Page(CreateInstanceDialog& dlg)
    : ui(dlg.getUI()), m_dlg(dlg), m_pc(dlg.pluginContainer())
  {
  }

  virtual bool ready() const
  {
    return true;
  }

  virtual bool skip() const
  {
    // no-op
    return false;
  }

  virtual void activated()
  {
    // no-op
  }

  void updateNavigation()
  {
    m_dlg.updateNavigation();
  }

  void next()
  {
    m_dlg.next();
  }


  virtual CreateInstanceDialog::Types selectedType() const
  {
    // no-op
    return CreateInstanceDialog::NoType;
  }

  virtual MOBase::IPluginGame* selectedGame() const
  {
    // no-op
    return nullptr;
  }

  virtual QString instanceName() const
  {
    // no-op
    return {};
  }

protected:
  Ui::CreateInstanceDialog* ui;
  CreateInstanceDialog& m_dlg;
  const PluginContainer& m_pc;
};


class TypePage : public Page
{
public:
  TypePage(CreateInstanceDialog& dlg)
    : Page(dlg), m_type(CreateInstanceDialog::NoType)
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
    return (m_type != CreateInstanceDialog::NoType);
  }

  CreateInstanceDialog::Types selectedType() const
  {
    return m_type;
  }

  void global()
  {
    m_type = CreateInstanceDialog::Global;

    ui->createGlobal->setChecked(true);
    ui->createPortable->setChecked(false);

    next();
  }

  void portable()
  {
    m_type = CreateInstanceDialog::Portable;

    ui->createGlobal->setChecked(false);
    ui->createPortable->setChecked(true);

    next();
  }

private:
  CreateInstanceDialog::Types m_type;
};


class GamePage : public Page
{
public:
  GamePage(CreateInstanceDialog& dlg)
    : Page(dlg), m_selection(nullptr)
  {
    createGames();
    fillList();

    QObject::connect(ui->showAllGames, &QCheckBox::clicked, [&]{ fillList(); });
  }

  bool ready() const override
  {
    return (m_selection != nullptr);
  }

  MOBase::IPluginGame* selectedGame() const override
  {
    if (!m_selection) {
      return nullptr;
    }

    return m_selection->game;
  }

  void select(MOBase::IPluginGame* game)
  {
    Game* checked = findGame(game);

    if (checked) {
      if (!checked->installed) {
        const auto path = QFileDialog::getExistingDirectory(
          &m_dlg, QObject::tr("Find game installation"));

        if (path.isEmpty()) {
          checked = nullptr;
        } else {
          checked = checkInstallation(path, checked);
        }
      }
    }

    m_selection = checked;
    selectButton(checked);
    updateNavigation();
  }

  void selectCustom()
  {
    const auto path = QFileDialog::getExistingDirectory(
      &m_dlg, QObject::tr("Find game installation"));

    if (path.isEmpty()) {
      selectButton(m_selection);
      return;
    }

    for (auto& g : m_games) {
      if (g->game->looksValid(path)) {
        g->dir = path;
        g->installed = true;
        select(g->game);
        updateButton(g.get());
        return;
      }
    }

    warnUnrecognized(path);
    selectButton(m_selection);
  }

  void warnUnrecognized(const QString& path)
  {
    QString supportedGames;
    for (auto* game : m_pc.plugins<MOBase::IPluginGame>()) {
      supportedGames += "<li>" + game->gameName() + "</li>";
    }

    QMessageBox::warning(&m_dlg,
      QObject::tr("Unrecognized game"),
      QObject::tr(
        "The folder %1 does not seem to contain a game Mod Organizer can "
        "manage.<br><br><b>These are the games that can be managed:</b>"
        "<ul>%2</ul>").arg(path).arg(supportedGames));
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
  Game* m_selection;


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

  void updateButton(Game* g)
  {
    if (!g || !g->button) {
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
    // go through each game, set the button that is for game `g` as active;
    // some button might not exist, which happens when selecting a custom
    // folder for a game that was considered uninstalled

    for (const auto& gg : m_games) {
      if (!g) {
        // nothing should be selected
        if (gg->button) {
          gg->button->setChecked(false);
        }

        continue;
      }

      if (gg->game == g->game) {
        // this is the button that should be selected

        if (!gg->button) {
          // this happens when the button wasn't visible because the game
          // was not installed; create it and show it
          // and it has a button, just check it
          createGameButton(gg.get());
          ui->games->addButton(gg->button, QDialogButtonBox::AcceptRole);
        }

        gg->button->setChecked(true);
        gg->button->setFocus();
      } else {
        // this is not the button you're looking for
        if (gg->button) {
          gg->button->setChecked(false);
        }
      }
    }
  }

  QCommandLinkButton* createCustomButton()
  {
    auto* b = new QCommandLinkButton;

    b->setText(QObject::tr("Browse..."));
    b->setDescription(
      QObject::tr("The folder must contain a valid game installation"));

    QObject::connect(b, &QAbstractButton::clicked, [&] {
      selectCustom();
    });

    return b;
  }

  void createGameButton(Game* g)
  {
    g->button = new QCommandLinkButton;
    g->button->setCheckable(true);

    updateButton(g);

    QObject::connect(g->button, &QAbstractButton::clicked, [g, this] {
      select(g->game);
    });
  }

  void fillList()
  {
    const bool showAll = ui->showAllGames->isChecked();

    ui->games->clear();

    ui->games->addButton(createCustomButton(), QDialogButtonBox::AcceptRole);

    for (auto& g : m_games) {
      g->button = nullptr;

      if (!showAll && !g->installed) {
        // not installed
        continue;
      }

      createGameButton(g.get());
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


class EditionsPage : public Page
{
public:
  EditionsPage(CreateInstanceDialog& dlg)
    : Page(dlg), m_previousGame(nullptr)
  {
  }

  bool ready() const override
  {
    return !m_selection.isEmpty();
  }

  bool skip() const override
  {
    auto* g = m_dlg.selectedGame();
    if (!g) {
      // shouldn't happen
      return true;
    }

    const auto variants = g->gameVariants();
    return (variants.size() < 2);
  }

  void activated() override
  {
    auto* g = m_dlg.selectedGame();

    if (m_previousGame != g) {
      m_previousGame = g;
      m_selection = "";
      fillList();
    }
  }

  void select(const QString& variant)
  {
    for (auto* b : m_buttons) {
      if (b->text() == variant) {
        m_selection = variant;
        b->setChecked(true);
      } else {
        b->setChecked(false);
      }
    }

    updateNavigation();
  }

private:
  MOBase::IPluginGame* m_previousGame;
  std::vector<QCommandLinkButton*> m_buttons;
  QString m_selection;

  void fillList()
  {
    ui->editions->clear();
    m_buttons.clear();

    auto* g = m_dlg.selectedGame();
    if (!g) {
      // shouldn't happen
      return;
    }

    const auto variants = g->gameVariants();
    for (auto& v : variants) {
      auto* b = new QCommandLinkButton(v);
      b->setCheckable(true);

      QObject::connect(b, &QAbstractButton::clicked, [v, this] {
        select(v);
      });

      ui->editions->addButton(b, QDialogButtonBox::AcceptRole);
      m_buttons.push_back(b);
    }
  }
};


class NamePage : public Page
{
public:
  NamePage(CreateInstanceDialog& dlg)
    : Page(dlg), m_modified(false), m_okay(false)
  {
    m_originalLabel = ui->instanceNameLabel->text();

    QObject::connect(
      ui->instanceName, &QLineEdit::textEdited, [&]{ onChanged(); });
  }

  bool ready() const override
  {
    return m_okay;
  }

  bool skip() const override
  {
    return (m_dlg.selectedType() == CreateInstanceDialog::Portable);
  }

  void activated() override
  {
    auto* g = m_dlg.selectedGame();
    if (!g) {
      // shouldn't happen, next should be disabled
      return;
    }

    ui->instanceNameLabel->setText(m_originalLabel.arg(g->gameName()));

    if (!m_modified || ui->instanceName->text().isEmpty()) {
      const auto n = InstanceManager::instance().makeUniqueName(g->gameName());
      ui->instanceName->setText(n);
      m_modified = false;
    }

    updateWarnings();
  }

  QString instanceName() const override
  {
    if (!m_okay) {
      return {};
    }

    const auto text = ui->instanceName->text().trimmed();
    return InstanceManager::instance().sanitizeInstanceName(text);
  }

private:
  QString m_originalLabel;
  bool m_modified;
  bool m_okay;

  void onChanged()
  {
    m_modified = true;
    updateWarnings();
  }

  void updateWarnings()
  {
    bool exists = false;
    bool invalid = false;
    bool empty = false;

    auto& m = InstanceManager::instance();

    const auto text = ui->instanceName->text().trimmed();

    if (text.isEmpty()) {
      empty = true;
    } else {
      const auto sanitized = m.sanitizeInstanceName(text);
      if (text != sanitized) {
        invalid = true;
      } else {
        exists = m.instanceExists(text);
      }
    }

    if (exists) {
      m_okay = false;
      ui->instanceNameExists->setVisible(true);
      ui->instanceNameInvalid->setVisible(false);
    } else if (invalid) {
      m_okay = false;
      ui->instanceNameExists->setVisible(false);
      ui->instanceNameInvalid->setVisible(true);
    } else {
      m_okay = !empty;
      ui->instanceNameExists->setVisible(false);
      ui->instanceNameInvalid->setVisible(false);
    }

    updateNavigation();
  }
};


class PathsPage : public Page
{
public:
  PathsPage(CreateInstanceDialog& dlg)
    : Page(dlg)
  {
    QObject::connect(
      ui->advancedPathOptions, &QCheckBox::clicked, [&]{ onAdvanced(); });

    ui->pathPages->setCurrentIndex(0);
  }

  void activated() override
  {
    const auto root = InstanceManager::instance().instancesPath();
    const auto path = QDir::toNativeSeparators(root + "/" + m_dlg.instanceName());

    ui->location->setText(path);
  }

private:
  void onAdvanced()
  {
    if (ui->advancedPathOptions->isChecked()) {
      ui->pathPages->setCurrentIndex(1);
    } else {
      ui->pathPages->setCurrentIndex(0);
    }
  }
};

} // namespace


CreateInstanceDialog::CreateInstanceDialog(
  const PluginContainer& pc, QWidget *parent)
    : QDialog(parent), ui(new Ui::CreateInstanceDialog), m_pc(pc)
{
  using namespace cid;

  ui->setupUi(this);

  m_pages.push_back(std::make_unique<TypePage>(*this));
  m_pages.push_back(std::make_unique<GamePage>(*this));
  m_pages.push_back(std::make_unique<EditionsPage>(*this));
  m_pages.push_back(std::make_unique<NamePage>(*this));
  m_pages.push_back(std::make_unique<PathsPage>(*this));

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

void CreateInstanceDialog::next()
{
  selectPage(ui->pages->currentIndex() + 1);
}

void CreateInstanceDialog::back()
{
  selectPage(ui->pages->currentIndex() - 1);
}

void CreateInstanceDialog::selectPage(std::size_t i)
{
  while (i < m_pages.size()) {
    if (!m_pages[i]->skip()) {
      break;
    }

    ++i;
  }

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
  const auto last = (i == (ui->pages->count() - 1));

  ui->next->setEnabled(m_pages[i]->ready() && !last);
  ui->back->setEnabled(i > 0);
}

CreateInstanceDialog::Types CreateInstanceDialog::selectedType() const
{
  for (auto&& p : m_pages) {
    const auto t = p->selectedType();
    if (t != NoType) {
      return t;
    }
  }

  return NoType;
}

MOBase::IPluginGame* CreateInstanceDialog::selectedGame() const
{
  for (auto&& p : m_pages) {
    if (auto* g=p->selectedGame()) {
      return g;
    }
  }

  return nullptr;
}

QString CreateInstanceDialog::instanceName() const
{
  for (auto&& p : m_pages) {
    const auto s = p->instanceName();
    if (!s.isEmpty()) {
      return s;
    }
  }

  return {};
}
