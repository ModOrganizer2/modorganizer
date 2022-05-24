#ifndef MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED
#define MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED

#include <QDialog>

namespace MOBase
{
class IPluginGame;
}
namespace Ui
{
class CreateInstanceDialog;
};
namespace cid
{
class Page;
}

class PluginManager;
class Settings;

// this is a wizard for creating a new instance, it is made out of Page objects,
// see createinstancedialogpages.h
//
// each page can give back one or more pieces of information that is collected
// in creationInfo() and used by finish() to do the actual creation
//
// pages can be disabled if they return true in skip(), which happens globally
// for some (IntroPage has a setting in the registry), depending on context
// (NexusPage is skipped if the API key already exists) or explicitly (when
// only some info about the instance is missing on startup, such as a game
// variant)
//
class CreateInstanceDialog : public QDialog
{
  Q_OBJECT

public:
  enum class Actions
  {
    Find = 1
  };

  // instance type
  //
  enum Types
  {
    NoType = 0,
    Global,
    Portable
  };

  // all the paths required by the instance, some may be empty, such as
  // basically all of them except for `base` when the user doesn't use the
  // "Advanced" part of the paths page
  //
  struct Paths
  {
    QString base;
    QString downloads;
    QString mods;
    QString profiles;
    QString overwrite;
    QString ini;

    auto operator<=>(const Paths&) const = default;
  };

  struct ProfileSettings
  {
    bool localInis;
    bool localSaves;
    bool archiveInvalidation;

    auto operator<=>(const ProfileSettings&) const = default;
  };

  // all the info filled in the various pages
  //
  struct CreationInfo
  {
    Types type;
    MOBase::IPluginGame* game;
    QString gameLocation;
    QString gameVariant;
    QString instanceName;
    QString dataPath;
    QString iniPath;
    Paths paths;
    ProfileSettings profileSettings;
  };

  CreateInstanceDialog(const PluginManager& pc, Settings* s, QWidget* parent = nullptr);

  ~CreateInstanceDialog();

  Ui::CreateInstanceDialog* getUI();
  const PluginManager& pluginManager();
  Settings* settings();

  // disables all the pages except for the given one, used on startup when some
  // specific info is missing
  template <class Page>
  void setSinglePage(const QString& instanceName)
  {
    for (auto&& p : m_pages) {
      if (auto* tp = dynamic_cast<Page*>(p.get())) {
        tp->setSkip(false);
      } else {
        p->setSkip(true);
      }
    }

    setSinglePageImpl(instanceName);
  }

  // returns the page having the give path, or null
  //
  template <class Page>
  Page* getPage()
  {
    for (auto&& p : m_pages) {
      if (auto* tp = dynamic_cast<Page*>(p.get())) {
        return tp;
      }
    }

    return nullptr;
  }

  // moves to the next page; if `allowFinish` is true, calls finish() if
  // currently on the last page
  //
  void next(bool allowFinish = true);

  // moves to the previous page, if any
  //
  void back();

  // whether the current page reports that it is ready; if this is the last
  // page, next() would call finish()
  //
  bool canNext() const;

  // whether the current page is not the first one and there is an enabled page
  // prior
  //
  bool canBack() const;

  // selects the given page by index; this doesn't check if the page should be
  // skipped
  //
  void selectPage(std::size_t i);

  // moves by `d` pages, can be negative to move back
  //
  void changePage(int d);

  // creates the instance and closes the dialog
  //
  void finish();

  // updates the navigation buttons based on the current page
  //
  void updateNavigation();

  // whether this is the last enabled page
  //
  bool isOnLastPage() const;

  // returns whether the user has requested to switch to the new instance
  //
  bool switching() const;

  // gathers the info from all the pages as it appears, paths are not fixed;
  // see creationInfo()
  //
  CreationInfo rawCreationInfo() const;

  // gathers the info from all the pages: paths are converted to absolute and
  // the base dir variable is expanded everywhere; see rawCreationInfo()
  //
  CreationInfo creationInfo() const;

private:
  std::unique_ptr<Ui::CreateInstanceDialog> ui;
  const PluginManager& m_pc;
  Settings* m_settings;
  std::vector<std::unique_ptr<cid::Page>> m_pages;
  QString m_originalNext;
  bool m_switching;
  bool m_singlePage;

  // creates a shortcut for the given sequence
  //
  void addShortcut(QKeySequence seq, std::function<void()> f);

  // creates a shortcut for the given sequence and executes the action when
  // activated
  //
  void addShortcutAction(QKeySequence seq, Actions a);

  // calls action() with the given action on the selected page, if any
  //
  void doAction(Actions a);

  // called from setSinglePage(), does whatever doesn't need the T
  //
  void setSinglePageImpl(const QString& instanceName);

  // adds a line to the creation log
  //
  void logCreation(const QString& s);
  void logCreation(const std::wstring& s);

  // calls the given member function on all pages until one returns an object
  // that's not empty; used by gatherInfo()
  //
  template <class MF, class... Args>
  auto getSelected(MF mf, Args&&... args) const
  {
    // return type
    using T = decltype((std::declval<cid::Page>().*mf)(std::forward<Args>(args)...));

    for (auto&& p : m_pages) {
      const auto t = (p.get()->*mf)(std::forward<Args>(args)...);
      if (t != T()) {
        return t;
      }
    }

    return T();
  }
};

#endif  // MODORGANIZER_CREATEINSTANCEDIALOG_INCLUDED
