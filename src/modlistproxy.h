#ifndef MODLISTPROXY_H
#define MODLISTPROXY_H

#include <imodlist.h>

class OrganizerProxy;

class ModListProxy : public MOBase::IModList
{

public:

  ModListProxy(OrganizerProxy* oproxy, IModList* modlist);
  virtual ~ModListProxy() { }

  QString displayName(const QString& internalName) const override;
  QStringList allMods() const override;
  ModStates state(const QString& name) const override;
  bool setActive(const QString& name, bool active) override;
  int setActive(const QStringList& names, bool active) override;
  int priority(const QString& name) const override;
  bool setPriority(const QString& name, int newPriority) override;
  bool onModStateChanged(const std::function<void(const std::map<QString, ModStates>&)>& func) override;
  bool onModMoved(const std::function<void(const QString&, int, int)>& func) override;

private:

  OrganizerProxy* m_OrganizerProxy;
  IModList* m_Proxied;
};

#endif // ORGANIZERPROXY_H
