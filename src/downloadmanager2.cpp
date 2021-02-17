#include "downloadmanager2.h"
#include "curldownloader.h"
#include <log.h>

namespace dm
{

using namespace MOBase;

log::LogWrapper& dmLog()
{
  static log::LogWrapper lg(log::getDefault(), "dm: ");
  return lg;
}


Download::Download(QUrl url)
  : m_url(std::move(url)), m_state(None)
{
  m_file = "c:\\tmp\\dm\\" + m_url.fileName();
}

const QUrl& Download::url() const
{
  return m_url;
}

const QString& Download::file() const
{
  return m_file;
}

Download::States Download::state() const
{
  return m_state;
}

void Download::start(std::shared_ptr<curl::Download> d)
{
  m_state = Running;
  m_download = d;
}

void Download::cancel()
{
  if (m_download) {
    m_download->stop();
    m_state = Cancelling;
  } else {
    m_state = Cancelled;
  }
}

void Download::pause()
{
  if (m_download) {
    m_download->stop();
    m_state = Pausing;
  } else {
    m_state = Paused;
  }
}

void Download::queue()
{
  if (m_download) {
    m_download->stop();
    m_state = Queueing;
  } else {
    m_state = Queued;
  }
}

void Download::tick()
{
  if (m_download) {
    if (m_state == Cancelling) {
      if (m_download->state() == curl::Download::Stopped) {
        m_state = Cancelled;
        m_download = {};
      }
    } else if (m_state == Pausing) {
      if (m_download->state() == curl::Download::Stopped) {
        m_state = Paused;
        m_download = {};
      }
    } else if (m_state == Queueing) {
      if (m_download->state() == curl::Download::Stopped) {
        m_state = Queued;
        m_download = {};
      }
    } else if (m_download->state() == curl::Download::Finished) {
      m_state = Finished;
      m_download = {};
    } else {
      m_state = Running;
    }
  }
}

QString Download::debugName() const
{
  return m_url.fileName();
}


DownloadManager2::DownloadManager2() :
  m_maxActive(NoLimit), m_maxSpeed(NoLimit), m_hasActive(false),
  m_downloader(new curl::Downloader)
{
  m_thread = std::thread([&]{ run(); });
}

DownloadManager2::~DownloadManager2()
{
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

void DownloadManager2::add(const QUrl& url)
{
  dmLog().debug("adding {}", url.toString());

  std::scoped_lock lock(m_tempMutex);
  m_temp.push_back(std::make_shared<Download>(url));
}

bool DownloadManager2::hasActive() const
{
  return m_hasActive;
}

void DownloadManager2::run()
{
  try {
    for (;;) {
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

    if (start(d)) {
      q = m_queued.erase(q);
      m_active.push_back(d);
    } else {
      dmLog().debug("failed to activate {}", d->debugName());
      ++q;
    }
  }
}

bool DownloadManager2::start(std::shared_ptr<Download> d)
{
  d->start(m_downloader->add(
    d->url().toString().toStdString(),
    fs::path(d->file().toStdWString())));

  return true;
}

} // namespace
