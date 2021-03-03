#include "downloadmanager2.h"
#include "curldownloader.h"
#include "plugincontainer.h"
#include <log.h>

static std::unique_ptr<dm::DownloadManager2> g_dm;


std::optional<int> testDM(PluginContainer& pc)
{
  g_dm.reset(new dm::DownloadManager2(pc));
  //g_dm->maxSpeed(1024 * 300);
  //g_dm->maxActive(1);
  return {};
}


void dmAdd(const QString& url)
{
  g_dm->add(url);
}


namespace dm
{

using namespace MOBase;

log::LogWrapper& dmLog()
{
  static log::LogWrapper lg(log::getDefault(), "dm: ");
  return lg;
}

QString toString(Download::States s)
{
  using S = Download::States;

  switch (s)
  {
    case S::None: return "none";
    case S::Queueing: return "queueing";
    case S::Queued: return "queued";
    case S::Finished: return "finished";
    case S::Errored: return "errored";
    case S::Running: return "running";
    case S::Pausing: return "pausing";
    case S::Paused: return "paused";
    case S::Cancelling: return "cancelling";
    case S::Cancelled: return "cancelled";

    default:
    {
      const auto n = static_cast<std::underlying_type_t<Download::States>>(s);
      return QString::fromStdString("?" + std::to_string(n));
    }
  }
}


class DefaultDownload : public IRepositoryDownload
{
public:
  DefaultDownload(const QString& what, IDownloader* d)
    : m_what(what), m_downloader(d), m_first(true)
  {
    setState(States::Downloading);

    IDownload::Info info;
    info.outputFile = downloadFilename(what);

    m_dl = d->add(what, info);
  }

  QString downloadFilename(const QString& what) const
  {
    return QUrl(what).fileName();
  }

  void tick() override
  {
    if (!m_dl) {
      return;
    }

    if (m_dl->state() == IDownload::Finished) {
      setState(States::Finished);
      m_dl = {};
      return;
    }
  }

  void stop() override
  {
    if (m_dl) {
      setState(States::Stopping);
      m_dl->stop();
    } else {
      setState(States::Finished);
    }
  }

  double progress() const override
  {
    if (m_dl) {
      return m_dl->stats().progress;
    } else {
      return -1;
    }
  }

private:
  const QString m_what;
  IDownloader* m_downloader;
  std::shared_ptr<MOBase::IDownload> m_dl;
  bool m_first;
};


class DefaultRepository : public IPluginRepository
{
public:
  enum States
  {
    RealFile = 1
  };


  bool init(MOBase::IOrganizer*) override
  {
    return true;
  }

  QString name() const override
  {
    return "Default Repository";
  }

  QString localizedName() const override
  {
    return QObject::tr("Default Repository");
  }

  QString author() const override
  {
    return "The Mod Organizer Team";
  }

  QString description() const override
  {
    return "";
  }

  MOBase::VersionInfo version() const override
  {
    return VersionInfo(1, 0, 0, VersionInfo::RELEASE_FINAL);
  }

  QList<PluginSetting> settings() const override
  {
    return {};
  }

  bool canHandleDownload(const QString& what) const override
  {
    return true;
  }

  QString downloadFilename(const QString& what) const override
  {
    return QUrl(what).fileName();
  }

