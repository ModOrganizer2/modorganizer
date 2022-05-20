#ifndef IUSERINTERFACE_H
#define IUSERINTERFACE_H

#include "modinfodialogfwd.h"
#include <QMainWindow>
#include <QMenu>
#include <delayedfilewriter.h>
#include <ipluginmodpage.h>
#include <iplugintool.h>

class IUserInterface
{
public:
  virtual void registerModPage(MOBase::IPluginModPage* modPage) = 0;

  virtual bool closeWindow()                  = 0;
  virtual void setWindowEnabled(bool enabled) = 0;

  virtual void displayModInformation(ModInfoPtr modInfo, unsigned int modIndex,
                                     ModInfoTabIDs tabID) = 0;

  virtual void updateBSAList(const QStringList& defaultArchives,
                             const QStringList& activeArchives) = 0;

  virtual MOBase::DelayedFileWriterBase& archivesWriter() = 0;

  virtual QMainWindow* mainWindow() = 0;
};

#endif  // IUSERINTERFACE_H
