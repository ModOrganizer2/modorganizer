#ifndef MODORGANIZER_CREATEINSTANCEDIALOGPAGES_INCLUDED
#define MODORGANIZER_CREATEINSTANCEDIALOGPAGES_INCLUDED

#include "createinstancedialog.h"
#include <filterwidget.h>

#include <QCommandLinkButton>
#include <QLabel>
#include <QLineEdit>

namespace MOBase
{
class IPluginGame;
}
class NexusConnectionUI;

namespace cid
{

// returns "%base_dir%/dir"
//
QString makeDefaultPath(const std::wstring& dir);

// remembers the original text of the given label and, if it contains a %1,
// sets it in setText()
//
class PlaceholderLabel
{
public:
  PlaceholderLabel(QLabel* label);

  // if the original label text contained a %1, replaces it by the arg and
  // sets that as the new label text
  //
  void setText(const QString& arg);

  // whether the label is visible
  //
  void setVisible(bool b);

private:
  QLabel* m_label;
  QString m_original;
};

// one page in the wizard
//
// each page can implement one or more selected*() below; those are called
// by CreateInstanceDialog to gather data from all pages
//
class Page
{
public:
  Page(CreateInstanceDialog& dlg);

  // whether this page has been filled and is valid; used by the dialog to
  // determine if it can move to the next page
  //
  virtual bool ready() const;

  // called every time a page is shown in the screen
  //
  void activated();

  // overrides whether this page should be skipped; this is used by
  // CreateInstanceDialog::setSinglePage() to disable all other pages
  //
  void setSkip(bool b);

  // whether this page should be skipped
  //
  bool skip() const;

  // asks the dialog to update its navigation buttons, typically used when a
  // page changes its ready state without moving to a different page
  //
  void updateNavigation();

  // asks the dialog to move to the next page; some pages will automatically
  // advance once the user has made the proper selection
  //
  void next();

  // called from the dialog when an action is requested on the current page;
  // returns true when handled
  //
  virtual bool action(CreateInstanceDialog::Actions a);

  // returns the instance type
  //
  virtual CreateInstanceDialog::Types selectedInstanceType() const;

  // returns the game plugin
  //
  virtual MOBase::IPluginGame* selectedGame() const;

  // returns the game directory
  //
  virtual QString selectedGameLocation() const;

  // returns the game variant
  //
  virtual QString selectedGameVariant(MOBase::IPluginGame* game) const;

  // returns the instance name
  //
  virtual QString selectedInstanceName() const;

  // returns the various paths
  //
  virtual CreateInstanceDialog::Paths selectedPaths() const;

  // returns the profile settings
  //
  virtual CreateInstanceDialog::ProfileSettings profileSettings() const;

protected:
  Ui::CreateInstanceDialog* ui;
  CreateInstanceDialog& m_dlg;
  const PluginManager& m_pc;
  bool m_skip;
  bool m_firstActivation;

  // called every time a page is shown in the screen; `firstTime` is true for
  // first activation
  //
  virtual void doActivated(bool firstTime);

  // implemented by derived classes, overridden by setSkip(true)
  //
  virtual bool doSkip() const;
};

// introduction page, can be disabled by a global setting
//
class IntroPage : public Page
{
public:
  IntroPage(CreateInstanceDialog& dlg);

protected:
  bool doSkip() const override;

private:
  // the setting is only checked once when opening the dialog, or going forwards
  // then back after checking the box wouldn't show the intro page any more,
  // which would be unexpected
  bool m_skip;
};

// instance type page
//
class TypePage : public Page
{
public:
  TypePage(CreateInstanceDialog& dlg);

  // whether a type has been been selected
  //
  bool ready() const override;

  // returns the selected type
  //
  CreateInstanceDialog::Types selectedInstanceType() const override;

  // selects a global instance
  //
  void global();

