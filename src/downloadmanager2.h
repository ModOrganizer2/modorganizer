#ifndef MODORGANIZER_DOWNLOADEDMANAGER2_INCLUDED
#define MODORGANIZER_DOWNLOADEDMANAGER2_INCLUDED

namespace dm::curl { class Downloader; class Download; }

namespace dm
{

namespace fs = std::filesystem;

class Download
{
public:
  enum States
  {
    None = 0,

    Queueing,
    Queued,

    Finished,
    Errored,

    Running,

    Pausing,
    Paused,

    Cancelling,
    Cancelled
  };

  Download(QUrl url);

  const QUrl& url() const;
  const QString& file() const;

  States state() const;

  void start(std::shared_ptr<curl::Download> d);
  void cancel();
  void pause();
  void queue();
  void tick();

  QString debugName() const;

private:
  QUrl m_url;
  QString m_file;
  States m_state;
  std::shared_ptr<curl::Download> m_download;
};


class DownloadManager2
{
public:
  static const std::size_t NoLimit =
    std::numeric_limits<std::size_t>::max();

  DownloadManager2();
  ~DownloadManager2();

  void add(const QUrl& url);

  void maxActive(std::size_t n);
  void maxSpeed(std::size_t bytesPerSecond);

  bool hasActive() const;

private:
  using DownloadList = std::list<std::shared_ptr<Download>>;

  std::thread m_thread;
  std::vector<std::shared_ptr<Download>> m_temp;
  std::mutex m_tempMutex;
  std::condition_variable m_cv;

  DownloadList m_queued, m_active, m_inactive;
  std::atomic<std::size_t> m_maxActive, m_maxSpeed;
  std::atomic<bool> m_hasActive;

  std::unique_ptr<curl::Downloader> m_downloader;


  void run();
  void checkTemp();
  void checkQueue();
  void cleanupActive();
  void stopOverMax(std::size_t max);
  void addFromQueue(std::size_t max);
  bool start(std::shared_ptr<Download> d);
};

} // namespace

#endif // MODORGANIZER_DOWNLOADEDMANAGER2_INCLUDED
