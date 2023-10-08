#include "downloadmanagerproxy.h"

#include "organizerproxy.h"
#include "proxyutils.h"

using namespace MOBase;
using namespace MOShared;

DownloadManagerProxy::DownloadManagerProxy(OrganizerProxy* oproxy,
                                           DownloadManager* downloadManager)
    : m_OrganizerProxy(oproxy), m_Proxied(downloadManager)
{}

DownloadManagerProxy::~DownloadManagerProxy()
{
  disconnectSignals();
}

void DownloadManagerProxy::connectSignals()
{
  m_Connections.push_back(m_Proxied->onDownloadComplete(
      callSignalIfPluginActive(m_OrganizerProxy, m_DownloadComplete)));
  m_Connections.push_back(m_Proxied->onDownloadFailed(
      callSignalIfPluginActive(m_OrganizerProxy, m_DownloadFailed)));
  m_Connections.push_back(m_Proxied->onDownloadRemoved(
      callSignalIfPluginActive(m_OrganizerProxy, m_DownloadRemoved)));
  m_Connections.push_back(m_Proxied->onDownloadPaused(
      callSignalIfPluginActive(m_OrganizerProxy, m_DownloadPaused)));
}

void DownloadManagerProxy::disconnectSignals()
{
  for (auto& conn : m_Connections) {
    conn.disconnect();
  }
  m_Connections.clear();
}

int DownloadManagerProxy::startDownloadURLs(const QStringList& urls)
{
  return m_Proxied->startDownloadURLs(urls);
}

int DownloadManagerProxy::startDownloadNexusFile(int modID, int fileID)
{
  return m_Proxied->startDownloadNexusFile(modID, fileID);
}

QString DownloadManagerProxy::downloadPath(QString moId)
{
  return m_Proxied->downloadPath(moId);
}

bool DownloadManagerProxy::onDownloadComplete(
    const std::function<void(QString moId)>& callback)
{
  return m_DownloadComplete.connect(callback).connected();
}

bool DownloadManagerProxy::onDownloadPaused(
    const std::function<void(QString moId)>& callback)
{
  return m_DownloadPaused.connect(callback).connected();
}

bool DownloadManagerProxy::onDownloadFailed(
    const std::function<void(QString moId)>& callback)
{
  return m_DownloadFailed.connect(callback).connected();
}

bool DownloadManagerProxy::onDownloadRemoved(
    const std::function<void(QString moId)>& callback)
{
  return m_DownloadRemoved.connect(callback).connected();
}
