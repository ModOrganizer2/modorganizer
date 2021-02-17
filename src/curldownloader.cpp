#include "curldownloader.h"
#include <log.h>
#include <utility.h>

namespace dm::curl
{

using namespace MOBase;

class CurlException {};
class Cancelled {};

static std::mutex g_curlHandleMutex;
static std::weak_ptr<GlobalHandle> g_curlHandle;
const defer_t defer;


class LogWrapper
{
public:
  LogWrapper(log::Logger& lg, std::string prefix, bool enabled=true)
    : m_lg(lg), m_enabled(enabled), m_prefix(std::move(prefix))
  {
  }

  void enabled(bool b)
  {
    m_enabled = b;
  }

  bool enabled() const
  {
    return m_enabled;
  }

  template <class F, class... Args>
  void debug(F&& format, Args&&... args) noexcept
  {
    log(log::Debug, std::forward<F>(format), std::forward<Args>(args)...);
  }

  template <class F, class... Args>
  void info(F&& format, Args&&... args) noexcept
  {
    log(log::Info, std::forward<F>(format), std::forward<Args>(args)...);
  }

  template <class F, class... Args>
  void warn(F&& format, Args&&... args) noexcept
  {
    log(log::Warning, std::forward<F>(format), std::forward<Args>(args)...);
  }

  template <class F, class... Args>
  void error(F&& format, Args&&... args) noexcept
  {
    log(log::Error, std::forward<F>(format), std::forward<Args>(args)...);
  }

