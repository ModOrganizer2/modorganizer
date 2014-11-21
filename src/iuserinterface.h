#ifndef IUSERINTERFACE_H
#define IUSERINTERFACE_H


#include "modinfo.h"
#include <iplugintool.h>
#include <ipluginmodpage.h>


class IUserInterface
{
public:

  void storeSettings(QSettings &settings);

  virtual HANDLE startApplication(const QString &executable, const QStringList &args = QStringList(), const QString &cwd = "", const QString &profile = "") = 0;

  virtual bool waitForProcessOrJob(HANDLE processHandle, LPDWORD exitCode = NULL) = 0;

  virtual void registerPluginTool(MOBase::IPluginTool *tool) = 0;
  virtual void registerModPage(MOBase::IPluginModPage *modPage) = 0;

  virtual void installTranslator(const QString &name) = 0;

  virtual void disconnectPlugins() = 0;

  virtual bool close() = 0;

  virtual void setEnabled(bool enabled) = 0;

  virtual void displayModInformation(ModInfo::Ptr modInfo, unsigned int index, int tab) = 0;

  virtual void updateBSAList(const QStringList &defaultArchives, const QStringList &activeArchives) = 0;

  virtual bool saveArchiveList() = 0;

};

#endif // IUSERINTERFACE_H
