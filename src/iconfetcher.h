#ifndef MODORGANIZER_ICONFETCHER_INCLUDED
#define MODORGANIZER_ICONFETCHER_INCLUDED
#include <QFileIconProvider>
#include <QStringView>
#include <mutex>
#include <set>

class IconFetcher
{
public:
  IconFetcher();
  ~IconFetcher();

  void stop();

  QVariant icon(const QString& path) const;
  QPixmap genericFileIcon() const;
  QPixmap genericDirectoryIcon() const;

private:
  struct QuickCache
  {
    QPixmap file;
    QPixmap directory;
  };

  struct Cache
  {
    std::map<QString, QPixmap, std::less<>> map;
    std::mutex mapMutex;

    std::set<QString> queue;
    std::mutex queueMutex;
  };

  class Waiter
  {
  public:
    void wait();
    void wakeUp();

  private:
    mutable std::mutex m_wakeUpMutex;
    std::condition_variable m_wakeUp;
    bool m_queueAvailable = false;
  };

  const int m_iconSize;
  QFileIconProvider m_provider;
  std::thread m_thread;
  std::atomic<bool> m_stop;

  mutable QuickCache m_quickCache;
  mutable Cache m_extensionCache;
  mutable Cache m_fileCache;
  mutable Waiter m_waiter;

  bool hasOwnIcon(const QString& path) const;

  template <class T>
  QPixmap getPixmapIcon(T&& t) const
  {
    return m_provider.icon(t).pixmap({m_iconSize, m_iconSize});
  }

  void threadFun();

  void checkCache(Cache& cache);
  void queue(Cache& cache, QString path) const;

  QVariant fileIcon(const QString& path) const;
  QVariant extensionIcon(const QStringView& ext) const;
};

#endif  // MODORGANIZER_ICONFETCHER_INCLUDED
