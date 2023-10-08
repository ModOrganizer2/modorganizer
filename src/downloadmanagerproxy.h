#ifndef DOWNLOADMANAGERPROXY_H
#define DOWNLOADMANAGERPROXY_H

#include "downloadmanager.h"
#include <idownloadmanager.h>

class OrganizerProxy;

class DownloadManagerProxy : public MOBase::IDownloadManager
{

public:
  DownloadManagerProxy(OrganizerProxy* oproxy, DownloadManager* downloadManager);
  virtual ~DownloadManagerProxy();

  int startDownloadURLs(const QStringList& urls) override;
  int startDownloadNexusFile(int modID, int fileID) override;
  QString downloadPath(QString moId) override;

  bool onDownloadComplete(const std::function<void(QString)>& callback) override;
  bool onDownloadPaused(const std::function<void(QString)>& callback) override;
  bool onDownloadFailed(const std::function<void(QString)>& callback) override;
  bool onDownloadRemoved(const std::function<void(QString)>& callback) override;

private:
  friend class OrganizerProxy;

  // See OrganizerProxy::connectSignals().
  void connectSignals();
  void disconnectSignals();

  OrganizerProxy* m_OrganizerProxy;
  DownloadManager* m_Proxied;

  DownloadManager::SignalDownloadCallback m_DownloadComplete;
  DownloadManager::SignalDownloadCallback m_DownloadPaused;
  DownloadManager::SignalDownloadCallback m_DownloadFailed;
  DownloadManager::SignalDownloadCallback m_DownloadRemoved;

  std::vector<boost::signals2::connection> m_Connections;
};

#endif  // ORGANIZERPROXY_H