  template <class F, class... Args>
  void log(log::Levels lv, F&& format, Args&&... args) noexcept
  {
    if (!m_enabled) {
      return;
    }

    m_lg.log(lv, m_prefix + std::forward<F>(format), std::forward<Args>(args)...);
  }

private:
  log::Logger& m_lg;
  bool m_enabled;
  std::string m_prefix;
};


LogWrapper& curlLog()
{
  static LogWrapper lg(log::getDefault(), "curl: ");
  return lg;
}

LogWrapper& curlLogV()
{
  static LogWrapper lg(log::getDefault(), "curl: ", false);
  return lg;
}


std::shared_ptr<GlobalHandle> curlGlobal()
{
  std::scoped_lock lock(g_curlHandleMutex);

  if (auto sp=g_curlHandle.lock()) {
    return sp;
  }

  std::shared_ptr<GlobalHandle> sp(new GlobalHandle);
  g_curlHandle = sp;

  return sp;
}

fs::path unfinished(const fs::path& p)
{
  return p.parent_path() / (p.filename().native() + L".unfinished");
}

std::string curlError(CURLcode c)
{
  return curl_easy_strerror(c);
}

std::string curlError(CURLMcode c)
{
  return curl_multi_strerror(c);
}

std::string_view trim(std::string_view sv)
{
  while (!sv.empty() && std::isspace(sv.front()))
    sv = sv.substr(1);

  while (!sv.empty() && std::isspace(sv.back()))
    sv = sv.substr(0, sv.size() - 1);

  return sv;
}

void curlDebug(curl_infotype type, std::string_view data)
{
  // return early, this function is called a lot by curl
  if (!curlLogV().enabled()) {
    return;
  }

  std::string_view ts;

  switch (type)
  {
    case CURLINFO_TEXT:
      break;

    case CURLINFO_HEADER_OUT:
      ts = "send header";
      break;

    case CURLINFO_DATA_OUT:
      ts = "send data";
      break;

    case CURLINFO_SSL_DATA_OUT:
      ts = "send ssl data";
      break;

    case CURLINFO_HEADER_IN:
      ts = "recv header";
      break;

    case CURLINFO_DATA_IN:
      ts = "recv data";
      break;

    case CURLINFO_SSL_DATA_IN:
      ts = "rcv ssl data";
      break;

    default:
      ts = "?";
      break;
  }

  if (type == CURLINFO_TEXT) {
    curlLogV().debug("{}", trim(data));
  } else {
    curlLogV().debug("{}", ts);
  }
}


GlobalHandle::GlobalHandle()
{
  const auto* v = curl_version_info(CURLVERSION_NOW);

  if (v) {
    curlLog().debug(
      "{} ssl {} zlib {}",
      v->version, v->ssl_version, v->libz_version);
  } else {
    curlLog().debug("no version");
  }

  curlLog().debug("global init");
  const auto r = curl_global_init(CURL_GLOBAL_ALL);

  if (r != 0) {
    curlLog().error("failed to initialize, {}", curlError(r));
    throw CurlException();
  }
}

GlobalHandle::~GlobalHandle()
{
  curlLogV().debug("global cleanup");
  curl_global_cleanup();
}


EasyHandle::EasyHandle(defer_t)
  : m_handle(nullptr)
{
}

EasyHandle::EasyHandle()
  : EasyHandle(defer)
{
  if (!create()) {
    throw CurlException();
  }
}

bool EasyHandle::create()
{
  if (m_handle) {
    curl_easy_cleanup(m_handle);
    m_handle = nullptr;
  }

  m_handle = curl_easy_init();

  if (!m_handle) {
    curlLog().error("failed to create easy handle");
    return false;
  }

  curlLogV().debug("new easy handle {}", (void*)m_handle);

  return true;
}

EasyHandle::~EasyHandle()
{
  if (m_handle) {
    curl_easy_cleanup(m_handle);
    curlLogV().debug("cleanup easy handle {}", (void*)m_handle);
  }
}

CURL* EasyHandle::get() const
{
  return m_handle;
}


MultiHandle::MultiHandle(defer_t)
  : m_handle(nullptr)
{
}

MultiHandle::MultiHandle()
  : MultiHandle(defer)
{
  if (!create()) {
    throw CurlException();
  }
}

bool MultiHandle::create()
{
  m_handle = curl_multi_init();

  if (!m_handle) {
    curlLog().error("failed to create multi handle");
    return false;
  }

  curlLogV().debug("new multi handle {}", (void*)m_handle);

  return true;
}

MultiHandle::~MultiHandle()
{
  if (m_handle) {
    const auto r= curl_multi_cleanup(m_handle);

    if (r != CURLM_OK) {
      curlLog().error("failed to cleanup multi {}, {}", (void*)m_handle, curlError(r));
    } else {
      curlLogV().debug("cleanup multi handle {}", (void*)m_handle);
    }
  }
}

CURLM* MultiHandle::get() const
{
  return m_handle;
}


FileHandle::FileHandle()
  : m_handle(INVALID_HANDLE_VALUE)
{
}

FileHandle::~FileHandle()
{
  close();
}

bool FileHandle::opened() const
{
  return (m_handle != INVALID_HANDLE_VALUE);
}

std::size_t FileHandle::open(fs::path p, bool append)
{
  curlLog().debug("opening {}", p);

  m_path = std::move(p);

  if (!doOpen(append)) {
    return -1;
  }

  if (append) {
    LARGE_INTEGER pos = {0};
    LARGE_INTEGER newPos = {};

    if (!SetFilePointerEx(m_handle, pos, &newPos, FILE_END)) {
      const auto e = GetLastError();

      curlLog().error(
        "failed to set file pointer for {}, {}, reopening",
        m_path, formatSystemMessage(e));

      CloseHandle(m_handle);
      m_handle = INVALID_HANDLE_VALUE;

      if (!doOpen(false)) {
        return -1;
      }

      return 0;
    }

    return newPos.QuadPart;
  }

  return 0;
}

bool FileHandle::doOpen(bool append)
{
  const DWORD flags = (append ? OPEN_ALWAYS : CREATE_ALWAYS);

  m_handle = CreateFileW(
    m_path.native().c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
    flags, FILE_ATTRIBUTE_NORMAL, 0);

  if (m_handle == INVALID_HANDLE_VALUE) {
    const auto e = GetLastError();
    curlLog().error("failed to open {}, {}", m_path, formatSystemMessage(e));
    return false;
  }

  return true;
}

void FileHandle::close()
{
  if (m_handle != INVALID_HANDLE_VALUE) {
    curlLogV().debug("closing handle {} for {}", (void*)m_handle, m_path);
    CloseHandle(m_handle);
    m_handle = INVALID_HANDLE_VALUE;
  }
}

bool FileHandle::write(std::string_view sv)
{
  DWORD written = 0;

  if (!WriteFile(m_handle, sv.data(), sv.size(), &written, nullptr)) {
    const auto e = GetLastError();
    curlLog().error("failed to write to {}, {}", m_path, formatSystemMessage(e));
    return false;
  }

  if (written != sv.size()) {
    curlLog().error("wrote {}/{} bytes, failure", written, sv.size());
    return false;
  }

  return true;
}


Download::Download(std::string url, fs::path file) :
  m_url(std::move(url)), m_file(std::move(file)), m_handle(defer),
  m_state(Stopped), m_bytes(0), m_bytesPerSecond(0)
{
}

CURL* Download::setup()
{
  if (!m_handle.create()) {
    curlLog().error("easy init failed for {}", debug_name());
    return nullptr;
  }

  auto* h = m_handle.get();

  curl_easy_setopt(h, CURLOPT_URL, m_url.c_str());
  curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0);
  curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, &s_xfer);
  curl_easy_setopt(h, CURLOPT_XFERINFODATA, this);
  curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, &s_header);
  curl_easy_setopt(h, CURLOPT_HEADERDATA, this);
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &s_write);
  curl_easy_setopt(h, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1);

  curl_easy_setopt(h, CURLOPT_VERBOSE, 1);
  curl_easy_setopt(h, CURLOPT_DEBUGFUNCTION, &s_debug);

  const auto rf = resumeFrom();
  if (rf > 0) {
    curlLog().debug("{} resume from {}", debug_name(), rf);
    curl_easy_setopt(h, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)rf);
  }

  return h;
}

