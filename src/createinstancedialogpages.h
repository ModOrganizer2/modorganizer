#ifndef MODORGANIZER_CREATEINSTANCEDIALOGPAGES_INCLUDED
#define MODORGANIZER_CREATEINSTANCEDIALOGPAGES_INCLUDED

#include <QLabel>
#include <QLineEdit>
#include <QCommandLinkButton>
#include "createinstancedialog.h"

namespace MOBase { class IPluginGame; }

namespace cid
{

QString makeDefaultPath(const std::wstring& dir);


class PlaceholderLabel
{
public:
  PlaceholderLabel(QLabel* label);
  void setText(const QString& arg);
  void setVisible(bool b);

private:
  QLabel* m_label;
  QString m_original;
};


class Page
{
public:
  Page(CreateInstanceDialog& dlg);

  virtual bool ready() const;
  virtual bool skip() const;
  virtual void activated();

  void updateNavigation();
  void next();

  virtual CreateInstanceDialog::Types selectedInstanceType() const;
  virtual MOBase::IPluginGame* selectedGame() const;
  virtual QString selectedGameLocation() const;
  virtual QString selectedGameEdition() const;
  virtual QString selectedInstanceName() const;
  virtual CreateInstanceDialog::Paths selectedPaths() const;

protected:
  Ui::CreateInstanceDialog* ui;
  CreateInstanceDialog& m_dlg;
  const PluginContainer& m_pc;
};


class InfoPage : public Page
{
public:
  InfoPage(CreateInstanceDialog& dlg);
};


class TypePage : public Page
{
public:
  TypePage(CreateInstanceDialog& dlg);

  bool ready() const override;
  CreateInstanceDialog::Types selectedInstanceType() const override;

  void global();
  void portable();

private:
  CreateInstanceDialog::Types m_type;
};


class GamePage : public Page
{
public:
  GamePage(CreateInstanceDialog& dlg);

  bool ready() const override;
  MOBase::IPluginGame* selectedGame() const override;
  QString selectedGameLocation() const override;

  void select(MOBase::IPluginGame* game);
  void selectCustom();

  void warnUnrecognized(const QString& path);

private:
  struct Game
  {
    MOBase::IPluginGame* game = nullptr;
    QCommandLinkButton* button = nullptr;
    QString dir;
    bool installed = false;

    Game(MOBase::IPluginGame* g);
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
  };

  std::vector<std::unique_ptr<Game>> m_games;
  Game* m_selection;


  std::vector<MOBase::IPluginGame*> sortedGamePlugins() const;
  Game* findGame(MOBase::IPluginGame* game);
  void createGames();
  void updateButton(Game* g);
  void selectButton(Game* g);
  QCommandLinkButton* createCustomButton();
  void createGameButton(Game* g);
  void fillList();
  Game* checkInstallation(const QString& path, Game* g);
  MOBase::IPluginGame* findAnotherGame(const QString& path);
  bool confirmUnknown(const QString& path, MOBase::IPluginGame* game);
  MOBase::IPluginGame* confirmOtherGame(
    const QString& path,
    MOBase::IPluginGame* selectedGame, MOBase::IPluginGame* guessedGame);
};


class EditionsPage : public Page
{
public:
  EditionsPage(CreateInstanceDialog& dlg);

  bool ready() const override;
  bool skip() const override;
  void activated() override;
  QString selectedGameEdition() const override;

  void select(const QString& variant);

private:
  MOBase::IPluginGame* m_previousGame;
  std::vector<QCommandLinkButton*> m_buttons;
  QString m_selection;

  void fillList();
};


class NamePage : public Page
{
public:
  NamePage(CreateInstanceDialog& dlg);

  bool ready() const override;
  bool skip() const override;
  void activated() override;
  QString selectedInstanceName() const override;

private:
  mutable PlaceholderLabel m_label, m_exists, m_invalid;
  bool m_modified;
  bool m_okay;

  void onChanged();
  void updateWarnings();
  bool checkName(QString parentDir, QString name);
};


class PathsPage : public Page
{
public:
  PathsPage(CreateInstanceDialog& dlg);

  bool ready() const override;
  void activated() override;

  CreateInstanceDialog::Paths selectedPaths() const override;

private:
  QString m_lastInstanceName;
  CreateInstanceDialog::Types m_lastType;
  PlaceholderLabel m_label;
  mutable PlaceholderLabel m_simpleExists, m_simpleInvalid;
  mutable PlaceholderLabel m_advancedExists, m_advancedInvalid;

  void onChanged();
  bool checkPaths() const;
  bool checkAdvancedPath(const QString& path) const;
  QString resolve(const QString& path) const;
  void onAdvanced();
  void setPaths(const QString& name, bool force);
  void setIfEmpty(QLineEdit* e, const QString& path, bool force);
  bool checkPath(
    QString path,
    PlaceholderLabel& existsLabel, PlaceholderLabel& invalidLabel) const;
};


class ConfirmationPage : public Page
{
public:
  ConfirmationPage(CreateInstanceDialog& dlg);

  void activated() override;

  QString toLocalizedString(CreateInstanceDialog::Types t) const;
  QString makeReview() const;
  QString dirLine(const QString& caption, const QString& path) const;
};

} // namespace

#endif // MODORGANIZER_CREATEINSTANCEDIALOGPAGES_INCLUDED
