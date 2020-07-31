#ifndef MO2_THREAD_UTILS_H
#define MO2_THREAD_UTILS_H

#include <log.h>
#include <functional>
#include <mutex>
#include <thread>

// in main.cpp
void setUnhandledExceptionHandler();
LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *exceptionPtrs);


namespace MOShared {

// starts an std::thread with an unhandled exception handler for core dumps
// and a top-level catch
//
template <class F>
std::thread startSafeThread(F&& f)
{
  return std::thread([f=std::forward<F>(f)] {
    setUnhandledExceptionHandler();
    f();
  });
}


/**
 * Class that can be used to perform thread-safe memoization.
 *
 * Each instance hold a flag indicating if the current value is up-to-date
 * or not. This flag can be reset using `invalidate()`. When the value is queried,
 * the flag is checked, and if it is not up-to-date, the given callback is used
 * to compute the value.
 *
 * The computation and update of the value is locked to avoid concurrent modifications.
 *
 * @tparam T Type of value ot memoized.
 * @tparam Fn Type of the callback.
 */
template <class T, class Fn = std::function<T()>>
struct MemoizedLocked {

  template <class Callable>
  MemoizedLocked(Callable &&callable, T value = {}) :
    m_Fn{ std::forward<Callable>(callable) }, m_Value{ std::move(value) } { }

  template <class... Args>
  T& value(Args&&... args) const {
    if (m_NeedUpdating) {
      std::scoped_lock lock(m_Mutex);
      if (m_NeedUpdating) {
        m_Value = std::invoke(m_Fn, std::forward<Args>(args)... );
        m_NeedUpdating = false;
      }
    }
    return m_Value;
  }

  void invalidate() {
    m_NeedUpdating = true;
  }

private:
  mutable std::mutex m_Mutex;
  mutable std::atomic<bool> m_NeedUpdating{ true };

  Fn m_Fn;
  mutable T m_Value;
};

/**
 * @brief Apply the given callable to each element between the two given iterators
 *     in a parallel way.
 *
 * The callable should be independent, or properly synchronized, and the source of
 * the range should not change during this call.
 *
 * @param start Beginning of the range.
 * @param end End of the range.
 * @param callable Callable to apply to every element of the range. See std::invoke
 *     requirements. Must be copiable.
 * @param nThreads Number of threads to use.
 *
 */
template <class It, class Callable>
void parallelMap(It begin, It end, Callable callable, std::size_t nThreads)
{
  std::mutex m;
  std::vector<std::thread> threads(nThreads);

  // Create the thread:
  //  - The mutex is only used to fetch/increment the iterator.
  //  - The callable is copied in each thread to avoid conflicts.
  for (auto &thread: threads) {
    thread = startSafeThread([&m, &begin, end, callable]() {
      while (true) {
        decltype(begin) it;
        {
          std::scoped_lock lock(m);
          if (begin == end) {
            break;
          }
          it = begin++;
        }
        if (it != end) {
          std::invoke(callable, *it);
        }
      }
    });
  }

  // Join everything:
  for (auto& t : threads) {
    t.join();
  }
}

}

#endif
