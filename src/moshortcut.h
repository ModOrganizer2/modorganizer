#ifndef MODORGANIZER_MOSHORTCUT_INCLUDED
#define MODORGANIZER_MOSHORTCUT_INCLUDED

#include <QString>
#include "instancemanager.h"

class MOShortcut
{
public:
  MOShortcut(const QString& link={});

  // true if initialized using a valid moshortcut link
  //
  bool isValid() const;

  // whether an instance name was given
  //
  bool hasInstance() const;

  // whether an executable name was given
  //
  bool hasExecutable() const;

  // name of the instance given, "Portable" for portable; undefined if
  // hasInstance() returns false
  //
  QString instanceDisplayName() const;

  // name of the instance given, empty for portable; undefined if hasInstance()
  // returns false
  //
  const QString& instanceName() const;

  // name of the executable given
  //
  const QString& executableName() const;

  // whether this shortcut is for the given instance
  //
  bool isForInstance(const Instance& i) const;

  QString toString() const;

private:
  QString m_instance;
  QString m_executable;
  bool m_valid;
  bool m_hasInstance;
  bool m_hasExecutable;
};

#endif  // MODORGANIZER_MOSHORTCUT_INCLUDED
