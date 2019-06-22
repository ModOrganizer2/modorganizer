#include "modinfodialogtab.h"

bool ModInfoDialogTab::feedFile(const QString&, const QString&)
{
  // no-op
  return false;
}

bool ModInfoDialogTab::canClose()
{
  return true;
}

void ModInfoDialogTab::saveState(Settings&)
{
  // no-op
}

void ModInfoDialogTab::restoreState(const Settings& s)
{
  // no-op
}

void ModInfoDialogTab::setMod(ModInfo::Ptr, MOShared::FilesOrigin*)
{
  // no-op
}

void ModInfoDialogTab::update()
{
  // no-op
}

void ModInfoDialogTab::emitOriginModified(int originID)
{
  emit originModified(originID);
}

void ModInfoDialogTab::emitModOpen(QString name)
{
  emit modOpen(name);
}