  std::unique_ptr<IRepositoryDownload> download(
    const QString& what, IDownloader* d) override
  {
    return std::make_unique<DefaultDownload>(what, d);
  }
};

static DefaultRepository g_defaultRepo;

Download::Download(DownloadManager2& dm, IPluginRepository& repo, QString what)
  : m_dm(dm), m_repo(repo), m_what(std::move(what)), m_state(None)
{
}

const QString& Download::what() const
{
  return m_what;
}

Download::States Download::state() const
{
  return m_state;
}

void Download::setState(States s)
{
  if (m_state != s) {
    dmLog().debug(
      "{} state changed from {} to {}",
      debugName(), toString(m_state), toString(s));

    m_state = s;
  }
}

bool Download::start()
{
  setState(Running);
  m_download = m_repo.download(m_what, &m_dm.downloader());
  return (m_download.get() != nullptr);
}

void Download::cancel()
{
  if (m_download) {
    m_download->stop();
    setState(Cancelling);
  } else {
    setState(Cancelled);
  }
}

void Download::pause()
{
  if (m_download) {
    m_download->stop();
    setState(Pausing);
  } else {
    setState(Paused);
  }
}

void Download::queue()
{
  if (m_download) {
    m_download->stop();
    setState(Queueing);
  } else {
    setState(Queued);
  }
}

void Download::tick()
{
  using S = IRepositoryDownload::States;

  if (!m_download) {
    return;
  }

  m_download->tick();

  switch (m_state)
  {
    case Cancelling:
    {
      if (m_download->state() == S::Finished) {
        setState(Cancelled);
        m_download = {};
      }

      break;
    }

    case Pausing:
    {
      if (m_download->state() == S::Finished) {
        setState(Paused);
        m_download = {};
      }

      break;
    }

    case Queueing:
    {
      if (m_download->state() == S::Finished) {
        setState(Queued);
        m_download = {};
      }

      break;
    }

    default:
    {
      if (m_download->state() == S::Errored) {
        const auto e = m_download->error();

        setState(Errored);
        m_error = QString("%1").arg(e);
        m_download = {};
      }
      else if (m_download->state() == S::Finished) {
        setState(Finished);
      } else {
        setState(Running);
        std::cout << m_download->progress() << "\n";
      }
    }
  }
}

QString Download::debugName() const
{
  return QUrl(m_what).fileName();
}


DownloadManager2::DownloadManager2(PluginContainer& pc) :
  m_stop(false), m_pc(pc), m_maxActive(NoLimit), m_maxSpeed(NoLimit),
  m_hasActive(false), m_downloader(new curl::Downloader)
{
  m_thread = std::thread([&]{ run(); });
}

DownloadManager2::~DownloadManager2()
{
  m_stop = true;
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void DownloadManager2::maxActive(std::size_t n)
{
  const std::size_t old = m_maxActive;

  if (n == 0) {
    n = NoLimit;
  }

  if (n != old) {
    dmLog().debug(
      "changed maxActive from {} to {}",
      (old == NoLimit ? "none" : std::to_string(old)),
      (n == NoLimit ? "none" : std::to_string(n)));

    m_maxActive = (n == 0 ? NoLimit : n);
    m_cv.notify_one();
  }
}

void DownloadManager2::maxSpeed(std::size_t bytesPerSecond)
{
  m_downloader->maxSpeed(bytesPerSecond);
}

void DownloadManager2::add(QString what)
{
  dmLog().debug("adding {}", what);

  IPluginRepository* repo = &g_defaultRepo;

  for (auto* p : m_pc.plugins<IPluginRepository>()) {
    if (p->canHandleDownload(what)) {
      repo = p;
      break;
    }
  }

  {
    std::scoped_lock lock(m_tempMutex);
    m_temp.push_back(std::make_shared<Download>(*this, *repo, std::move(what)));
  }
}

bool DownloadManager2::hasActive() const
{
  return m_hasActive;
}

curl::Downloader& DownloadManager2::downloader()
{
  return *m_downloader;
}

void DownloadManager2::run()
{
  try {
    while (!m_stop) {
      checkTemp();
      checkQueue();

      for (auto&& d : m_active) {
        d->tick();
      }

      ::Sleep(100);
    }
  } catch (...) {
    dmLog().error("uncaught exception in download manager thread");
  }
}

void DownloadManager2::checkTemp()
{
  std::vector<std::shared_ptr<Download>> temp;

  {
    std::scoped_lock lock(m_tempMutex);
    temp = std::move(m_temp);
    m_temp.clear();
  }

  if (!temp.empty()) {
    dmLog().debug("{} new downloads", temp.size());

    for (auto& d : temp) {
      d->queue();
      m_queued.push_back(std::move(d));
    }
  }
}

void DownloadManager2::checkQueue()
{
  const std::size_t max = m_maxActive;

  cleanupActive();
  stopOverMax(max);
  addFromQueue(max);

  m_hasActive = !m_active.empty();
}

void DownloadManager2::cleanupActive()
{
  auto itor = m_active.begin();

  while (itor != m_active.end()) {
    auto d = *itor;

    switch (d->state())
    {
      case Download::Queued:
      {
        dmLog().debug("{} queued, moving to queue", d->debugName());
        itor = m_active.erase(itor);
        m_queued.push_front(std::move(d));
        break;
      }

      case Download::Finished:
      case Download::Errored:
      case Download::Paused:
      case Download::Cancelled:
      case Download::None:
      {
        dmLog().debug("{} finished, moving to inactive", d->debugName());
        itor = m_active.erase(itor);
        m_inactive.push_back(std::move(d));
        break;
      }

      case Download::Queueing:
      case Download::Running:
      case Download::Pausing:
      case Download::Cancelling:
      default:
      {
        ++itor;
        break;
      }
    }
  }
}

void DownloadManager2::stopOverMax(std::size_t max)
{
  std::size_t runningCount = 0;

  for (auto&& d : m_active) {
    if (d->state() == Download::Running) {
      ++runningCount;
    }
  }

  if (runningCount > max) {
    dmLog().debug("running count {} over {}, stopping", runningCount, max);

    for (auto itor=m_active.rbegin(); itor!=m_active.rend(); ++itor) {
      auto d = *itor;
      if (d->state() == Download::Running) {
        dmLog().debug("stopping {}", d->debugName());
        d->queue();

        --runningCount;
        if (runningCount <= max) {
          break;
        }
      }
    }
  }
}

void DownloadManager2::addFromQueue(std::size_t max)
{
  auto q = m_queued.begin();

  while (m_active.size() < max && q != m_queued.end()) {
    std::shared_ptr<Download> d = *q;
    dmLog().debug("activating {}", d->debugName());

    if (d->start()) {
      q = m_queued.erase(q);
      m_active.push_back(d);
    } else {
      dmLog().debug("failed to activate {}", d->debugName());
      ++q;
    }
  }
}

} // namespace
