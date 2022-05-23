#ifndef EXTENSIONWATCHER_H
#define EXTENSIONWATCHER_H

#include <extension.h>

// an extension watcher is a class that watches extensions get loaded/unloaded,
// typically to extract information from theme that are needed by MO2
//
template <class ExtensionType>
class ExtensionWatcher
{
  static_assert(std::is_base_of_v<MOBase::IExtension, ExtensionType>);

public:
  // called when a new extension is found and loaded
  //
  virtual void extensionLoaded(ExtensionType const& extension) = 0;

  // called when a new extension is unloaded
  //
  virtual void extensionUnloaded(ExtensionType const& extension) = 0;

  // called when a new extension is disabled
  //
  virtual void extensionEnabled(ExtensionType const& extension) = 0;

  // called when a new extension is disabled
  //
  virtual void extensionDisabled(ExtensionType const& extension) = 0;

  virtual ~ExtensionWatcher() {}
};

#endif
