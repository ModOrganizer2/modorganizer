#include "downloadmanagerproxy.h"

#include "proxyutils.h"
#include "organizerproxy.h"

using namespace MOBase;

DownloadManagerProxy::DownloadManagerProxy(OrganizerProxy* oproxy, IDownloadManager* downloadManager) :
  m_OrganizerProxy(oproxy), m_Proxied(downloadManager) { }

int DownloadManagerProxy::startDownloadURLs(const QStringList& urls)
{
  return m_Proxied->startDownloadURLs(urls);
}

int DownloadManagerProxy::startDownloadNexusFile(int modID, int fileID)
{
  return m_Proxied->startDownloadNexusFile(modID, fileID);
}

QString DownloadManagerProxy::downloadPath(int id)
{
  return m_Proxied->downloadPath(id);
}

bool DownloadManagerProxy::onDownloadComplete(const std::function<void(int)>& callback)
{
  return m_Proxied->onDownloadComplete(MOShared::callIfPluginActive(m_OrganizerProxy, callback));
}

bool DownloadManagerProxy::onDownloadPaused(const std::function<void(int)>& callback)
{
  return m_Proxied->onDownloadPaused(MOShared::callIfPluginActive(m_OrganizerProxy, callback));
}

bool DownloadManagerProxy::onDownloadFailed(const std::function<void(int)>& callback)
{
  return m_Proxied->onDownloadFailed(MOShared::callIfPluginActive(m_OrganizerProxy, callback));
}

bool DownloadManagerProxy::onDownloadRemoved(const std::function<void(int)>& callback)
{
  return m_Proxied->onDownloadRemoved(MOShared::callIfPluginActive(m_OrganizerProxy, callback));
}
