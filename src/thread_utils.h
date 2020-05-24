#ifndef MO2_THREAD_UTILS_H
#define MO2_THREAD_UTILS_H

#include <mutex>
#include <thread>

namespace MOShared {

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
void parallelMap(It begin, It end, Callable callable, std::size_t nThreads) {
  std::vector<std::thread> threads(nThreads);

  std::mutex m;
  for (auto &thread: threads) {
    thread = std::thread([&m, &begin, end, callable]() {
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