CURL* Download::handle() const
{
  return m_handle.get();
}

Download::States Download::state() const
{
  return m_state;
}

std::string Download::debug_name() const
{
  return m_file.filename().string();
}

std::size_t Download::resumeFrom()
{
  const auto p = unfinished(m_file);

  if (fs::exists(p)) {
    const auto s = m_out.open(p, true);

    if (s == -1) {
      curlLog().error("failed to open {}, no resume", m_file);
      return 0;
    }

    return s;
  }

  return 0;
}

bool Download::xfer(
  curl_off_t dltotal, curl_off_t dlnow,
  curl_off_t ultotal, curl_off_t ulnow)
{
  return (m_state == Running);
}

bool Download::header(std::string_view sv)
{
  //curlLog().debug("header {} bytes '{}'", sv.size(), sv);
  return (m_state == Running);
}

bool Download::write(std::string_view data)
{
  using namespace std::chrono;

  //curlLog().debug("write {} bytes", data.size());

  if (!m_out.opened()) {
    if (m_out.open(unfinished(m_file), false) == -1) {
      return false;
    }
  }

  if (!m_out.write(data)) {
    return false;
  }


  m_bytes += data.size();

  const auto now = hr_clock::now();

  if (m_lastCheck == hr_clock::time_point()) {
    m_lastCheck = now;
  } else {
    const auto d = (now - m_lastCheck);
    if (duration_cast<seconds>(d) >= seconds(1)) {
      const double s = duration_cast<milliseconds>(d).count() / 1000.0;
      m_bytesPerSecond = m_bytes / s;
      m_bytes = 0;
      m_lastCheck = now;
    }
  }

  return (m_state == Running);
}

void Download::start()
{
  m_state = Running;
}

void Download::stop()
{
  m_state = Stopping;
}

bool Download::finish()
{
  bool b = true;

  if (m_out.opened() && m_state == Running) {
    b = rename();
  } else {
    m_out.close();
  }

  if (m_state == Running) {
    m_state = Finished;
  } else {
    m_state = Stopped;
  }

  return b;
}

bool Download::rename()
{
  m_out.close();

  const auto from = unfinished(m_file);

  if (fs::exists(m_file)) {
    curlLog().error("can't rename {} to {}, already exists", from, m_file);
    return false;
  }

  std::error_code ec;
  fs::rename(from, m_file, ec);

  if (ec) {
    curlLog().error(
      "failed to rename {} to {}, {}",
      from, m_file, ec.message());

    return false;
  }

  return true;
}

void Download::debug(curl_infotype t, std::string_view data)
{
  curlDebug(t, data);
}

int Download::s_xfer(
  void* p,
  curl_off_t dltotal, curl_off_t dlnow,
  curl_off_t ultotal, curl_off_t ulnow)
{
  const auto b = static_cast<Download*>(p)->xfer(
    dltotal, dlnow, ultotal, ulnow);

  return (b ? 0 : -1);
}

size_t Download::s_header(char* data, size_t size, size_t n, void* p)
{
  const auto b = static_cast<Download*>(p)->header(
    trim(std::string_view((const char*)data, size * n)));

  return (b ? size * n : -1);
}