  // selects a portable instance
  //
  void portable();

protected:
  // focuses global instance button on first activation
  //
  void doActivated(bool firstTime) override;

private:
  CreateInstanceDialog::Types m_type;
};

// game plugin page, displays a list of command buttons for each game, along
// with a "browse" button for custom directories and filtering stuff
//
// the game list initially only shows plugins that report isInstalled(), and the
// user has two ways of specifying paths for games that were not found:
//
//   1) by clicking the "Browse..." button and selecting an arbitrary directory
//
//      all plugins are checked until one returns true for looksValid(); if none
//      of them do, this is an error
//
//   2) by checking the "Show all supported games" checkbox and clicking one
//      of the games on the list
//
//      if the selected plugin doesn't recognize the directory, the user is
//      warned, but is allowed to continue; there's also some logic to try to
//      find another plugin that can manage this directory and suggest it
//      instead
//
class GamePage : public Page
{
public:
  GamePage(CreateInstanceDialog& dlg);

  // whether a game has been selected
  //
  bool ready() const override;

  // handles find
  //
  bool action(CreateInstanceDialog::Actions a) override;

  // returns the selected game
  //
  MOBase::IPluginGame* selectedGame() const override;

  // returns the selected game directory
  QString selectedGameLocation() const override;

  // selects the given game and toggles its associated button; the game
  // directory can be overridden
  //
  // pops up a directory selection dialog if `dir` is empty and the plugin
  // hasn't detected the game
  //
  void select(MOBase::IPluginGame* game, const QString& dir = {});

  // pops up a directory selection dialog and looks for a plugin to manage
  // it
  //
  void selectCustom();

  // pops up a warning dialog that the game at the given path is not supported
  // by any plugin, includes a list of all game plugins in the details section
  // of the dialog
  //
  void warnUnrecognized(const QString& path);

private:
  // a single game, with its button and custom directory, if any
  //
  struct Game
  {
    // game plugin
    MOBase::IPluginGame* game = nullptr;

    // button on the ui
    QCommandLinkButton* button = nullptr;

    // game directory; set in ctor if the plugin has detected the game, or
    // set later when the user selects a directory
    QString dir;

    // whether a directory has been set for this game, either auto detected
    // or by the user
    bool installed = false;

    Game(MOBase::IPluginGame* g);
    Game(const Game&)            = delete;
    Game& operator=(const Game&) = delete;
  };

  // list of all game plugins, even if they're not installed; those are filtered
  // from the ui if the checkbox isn't checked
  std::vector<std::unique_ptr<Game>> m_games;

  // current selection
  Game* m_selection;

  // filter
  MOBase::FilterWidget m_filter;

  // returns a list of all the game plugins sorted with natsort
  //
  std::vector<MOBase::IPluginGame*> sortedGamePlugins() const;

  // creates the m_games list
  //
  void createGames();

  // finds the game struct associated with the given game
  //
  Game* findGame(MOBase::IPluginGame* game);

  // creates the ui for the given game button
  //
  void createGameButton(Game* g);

  // adds the given button to the ui
  //
  void addButton(QAbstractButton* b);

  // updates the given button on the ui, sets the text, icon, etc.
  //
  void updateButton(Game* g);

  // game buttons are toggles, this creates the button for the given game if
  // it doesn't exist and toggles it on
  //
  // the button might not exist if, for example:
  //   1) this game is currently filtered out (not installed, doesn't match
  //      filter text, etc) and,
  //   2) the user browses to a directory that a hidden plugin can use
  //
  void selectButton(Game* g);

  // removes all buttons from the ui
  //
  void clearButtons();

  // creates the "Browse" button
  //
  QCommandLinkButton* createCustomButton();

  // clears the button list and adds all the buttons to it, depending on
  // filtering and stuff
  //
  void fillList();

