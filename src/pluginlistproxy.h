#ifndef PLUGINLISTPROXY_H
#define PLUGINLISTPROXY_H

#include "pluginlist.h"
#include <ipluginlist.h>

class OrganizerProxy;

class PluginListProxy : public MOBase::IPluginList
{
public:
  PluginListProxy(OrganizerProxy* oproxy, PluginList* pluginlist);
  virtual ~PluginListProxy();

  QStringList pluginNames() const override;
  PluginStates state(const QString& name) const override;
  void setState(const QString& name, PluginStates state) override;
  int priority(const QString& name) const override;
  bool setPriority(const QString& name, int newPriority) override;
  int loadOrder(const QString& name) const override;
  void setLoadOrder(const QStringList& pluginList) override;
  [[deprecated]] bool isMaster(const QString& name) const override;
  QStringList masters(const QString& name) const override;
  QString origin(const QString& name) const override;

  bool onRefreshed(const std::function<void()>& callback) override;
  bool
  onPluginMoved(const std::function<void(const QString&, int, int)>& func) override;
  bool onPluginStateChanged(
      const std::function<void(const std::map<QString, PluginStates>&)>& func) override;

  bool hasMasterExtension(const QString& name) const override;
  bool hasLightExtension(const QString& name) const override;
  bool isMasterFlagged(const QString& name) const override;
  bool isLightFlagged(const QString& name) const override;
  bool isOverlayFlagged(const QString& name) const override;
  bool hasNoRecords(const QString& name) const override;

private:
  friend class OrganizerProxy;

  // See OrganizerProxy::connectSignals().
  void connectSignals();
  void disconnectSignals();

  OrganizerProxy* m_OrganizerProxy;
  PluginList* m_Proxied;

  PluginList::SignalRefreshed m_Refreshed;
  PluginList::SignalPluginMoved m_PluginMoved;
  PluginList::SignalPluginStateChanged m_PluginStateChanged;

  std::vector<boost::signals2::connection> m_Connections;
};

#endif  // ORGANIZERPROXY_H
