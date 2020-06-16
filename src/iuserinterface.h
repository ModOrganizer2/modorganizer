#ifndef IUSERINTERFACE_H
#define IUSERINTERFACE_H


#include "modinfodialogfwd.h"
#include <iplugintool.h>
#include <ipluginmodpage.h>
#include <delayedfilewriter.h>
#include <QMenu>
#include <QMainWindow>


class IUserInterface
{
public:
  virtual void registerPluginTool(MOBase::IPluginTool *tool, QString name = QString(), QMenu *menu = nullptr) = 0;
  virtual void registerPluginTools(std::vector<MOBase::IPluginTool *> toolPlugins) = 0;
  virtual void registerModPage(MOBase::IPluginModPage *modPage) = 0;

  virtual void installTranslator(const QString &name) = 0;

  virtual void disconnectPlugins() = 0;

  virtual bool closeWindow() = 0;
  virtual void setWindowEnabled(bool enabled) = 0;

  virtual void displayModInformation(
    ModInfoPtr modInfo, unsigned int modIndex, ModInfoTabIDs tabID) = 0;

  virtual void updateBSAList(const QStringList &defaultArchives, const QStringList &activeArchives) = 0;

  virtual MOBase::DelayedFileWriterBase &archivesWriter() = 0;

  virtual QMainWindow* mainWindow() = 0;
};

#endif // IUSERINTERFACE_H
