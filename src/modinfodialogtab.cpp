#include "modinfodialogtab.h"

ModInfoDialogTab::ModInfoDialogTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui)
    : ui(ui), m_core(oc), m_plugin(plugin), m_parent(parent), m_origin(nullptr)
{
}

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

void ModInfoDialogTab::update()
{
  // no-op
}

void ModInfoDialogTab::setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin)
{
  m_mod = mod;
  m_origin = origin;
}

ModInfo::Ptr ModInfoDialogTab::mod() const
{
  return m_mod;
}

MOShared::FilesOrigin* ModInfoDialogTab::origin() const
{
  return m_origin;
}

OrganizerCore& ModInfoDialogTab::core()
{
  return m_core;
}

PluginContainer& ModInfoDialogTab::plugin()
{
  return m_plugin;
}

QWidget* ModInfoDialogTab::parentWidget()
{
  return m_parent;
}

void ModInfoDialogTab::emitOriginModified(int originID)
{
  emit originModified(originID);
}

void ModInfoDialogTab::emitModOpen(QString name)
{
  emit modOpen(name);
}
