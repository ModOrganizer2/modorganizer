#ifndef MO2_THREAD_UTILS_H
#define MO2_THREAD_UTILS_H

#include <functional>
#include <log.h>
#include <mutex>
#include <thread>

// in main.cpp
void setExceptionHandlers();

namespace MOShared
{

// starts an std::thread with an unhandled exception handler for core dumps
// and a top-level catch
//
template <class F>
std::thread startSafeThread(F&& f)
{
  return std::thread([f = std::forward<F>(f)] {
    setExceptionHandlers();
    f();
  });
}

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
  for (auto& thread : threads) {
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

}  // namespace MOShared

#endif
