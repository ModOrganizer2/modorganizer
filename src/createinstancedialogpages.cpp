#include "createinstancedialogpages.h"
#include "ui_createinstancedialog.h"
#include "instancemanager.h"
#include "settings.h"
#include "plugincontainer.h"
#include "settingsdialognexus.h"
#include "shared/appconfig.h"
#include <iplugingame.h>
#include <report.h>
#include <utility.h>

namespace cid
{

using namespace MOBase;
using MOBase::TaskDialog;

QString makeDefaultPath(const std::wstring& dir)
{
  return QDir::toNativeSeparators(PathSettings::makeDefaultPath(
    QString::fromStdWString(dir)));
}



PlaceholderLabel::PlaceholderLabel(QLabel* label)
  : m_label(label), m_original(label->text())
{
}

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
  : ui(dlg.getUI()), m_dlg(dlg), m_pc(dlg.pluginContainer()), m_skip(false)
{
}

bool Page::ready() const
{
  return true;
}

bool Page::skip() const
{
  return m_skip || doSkip();
}

bool Page::doSkip() const
{
  return false;
}

void Page::activated()
{
  // no-op
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


IntroPage::IntroPage(CreateInstanceDialog& dlg)
  : Page(dlg)
{
}

bool IntroPage::doSkip() const
{
  return GlobalSettings::hideCreateInstanceIntro();
}


TypePage::TypePage(CreateInstanceDialog& dlg)
  : Page(dlg), m_type(CreateInstanceDialog::NoType)
{
  ui->createGlobal->setDescription(
    ui->createGlobal->description()
      .arg(InstanceManager::singleton().globalInstancesRootPath()));

  ui->createPortable->setDescription(
    ui->createPortable->description()
      .arg(InstanceManager::singleton().portablePath()));

  if (InstanceManager::singleton().portableInstanceExists()) {
    ui->createPortable->setEnabled(false);
    ui->portableExistsLabel->setVisible(true);
  } else {
    ui->createPortable->setEnabled(true);
    ui->portableExistsLabel->setVisible(false);
  }

  QObject::connect(
    ui->createGlobal, &QAbstractButton::clicked, [&]{ global(); });

  QObject::connect(
    ui->createPortable, &QAbstractButton::clicked, [&]{ portable(); });
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


GamePage::Game::Game(IPluginGame* g)
  : game(g), installed(g->isInstalled())
{
  if (installed) {
    dir = game->gameDirectory().path();
  }
}


GamePage::GamePage(CreateInstanceDialog& dlg)
  : Page(dlg), m_selection(nullptr)
{
  createGames();
  fillList();

  m_filter.setEdit(ui->gamesFilter);

  QObject::connect(&m_filter, &FilterWidget::changed, [&]{ fillList(); });
  QObject::connect(ui->showAllGames, &QCheckBox::clicked, [&]{ fillList(); });
}

bool GamePage::ready() const
{
  return (m_selection != nullptr);
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
    if (!checked->installed) {
      if (dir.isEmpty()) {
        const auto path = QFileDialog::getExistingDirectory(
          &m_dlg, QObject::tr("Find game installation"));

        if (path.isEmpty()) {
          checked = nullptr;
        } else {
          checked = checkInstallation(path, checked);

          if (checked) {
            // remember this path
            checked->dir = path;
            checked->installed = true;
          }
        }
      } else {
       checked->dir = dir;
       checked->installed = true;
      }
    }
  }

  m_selection = checked;

  selectButton(checked);
  updateButton(checked);
  updateNavigation();

  if (checked) {
    next();
  }
}

void GamePage::selectCustom()
{
  const auto path = QFileDialog::getExistingDirectory(
    &m_dlg, QObject::tr("Find game installation"));

  if (path.isEmpty()) {
    // reselect the previous button
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

void GamePage::warnUnrecognized(const QString& path)
{
  QString supportedGames;
  for (auto* game : sortedGamePlugins()) {
    supportedGames += game->gameName() + "\n";
  }

  QMessageBox dlg(&m_dlg);

  dlg.setWindowTitle(QObject::tr("Unrecognized game"));
  dlg.setText(QObject::tr(
    "The folder %1 does not seem to contain a game Mod Organizer can "
    "manage.").arg(path));
  dlg.setInformativeText(
    QObject::tr("See details for the list of supported games."));
  dlg.setDetailedText(supportedGames);
  dlg.setIcon(QMessageBox::Warning);
  dlg.setStandardButtons(QMessageBox::Ok);

  dlg.exec();
}

std::vector<IPluginGame*> GamePage::sortedGamePlugins() const
{
  std::vector<IPluginGame*> v;

  for (auto* game : m_pc.plugins<IPluginGame>()) {
    v.push_back(game);
  }

  std::sort(v.begin(), v.end(), [](auto* a, auto* b) {
    return (naturalCompare(a->gameName(), b->gameName()) < 0);
  });

  return v;
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

void GamePage::createGames()
{
  m_games.clear();

  for (auto* game : sortedGamePlugins()) {
    m_games.push_back(std::make_unique<Game>(game));
  }
}

void GamePage::updateButton(Game* g)
{
  if (!g || !g->button) {
    return;
  }

  g->button->setText(g->game->gameName());
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

QCommandLinkButton* GamePage::createCustomButton()
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

void GamePage::createGameButton(Game* g)
{
  g->button = new QCommandLinkButton;
  g->button->setCheckable(true);

  updateButton(g);

  QObject::connect(g->button, &QAbstractButton::clicked, [g, this] {
    select(g->game);
  });
}

void GamePage::fillList()
{
  const bool showAll = ui->showAllGames->isChecked();

  clearButtons();

  for (auto& g : m_games) {
    g->button = nullptr;

    if (!showAll && !g->installed) {
      // not installed
      continue;
    }

    if (!m_filter.matches(g->game->gameName())) {
      continue;
    }

    createGameButton(g.get());
    addButton(g->button);
  }

  addButton(createCustomButton());
}

void GamePage::clearButtons()
{
  auto* ly = static_cast<QVBoxLayout*>(ui->games->layout());

  ui->games->setUpdatesEnabled(false);

  // delete all children
  qDeleteAll(ui->games->findChildren<QWidget*>("", Qt::FindDirectChildrenOnly));

  // stretch widgets added with addStretch() are not in the parent widget,
  // they have to be deleted from the layout itself
  while (auto* child=ly->takeAt(0))
    delete child;

  // add a stretch, buttons will be added before
  ly->addStretch();

  ui->games->setUpdatesEnabled(true);
}

void GamePage::addButton(QAbstractButton* b)
{
  auto* ly = static_cast<QVBoxLayout*>(ui->games->layout());

  // insert before the stretch
  ly->insertWidget(ly->count() - 1, b);
}

GamePage::Game* GamePage::checkInstallation(const QString& path, Game* g)
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

IPluginGame* GamePage::findAnotherGame(const QString& path)
{
  for (auto* otherGame : m_pc.plugins<IPluginGame>()) {
    if (otherGame->looksValid(path)) {
      return otherGame;
    }
  }

  return nullptr;
}

bool GamePage::confirmUnknown(const QString& path, IPluginGame* game)
{
  const auto r = TaskDialog(&m_dlg)
    .title(QObject::tr("Unrecognized game"))
    .main(QObject::tr("Unrecognized game"))
    .content(QObject::tr(
      "The folder %1 does not seem to contain an installation for "
      "<span style=\"white-space: nowrap; font-weight: bold;\">%2</span> or "
      "for any other game Mod Organizer can manage.")
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

IPluginGame* GamePage::confirmOtherGame(
  const QString& path,
  IPluginGame* selectedGame, IPluginGame* guessedGame)
{
  const auto r = TaskDialog(&m_dlg)
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


VariantsPage::VariantsPage(CreateInstanceDialog& dlg)
  : Page(dlg), m_previousGame(nullptr)
{
}

bool VariantsPage::ready() const
{
  return !m_selection.isEmpty();
}

bool VariantsPage::doSkip() const
{
  auto* g = m_dlg.rawCreationInfo().game;
  if (!g) {
    // shouldn't happen
    return true;
  }

  const auto variants = g->gameVariants();
  return (variants.size() < 2);
}

void VariantsPage::activated()
{
  auto* g = m_dlg.rawCreationInfo().game;

  if (m_previousGame != g) {
    m_previousGame = g;
    m_selection = "";
    fillList();
  }
}

void VariantsPage::select(const QString& variant)
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

  if (!m_selection.isEmpty()) {
    next();
  }
}

QString VariantsPage::selectedGameVariant(MOBase::IPluginGame* game) const
{
  if (!game) {
    return {};
  }

  const auto variants = game->gameVariants();
  if (variants.size() < 2) {
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


NamePage::NamePage(CreateInstanceDialog& dlg) :
  Page(dlg), m_modified(false), m_okay(false),
  m_label(ui->instanceNameLabel), m_exists(ui->instanceNameExists),
  m_invalid(ui->instanceNameInvalid)
{
  QObject::connect(
    ui->instanceName, &QLineEdit::textEdited, [&]{ onChanged(); });
}

bool NamePage::ready() const
{
  return m_okay;
}

bool NamePage::doSkip() const
{
  return (m_dlg.rawCreationInfo().type == CreateInstanceDialog::Portable);
}

void NamePage::activated()
{
  auto* g = m_dlg.rawCreationInfo().game;
  if (!g) {
    // shouldn't happen, next should be disabled
    return;
  }

  m_label.setText(g->gameName());

  if (!m_modified || ui->instanceName->text().isEmpty()) {
    const auto n = InstanceManager::singleton().makeUniqueName(g->gameName());
    ui->instanceName->setText(n);
    m_modified = false;
  }

  updateWarnings();
}

QString NamePage::selectedInstanceName() const
{
  if (!m_okay) {
    return {};
  }

  const auto text = ui->instanceName->text().trimmed();
  return InstanceManager::singleton().sanitizeInstanceName(text);
}

void NamePage::onChanged()
{
  m_modified = true;
  updateWarnings();
}

void NamePage::updateWarnings()
{
  const auto root = InstanceManager::singleton().globalInstancesRootPath();

  m_okay = checkName(root, ui->instanceName->text());
  updateNavigation();
}

bool NamePage::checkName(QString parentDir, QString name)
{
  bool exists = false;
  bool invalid = false;
  bool empty = false;

  name = name.trimmed();

  if (name.isEmpty()) {
    empty = true;
  } else {
    if (InstanceManager::singleton().validInstanceName(name)) {
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


PathsPage::PathsPage(CreateInstanceDialog& dlg) :
  Page(dlg), m_lastType(CreateInstanceDialog::NoType),
  m_label(ui->pathsLabel),
  m_simpleExists(ui->locationExists), m_simpleInvalid(ui->locationInvalid),
  m_advancedExists(ui->advancedDirExists),
  m_advancedInvalid(ui->advancedDirInvalid)
{
  QObject::connect(ui->location, &QLineEdit::textEdited, [&]{ onChanged(); });
  QObject::connect(ui->base, &QLineEdit::textEdited, [&]{ onChanged(); });
  QObject::connect(ui->downloads, &QLineEdit::textEdited, [&]{ onChanged(); });
  QObject::connect(ui->mods, &QLineEdit::textEdited, [&]{ onChanged(); });
  QObject::connect(ui->profiles, &QLineEdit::textEdited, [&]{ onChanged(); });
  QObject::connect(ui->overwrite, &QLineEdit::textEdited, [&]{ onChanged(); });

  QObject::connect(
    ui->advancedPathOptions, &QCheckBox::clicked, [&]{ onAdvanced(); });

  ui->pathPages->setCurrentIndex(0);
}


bool PathsPage::ready() const
{
  return checkPaths();
}

void PathsPage::activated()
{
  const auto name = m_dlg.rawCreationInfo().instanceName;
  const auto type = m_dlg.rawCreationInfo().type;

  const bool changed = (m_lastInstanceName != name) || (m_lastType != type);

  setPaths(name, changed);
  checkPaths();
  updateNavigation();

  m_label.setText(m_dlg.rawCreationInfo().game->gameName());
  m_lastInstanceName = name;
  m_lastType = type;
}

CreateInstanceDialog::Paths PathsPage::selectedPaths() const
{
  CreateInstanceDialog::Paths p;

  if (ui->advancedPathOptions->isChecked()) {
    p.base = ui->base->text();
    p.downloads = ui->downloads->text();
    p.mods = ui->mods->text();
    p.profiles = ui->profiles->text();
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

bool PathsPage::checkPaths() const
{
  if (ui->advancedPathOptions->isChecked()) {
    return
      checkAdvancedPath(ui->base->text()) &&
      checkAdvancedPath(resolve(ui->downloads->text())) &&
      checkAdvancedPath(resolve(ui->mods->text())) &&
      checkAdvancedPath(resolve(ui->profiles->text())) &&
      checkAdvancedPath(resolve(ui->overwrite->text()));
  } else {
    return checkPath(ui->location->text(), m_simpleExists, m_simpleInvalid);
  }
}

bool PathsPage::checkAdvancedPath(const QString& path) const
{
  return checkPath(path, m_advancedExists, m_advancedInvalid);
}

QString PathsPage::resolve(const QString& path) const
{
  return PathSettings::resolve(path, ui->base->text());
}

void PathsPage::onAdvanced()
{
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
  QString path;

  if (m_dlg.rawCreationInfo().type == CreateInstanceDialog::Portable) {
    path = InstanceManager::singleton().portablePath();
  } else {
    const auto root = InstanceManager::singleton().globalInstancesRootPath();
    path = root + "/" + name;
  }

  path = QDir::toNativeSeparators(QDir::cleanPath(path));

  setIfEmpty(ui->location, path, force);
  setIfEmpty(ui->base, path, force);

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

bool PathsPage::checkPath(
  QString path,
  PlaceholderLabel& existsLabel, PlaceholderLabel& invalidLabel) const
{
  bool exists = false;
  bool invalid = false;
  bool empty = false;

  path = QDir::toNativeSeparators(path.trimmed());

  if (path.isEmpty()) {
    empty = true;
  } else {
    const QDir d(path);

    if (InstanceManager::singleton().validInstanceName(d.dirName())) {
      if (m_dlg.rawCreationInfo().type == CreateInstanceDialog::Portable) {
        // the default data path for a portable instance is the application
        // directory, so it's not an error if it exists
        if (QDir(path) != InstanceManager::singleton().portablePath()) {
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


NexusPage::NexusPage(CreateInstanceDialog& dlg)
  : Page(dlg), m_skip(false)
{
  m_connectionUI.reset(new NexusConnectionUI(
    &m_dlg,
    dlg.settings(),
    ui->nexusConnect,
    nullptr,
    ui->nexusManual,
    ui->nexusLog));

  // just check it once, or connecting and then going back and forth would skip
  // the page, which would be unexpected
  m_skip = GlobalSettings::hasNexusApiKey();
}

NexusPage::~NexusPage() = default;

bool NexusPage::ready() const
{
  return true;
}

bool NexusPage::doSkip() const
{
  return m_skip;
}

void NexusPage::activated()
{
}


ConfirmationPage::ConfirmationPage(CreateInstanceDialog& dlg)
  : Page(dlg)
{
}

void ConfirmationPage::activated()
{
  ui->review->setPlainText(makeReview());
  ui->creationLog->clear();
}

QString ConfirmationPage::toLocalizedString(CreateInstanceDialog::Types t) const
{
  switch (t)
  {
    case CreateInstanceDialog::Global:
      return QObject::tr("Global");

    case CreateInstanceDialog::Portable:
      return QObject::tr("Portable");

    default:
      return QObject::tr("Instance type: %1").arg(QObject::tr("?"));
  }
}

QString ConfirmationPage::makeReview() const
{
  QStringList lines;

  const auto ci = m_dlg.rawCreationInfo();

  lines.push_back(QObject::tr("Instance type: %1")
    .arg(toLocalizedString(ci.type)));

  lines.push_back(QObject::tr("Instance location: %1")
    .arg(ci.dataPath));

  if (ci.type != CreateInstanceDialog::Portable) {
    lines.push_back(QObject::tr("Instance name: %1").arg(ci.instanceName));
  }

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

} // namespace
