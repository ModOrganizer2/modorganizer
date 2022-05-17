#ifndef PROXYUTILS_H
#define PROXYUTILS_H

#include <type_traits>

#include "organizerproxy.h"

namespace MOShared
{

template <class Fn, class T = int>
auto callIfPluginActive(OrganizerProxy* proxy, Fn&& callback, T defaultReturn = T{})
{
  return [fn = std::forward<Fn>(callback), proxy, defaultReturn](auto&&... args) {
    if (proxy->isPluginEnabled(proxy->plugin())) {
      return fn(std::forward<decltype(args)>(args)...);
    } else {
      if constexpr (!std::is_same_v<
                        std::invoke_result_t<std::decay_t<Fn>, decltype(args)...>,
                        void>) {
        return defaultReturn;
      }
    }
  };
}

// We need to connect to the organizer.
template <class Signal, class T = int>
auto callSignalIfPluginActive(OrganizerProxy* proxy, const Signal& signal,
                              T defaultReturn = T{})
{
  return callIfPluginActive(
      proxy,
      [&signal](auto&&... args) {
        return signal(std::forward<decltype(args)>(args)...);
      },
      defaultReturn);
}

template <class Signal, class T = int>
auto callSignalAlways(const Signal& signal)
{
  return [&signal](auto&&... args) {
    return signal(std::forward<decltype(args)>(args)...);
  };
}

}  // namespace MOShared

#endif
