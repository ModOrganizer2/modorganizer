#include "downloadmanager2.h"
#include <log.h>
#include <utility.h>

namespace dm
{

using namespace MOBase;

class CurlException {};
class Cancelled {};

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
    //log::debug("curl: {}", trim(data));
  } else {
    //log::debug("curl: {}", ts);
  }
}


class CurlGlobalHandle
{
public:
  CurlGlobalHandle()
  {
    log::debug("curl: global init");
    const auto r = curl_global_init(CURL_GLOBAL_ALL);

    if (r != 0) {
      log::error("curl: failed to initialize, {}", curlError(r));
      throw CurlException();
    }
  }

  ~CurlGlobalHandle()
  {
    log::debug("curl: global cleanup");
    curl_global_cleanup();
  }

  CurlGlobalHandle(const CurlGlobalHandle&) = delete;
  CurlGlobalHandle& operator=(const CurlGlobalHandle) = delete;
};


CurlDownloader::EasyHandle::EasyHandle(defer_t)
  : m_handle(nullptr)
{
}

CurlDownloader::EasyHandle::EasyHandle()
  : EasyHandle(defer)
{
  if (!create()) {
    throw CurlException();
  }
}

bool CurlDownloader::EasyHandle::create()
{
  if (m_handle) {
    curl_easy_cleanup(m_handle);
    m_handle = nullptr;
  }

  m_handle = curl_easy_init();

  if (!m_handle) {
    log::error("curl: failed to create easy handle");
    return false;
  }

  log::debug("curl: new easy handle {}", (void*)m_handle);

  return true;
}

CurlDownloader::EasyHandle::~EasyHandle()
{
  if (m_handle) {
    curl_easy_cleanup(m_handle);
    log::debug("curl: cleanup easy handle {}", (void*)m_handle);
  }
}

CURL* CurlDownloader::EasyHandle::get() const
{
  return m_handle;
}


CurlDownloader::MultiHandle::MultiHandle(defer_t)
  : m_handle(nullptr)
{
}

CurlDownloader::MultiHandle::MultiHandle()
  : MultiHandle(defer)
{
  if (!create()) {
    throw CurlException();
  }
}

bool CurlDownloader::MultiHandle::create()
{
  m_handle = curl_multi_init();

  if (!m_handle) {
    log::error("curl: failed to create multi handle");
    return false;
  }

  log::debug("curl: new multi handle {}", (void*)m_handle);

  return true;
}

CurlDownloader::MultiHandle::~MultiHandle()
{
  if (m_handle) {
    const auto r= curl_multi_cleanup(m_handle);

    if (r != CURLM_OK) {
      log::error("curl: failed to cleanup multi {}, {}", (void*)m_handle, curlError(r));
    } else {
      log::debug("curl: cleanup multi handle {}", (void*)m_handle);
    }
  }
}

CURLM* CurlDownloader::MultiHandle::get() const
{
  return m_handle;
}


CurlDownloader::FileHandle::FileHandle()
  : m_handle(INVALID_HANDLE_VALUE)
{
}

CurlDownloader::FileHandle::~FileHandle()
{
  close();
}

bool CurlDownloader::FileHandle::opened() const
{
  return (m_handle != INVALID_HANDLE_VALUE);
}