  // checks whether the given path looks valid to the given game plugin
  //
  // if the plugin doesn't like the path, allows the user to override and
  // accept, but also attempts to find another plugin that wants it and
  // propose that as an alternative, if there's one
  //
  // returns:
  //   - if the user selects the alternative plugin, returns that plugin
  //     instead;
  //   - if the path is bad but the user overrides, returns the given plugin
  //   - if the user cancels or if no plugins can manage the directory, returns
  //     null
  //
  Game* checkInstallation(const QString& path, Game* g);

  // tells the user that the path cannot be handled by any game plugin, returns
  // true if the user decides to accept anyway
  //
  bool confirmUnknown(const QString& path, MOBase::IPluginGame* game);

  // tells the user that the path can be handled by a different plugin than the
  // selected one and allows them to either
  //   1) use the alternative, guessedGame is returned;
  //   2) use the selection anyway, selectedGame is returned; or
  //   3) cancel, null is returned
  //
  MOBase::IPluginGame* confirmOtherGame(const QString& path,
                                        MOBase::IPluginGame* selectedGame,
                                        MOBase::IPluginGame* guessedGame);

  // detects if the given path likely contains a Microsoft Store game
  //
  bool detectMicrosoftStore(const QString& path);

  // tells the user that the path probably contains a Microsoft Store game that
  // is not supported, returns true if the user decides to accept anyway.
  //
  bool confirmMicrosoftStore(const QString& path, MOBase::IPluginGame* game);
};

// game variants page; displays a list of command buttons for game variants, as
// reported by the game plugin
//
// this page is always skipped if the game plugin reports no variants
//
class VariantsPage : public Page
{
public:
  VariantsPage(CreateInstanceDialog& dlg);

  // whether a variant has been selected or the game plugin reports no variants
  //
  bool ready() const override;

  // uses the game selected in the previous page to fill the list, this must be
  // called every time because the user may go back in forth in the wizard
  //
  void doActivated(bool firstTime) override;

  // returns the selected variant, if any
  //
  QString selectedGameVariant(MOBase::IPluginGame* game) const override;

  // selects the given variant
  //
  void select(const QString& variant);

protected:
  // returns true if the game has no variants
  //
  bool doSkip() const override;

private:
  // game that was selected the last time this page was active
  MOBase::IPluginGame* m_previousGame;

  // buttons
  std::vector<QCommandLinkButton*> m_buttons;

  // selected variant
  QString m_selection;

  // fills the list with buttons
  void fillList();
};

// instance name page; displays a textbox where the user can enter a name and
// does basic checks to make sure the name is valid and not a duplicate
//
// skipped for portable instances
//
class NamePage : public Page
{
public:
  NamePage(CreateInstanceDialog& dlg);

  // whether a valid name has been entered
  //
  bool ready() const override;

  // returns the instance name
  //
  QString selectedInstanceName() const override;

protected:
  // uses the selected game to generate an instance name
  //
  // as long as the user hasn't modified the textbox, this will regenerate a new
  // instance name every time the selected game changes
  //
  void doActivated(bool firstTime) override;

  // returns true for portable instances
  //
  bool doSkip() const override;

private:
  // game label, replaces %1 with the game name
  PlaceholderLabel m_label;

  // "instance already exists" label, replaces %1 with instance name
  PlaceholderLabel m_exists;

  // "instance name invalid" label, replaces %1 with instance name
  PlaceholderLabel m_invalid;

  // whether the user has modified the text, prevents auto generation when the
  // selected game changes
  bool m_modified;

  // whether the instance name is valid
  bool m_okay;

  // called when the user modifies the textbox, remember that it has changed and
  // calls verify()
  //
  void onChanged();

  // check if the entered name is valid, sets m_okay and calls checkName()
  //
  void verify();

  // updates the ui depending on whether the given instance name is valid in
  // the given directory; returns false if the name is invalid
  //
  bool checkName(QString parentDir, QString name);
};

// instance paths page; shows a single textbox for the base directory, or a
// series of textboxes for all the configurable paths if the advanced checkbox
// is checked
//
class PathsPage : public Page
{
public:
  PathsPage(CreateInstanceDialog& dlg);

