#include "createinstancedialogpages.h"
#include "filesystemutilities.h"
#include "instancemanager.h"
#include "pluginmanager.h"
#include "settings.h"
#include "settingsdialognexus.h"
#include "shared/appconfig.h"
#include "ui_createinstancedialog.h"
#include <iplugingame.h>
#include <report.h>
#include <utility.h>

namespace cid
{

using namespace MOBase;
using MOBase::TaskDialog;

// returns %base_dir%/dir
//
QString makeDefaultPath(const std::wstring& dir)
{
  return QDir::toNativeSeparators(
      PathSettings::makeDefaultPath(QString::fromStdWString(dir)));
}

QString toLocalizedString(CreateInstanceDialog::Types t)
{
  switch (t) {
  case CreateInstanceDialog::Global:
    return QObject::tr("Global");

  case CreateInstanceDialog::Portable:
    return QObject::tr("Portable");

  default:
    return QObject::tr("Instance type: %1").arg(QObject::tr("?"));
  }
}

PlaceholderLabel::PlaceholderLabel(QLabel* label)
    : m_label(label), m_original(label->text())
{}

void PlaceholderLabel::setText(const QString& arg)
{
  if (m_original.contains("%1")) {
    m_label->setText(m_original.arg(arg));
  }
}

void PlaceholderLabel::setVisible(bool b)
{
  m_label->setVisible(b);
}

Page::Page(CreateInstanceDialog& dlg)
    : ui(dlg.getUI()), m_dlg(dlg), m_pc(dlg.pluginManager()), m_skip(false),
      m_firstActivation(true)
{}

bool Page::ready() const
{
  return true;
}

bool Page::skip() const
{
  // setSkip() overrides this if it's true
  return m_skip || doSkip();
}

bool Page::doSkip() const
{
  return false;
}

void Page::doActivated(bool)
{
  // no-op
}

void Page::activated()
{
  doActivated(m_firstActivation);
  m_firstActivation = false;
}

void Page::setSkip(bool b)
{
  m_skip = b;
}

void Page::updateNavigation()
{
  m_dlg.updateNavigation();
}

void Page::next()
{
  m_dlg.next();
}

bool Page::action(CreateInstanceDialog::Actions a)
{
  // no-op
  return false;
}

CreateInstanceDialog::Types Page::selectedInstanceType() const
{
  // no-op
  return CreateInstanceDialog::NoType;
}

IPluginGame* Page::selectedGame() const
{
  // no-op
  return nullptr;
}

QString Page::selectedGameLocation() const
{
  // no-op
  return {};
}

QString Page::selectedGameVariant(MOBase::IPluginGame*) const
{
  // no-op
  return {};
}

QString Page::selectedInstanceName() const
{
  // no-op
  return {};
}

CreateInstanceDialog::Paths Page::selectedPaths() const
{
  // no-op
  return {};
}

CreateInstanceDialog::ProfileSettings Page::profileSettings() const
{
  // no-op
  return {};
}

IntroPage::IntroPage(CreateInstanceDialog& dlg)
    : Page(dlg), m_skip(GlobalSettings::hideCreateInstanceIntro())
{
  QObject::connect(ui->hideIntro, &QCheckBox::toggled, [&] {
    GlobalSettings::setHideCreateInstanceIntro(ui->hideIntro->isChecked());
  });
}

bool IntroPage::doSkip() const
{
  return m_skip;
}

TypePage::TypePage(CreateInstanceDialog& dlg)
    : Page(dlg), m_type(CreateInstanceDialog::NoType)
{
  // replace placeholders with actual paths
  ui->createGlobal->setDescription(ui->createGlobal->description().arg(
      InstanceManager::singleton().globalInstancesRootPath()));

  ui->createPortable->setDescription(ui->createPortable->description().arg(
      InstanceManager::singleton().portablePath()));

  // disable portable button if it already exists
  if (InstanceManager::singleton().portableInstanceExists()) {
    ui->createPortable->setEnabled(false);
    ui->portableExistsLabel->setVisible(true);
  } else {
    ui->createPortable->setEnabled(true);
    ui->portableExistsLabel->setVisible(false);
  }

  QObject::connect(ui->createGlobal, &QAbstractButton::clicked, [&] {
    global();
  });

  QObject::connect(ui->createPortable, &QAbstractButton::clicked, [&] {
    portable();
  });
}

bool TypePage::ready() const
{
  return (m_type != CreateInstanceDialog::NoType);
}

CreateInstanceDialog::Types TypePage::selectedInstanceType() const
{
  return m_type;
}

void TypePage::global()
{
  m_type = CreateInstanceDialog::Global;

  ui->createGlobal->setChecked(true);
  ui->createPortable->setChecked(false);

  next();
}

void TypePage::portable()
{
  m_type = CreateInstanceDialog::Portable;

  ui->createGlobal->setChecked(false);
  ui->createPortable->setChecked(true);

  next();
}

void TypePage::doActivated(bool firstTime)
{
  if (firstTime) {
    ui->createGlobal->setFocus();
  }
}

GamePage::Game::Game(IPluginGame* g) : game(g), installed(g->isInstalled())
{
  if (installed) {
    dir = game->gameDirectory().path();
  }
}

GamePage::GamePage(CreateInstanceDialog& dlg) : Page(dlg), m_selection(nullptr)
{
  createGames();
  fillList();

  m_filter.setEdit(ui->gamesFilter);

  QObject::connect(&m_filter, &FilterWidget::changed, [&] {
    fillList();
  });
  QObject::connect(ui->showAllGames, &QCheckBox::clicked, [&] {
    fillList();
  });
}

bool GamePage::ready() const
{
  return (m_selection != nullptr);
}

bool GamePage::action(CreateInstanceDialog::Actions a)
{
  using Actions = CreateInstanceDialog::Actions;

  if (a == Actions::Find) {
    ui->gamesFilter->setFocus();
    return true;
  }

  return false;
}

IPluginGame* GamePage::selectedGame() const
{
  if (!m_selection) {
    return nullptr;
  }

  return m_selection->game;
}

QString GamePage::selectedGameLocation() const
{
  if (!m_selection) {
    return {};
  }

  return QDir::toNativeSeparators(m_selection->dir);
}

void GamePage::select(IPluginGame* game, const QString& dir)
{
  Game* checked = findGame(game);

  if (checked) {
    if (!checked->installed || (detectMicrosoftStore(checked->dir) &&
                                !confirmMicrosoftStore(checked->dir, checked->game))) {
      if (dir.isEmpty()) {
        // the selected game has no installation directory and none was given,
        // ask the user

        const auto path = QFileDialog::getExistingDirectory(
            &m_dlg, QObject::tr("Find game installation for %1").arg(game->gameName()));

        if (path.isEmpty()) {
          // cancelled
          checked = nullptr;
        } else if (detectMicrosoftStore(path) && !confirmMicrosoftStore(path, game)) {
          // cancelled
          checked = nullptr;
        } else {
          // check whether a plugin supports the given directory; this can
          // return the same plugin, a different one, or null
          checked = checkInstallation(path, checked);

          if (checked) {
            // plugin was found, remember this path
            checked->dir       = path;
            checked->installed = true;
          }
        }
      } else {
        // the selected game didn't detect anything, but a directory was given,
        // so use that
        checked->dir       = dir;
        checked->installed = true;
      }
    }
  }

  // select this plugin, if any
  m_selection = checked;

  // update the button associated with it in case the paths have changed
  updateButton(checked);

  // toggle it on
  selectButton(checked);

  updateNavigation();

  if (checked) {
    // automatically move to the next page when a game is selected
    next();
  }
}

void GamePage::selectCustom()
{
  const auto path =
      QFileDialog::getExistingDirectory(&m_dlg, QObject::tr("Find game installation"));

  if (path.isEmpty()) {
    // reselect the previous button
    selectButton(m_selection);
    return;
  }

  // Microsoft store games are not supported
  if (detectMicrosoftStore(path) && !confirmMicrosoftStore(path, nullptr)) {
    // reselect the previous button
    selectButton(m_selection);
    return;
  }

  // try to find a plugin that likes this directory
  for (auto& g : m_games) {
    if (g->game->looksValid(path)) {
      // found one
      g->dir       = path;
      g->installed = true;

      // select it
      select(g->game);

      // update the button because the path has changed
      updateButton(g.get());

      return;
    }
  }

  // warning to the user
  warnUnrecognized(path);

  // reselect the previous button
  selectButton(m_selection);
}

void GamePage::warnUnrecognized(const QString& path)
{
  // put the list of supported games in the details textbox
  QString supportedGames;
  for (auto* game : sortedGamePlugins()) {
    supportedGames += game->gameName() + "\n";
  }

  QMessageBox dlg(&m_dlg);

  dlg.setWindowTitle(QObject::tr("Unrecognized game"));
  dlg.setText(
      QObject::tr("The folder %1 does not seem to contain a game Mod Organizer can "
                  "manage.")
          .arg(path));
  dlg.setInformativeText(QObject::tr("See details for the list of supported games."));
  dlg.setDetailedText(supportedGames);
  dlg.setIcon(QMessageBox::Warning);
  dlg.setStandardButtons(QMessageBox::Ok);

  dlg.exec();
}

std::vector<IPluginGame*> GamePage::sortedGamePlugins() const
{
  std::vector<IPluginGame*> v;

  // all game plugins
  for (auto* game : m_pc.plugins<IPluginGame>()) {
    v.push_back(game);
  }

  // natsort
  std::sort(v.begin(), v.end(), [](auto* a, auto* b) {
    return (naturalCompare(a->gameName(), b->gameName()) < 0);
  });

  return v;
}

void GamePage::createGames()
{
  m_games.clear();

  for (auto* game : sortedGamePlugins()) {
    m_games.push_back(std::make_unique<Game>(game));
  }
}

GamePage::Game* GamePage::findGame(IPluginGame* game)
{
  for (auto& g : m_games) {
    if (g->game == game) {
      return g.get();
    }
  }

  return nullptr;
}

void GamePage::createGameButton(Game* g)
{
  g->button = new QCommandLinkButton;
  g->button->setCheckable(true);

  updateButton(g);

  QObject::connect(g->button, &QAbstractButton::clicked, [g, this] {
    select(g->game);
  });
}

void GamePage::addButton(QAbstractButton* b)
{
  auto* ly = static_cast<QVBoxLayout*>(ui->games->layout());

  // insert before the stretch
  ly->insertWidget(ly->count() - 1, b);
}

void GamePage::updateButton(Game* g)
{
  if (!g || !g->button) {
    return;
  }

  g->button->setText(g->game->gameName().replace("&", "&&"));
  g->button->setIcon(g->game->gameIcon());

  if (g->installed) {
    g->button->setDescription(g->dir);
  } else {
    g->button->setDescription(QObject::tr("No installation found"));
  }
}

void GamePage::selectButton(Game* g)
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
        addButton(gg->button);
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

void GamePage::clearButtons()
{
  auto* ly = static_cast<QVBoxLayout*>(ui->games->layout());

  ui->games->setUpdatesEnabled(false);

  // delete all children
  qDeleteAll(ui->games->findChildren<QWidget*>("", Qt::FindDirectChildrenOnly));

  // stretch widgets added with addStretch() are not in the parent widget,
  // they have to be deleted from the layout itself
  while (auto* child = ly->takeAt(0))
    delete child;

  // add a stretch, buttons will be added before
  ly->addStretch();

  ui->games->setUpdatesEnabled(true);

  for (auto& g : m_games) {
    // all buttons have been deleted
    g->button = nullptr;
  }
}

QCommandLinkButton* GamePage::createCustomButton()
{
  auto* b = new QCommandLinkButton;

  b->setText(QObject::tr("Browse..."));
  b->setDescription(QObject::tr("The folder must contain a valid game installation"));

  QObject::connect(b, &QAbstractButton::clicked, [&] {
    selectCustom();
  });

  return b;
}

void GamePage::fillList()
{
  const bool showAll = ui->showAllGames->isChecked();

  clearButtons();

  Game* firstButton = nullptr;

  for (auto& g : m_games) {
    if (!showAll && !g->installed) {
      // not installed
      continue;
    }

    if (!m_filter.matches(g->game->gameName())) {
      // filtered out
      continue;
    }

    createGameButton(g.get());
    addButton(g->button);

    if (!firstButton) {
      firstButton = g.get();
    }
  }

  // browse button
  addButton(createCustomButton());

  if (firstButton) {
    firstButton->button->setDefault(true);
  }
}

GamePage::Game* GamePage::checkInstallation(const QString& path, Game* g)
{
  if (g->game->looksValid(path)) {
    // okay
    return g;
  }

  if (detectMicrosoftStore(path) && confirmMicrosoftStore(path, g->game)) {
    // okay
    return g;
  }

  // the selected game can't use that folder, find another one
  IPluginGame* otherGame = nullptr;

  for (auto* gg : m_pc.plugins<IPluginGame>()) {
    if (gg->looksValid(path)) {
      otherGame = gg;
      break;
    }
  }

  if (otherGame == g->game) {
    // shouldn't happen, but okay
    return g;
  }

  if (otherGame) {
    // an alternative was found, ask the user about it
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
  g->dir       = path;
  g->installed = true;

  updateButton(g);

  return g;
}

bool GamePage::detectMicrosoftStore(const QString& path)
{
  return path.contains("/ModifiableWindowsApps/") || path.contains("/WindowsApps/");
}

bool GamePage::confirmMicrosoftStore(const QString& path, IPluginGame* game)
{
  const auto r =
      TaskDialog(&m_dlg)
          .title(QObject::tr("Microsoft Store game"))
          .main(QObject::tr("Microsoft Store game"))
          .content(
              QObject::tr(
                  "The folder %1 seems to be a Microsoft Store game install.  Games"
                  " installed through the Microsoft Store are not supported by Mod "
                  "Organizer"
                  " and will not work properly.")
                  .arg(path))
          .button({game ? QObject::tr("Use this folder for %1").arg(game->gameName())
                        : QObject::tr("Use this folder"),
                   QObject::tr("I know what I'm doing"), QMessageBox::Ignore})
          .button({QObject::tr("Cancel"), QMessageBox::Cancel})
          .exec();

  return (r == QMessageBox::Ignore);
}

bool GamePage::confirmUnknown(const QString& path, IPluginGame* game)
{
  const auto r =
      TaskDialog(&m_dlg)
          .title(QObject::tr("Unrecognized game"))
          .main(QObject::tr("Unrecognized game"))
          .content(
              QObject::tr("The folder %1 does not seem to contain an installation for "
                          "<span style=\"white-space: nowrap; font-weight: "
                          "bold;\">%2</span> or "
                          "for any other game Mod Organizer can manage.")
                  .arg(path)
                  .arg(game->gameName()))
          .button({QObject::tr("Use this folder for %1").arg(game->gameName()),
                   QObject::tr("I know what I'm doing"), QMessageBox::Ignore})
          .button({QObject::tr("Cancel"), QMessageBox::Cancel})
          .exec();

  return (r == QMessageBox::Ignore);
}

IPluginGame* GamePage::confirmOtherGame(const QString& path, IPluginGame* selectedGame,
                                        IPluginGame* guessedGame)
{
  const auto r =
      TaskDialog(&m_dlg)
          .title(QObject::tr("Incorrect game"))
          .main(QObject::tr("Incorrect game"))
          .content(
              QObject::tr(
                  "The folder %1 seems to contain an installation for "
                  "<span style=\"white-space: nowrap; font-weight: bold;\">%2</span>, "
                  "not "
                  "<span style=\"white-space: nowrap; font-weight: bold;\">%3</span>.")
                  .arg(path)
                  .arg(guessedGame->gameName())
                  .arg(selectedGame->gameName()))
          .button({QObject::tr("Manage %1 instead").arg(guessedGame->gameName()),
                   QMessageBox::Ok})
          .button({QObject::tr("Use this folder for %1").arg(selectedGame->gameName()),
                   QObject::tr("I know what I'm doing"), QMessageBox::Ignore})
          .button({QObject::tr("Cancel"), QMessageBox::Cancel})
          .exec();

  switch (r) {
  case QMessageBox::Ok:
    return guessedGame;

  case QMessageBox::Ignore:
    return selectedGame;

  case QMessageBox::Cancel:
  default:
    return nullptr;
  }
}

VariantsPage::VariantsPage(CreateInstanceDialog& dlg)
    : Page(dlg), m_previousGame(nullptr)
{}

bool VariantsPage::ready() const
{
  // note that this isn't called when doSkip() is true, which happens when
  // the game has no variants

  return !m_selection.isEmpty();
}

bool VariantsPage::doSkip() const
{
  auto* g = m_dlg.rawCreationInfo().game;
  if (!g) {
    // shouldn't happen
    return true;
  }

  return (g->gameVariants().size() < 2);
}

void VariantsPage::doActivated(bool)
{
  auto* g = m_dlg.rawCreationInfo().game;

  if (m_previousGame != g) {
    // recreate the list, the game has changed
    m_previousGame = g;
    m_selection    = "";
    fillList();
  }
}

void VariantsPage::select(const QString& variant)
{
  m_selection = variant;

  // find the button, set it checked
  for (auto* b : m_buttons) {
    if (b->text() == variant) {
      b->setChecked(true);
    } else {
      b->setChecked(false);
    }
  }

  updateNavigation();

  if (!m_selection.isEmpty()) {
    // automatically move to the next page when a variant is selected
    next();
  }
}

QString VariantsPage::selectedGameVariant(MOBase::IPluginGame* game) const
{
  if (!game) {
    return {};
  }

  if (game->gameVariants().size() < 2) {
    return {};
  } else {
    return m_selection;
  }
}

void VariantsPage::fillList()
{
  ui->editions->clear();
  m_buttons.clear();

  auto* g = m_dlg.rawCreationInfo().game;
  if (!g) {
    // shouldn't happen
    return;
  }

  // for each variant, create a checkable button and add it
  for (auto& v : g->gameVariants()) {
    auto* b = new QCommandLinkButton(v);
    b->setCheckable(true);

    QObject::connect(b, &QAbstractButton::clicked, [v, this] {
      select(v);
    });

    ui->editions->addButton(b, QDialogButtonBox::AcceptRole);
    m_buttons.push_back(b);
  }

  if (!m_buttons.empty()) {
    m_buttons[0]->setDefault(true);
  }
}

NamePage::NamePage(CreateInstanceDialog& dlg)
    : Page(dlg), m_modified(false), m_okay(false), m_label(ui->instanceNameLabel),
      m_exists(ui->instanceNameExists), m_invalid(ui->instanceNameInvalid)
{
  QObject::connect(ui->instanceName, &QLineEdit::textEdited, [&] {
    onChanged();
  });

  QObject::connect(ui->instanceName, &QLineEdit::returnPressed, [&] {
    next();
  });
}

bool NamePage::ready() const
{
  // checked when textboxes change or when the page is activated
  return m_okay;
}

bool NamePage::doSkip() const
{
  // portable instances have no name
  return (m_dlg.rawCreationInfo().type == CreateInstanceDialog::Portable);
}

void NamePage::doActivated(bool)
{
  auto* g = m_dlg.rawCreationInfo().game;
  if (!g) {
    // shouldn't happen, next should be disabled
    return;
  }

  m_label.setText(g->gameName());

  // generate a name if the user hasn't changed the text in case the game
  // changed, or if it's empty
  if (!m_modified || ui->instanceName->text().isEmpty()) {
    const auto n = InstanceManager::singleton().makeUniqueName(g->gameName());
    ui->instanceName->setText(n);
    m_modified = false;
  }

  verify();
}

QString NamePage::selectedInstanceName() const
{
  if (!m_okay) {
    return {};
  }

  const auto text = ui->instanceName->text().trimmed();
  return MOBase::sanitizeFileName(text);
}

void NamePage::onChanged()
{
  m_modified = true;
  verify();
}

void NamePage::verify()
{
  const auto root = InstanceManager::singleton().globalInstancesRootPath();
  m_okay          = checkName(root, ui->instanceName->text());
  updateNavigation();
}

bool NamePage::checkName(QString parentDir, QString name)
{
  bool exists  = false;
  bool invalid = false;
  bool empty   = false;

  name = name.trimmed();

  if (name.isEmpty()) {
    empty = true;
  } else {
    if (MOBase::validFileName(name)) {
      exists = QDir(parentDir).exists(name);
    } else {
      invalid = true;
    }
  }

  bool okay = false;

  if (exists) {
    m_exists.setVisible(true);
    m_exists.setText(QDir(parentDir).filePath(name));
    m_invalid.setVisible(false);
  } else if (invalid) {
    m_exists.setVisible(false);
    m_invalid.setVisible(true);
    m_invalid.setText(name);
  } else {
    okay = !empty;
    m_exists.setVisible(false);
    m_invalid.setVisible(false);
  }

  return okay;
}

ProfilePage::ProfilePage(CreateInstanceDialog& dlg) : Page(dlg) {}

bool ProfilePage::ready() const
{
  return true;
}

CreateInstanceDialog::ProfileSettings ProfilePage::profileSettings() const
{
  CreateInstanceDialog::ProfileSettings profileSettings;
  profileSettings.localInis           = ui->profileInisCheckbox->isChecked();
  profileSettings.localSaves          = ui->profileSavesCheckbox->isChecked();
  profileSettings.archiveInvalidation = ui->archiveInvalidationCheckbox->isChecked();
  return profileSettings;
}

PathsPage::PathsPage(CreateInstanceDialog& dlg)
    : Page(dlg), m_lastType(CreateInstanceDialog::NoType), m_label(ui->pathsLabel),
      m_simpleExists(ui->locationExists), m_simpleInvalid(ui->locationInvalid),
      m_advancedExists(ui->advancedDirExists),
      m_advancedInvalid(ui->advancedDirInvalid), m_okay(false)
{
  auto setEdit = [&](QLineEdit* e) {
    QObject::connect(e, &QLineEdit::textEdited, [&] {
      onChanged();
    });
    QObject::connect(e, &QLineEdit::returnPressed, [&] {
      next();
    });
  };

  auto setBrowse = [&](QAbstractButton* b, QLineEdit* e) {
    QObject::connect(b, &QAbstractButton::clicked, [this, e] {
      browse(e);
    });
  };

  setEdit(ui->location);
  setEdit(ui->base);
  setEdit(ui->downloads);
  setEdit(ui->mods);
  setEdit(ui->profiles);
  setEdit(ui->overwrite);

  setBrowse(ui->browseLocation, ui->location);
  setBrowse(ui->browseBase, ui->base);
  setBrowse(ui->browseDownloads, ui->downloads);
  setBrowse(ui->browseMods, ui->mods);
  setBrowse(ui->browseProfiles, ui->profiles);
  setBrowse(ui->browseOverwrite, ui->overwrite);

  QObject::connect(ui->advancedPathOptions, &QCheckBox::clicked, [&] {
    onAdvanced();
  });

  ui->pathPages->setCurrentIndex(0);
}

bool PathsPage::ready() const
{
  // set when the page is activated, textboxes are changed or the advanced
  // checkbox is toggled
  return m_okay;
}

void PathsPage::doActivated(bool firstTime)
{
  const auto name = m_dlg.rawCreationInfo().instanceName;
  const auto type = m_dlg.rawCreationInfo().type;

  // if the instance name or type have changed, all the paths must be
  // regenerated
  const bool changed = (m_lastInstanceName != name) || (m_lastType != type);

  // generating and paths
  setPaths(name, changed);
  checkPaths();

  updateNavigation();

  m_label.setText(m_dlg.rawCreationInfo().game->gameName());
  m_lastInstanceName = name;
  m_lastType         = type;

  if (firstTime) {
    ui->location->setFocus();
  }
}

CreateInstanceDialog::Paths PathsPage::selectedPaths() const
{
  CreateInstanceDialog::Paths p;

  if (ui->advancedPathOptions->isChecked()) {
    p.base      = ui->base->text();
    p.downloads = ui->downloads->text();
    p.mods      = ui->mods->text();
    p.profiles  = ui->profiles->text();
    p.overwrite = ui->overwrite->text();
  } else {
    p.base = ui->location->text();
  }

  return p;
}

void PathsPage::onChanged()
{
  checkPaths();
  updateNavigation();
}

void PathsPage::browse(QLineEdit* e)
{
  const auto s = QFileDialog::getExistingDirectory(&m_dlg, {}, e->text());
  if (s.isNull() || s.isEmpty()) {
    return;
  }

  e->setText(QDir::toNativeSeparators(s));
}

void PathsPage::checkPaths()
{
  if (ui->advancedPathOptions->isChecked()) {
    // checking advanced paths
    m_okay = checkAdvancedPath(ui->base->text()) &&
             checkAdvancedPath(resolve(ui->downloads->text())) &&
             checkAdvancedPath(resolve(ui->mods->text())) &&
             checkAdvancedPath(resolve(ui->profiles->text())) &&
             checkAdvancedPath(resolve(ui->overwrite->text()));
  } else {
    // checking simple path
    m_okay = checkSimplePath(ui->location->text());
  }
}

bool PathsPage::checkSimplePath(const QString& path)
{
  return checkPath(path, m_simpleExists, m_simpleInvalid);
}

bool PathsPage::checkAdvancedPath(const QString& path)
{
  return checkPath(path, m_advancedExists, m_advancedInvalid);
}

QString PathsPage::resolve(const QString& path) const
{
  return PathSettings::resolve(path, ui->base->text());
}

void PathsPage::onAdvanced()
{
  // the base/location textboxes are different widgets but they represent the
  // same base path value, so they're synced between pages

  if (ui->advancedPathOptions->isChecked()) {
    ui->base->setText(ui->location->text());
    ui->pathPages->setCurrentIndex(1);
  } else {
    ui->location->setText(ui->base->text());
    ui->pathPages->setCurrentIndex(0);
  }

  checkPaths();
}

void PathsPage::setPaths(const QString& name, bool force)
{
  QString basePath;

  if (m_dlg.rawCreationInfo().type == CreateInstanceDialog::Portable) {
    basePath = InstanceManager::singleton().portablePath();
  } else {
    const auto root = InstanceManager::singleton().globalInstancesRootPath();
    basePath        = root + "/" + name;
  }

  basePath = QDir::toNativeSeparators(QDir::cleanPath(basePath));

  // all paths are set regardless of advanced checkbox

  setIfEmpty(ui->location, basePath, force);
  setIfEmpty(ui->base, basePath, force);

  setIfEmpty(ui->downloads, makeDefaultPath(AppConfig::downloadPath()), force);
  setIfEmpty(ui->mods, makeDefaultPath(AppConfig::modsPath()), force);
  setIfEmpty(ui->profiles, makeDefaultPath(AppConfig::profilesPath()), force);
  setIfEmpty(ui->overwrite, makeDefaultPath(AppConfig::overwritePath()), force);
}

void PathsPage::setIfEmpty(QLineEdit* e, const QString& path, bool force)
{
  if (e->text().isEmpty() || force) {
    e->setText(path);
  }
}

bool PathsPage::checkPath(QString path, PlaceholderLabel& existsLabel,
                          PlaceholderLabel& invalidLabel)
{
  auto& m = InstanceManager::singleton();

  bool exists  = false;
  bool invalid = false;
  bool empty   = false;

  path = QDir::toNativeSeparators(path.trimmed());

  if (path.isEmpty()) {
    empty = true;
  } else {
    const QDir d(path);

    if (MOBase::validFileName(d.dirName())) {
      if (m_dlg.rawCreationInfo().type == CreateInstanceDialog::Portable) {
        // the default data path for a portable instance is the application
        // directory, so it's not an error if it exists
        if (QDir(path) != m.portablePath()) {
          exists = QDir(path).exists();
        }
      } else {
        exists = QDir(path).exists();
      }
    } else {
      invalid = true;
    }
  }

  bool okay = true;

  if (invalid) {
    okay = false;
    existsLabel.setVisible(false);
    invalidLabel.setVisible(true);
    invalidLabel.setText(path);
  } else if (empty) {
    okay = false;
    existsLabel.setVisible(false);
    invalidLabel.setVisible(false);
  } else if (exists) {
    // this is just a warning
    existsLabel.setVisible(true);
    existsLabel.setText(path);
    invalidLabel.setVisible(false);
  } else {
    okay = true;
    existsLabel.setVisible(false);
    invalidLabel.setVisible(false);
  }

  return okay;
}

NexusPage::NexusPage(CreateInstanceDialog& dlg) : Page(dlg), m_skip(false)
{
  m_connectionUI.reset(new NexusConnectionUI(&m_dlg, dlg.settings(), ui->nexusConnect,
                                             nullptr, ui->nexusManual, ui->nexusLog));

  // just check it once, or connecting and then going back and forth would skip
  // the page, which would be unexpected
  m_skip = GlobalSettings::hasNexusApiKey();
}

NexusPage::~NexusPage() = default;

bool NexusPage::ready() const
{
  // this page is optional
  return true;
}

bool NexusPage::doSkip() const
{
  return m_skip;
}

ConfirmationPage::ConfirmationPage(CreateInstanceDialog& dlg) : Page(dlg) {}

void ConfirmationPage::doActivated(bool)
{
  ui->review->setPlainText(makeReview());
  ui->creationLog->clear();
}

QString ConfirmationPage::makeReview() const
{
  QStringList lines;

  const auto ci = m_dlg.rawCreationInfo();

  lines.push_back(QObject::tr("Instance type: %1").arg(toLocalizedString(ci.type)));

  lines.push_back(QObject::tr("Instance location: %1").arg(ci.dataPath));

  if (ci.type != CreateInstanceDialog::Portable) {
    lines.push_back(QObject::tr("Instance name: %1").arg(ci.instanceName));
  }

  lines.push_back(QObject::tr("Profile settings:"));
  lines.push_back(
      QObject::tr("  Local INIs: %1")
          .arg(ci.profileSettings.localInis ? QObject::tr("yes") : QObject::tr("no")));
  lines.push_back(
      QObject::tr("  Local Saves: %1")
          .arg(ci.profileSettings.localSaves ? QObject::tr("yes") : QObject::tr("no")));
  lines.push_back(QObject::tr("  Automatic Archive Invalidation: %1")
                      .arg(ci.profileSettings.archiveInvalidation ? QObject::tr("yes")
                                                                  : QObject::tr("no")));

  if (ci.paths.downloads.isEmpty()) {
    // simple settings
    if (ci.paths.base != ci.dataPath) {
      lines.push_back(QObject::tr("Base directory: %1").arg(ci.paths.base));
    }
  } else {
    // advanced settings
    lines.push_back(QObject::tr("Base directory: %1").arg(ci.paths.base));
    lines.push_back(dirLine(QObject::tr("Downloads"), ci.paths.downloads));
    lines.push_back(dirLine(QObject::tr("Mods"), ci.paths.mods));
    lines.push_back(dirLine(QObject::tr("Profiles"), ci.paths.profiles));
    lines.push_back(dirLine(QObject::tr("Overwrite"), ci.paths.overwrite));
  }

  // game
  QString name = ci.game->gameName();
  if (!ci.gameVariant.isEmpty()) {
    name += " (" + ci.gameVariant + ")";
  }

  lines.push_back(QObject::tr("Game: %1").arg(name));
  lines.push_back(QObject::tr("Game location: %1").arg(ci.gameLocation));

  return lines.join("\n");
}

QString ConfirmationPage::dirLine(const QString& caption, const QString& path) const
{
  return QString("  - %1: %2").arg(caption).arg(path);
}

}  // namespace cid
