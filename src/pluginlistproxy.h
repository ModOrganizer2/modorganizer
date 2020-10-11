#ifndef PLUGINLISTPROXY_H
#define PLUGINLISTPROXY_H

#include <ipluginlist.h>

class OrganizerProxy;

class PluginListProxy : public MOBase::IPluginList
{

public:

  PluginListProxy(OrganizerProxy* oproxy, IPluginList* pluginlist);
  virtual ~PluginListProxy() { }

  QStringList pluginNames() const override;
  PluginStates state(const QString& name) const override;
  void setState(const QString& name, PluginStates state) override;
  int priority(const QString& name) const override;
  bool setPriority(const QString& name, int newPriority) override;
  int loadOrder(const QString& name) const override;
  void setLoadOrder(const QStringList& pluginList) override;
  bool isMaster(const QString& name) const override;
  QStringList masters(const QString& name) const override;
  QString origin(const QString& name) const override;
  bool onRefreshed(const std::function<void()>& callback) override;
  bool onPluginMoved(const std::function<void(const QString&, int, int)>& func) override;
  bool onPluginStateChanged(const std::function<void(const std::map<QString, PluginStates>&)>& func) override;

private:

  OrganizerProxy* m_OrganizerProxy;
  IPluginList* m_Proxied;
};

#endif // ORGANIZERPROXY_H
