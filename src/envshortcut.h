#ifndef ENV_SHORTCUT_H
#define ENV_SHORTCUT_H

#include <QString>

class Executable;

namespace env
{

// an application shortcut that can be either on the desktop or the start menu
//
class Shortcut
{
public:
  // location of a shortcut
  //
  enum Locations
  {
    None = 0,

    // on the desktop
    Desktop,

    // in the start menu
    StartMenu
  };

  // empty shortcut
  //
  Shortcut();

  // shortcut from an executable
  //
  explicit Shortcut(const Executable& exe);

  // sets the name of the shortcut, shown on icons and start menu entries
  //
  Shortcut& name(const QString& s);

  // the program to start
  //
  Shortcut& target(const QString& s);

  // arguments to pass
  //
  Shortcut& arguments(const QString& s);

  // shows in the status bar of explorer, for example
  //
  Shortcut& description(const QString& s);

  // path to a binary that contains the icon and its index
  //
  Shortcut& icon(const QString& s, int index = 0);

  // "start in" option for this shortcut
  //
  Shortcut& workingDirectory(const QString& s);

  // returns whether this shortcut already exists at the given location; this
  // does not check whether the shortcut parameters are different, it merely if
  // the .lnk file exists
  //
  bool exists(Locations loc) const;

  // calls remove() if exists(), or add()
  //
  bool toggle(Locations loc);

  // adds the shortcut to the given location
  //
  bool add(Locations loc);

  // removes the shortcut from the given location
  //
  bool remove(Locations loc);

private:
  QString m_name;
  QString m_target;
  QString m_arguments;
  QString m_description;
  QString m_icon;
  int m_iconIndex;
  QString m_workingDirectory;

  // returns the path where the shortcut file should be saved
  //
  QString shortcutPath(Locations loc) const;

  // returns the directory where the shortcut file should be saved
  //
  QString shortcutDirectory(Locations loc) const;

  // returns the filename of the shortcut file that should be used when saving
  //
  QString shortcutFilename() const;
};

// returns a string representation of the given location
//
QString toString(Shortcut::Locations loc);

}  // namespace env

#endif  // ENV_SHORTCUT_H
