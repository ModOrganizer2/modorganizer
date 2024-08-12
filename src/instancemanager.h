#ifndef MODORGANIZER_INSTANCEMANAGER_INCLUDED
#define MODORGANIZER_INSTANCEMANAGER_INCLUDED

#include <QSettings>
#include <QString>

namespace MOBase
{
class IPluginGame;
}

class Settings;
class PluginManager;

// represents an instance, either global or portable
//
// if setup() is not called, the game plugin is not available and the INI is
// not processed at all, so name(), directory() and isPortable() really are the
// only meaningful functions
//
// setup() must be called when MO wants to use the instance, it will read the
// INI, figure out the game plugin to use and set it up by calling
// setGameVaraint(), setGamePath(), etc. on it
//
// when setup() fails because the game name/directory or variant are missing,
// setGame() and setVariant() can be called before retrying setup(); this
// happens on startup if that information is missing
//
class Instance
{
public:
  // returned by setup()
  //
  enum class SetupResults
  {
    // instance is ready to be used
    Okay,

    // error while reading the INI
    BadIni,

    // both the game name and directory are missing from the ini; setup() will
    // attempt to recover if either are missing, but not when both are
    IniMissingGame,

    // either:
    // 1) there is no plugin with the given name, or
    // 2) if the name is missing, no plugin can handle the game directory
    PluginGone,

    // the selected plugin does not consider the game directory as being valid
    GameGone,

    // there is no game variant specified in the INI, but the plugin requires
    // one
    MissingVariant
  };

  // a file or directory owned by this instance, used by objectsForDeletion()
  //
  struct Object
  {
    // path to the file or directory
    QString path;

    // whether this object must be deleted to properly delete the instance;
    // typically true only for the instance directory itself, but not for the
    // base directory, etc.
    bool mandatoryDelete;

    Object(QString p, bool d = false) : path(std::move(p)), mandatoryDelete(d) {}

    // puts mandatory delete on top
    //
    bool operator<(const Object& o) const
    {
      if (mandatoryDelete && !o.mandatoryDelete) {
        return true;
      } else if (!mandatoryDelete && o.mandatoryDelete) {
        return false;
      }

      return false;
    }
  };

  // an instance that lives in the given directory; `portable` must be `true`
  // if this is a portable instance
  //
  // `profileName` can be given to override what's in the INI; this typically
  // happens when the profile is overriden on the command line
  //
  Instance(QString dir, bool portable, QString profileName = {});

  // reads in values from the INI if they were not given yet:
  //  - game name
  //  - game directory
  //  - game variant
  //  - profile name
  //
  // note that setup() already calls this
  //
  // returns false if the ini couldn't be read from
  //
  bool readFromIni();

  // finds the appropriate game plugin and sets it up so MO can use it; this
  // calls readFromIni() first
  //
  // setup() tries to recover from some errors, but can fail for a variety of
  // reasons, see SetupResults
  //
  SetupResults setup(PluginManager& plugins);

  // overrides the game name and directory
  //
  void setGame(const QString& name, const QString& dir);

  // overrides the game variant
  //
  void setVariant(const QString& name);

  // returns the instance name; this is the directory name or "Portable" for
  // portable instances
  //
  // be careful when using this function to check whether two instances are the
  // same, some parts of MO use an empty string to represent portable instances,
  // but this function will return "Portable" for them; it's safer to check
  // for isPortable() first
  //
  // can be called without setup()
  //
  QString displayName() const;

  // returns either:
  //   1) the game name from the INI, if readFromIni() was called;
  //   2) gameName() from the game plugin if it was missing and setup() was
  //      called; or
  //   3) whatever was given in setGame()
  //
  QString gameName() const;

  // returns either:
  //   1) the game directory from the INI, if readFromIni() was called;
  //   2) gameDirectory() from the game plugin if it was missing and setup()
  //      was called; or
  //   3) whatever was given in setGame()
  //
  QString gameDirectory() const;

  // returns the instance directory as given in the constructor
  //
  QString directory() const;

  // returns the base directory; empty if readFromIni() hasn't been called
  //
  QString baseDirectory() const;

  // returns whether this is a portable instance, as given in the constructor
  //
  bool isPortable() const;

  // returns the selected game plugin; will return null if setup() hasn't been
  // called, or if it failed
  //
  MOBase::IPluginGame* gamePlugin() const;

  // returns either:
  // 1) the profile name given in the constructor if not empty;
  // 2) the profile name from the INI if readFromIni() was called, or
  // 3) the default profile name if it's missing (see
  //    AppConfig::defaultProfileName())
  //
  QString profileName() const;

  // returns the path to the INI file for this instance; the file may not
  // exist
  //
  QString iniPath() const;

  // whether this is the currently active instance in MO
  //
  bool isActive() const;