size_t Download::s_write(char* data, size_t size, size_t n, void* p)
{
  const auto b = static_cast<Download*>(p)->write(
    std::string_view(data, size * n));

  return (b ? size * n : -1);
}

int Download::s_debug(
  CURL* h, curl_infotype type, char* data, size_t n, void *p)
{
  static_cast<Download*>(p)->debug(type, std::string_view(data, n));
  return 0;
}


Downloader::Downloader() :
  m_global(curlGlobal()),
  m_cancel(false), m_stop(false), m_finished(false),
  m_maxActive(NoLimit), m_maxSpeed(NoLimit)
{
  m_thread = std::thread([&]{ run(); });
}

Downloader::~Downloader()
{
  cancel();
  join();
}

void Downloader::cancel()
{
  curlLog().debug("cancelling");
  m_cancel = true;
  curl_multi_wakeup(m_handle.get());
  m_cv.notify_one();
}

void Downloader::stop()
{
  curlLog().debug("will stop");
  m_stop = true;
  m_cv.notify_one();
}

void Downloader::join()
{
  curlLog().debug("joining");
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void Downloader::maxActive(std::size_t n)
{
  if (n != m_maxActive) {
    const std::size_t old = m_maxActive;
    m_maxActive = (n == 0 ? NoLimit : n);

    curlLog().debug(
      "changed maxActive from {} to {}",
      (old == NoLimit ? "none" : std::to_string(old)),
      (n == NoLimit ? "none" : std::to_string(n)));
  }

  m_cv.notify_one();
}

void Downloader::maxSpeed(std::size_t s)
{
  if (s != m_maxSpeed) {
    curlLog().debug("changed maxSpeed from {} to {}", m_maxSpeed, s);
    m_maxSpeed = s;
    m_cv.notify_one();
  }
}

bool Downloader::finished() const
{
  return m_finished;
}

std::shared_ptr<Download> Downloader::add(
  std::string url, fs::path file)
{
  auto d = std::make_shared<Download>(std::move(url), std::move(file));

  curlLog().debug("adding {}", d->debug_name());

  {
    std::scoped_lock lock(m_tempMutex);
    m_temp.push_back(d);
    m_cv.notify_one();
  }

  return d;
}

void Downloader::run()
{
  try {
    std::size_t oldMaxSpeed = m_maxSpeed;

    for (;;) {
      checkCancel();

      checkTemp();
      checkCancel();

      checkQueue();
      checkCancel();

      perform();
      checkCancel();

      {
        const std::size_t newMaxSpeed = m_maxSpeed;
        if (newMaxSpeed != oldMaxSpeed) {
          oldMaxSpeed = newMaxSpeed;
          setLimits();
        }
      }

      if (m_active.empty() && m_queued.empty()) {
        if (m_stop) {
          curlLog().debug("finished, must stop, breaking");
          break;
        } else {
          curlLog().debug("nothing to do, sleeping");

          std::unique_lock lock(m_tempMutex);
          m_cv.wait(lock);

          curlLog().debug("woke up");
        }
      } else if (!m_active.empty()) {
        poll();
      }
    }
  } catch (Cancelled&) {
  }

  m_finished = true;
}

void Downloader::checkCancel()
{
  if (m_cancel) {
    curlLog().debug("cancel, breaking");
    throw Cancelled();
  }
}

void Downloader::checkTemp()
{
  std::vector<std::shared_ptr<Download>> temp;

  {
    std::scoped_lock lock(m_tempMutex);
    temp = std::move(m_temp);
    m_temp.clear();
  }

  if (!temp.empty()) {
    curlLog().debug("{} new downloads", temp.size());

    m_queued.insert(
      m_queued.end(),
      std::make_move_iterator(temp.begin()),
      std::make_move_iterator(temp.end()));
  }
}

void Downloader::perform()
{
  int running = 0;
  auto mr = curl_multi_perform(m_handle.get(), &running);

  if (mr != CURLM_OK) {
    curlLog().error("multi perform failed, {}", curlError(mr));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return;
  }

  for (;;) {
    int msgCount = 0;
    CURLMsg* msg = curl_multi_info_read(m_handle.get(), &msgCount);

    if (!msg) {
      break;
    }

    if (msg->msg == CURLMSG_DONE) {
      bool found = false;

      auto itor = m_activeMap.find(msg->easy_handle);

      if (itor == m_activeMap.end()) {
        curlLog().error("{} finished, but not in list", (void*)msg->easy_handle);
      } else {
        curlLog().debug("finished {}", (*itor->second)->debug_name());
      }

      mr = curl_multi_remove_handle(m_handle.get(), msg->easy_handle);
      if (mr != CURLM_OK) {
        curlLog().error("failed to remove easy handle, {}", curlError(mr));
      }

      if (itor != m_activeMap.end()) {
        (*itor->second)->finish();
      }
    }
  }
}

void Downloader::poll()
{
  const auto mr = curl_multi_poll(m_handle.get(), nullptr, 0, 1000, nullptr);

  if (mr != CURLM_OK) {
    curlLog().error("multi wait failed, {}, sleeping", curlError(mr));
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void Downloader::checkQueue()
{
  const std::size_t max = m_maxActive;
  bool changed = false;

  if (cleanupActive()) {
    changed = true;
  }

  stopOverMax(max);

  if (addFromQueue(max)) {
    changed = true;
  }

  if (changed) {
    setLimits();
  }
}

bool Downloader::cleanupActive()
{
  bool changed = false;
  auto itor = m_active.begin();

  while (itor != m_active.end()) {
    auto d = *itor;

    if (d->state() == Download::Stopped) {
      curlLog().debug("{} stopped, moving to queue", d->debug_name());
      itor = removeFromActive(itor);
      m_queued.push_front(std::move(d));
      changed = true;
    } else if (d->state() == Download::Finished) {
      curlLog().debug("{} finished, removing from list", d->debug_name());
      itor = removeFromActive(itor);
      changed = true;
    } else {
      ++itor;
    }
  }

  return changed;
}

Downloader::DownloadList::iterator Downloader::removeFromActive(
  DownloadList::iterator itor)
{
  auto mitor = m_activeMap.find((*itor)->handle());

  if (mitor == m_activeMap.end()) {
    curlLog().error("{} not found in active map", (*itor)->debug_name());
  } else {
    m_activeMap.erase(mitor);
  }

  return m_active.erase(itor);
}

void Downloader::stopOverMax(std::size_t max)
{
  std::size_t runningCount = 0;

  for (auto&& d : m_active) {
    if (d->state() == Download::Running) {
      ++runningCount;
    }
  }

  if (runningCount > max) {
    curlLog().debug("running count {} over {}, stopping", runningCount, max);

    for (auto itor=m_active.rbegin(); itor!=m_active.rend(); ++itor) {
      auto d = *itor;
      if (d->state() == Download::Running) {
        curlLog().debug("stopping {}", d->debug_name());
        d->stop();

        --runningCount;
        if (runningCount <= max) {
          break;
        }
      }
    }
  }
}

bool Downloader::addFromQueue(std::size_t max)
{
  bool changed = false;
  auto q = m_queued.begin();

  while (m_active.size() < max && q != m_queued.end()) {
    std::shared_ptr<Download> d = *q;
    curlLog().debug("activating {}", d->debug_name());

    if (start(d)) {
      q = m_queued.erase(q);
      m_active.push_back(d);
      m_activeMap.emplace(d->handle(), std::prev(m_active.end()));
      changed = true;
    } else {
      curlLog().debug("failed to active {}", d->debug_name());
      ++q;
    }
  }

  return changed;
}

void Downloader::setLimits()
{
  if (m_active.empty()) {
    return;
  }

  const std::size_t maxSpeed = m_maxSpeed;
  const curl_off_t maxPer = (maxSpeed == NoLimit ? 0 : maxSpeed / m_active.size());

  if (maxSpeed == NoLimit) {
    curlLogV().debug("setting speed limit to unlimited");
  } else {
    curlLogV().debug(
      "setting speed limit to max={} count={} maxPer={}",
      maxSpeed, m_active.size(), maxPer);
  }

  for (auto&& d : m_active) {
    curl_easy_setopt(d->handle(), CURLOPT_MAX_RECV_SPEED_LARGE, maxPer);
  }
}

bool Downloader::start(std::shared_ptr<Download> d)
{
  auto* h = d->setup();
  if (!h) {
    return false;
  }

  curlLogV().debug("adding {} to multi handle", d->debug_name());

  const auto r = curl_multi_add_handle(m_handle.get(), h);
  if (r != CURLM_OK) {
    curlLog().error("failed to add easy handle, {}", curlError(r));
    return false;
  }

  d->start();

  return true;
}

} // namespace
