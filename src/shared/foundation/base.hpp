#pragma once

#include "memory"

namespace astralix {

extern float distance;
extern float far_plane, near_plane;

#define ASTRA_BIND_EVENT_FN(fn)                                                \
  [this](auto &&...args) -> decltype(auto) {                                   \
    return this->fn(std::forward<decltype(args)>(args)...);                    \
  }

#define CHECK_ACTIVE(t)                                                        \
  if (!t->is_active())                                                         \
  return

template <typename... T> constexpr bool has_components(T *...ptrs) {
  return ((ptrs != nullptr) && ...);
}

#define REQUIRE_COMPONENTS(...)                                                \
  do {                                                                         \
    if (!has_components(__VA_ARGS__))                                          \
      return;                                                                  \
  } while (0)

template <typename T> using Scope = std::unique_ptr<T>;
template <typename T, typename... Args>
constexpr Scope<T> create_scope(Args &&...args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
};

template <typename T> using Ref = std::shared_ptr<T>;
template <typename T, typename... Args>
constexpr Ref<T> create_ref(Args &&...args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
};

}; // namespace astralix
