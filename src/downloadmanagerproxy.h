#ifndef DOWNLOADMANAGERPROXY_H
#define DOWNLOADMANAGERPROXY_H

#include <idownloadmanager.h>

class OrganizerProxy;

class DownloadManagerProxy : public MOBase::IDownloadManager
{

public:

  DownloadManagerProxy(OrganizerProxy* oproxy, IDownloadManager* downloadManager);
  virtual ~DownloadManagerProxy() { }

  int startDownloadURLs(const QStringList& urls) override;
  int startDownloadNexusFile(int modID, int fileID) override;
  QString downloadPath(int id) override;

  bool onDownloadComplete(const std::function<void(int)>& callback) override;
  bool onDownloadPaused(const std::function<void(int)>& callback) override;
  bool onDownloadFailed(const std::function<void(int)>& callback) override;
  bool onDownloadRemoved(const std::function<void(int)>& callback) override;

private:

  OrganizerProxy* m_OrganizerProxy;
  IDownloadManager* m_Proxied;
};

#endif // ORGANIZERPROXY_H
