#ifndef EXTENSIONWATCHER_H
#define EXTENSIONWATCHER_H

#include <extension.h>

// an extension watcher is a class that watches extensions get loaded/unloaded,
// typically to extract information from theme that are needed by MO2
//
class ExtensionWatcher
{
public:
  // called when a new extension is found and loaded
  //
  virtual void extensionLoaded(MOBase::IExtension const& extension) = 0;

  // called when a new extension is unloaded
  //
  virtual void extensionUnloaded(MOBase::IExtension const& extension) = 0;

  // called when a new extension is disabled
  //
  virtual void extensionEnabled(MOBase::IExtension const& extension) = 0;

  // called when a new extension is disabled
  //
  virtual void extensionDisabled(MOBase::IExtension const& extension) = 0;

  virtual ~ExtensionWatcher() {}
};

#endif