  // returns a list of files and directories that must be deleted when deleting
  // this instance; this will read the INI and fail if it's not accessible
  //
  // returns an empty list on failure
  //
  std::vector<Object> objectsForDeletion() const;

private:
  QString m_dir;
  bool m_portable;
  QString m_gameName, m_gameDir, m_gameVariant, m_baseDir;
  MOBase::IPluginGame* m_plugin;
  QString m_profile;

  // figures out the game plugin for this instance
  //
  SetupResults getGamePlugin(PluginManager& plugins);

  // figures out the profile name for this instance
  //
  void getProfile(const Settings& s);

  // updates the ini with the given values and the ones found by setup()
  //
  void updateIni();
};

// manages global and portable instances
//
class InstanceManager
{
public:
  // there is only one manager; this isn't called instance() because it's hella
  // confusing
  //
  static InstanceManager& singleton();

  // overrides instance name found in registry
  //
  void overrideInstance(const QString& instanceName);

  // overrides profile name from INI for currentInstance()
  //
  void overrideProfile(const QString& profileName);

  // clears instance and profile overrides, used when restarting MO to select
  // another instance
  //
  void clearOverrides();

  // returns a game plugin that considers the given directory valid
  //
  // this will check for an INI file in the directory and use its game name
  // and directory if available
  //
  // if there is no INI, if it's missing these values or if there are no game
  // plugins that can handle these values, this returns the first plugin that
  // considers the given directory valid
  //
  // returns null if all of this fails
  //
  const MOBase::IPluginGame* gamePluginForDirectory(const QString& dir,
                                                    const PluginManager& plugins) const;

  MOBase::IPluginGame* gamePluginForDirectory(const QString& dir,
                                              PluginManager& plugins) const;

  // clears the instance name from the registry; on restart, this will make MO
  // either select the portable instance if it exists, or display the instance
  // selection/creation dialog
  //
  void clearCurrentInstance();

  // returns the current instance from the registry; this may be empty if the
  // instance name in the registry is empty or non-existent and there is no
  // portable instance set up
  //
  std::unique_ptr<Instance> currentInstance() const;

  // sets the instance name in the registry so the same instance is opened next
  // time MO runs
  //
  void setCurrentInstance(const QString& name);

  // whether MO should allow the user to change the current instance from the
  // user interface
  //
  bool allowedToChangeInstance() const;

  // whether a portable instance exists; this basically checks for an INI in
  // the application directory
  //
  bool portableInstanceExists() const;

  // whether any instance exists, whether global or portable
  //
  bool hasAnyInstances() const;

  // returns the absolute path to the portable instance, regardless of whether
  // one exists
  //
  QString portablePath() const;

  // returns the absolute path to the directory that contains global instances
  // (typically AppData/Local/ModOrganizer)
  //
  QString globalInstancesRootPath() const;

  // returns the list of absolute path to all existing global instances; this
  // does not include the portable instance
  //
  std::vector<QString> globalInstancePaths() const;

  // sanitizes the given instance name and either
  // 1) returns it if there is no instance with this name
  // 2) tries to add " (N)" at the end until it works
  //
  // may return an empty string if no unique name can be found
  //
  QString makeUniqueName(const QString& instanceName) const;

  // returns whether a global instance with this name already exists
  //
  bool instanceExists(const QString& instanceName) const;

  // returns the absolute path of a global instance with the given name; this
  // does not check if the name is valid or if exists
  //
  QString instancePath(const QString& instanceName) const;

  // returns the absolute path to the INI file for the given instance directory;
  // the file may not exist
  //
  QString iniPath(const QString& instanceDir) const;

private:
  InstanceManager();

private:
  std::optional<QString> m_overrideInstanceName;
  std::optional<QString> m_overrideProfileName;
};

// see setupInstance()
//
enum class SetupInstanceResults
{
  Okay,
  TryAgain,
  SelectAnother,
  Exit
};

// if there are no instances configured, global or portable, shows the
// create instance dialog and returns the new instance or empty if the user
// cancelled
//
// if there is at least one instance available, unconditionally show the
// instance manager dialog and returns the selected instance or empty if the
// user cancelled
//
std::unique_ptr<Instance> selectInstance();

// calls instance.setup() tries to handle problems by itself:
//
//  - if the ini is missing some information, will show dialogs and ask the user
//    to fill in what's required (such as the game directory, variant, etc.);
//    if successful, returns TryAgain and setupInstance() can be called again
//    with the same instance
//
//  - if the instance cannot be used (no game plugin found for it, ini can't
//    be read, etc.), returns SelectAnother
//
//  - if the user cancels at any point, returns Exit
//
//  - if the instance has been set up correctly, returns Okay
//
SetupInstanceResults setupInstance(Instance& instance, PluginManager& pc);

#endif  // MODORGANIZER_INSTANCEMANAGER_INCLUDED
