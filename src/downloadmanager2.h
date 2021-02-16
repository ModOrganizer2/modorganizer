#ifndef MODORGANIZER_DOWNLOADEDMANAGER2_INCLUDED
#define MODORGANIZER_DOWNLOADEDMANAGER2_INCLUDED

#include <curl/curl.h>

namespace dm
{

class CurlGlobalHandle;

namespace fs = std::filesystem;
using hr_clock = std::chrono::high_resolution_clock;

class CurlDownloader
{
public:
  struct defer_t {};
  static const defer_t defer;


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


  struct Download
  {
    std::string url;
    fs::path file;
    EasyHandle handle;
    FileHandle out;

    hr_clock::time_point lastCheck;
    std::size_t bytes;
    std::atomic<double> bytesPerSecond;

    Download(std::string url, fs::path file);

    std::size_t resumeFrom();
    bool finish();

    int xfer(
      curl_off_t dltotal, curl_off_t dlnow,
      curl_off_t ultotal, curl_off_t ulnow);

    bool header(std::string_view sv);
    bool write(std::string_view data);
    void debug(curl_infotype t, std::string_view data);
  };


  CurlDownloader();
  ~CurlDownloader();

  void cancel();
  void stop();
  void join();

  std::size_t queued() const;
  bool finished() const;

  std::shared_ptr<Download> add(std::string url, fs::path file);

private:
  std::shared_ptr<CurlGlobalHandle> m_global;
  MultiHandle m_handle;

  std::atomic<std::size_t> m_queuedCount;
  std::vector<std::shared_ptr<Download>> m_temp;
  std::mutex m_tempMutex;

  std::thread m_thread;
  std::atomic<bool> m_cancel, m_stop, m_finished;
  std::condition_variable m_cv;
  std::vector<std::shared_ptr<Download>> m_queued, m_active;

  void run();
  void checkTemp();
  void perform();
  void poll();
  void checkQueue();
  bool start(std::shared_ptr<Download> d);
  void setLimits();
  void checkCancel();

  static int s_xfer(
    void* p,
    curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow);

  static size_t s_header(char* data, size_t size, size_t n, void* p);
  static size_t s_write(char* data, size_t size, size_t n, void* p);
  static int s_debug(CURL* h, curl_infotype t, char* data, size_t n, void *p);
};



class Download
{
};


class DownloadManager2
{
public:

private:
  CurlDownloader m_downloader;
};

} // namespace

#endif // MODORGANIZER_DOWNLOADEDMANAGER2_INCLUDED