std::size_t CurlDownloader::FileHandle::open(fs::path p, bool append)
{
  log::debug("curl: opening {}", p);

  m_path = std::move(p);

  if (!doOpen(append)) {
    return -1;
  }

  if (append) {
    LARGE_INTEGER pos = {0};
    LARGE_INTEGER newPos = {};

    if (!SetFilePointerEx(m_handle, pos, &newPos, FILE_END)) {
      const auto e = GetLastError();

      log::error(
        "curl: failed to set file pointer for {}, {}, reopening",
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

bool CurlDownloader::FileHandle::doOpen(bool append)
{
  const DWORD flags = (append ? OPEN_ALWAYS : CREATE_ALWAYS);

  m_handle = CreateFileW(
    m_path.native().c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
    flags, FILE_ATTRIBUTE_NORMAL, 0);

  if (m_handle == INVALID_HANDLE_VALUE) {
    const auto e = GetLastError();
    log::error("curl: failed to open {}, {}", m_path, formatSystemMessage(e));
    return false;
  }

  return true;
}

void CurlDownloader::FileHandle::close()
{
  if (m_handle != INVALID_HANDLE_VALUE) {
    log::debug("curl: closing handle {} for {}", (void*)m_handle, m_path);
    CloseHandle(m_handle);
    m_handle = INVALID_HANDLE_VALUE;
  }
}

bool CurlDownloader::FileHandle::write(std::string_view sv)
{
  DWORD written = 0;

  if (!WriteFile(m_handle, sv.data(), sv.size(), &written, nullptr)) {
    const auto e = GetLastError();
    log::error("curl: failed to write to {}, {}", m_path, formatSystemMessage(e));
    return false;
  }

  if (written != sv.size()) {
    log::error("curl: wrote {}/{} bytes, failure", written, sv.size());
    return false;
  }

  return true;
}


static std::mutex g_curlHandleMutex;
static std::weak_ptr<CurlGlobalHandle> g_curlHandle;
const CurlDownloader::defer_t CurlDownloader::defer;

std::shared_ptr<CurlGlobalHandle> curlGlobal()
{
  std::scoped_lock lock(g_curlHandleMutex);

  if (auto sp=g_curlHandle.lock()) {
    return sp;
  }

  std::shared_ptr<CurlGlobalHandle> sp(new CurlGlobalHandle);
  g_curlHandle = sp;

  return sp;
}


fs::path unfinished(const fs::path& p)
{
  return p.parent_path() / (p.filename().native() + L".unfinished");
}


CurlDownloader::Download::Download(std::string url, fs::path file) :
  m_url(std::move(url)), m_file(std::move(file)), m_handle(defer),
  m_state(Stopped), m_bytes(0), m_bytesPerSecond(0)
{
}

CURL* CurlDownloader::Download::setup()
{
  if (!m_handle.create()) {
    log::error("curl: easy init failed for {}", debug_name());
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
    log::debug("curl: {} resume from {}", debug_name(), rf);
    curl_easy_setopt(h, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)rf);
  }

  return h;
}

CURL* CurlDownloader::Download::handle() const
{
  return m_handle.get();
}

CurlDownloader::Download::States CurlDownloader::Download::state() const
{
  return m_state;
}

std::string CurlDownloader::Download::debug_name() const
{
  return m_file.filename().string();
}

std::size_t CurlDownloader::Download::resumeFrom()
{
  const auto p = unfinished(m_file);

  if (fs::exists(p)) {
    const auto s = m_out.open(p, true);

    if (s == -1) {
      log::error("curl: failed to open {}, no resume", m_file);
      return 0;
    }

    return s;
  }

  return 0;
}

bool CurlDownloader::Download::xfer(
  curl_off_t dltotal, curl_off_t dlnow,
  curl_off_t ultotal, curl_off_t ulnow)
{
  return (m_state == Running);
}

bool CurlDownloader::Download::header(std::string_view sv)
{
  //log::debug("curl: header {} bytes '{}'", sv.size(), sv);
  return (m_state == Running);
}

bool CurlDownloader::Download::write(std::string_view data)
{
  using namespace std::chrono;

  //log::debug("curl: write {} bytes", data.size());

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

void CurlDownloader::Download::start()
{
  m_state = Running;
}

void CurlDownloader::Download::stop()
{
  m_state = Stopping;
}

bool CurlDownloader::Download::finish()
{
  bool b = true;

  if (m_out.opened() && m_state == Running) {
    b = rename();
  }

  m_state = Stopped;

  return b;
}

bool CurlDownloader::Download::rename()
{
  m_out.close();

  const auto from = unfinished(m_file);

  if (fs::exists(m_file)) {
    log::error("curl: can't rename {} to {}, already exists", from, m_file);
    return false;
  }

  std::error_code ec;
  fs::rename(from, m_file, ec);

  if (ec) {
    log::error(
      "curl: failed to rename {} to {}, {}",
      from, m_file, ec.message());

    return false;
  }

  return true;
}

void CurlDownloader::Download::debug(curl_infotype t, std::string_view data)
{
  curlDebug(t, data);
}


CurlDownloader::CurlDownloader() :
  m_global(curlGlobal()),
  m_cancel(false), m_stop(false), m_finished(false),
  m_maxActive(NoMaxActive), m_maxSpeed(0)
{
  m_thread = std::thread([&]{ run(); });
}

CurlDownloader::~CurlDownloader()
{
  cancel();
  join();
}

void CurlDownloader::cancel()
{
  log::debug("curl: cancelling");
  m_cancel = true;
  curl_multi_wakeup(m_handle.get());
  m_cv.notify_one();
}

void CurlDownloader::stop()
{
  log::debug("curl: will stop");
  m_stop = true;
  m_cv.notify_one();
}

void CurlDownloader::join()
{
  log::debug("curl: joining");
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void CurlDownloader::maxActive(std::size_t n)
{
  if (n != m_maxActive) {
    const std::size_t old = m_maxActive;
    m_maxActive = (n == 0 ? NoMaxActive : n);

    log::debug(
      "curl: changed maxActive from {} to {}",
      (old == NoMaxActive ? "none" : std::to_string(old)),
      (n == NoMaxActive ? "none" : std::to_string(n)));
  }

  m_cv.notify_one();
}

void CurlDownloader::maxSpeed(std::size_t s)
{
  if (s != m_maxSpeed) {
    log::debug("curl: changed maxSpeed from {} to {}", m_maxSpeed, s);
    m_maxSpeed = s;
    m_cv.notify_one();
  }
}

bool CurlDownloader::finished() const
{
  return m_finished;
}

std::shared_ptr<CurlDownloader::Download> CurlDownloader::add(
  std::string url, fs::path file)
{
  auto d = std::make_shared<Download>(std::move(url), std::move(file));

  {
    std::scoped_lock lock(m_tempMutex);
    m_temp.push_back(d);
    m_cv.notify_one();
  }

  return d;
}

void CurlDownloader::run()
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
          log::debug("curl: finished, must stop, breaking");
          break;
        } else {
          log::debug("curl: nothing to do, sleeping");

          std::unique_lock lock(m_tempMutex);
          m_cv.wait(lock);

          log::debug("curl: woke up");
        }
      } else if (!m_active.empty()) {
        poll();
      }
    }
  } catch (Cancelled&) {
  }

  m_finished = true;
}

