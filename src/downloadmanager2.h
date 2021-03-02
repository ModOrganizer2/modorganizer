#ifndef MODORGANIZER_DOWNLOADEDMANAGER2_INCLUDED
#define MODORGANIZER_DOWNLOADEDMANAGER2_INCLUDED

#include <ipluginrepository.h>

class PluginContainer;
namespace dm::curl { class Downloader; class Download; }


namespace dm
{

namespace fs = std::filesystem;

class DownloadManager2;

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

  Download(DownloadManager2& dm, MOBase::IPluginRepository& repo, QString what);

  const QString& what() const;
  const QString& error() const;

  States state() const;

  bool start();
  void cancel();
  void pause();
  void queue();
  void tick();

  QString debugName() const;

private:
  DownloadManager2& m_dm;
  MOBase::IPluginRepository& m_repo;
  std::unique_ptr<MOBase::IRepositoryDownload> m_download;
  QString m_what;
  States m_state;
  QString m_error;

  void setState(States s);
  void next();
};


class DownloadManager2
{
public:
  static const std::size_t NoLimit =
    std::numeric_limits<std::size_t>::max();

  DownloadManager2(PluginContainer& pc);
  ~DownloadManager2();

  void add(QString what);

  void maxActive(std::size_t n);
  void maxSpeed(std::size_t bytesPerSecond);

  bool hasActive() const;

  curl::Downloader& downloader();

private:
  using DownloadList = std::list<std::shared_ptr<Download>>;

  PluginContainer& m_pc;

  std::thread m_thread;
  std::atomic<bool> m_stop;

  std::unique_ptr<curl::Downloader> m_downloader;

  std::vector<std::shared_ptr<Download>> m_temp;
  std::mutex m_tempMutex;
  std::condition_variable m_cv;

  DownloadList m_queued, m_active, m_inactive;
  std::atomic<std::size_t> m_maxActive, m_maxSpeed;
  std::atomic<bool> m_hasActive;


  void run();
  void checkTemp();
  void checkQueue();
  void cleanupActive();
  void stopOverMax(std::size_t max);
  void addFromQueue(std::size_t max);
};

} // namespace

#endif // MODORGANIZER_DOWNLOADEDMANAGER2_INCLUDED
