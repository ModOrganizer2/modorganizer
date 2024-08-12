#include "extensionlistproxy.h"

#include <log.h>

using namespace MOBase;

ExtensionListProxy::ExtensionListProxy(OrganizerProxy* oproxy,
                                       const ExtensionManager& manager)
    : m_oproxy(oproxy), m_manager(&manager)
{}

ExtensionListProxy ::~ExtensionListProxy() {}

bool ExtensionListProxy::installed(const QString& identifier) const
{
  return m_manager->extension(identifier) != nullptr;
}

bool ExtensionListProxy::enabled(const QString& extension) const
{
  return m_manager->isEnabled(extension);
}

bool ExtensionListProxy::enabled(const IExtension& extension) const
{
  return m_manager->isEnabled(extension);
}

const IExtension& ExtensionListProxy::get(QString const& identifier) const
{
  auto* extension = m_manager->extension(identifier);
  if (extension) {
    return *extension;
  }
  throw std::out_of_range(std::format("extension '{}' not found", identifier));
}

const IExtension& ExtensionListProxy::at(std::size_t const& index) const
{
  return *m_manager->extensions().at(index);
}

const IExtension& ExtensionListProxy::operator[](std::size_t const& index) const
{
  return *m_manager->extensions().at(index);
}

std::size_t ExtensionListProxy::size() const
{
  return m_manager->extensions().size();
}