void CurlDownloader::checkCancel()
{
  if (m_cancel) {
    log::debug("curl: cancel, breaking");
    throw Cancelled();
  }
}

void CurlDownloader::checkTemp()
{
  std::vector<std::shared_ptr<Download>> temp;

  {
    std::scoped_lock lock(m_tempMutex);
    temp = std::move(m_temp);
    m_temp.clear();
  }

  if (!temp.empty()) {
    log::debug("curl: {} new downloads", temp.size());

    m_queued.insert(
      m_queued.end(),
      std::make_move_iterator(temp.begin()),
      std::make_move_iterator(temp.end()));
  }
}

void CurlDownloader::perform()
{
  int running = 0;
  auto mr = curl_multi_perform(m_handle.get(), &running);

  if (mr != CURLM_OK) {
    log::error("curl: multi perform failed, {}", curlError(mr));
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
        log::error("curl: {} finished, but not in list", (void*)msg->easy_handle);
      } else {
        log::debug("curl: finished {}", (*itor->second)->debug_name());
      }

      mr = curl_multi_remove_handle(m_handle.get(), msg->easy_handle);
      if (mr != CURLM_OK) {
        log::error("curl: failed to remove easy handle, {}", curlError(mr));
      }

      if (itor != m_activeMap.end()) {
        const auto stopping = ((*itor->second)->state() == Download::Stopping);

        (*itor->second)->finish();

        if (!stopping) {
          m_active.erase(itor->second);
          m_activeMap.erase(itor);
        }
      }
    }
  }
}

