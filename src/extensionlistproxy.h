#ifndef EXTENSIONLISTPROXY_H
#define EXTENSIONLISTPROXY_H

#include <uibase/extensions/iextensionlist.h>

#include "extensionmanager.h"

class OrganizerProxy;

class ExtensionListProxy : public MOBase::IExtensionList
{
public:
  ExtensionListProxy(OrganizerProxy* oproxy, const ExtensionManager& manager);
  virtual ~ExtensionListProxy();

  bool installed(const QString& identifier) const override;
  bool enabled(const QString& extension) const override;
  bool enabled(const MOBase::IExtension& extension) const override;
  const MOBase::IExtension& get(QString const& identifier) const override;
  const MOBase::IExtension& at(std::size_t const& index) const override;
  const MOBase::IExtension& operator[](std::size_t const& index) const override;
  std::size_t size() const override;

private:
  OrganizerProxy* m_oproxy;
  const ExtensionManager* m_manager;
};

#endif