  // whether all paths make sense
  //
  bool ready() const override;

  // returns the selected paths
  //
  CreateInstanceDialog::Paths selectedPaths() const override;

protected:
  // resets all the paths if the instance type or instance name have changed,
  // the current values are kept as long as these don't change; also updates the
  // game name in the ui
  //
  void doActivated(bool firstTime) override;

private:
  // instance name the last time this page was active
  QString m_lastInstanceName;

  // instance type the last time this page was active
  CreateInstanceDialog::Types m_lastType;

  // help label, replaces %1 by the game name
  PlaceholderLabel m_label;

  // path exists/is invalid labels for the simple page, replaces %1 with the
  // path
  PlaceholderLabel m_simpleExists, m_simpleInvalid;

  // path exists/is invalid labels for the advanced page, replaces %1 with the
  // path
  PlaceholderLabel m_advancedExists, m_advancedInvalid;

  // whether the paths are valid
  bool m_okay;

  // called when the user changes any textbox, checks the path and updates nav
  //
  void onChanged();

  // opens a browse directory dialog and sets the given textbox
  //
  void browse(QLineEdit* e);

  // checks the simple or advanced paths, sets m_okay
  //
  void checkPaths();

  // checks a simple path, forwards to checkPath() with the simple labels
  //
  bool checkSimplePath(const QString& path);

  // checks an advanced path, forwards to checkPath() with the advanced labels
  //
  bool checkAdvancedPath(const QString& path);

  // returns false if the path is invalid or already exists, sets the given
  // labels accordingly
  //
  bool checkPath(QString path, PlaceholderLabel& existsLabel,
                 PlaceholderLabel& invalidLabel);

  // replaces %base_dir% in the given path by whatever's in the base path
  // textbox
  //
  QString resolve(const QString& path) const;

  // called when the advanced checkbox is toggled, switches the active page
  // and checks the paths
  //
  void onAdvanced();

  // called whenever the page becomes active
  //
  // this normally doesn't change the textboxes unless they're empty, but if the
  // instance name or type have changed, `force` is true, which forces all paths
  // to reset
  //
  void setPaths(const QString& name, bool force);

  // sets the given textbox to the path if it's empty or if `force` is true
  //
  void setIfEmpty(QLineEdit* e, const QString& path, bool force);
};

// default settings for profiles page; allow the user to set their preferred
// defaults for the profile options
//
class ProfilePage : public Page
{
public:
  ProfilePage(CreateInstanceDialog& dlg);

  // always returns true, options are boolean
  //
  bool ready() const override;

  CreateInstanceDialog::ProfileSettings profileSettings() const override;
};

// nexus connection page; this reuses the ui found in the settings dialog and
// is skipped if there's already an api key in the credentials manager
//
class NexusPage : public Page
{
public:
  NexusPage(CreateInstanceDialog& dlg);
  ~NexusPage();

  // always returns true, this is an optional page
  //
  bool ready() const override;

protected:
  // returns true if the api key was already detected
  //
  bool doSkip() const override;

private:
  // connection ui
  std::unique_ptr<NexusConnectionUI> m_connectionUI;

  // set to true only if the api key was detected when opening the dialog, or
  // going back and forth would skip the page after the process is completed,
  // which would be unexpected
  bool m_skip;
};

// shows a text log of all the creation parameters
//
class ConfirmationPage : public Page
{
public:
  ConfirmationPage(CreateInstanceDialog& dlg);

  // recreates the log with the latest settings
  //
  void doActivated(bool firstTime) override;

  // returns the text for the log
  //
  QString makeReview() const;

private:
  // returns a log line with the given caption and path, something like
  // " - caption: path"
  //
  QString dirLine(const QString& caption, const QString& path) const;
};

}  // namespace cid

#endif  // MODORGANIZER_CREATEINSTANCEDIALOGPAGES_INCLUDED
