#ifndef PROXYUTILS_H
#define PROXYUTILS_H

#include <type_traits>

#include "organizerproxy.h"

namespace MOShared {

  template <class Fn, class T = int>
  auto callIfPluginActive(OrganizerProxy* proxy, Fn&& callback, T defaultReturn = T{}) {
    return [fn = std::forward<Fn>(callback), proxy, defaultReturn](auto&& ...args) {
      if (proxy->isPluginEnabled(proxy->plugin())) {
        return fn(std::forward<decltype(args)>(args)...);
      }
      else {
        if constexpr (!std::is_same_v<std::invoke_result_t<std::decay_t<Fn>, decltype(args)... >, void>) {
          return defaultReturn;
        }
      }
    };
  }

}

#endif
