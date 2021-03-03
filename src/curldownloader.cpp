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


log::LogWrapper& curlLog()
{
  static log::LogWrapper lg(log::getDefault(), "curl: ");
  return lg;
}

log::LogWrapper& curlLogV()
{
  static log::LogWrapper lg(log::getDefault(), "curl: ", false);
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


Download::Download(std::string url, Info info) :
  m_url(std::move(url)), m_info(std::move(info)),
  m_handle(defer), m_state(Stopped), m_bytes(0),
  m_bytesPerSecond(0), m_progress(0)
{
}

CURL* Download::setup(curl_off_t maxSpeed)
{
  if (!m_handle.create()) {
    curlLog().error("easy init failed for {}", debugName());
    return nullptr;
  }

  m_buffer = {};

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
  curl_easy_setopt(h, CURLOPT_MAX_RECV_SPEED_LARGE, maxSpeed);

  curl_easy_setopt(h, CURLOPT_VERBOSE, 1);
  curl_easy_setopt(h, CURLOPT_DEBUGFUNCTION, &s_debug);

  if (!m_info.headers.isEmpty()) {
    curl_slist* headers = nullptr;

    for (auto&& h : m_info.headers) {
      const QString s = h.first + ": " + h.second;
      headers = curl_slist_append(headers, s.toStdString().c_str());
    }

    curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);
    m_headers.reset(headers);
  }

  if (!m_info.userAgent.isEmpty()) {
    curl_easy_setopt(h, CURLOPT_USERAGENT, m_info.userAgent.toStdString().c_str());
  }

  const auto rf = resumeFrom();
  if (rf > 0) {
    curlLog().debug("{} resume from {}", debugName(), rf);
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

Download::Stats Download::stats() const
{
  Stats s;

  s.bytesPerSecond = m_bytesPerSecond;
  s.progress = m_progress;

  return s;
}

std::string Download::error() const
{
  return m_error;
}

std::string Download::stealBuffer()
{
  return std::move(m_buffer);
}

QByteArray Download::buffer() const
{
  return QByteArray(m_buffer.data(), m_buffer.size());
}

int Download::httpCode() const
{
  if (m_handle.get() == nullptr) {
    return -1;
  }

  long c = 0;
  curl_easy_getinfo(m_handle.get(), CURLINFO_RESPONSE_CODE, &c);

  return static_cast<int>(c);
}

std::string Download::debugName() const
{
  const auto p = m_url.find_last_of("/");
  if (p == std::string::npos) {
    return m_url;
  }

  const auto p2 = m_url.find_first_of("?#", p);
  if (p2 == std::string::npos) {
    return m_url.substr(p + 1);
  } else {
    return m_url.substr(p + 1, p2 - p - 1);
  }
}

std::size_t Download::resumeFrom()
{
  if (outputFile().empty()) {
    return 0;
  }

  const auto p = unfinished(outputFile());

  if (fs::exists(p)) {
    const auto s = m_out.open(p, true);

    if (s == -1) {
      curlLog().error("failed to open {}, no resume", outputFile());
      return 0;
    }

    return s;
  }

  return 0;
}

fs::path Download::outputFile() const
{
  return m_info.outputFile.toStdString();
}

bool Download::xfer(
  curl_off_t dltotal, curl_off_t dlnow,
  curl_off_t ultotal, curl_off_t ulnow)
{
  double d = -1;

  if (dltotal > 0) {
    d = static_cast<double>(dlnow) / dltotal * 100;
  }

  m_progress = d;

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

  if (outputFile().empty()) {
    m_buffer.append(data.begin(), data.end());
  } else {
    if (!m_out.opened()) {
      if (m_out.open(unfinished(outputFile()), false) == -1) {
        return false;
      }
    }

    if (!m_out.write(data)) {
      return false;
    }
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

bool Download::finish(CURLcode code)
{
  bool b = true;

  if (m_state == Running) {
    if (code == CURLE_OK) {
      if (m_out.opened()) {
        b = rename();
      }

      curlLog().debug("finished {}", debugName());
    } else {
      m_error = curl_easy_strerror(code);
      curlLog().error("{} errored: {}", debugName(), m_error);
    }

    m_state = Finished;
  } else {
    m_state = Stopped;
    m_out.close();
  }

  return b;
}

bool Download::rename()
{
  m_out.close();

  const auto from = unfinished(outputFile());

  if (fs::exists(outputFile())) {
    curlLog().error("can't rename {} to {}, already exists", from, outputFile());
    return false;
  }

  std::error_code ec;
  fs::rename(from, outputFile(), ec);

  if (ec) {
    curlLog().error(
      "failed to rename {} to {}, {}",
      from, outputFile(), ec.message());

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
  m_maxSpeed(NoLimit)
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

void Downloader::maxSpeed(std::size_t s)
{
  const std::size_t old = m_maxSpeed;

  if (s != old) {
    curlLog().debug(
      "changed maxSpeed from {} to {}",
      (old == NoLimit ? "unlimited" : std::to_string(old)),
      (s == NoLimit ? "unlimited" : std::to_string(s)));

    m_maxSpeed = s;
    m_cv.notify_one();
  }
}

bool Downloader::finished() const
{
  return m_finished;
}

std::shared_ptr<MOBase::IDownload> Downloader::add(
  const QUrl& url, const MOBase::IDownload::Info& info)
{
  auto d = std::make_shared<Download>(
    url.toString().toStdString(), std::move(info));

  curlLog().debug("adding {}", d->debugName());

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

      if (m_list.empty()) {
        if (m_stop) {
          curlLog().debug("finished, must stop, breaking");
          break;
        } else {
          curlLog().debug("nothing to do, sleeping");

          std::unique_lock lock(m_tempMutex);
          m_cv.wait(lock);

          curlLog().debug("woke up");
        }
      } else if (!m_list.empty()) {
        poll();
      }
    }
  } catch (Cancelled&) {
  } catch (std::exception& e) {
    curlLog().error("uncaught exception in curl downloader thread: {}", e.what());
  } catch (...) {
    curlLog().error("unknown exception in curl downloader thread");
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

    for (auto& d : temp) {
      start(d);
      m_list.push_back(d);
      m_map.emplace(d->handle(), std::prev(m_list.end()));
    }
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

      auto itor = m_map.find(msg->easy_handle);

      if (itor == m_map.end()) {
        curlLog().error("{} finished, but not in list", (void*)msg->easy_handle);
      }

      mr = curl_multi_remove_handle(m_handle.get(), msg->easy_handle);
      if (mr != CURLM_OK) {
        curlLog().error("failed to remove easy handle, {}", curlError(mr));
      }

      if (itor != m_map.end()) {
        (*itor->second)->finish(msg->data.result);
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
  bool changed = false;

  if (cleanupActive()) {
    changed = true;
  }

  if (changed) {
    setLimits();
  }
}

bool Downloader::cleanupActive()
{
  bool changed = false;
  auto itor = m_list.begin();

  while (itor != m_list.end()) {
    auto d = *itor;

    if (d->state() == Download::Finished) {
      curlLog().debug("{} finished, removing from list", d->debugName());
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
  auto mitor = m_map.find((*itor)->handle());

  if (mitor == m_map.end()) {
    curlLog().error("{} not found in active map", (*itor)->debugName());
  } else {
    m_map.erase(mitor);
  }

  return m_list.erase(itor);
}

curl_off_t Downloader::maxSpeedPer() const
{
  const std::size_t maxSpeed = m_maxSpeed;

  if (m_list.empty()) {
    return maxSpeed;
  }

  if (maxSpeed == NoLimit) {
    return 0;
  }

  return (maxSpeed / m_list.size());
}

void Downloader::setLimits()
{
  if (m_list.empty()) {
    return;
  }

  const curl_off_t maxPer = maxSpeedPer();

  if (maxPer == 0) {
    curlLogV().debug("setting speed limit to unlimited");
  } else {
    curlLog().debug("setting speed limit per download to {}", maxPer);
  }

  for (auto&& d : m_list) {
    curl_easy_setopt(d->handle(), CURLOPT_MAX_RECV_SPEED_LARGE, maxPer);
  }
}

bool Downloader::start(std::shared_ptr<Download> d)
{
  auto* h = d->setup(maxSpeedPer());
  if (!h) {
    return false;
  }

  curlLogV().debug("adding {} to multi handle", d->debugName());

  const auto r = curl_multi_add_handle(m_handle.get(), h);
  if (r != CURLM_OK) {
    curlLog().error("failed to add easy handle, {}", curlError(r));
    return false;
  }

  d->start();

  return true;
}

} // namespace
