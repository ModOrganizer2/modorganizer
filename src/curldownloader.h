#ifndef MODORGANIZER_CURLDOWNLOADER_INCLUDED
#define MODORGANIZER_CURLDOWNLOADER_INCLUDED

namespace dm::curl
{

namespace fs = std::filesystem;
using hr_clock = std::chrono::high_resolution_clock;

struct defer_t {};
extern const defer_t defer;

class GlobalHandle
{
public:
  GlobalHandle();
  ~GlobalHandle();

  GlobalHandle(const GlobalHandle&) = delete;
  GlobalHandle& operator=(const GlobalHandle) = delete;
};


class EasyHandle
{
public:
  EasyHandle();
  EasyHandle(defer_t);
  ~EasyHandle();

  EasyHandle(const EasyHandle&) = delete;
  EasyHandle& operator=(const EasyHandle&) = delete;

  bool create();
  CURL* get() const;

private:
  CURL* m_handle;
};


class MultiHandle
{
public:
  MultiHandle();
  MultiHandle(defer_t);
  ~MultiHandle();

  MultiHandle(const MultiHandle&) = delete;
  MultiHandle& operator=(const MultiHandle&) = delete;

  bool create();
  CURLM* get() const;

private:
  CURLM* m_handle;
};


class FileHandle
{
public:
  FileHandle();
  ~FileHandle();

  FileHandle(const FileHandle&) = delete;
  FileHandle& operator=(const FileHandle&) = delete;

  bool opened() const;
  std::size_t open(fs::path p, bool append);
  void close();

  bool write(std::string_view sv);

private:
  HANDLE m_handle;
  fs::path m_path;

  bool doOpen(bool append);
};


class Download
{
public:
  enum States
  {
    Stopped = 0,
    Running,
    Stopping,
    Finished
  };

  Download(std::string url, fs::path file);

  CURL* setup(curl_off_t maxSpeed);

  CURL* handle() const;
  States state() const;
  std::string debugName() const;

  void start();
  void stop();
  bool finish();

  bool xfer(
    curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow);

  bool header(std::string_view sv);
  bool write(std::string_view data);
  void debug(curl_infotype t, std::string_view data);

private:
  std::string m_url;
  fs::path m_file;
  EasyHandle m_handle;
  FileHandle m_out;
  States m_state;

  hr_clock::time_point m_lastCheck;
  std::size_t m_bytes;
  std::atomic<double> m_bytesPerSecond;

  std::size_t resumeFrom();
  bool rename();

  static int s_xfer(
    void* p,
    curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow);

  static size_t s_header(char* data, size_t size, size_t n, void* p);
  static size_t s_write(char* data, size_t size, size_t n, void* p);
  static int s_debug(CURL* h, curl_infotype t, char* data, size_t n, void *p);
};


class Downloader
{
public:
  static const std::size_t NoLimit =
    std::numeric_limits<std::size_t>::max();

  Downloader();
  ~Downloader();

  void cancel();
  void stop();
  void join();

  void maxSpeed(std::size_t bytesPerSecond);

  bool finished() const;

  std::shared_ptr<Download> add(std::string url, fs::path file);

private:
  using DownloadList = std::list<std::shared_ptr<Download>>;

  std::shared_ptr<GlobalHandle> m_global;
  MultiHandle m_handle;

  std::vector<std::shared_ptr<Download>> m_temp;
  std::mutex m_tempMutex;

  std::thread m_thread;
  std::atomic<bool> m_cancel, m_stop, m_finished;
  std::condition_variable m_cv;
  DownloadList m_list;
  std::map<CURL*, DownloadList::iterator> m_map;

  std::atomic<std::size_t> m_maxSpeed;


  void run();
  void checkTemp();
  void perform();
  void poll();
  bool start(std::shared_ptr<Download> d);
  void setLimits();
  void checkCancel();

  void checkQueue();
  bool cleanupActive();
  DownloadList::iterator removeFromActive(DownloadList::iterator itor);
  void stopOverMax(std::size_t max);
  bool addFromQueue(std::size_t max);
  curl_off_t maxSpeedPer() const;
};

} // namespace

#endif // MODORGANIZER_CURLDOWNLOADER_INCLUDED
