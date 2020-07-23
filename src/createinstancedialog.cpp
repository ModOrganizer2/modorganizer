#include "createinstancedialog.h"
#include "ui_createinstancedialog.h"
#include "instancemanager.h"

namespace cid
{

class Page
{
public:
  Page(CreateInstanceDialog& dlg, std::size_t i)
    : ui(dlg.getUI()), m_dlg(dlg), m_index(i)
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

private:
  CreateInstanceDialog& m_dlg;
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


CreateInstanceDialog::CreateInstanceDialog(QWidget *parent)
  : QDialog(parent), ui(new Ui::CreateInstanceDialog)
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

  //
  //SelectionDialog games(tr("Select a game to manage."));
  //
  //for (auto* game : m_pc.plugins<MOBase::IPluginGame>()) {
  //  if (game->isInstalled()) {
  //    games.addChoice(game->gameName(), game->gameDirectory().path(), {});
  //  } else {
  //    games.addChoice(game->gameName(), "", {});
  //  }
  //}
  //
  //games.exec();
}

CreateInstanceDialog::~CreateInstanceDialog() = default;

Ui::CreateInstanceDialog* CreateInstanceDialog::getUI()
{
  return ui.get();
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
