#ifndef MODLISTPROXY_H
#define MODLISTPROXY_H

#include "modlist.h"
#include <imodlist.h>

class OrganizerProxy;

class ModListProxy : public MOBase::IModList
{

public:
  ModListProxy(OrganizerProxy* oproxy, ModList* modlist);
  virtual ~ModListProxy();

  QString displayName(const QString& internalName) const override;
  QStringList allMods() const override;
  QStringList
  allModsByProfilePriority(MOBase::IProfile* profile = nullptr) const override;
  MOBase::IModInterface* getMod(const QString& name) const override;
  bool removeMod(MOBase::IModInterface* mod) override;
  MOBase::IModInterface* renameMod(MOBase::IModInterface* mod,
                                   const QString& name) override;
  ModStates state(const QString& name) const override;
  bool setActive(const QString& name, bool active) override;
  int setActive(const QStringList& names, bool active) override;
  int priority(const QString& name) const override;
  bool setPriority(const QString& name, int newPriority) override;
  bool onModInstalled(const std::function<void(MOBase::IModInterface*)>& func) override;
  bool onModRemoved(const std::function<void(QString const&)>& func) override;
  bool onModStateChanged(
      const std::function<void(const std::map<QString, ModStates>&)>& func) override;
  bool onModMoved(const std::function<void(const QString&, int, int)>& func) override;

private:
  friend class OrganizerProxy;

  // See OrganizerProxy::connectSignals().
  void connectSignals();
  void disconnectSignals();

  OrganizerProxy* m_OrganizerProxy;
  ModList* m_Proxied;

  ModList::SignalModInstalled m_ModInstalled;
  ModList::SignalModMoved m_ModMoved;
  ModList::SignalModRemoved m_ModRemoved;
  ModList::SignalModStateChanged m_ModStateChanged;

  std::vector<boost::signals2::connection> m_Connections;
};

#endif  // ORGANIZERPROXY_H
