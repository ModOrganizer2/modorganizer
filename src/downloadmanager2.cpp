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
  url(std::move(url)), file(std::move(file)), handle(defer),
  bytes(0), bytesPerSecond(0)
{
}

std::size_t CurlDownloader::Download::resumeFrom()
{
  const auto p = unfinished(file);

  if (fs::exists(p)) {
    const auto s = out.open(p, true);

    if (s == -1) {
      log::error("curl: failed to open {}, no resume", file);
      return 0;
    }

    return s;
  }

  return 0;
}

int CurlDownloader::Download::xfer(
  curl_off_t dltotal, curl_off_t dlnow,
  curl_off_t ultotal, curl_off_t ulnow)
{
  return 0;
}

bool CurlDownloader::Download::header(std::string_view sv)
{
  //log::debug("curl: header {} bytes '{}'", sv.size(), sv);
  return true;
}

bool CurlDownloader::Download::write(std::string_view data)
{
  using namespace std::chrono;

  //log::debug("curl: write {} bytes", data.size());

  if (!out.opened()) {
    if (out.open(unfinished(file), false) == -1) {
      return false;
    }
  }

  if (!out.write(data)) {
    return false;
  }


  bytes += data.size();

  const auto now = hr_clock::now();

  if (lastCheck == hr_clock::time_point()) {
    lastCheck = now;
  } else {
    const auto d = (now - lastCheck);
    if (duration_cast<seconds>(d) >= seconds(1)) {
      const double s = duration_cast<milliseconds>(d).count() / 1000.0;
      bytesPerSecond = bytes / s;
      bytes = 0;
      lastCheck = now;
    }
  }

  return true;
}

bool CurlDownloader::Download::finish()
{
  if (!out.opened()) {
    return true;
  }

  out.close();

  const auto from = unfinished(file);
  const auto to = file;

  if (fs::exists(to)) {
    log::error("curl: can't rename {} to {}, already exists", from, to);
    return false;
  }

  std::error_code ec;
  fs::rename(from, to, ec);

  if (ec) {
    log::error("curl: failed to rename {} to {}, {}", from, to, ec.message());
    return false;
  }

  return true;
}

void CurlDownloader::Download::debug(curl_infotype t, std::string_view data)
{
  curlDebug(t, data);
}


CurlDownloader::CurlDownloader() :
  m_global(curlGlobal()), m_queuedCount(0),
  m_cancel(false), m_stop(false), m_finished(false)
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

std::size_t CurlDownloader::queued() const
{
  return m_queuedCount;
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
    for (;;) {
      checkCancel();

      checkTemp();
      checkCancel();

      checkQueue();
      checkCancel();

      perform();
      checkCancel();

      if (m_active.empty() && m_queued.empty()) {
        if (m_stop) {
          log::debug("curl: finished, must stop, breaking");
          break;
        } else {
          log::debug("curl: nothing to do, sleeping");

          std::unique_lock lock(m_tempMutex);
          m_cv.wait(lock, [&]{ return !m_temp.empty() || m_cancel; });

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

    m_queuedCount = m_queued.size();
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

      auto itor = std::find_if(
        m_active.begin(), m_active.end(),
        [&](auto&& d) { return d->handle.get() == msg->easy_handle; });

      if (itor == m_active.end()) {
        log::error("curl: finished, but not in list");
      } else {
        log::debug("curl: finished {}", (*itor)->url);
      }

      mr = curl_multi_remove_handle(m_handle.get(), msg->easy_handle);
      if (mr != CURLM_OK) {
        log::error("curl: failed to remove easy handle, {}", curlError(mr));
      }

      if (itor != m_active.end()) {
        (*itor)->finish();
        m_active.erase(itor);
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
  const std::size_t max = 3;

  if (m_queued.empty()) {
    return;
  }

  std::size_t q = 0;
  bool added = false;

  while (m_active.size() < max && q < m_queued.size()) {
    auto d = m_queued[q];
    log::debug("curl: activating #{} {}", q, d->url);

    if (start(d)) {
      m_queued.erase(m_queued.begin() + q);
      m_queuedCount = m_queued.size();
      m_active.push_back(std::move(d));
      added = true;
    } else {
      log::debug("curl: failed to active #{} {}", q, d->url);
    }
  }

  if (added) {
    setLimits();
  }
}

void CurlDownloader::setLimits()
{
  if (m_active.empty()) {
    return;
  }

  const std::size_t max = 1024 * 500;
  const curl_off_t maxPer = max / m_active.size();

  log::debug(
    "curl: setting limits max={} count={} maxPer={}",
    max, m_active.size(), maxPer);

  for (auto&& d : m_active) {
    curl_easy_setopt(d->handle.get(), CURLOPT_MAX_RECV_SPEED_LARGE, maxPer);
  }
}

bool CurlDownloader::start(std::shared_ptr<Download> d)
{
  if (!d->handle.create()) {
    log::error("curl: easy init failed");
    return false;
  }

  auto* h = d->handle.get();

  curl_easy_setopt(h, CURLOPT_URL, d->url.c_str());
  curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0);
  curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, &s_xfer);
  curl_easy_setopt(h, CURLOPT_XFERINFODATA, d.get());
  curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, &s_header);
  curl_easy_setopt(h, CURLOPT_HEADERDATA, d.get());
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &s_write);
  curl_easy_setopt(h, CURLOPT_WRITEDATA, d.get());
  curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1);

  curl_easy_setopt(h, CURLOPT_VERBOSE, 1);
  curl_easy_setopt(h, CURLOPT_DEBUGFUNCTION, &s_debug);

  const auto rf = d->resumeFrom();
  if (rf > 0) {
    log::debug("curl: {} resume from {}", d->file, rf);
    curl_easy_setopt(h, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)rf);
  }

  log::debug("curl: adding {} to multi handle", d->url);

  const auto r = curl_multi_add_handle(m_handle.get(), h);
  if (r != CURLM_OK) {
    log::error("curl: failed to add easy handle, {}", curlError(r));
    return false;
  }

  return true;
}

int CurlDownloader::s_xfer(
  void* p,
  curl_off_t dltotal, curl_off_t dlnow,
  curl_off_t ultotal, curl_off_t ulnow)
{
  return static_cast<Download*>(p)->xfer(dltotal, dlnow, ultotal, ulnow);
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