void CurlDownloader::poll()
{
  const auto mr = curl_multi_poll(m_handle.get(), nullptr, 0, 1000, nullptr);

  if (mr != CURLM_OK) {
    log::error("curl: multi wait failed, {}, sleeping", curlError(mr));
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void CurlDownloader::checkQueue()
{
  const std::size_t max = m_maxActive;
  bool changed = false;

  if (moveStoppedToQueue()) {
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

bool CurlDownloader::moveStoppedToQueue()
{
  bool changed = false;
  auto itor = m_active.begin();

  while (itor != m_active.end()) {
    auto d = *itor;

    if (d->state() == Download::Stopped) {
      log::debug("curl: {} stopped, moving to queue", d->debug_name());

      auto mitor = m_activeMap.find(d->handle());
      if (mitor == m_activeMap.end()) {
        log::error("curl: {} not found in active map", d->debug_name());
      } else {
        m_activeMap.erase(mitor);
      }

      itor = m_active.erase(itor);
      m_queued.push_back(std::move(d));

      changed = true;
    } else {
      ++itor;
    }
  }

  return changed;
}

void CurlDownloader::stopOverMax(std::size_t max)
{
  std::size_t runningCount = 0;

  for (auto&& d : m_active) {
    if (d->state() == Download::Running) {
      ++runningCount;
    }
  }

  if (runningCount > max) {
    log::debug("curl: running count {} over {}, stopping", runningCount, max);

    for (auto itor=m_active.rbegin(); itor!=m_active.rend(); ++itor) {
      auto d = *itor;
      if (d->state() == Download::Running) {
        log::debug("curl: stopping {}", d->debug_name());
        d->stop();

        --runningCount;
        if (runningCount <= max) {
          break;
        }
      }
    }
  }
}

bool CurlDownloader::addFromQueue(std::size_t max)
{
  bool changed = false;
  auto q = m_queued.begin();

  while (m_active.size() < max && q != m_queued.end()) {
    std::shared_ptr<Download> d = *q;
    log::debug("curl: activating {}", d->debug_name());

    if (start(d)) {
      q = m_queued.erase(q);
      m_active.push_back(d);
      m_activeMap.emplace(d->handle(), std::prev(m_active.end()));
      changed = true;
    } else {
      log::debug("curl: failed to active {}", d->debug_name());
      ++q;
    }
  }

  return changed;
}

void CurlDownloader::setLimits()
{
  if (m_active.empty()) {
    return;
  }

  const std::size_t maxSpeed = m_maxSpeed;
  const curl_off_t maxPer = maxSpeed / m_active.size();

  log::debug(
    "curl: setting limits max={} count={} maxPer={}",
    maxSpeed, m_active.size(), maxPer);

  for (auto&& d : m_active) {
    curl_easy_setopt(d->handle(), CURLOPT_MAX_RECV_SPEED_LARGE, maxPer);
  }
}

bool CurlDownloader::start(std::shared_ptr<Download> d)
{
  auto* h = d->setup();
  if (!h) {
    return false;
  }

  log::debug("curl: adding {} to multi handle", d->debug_name());

  const auto r = curl_multi_add_handle(m_handle.get(), h);
  if (r != CURLM_OK) {
    log::error("curl: failed to add easy handle, {}", curlError(r));
    return false;
  }

  d->start();

  return true;
}

int CurlDownloader::s_xfer(
  void* p,
  curl_off_t dltotal, curl_off_t dlnow,
  curl_off_t ultotal, curl_off_t ulnow)
{
  const auto b = static_cast<Download*>(p)->xfer(
    dltotal, dlnow, ultotal, ulnow);

  return (b ? 0 : -1);
}

size_t CurlDownloader::s_header(char* data, size_t size, size_t n, void* p)
{
  const auto b = static_cast<Download*>(p)->header(
    trim(std::string_view((const char*)data, size * n)));

  return (b ? size * n : -1);
}

size_t CurlDownloader::s_write(char* data, size_t size, size_t n, void* p)
{
  const auto b = static_cast<Download*>(p)->write(
    std::string_view(data, size * n));

  return (b ? size * n : -1);
}

int CurlDownloader::s_debug(
  CURL* h, curl_infotype type, char* data, size_t n, void *p)
{
  static_cast<Download*>(p)->debug(type, std::string_view(data, n));
  return 0;
}

} // namespace
