#pragma once

#include "guid.hpp"

#include <cstdint>
#include <functional>
#include <string_view>
#include <type_traits>

namespace astralix::ui::im {

struct WidgetId {
  uint64_t value = 0u;

  constexpr explicit operator bool() const { return value != 0u; }
  auto operator==(const WidgetId &) const -> bool = default;
};

inline constexpr WidgetId k_invalid_widget_id{};

namespace detail {

inline uint64_t fnv1a_append(uint64_t hash, const void *data, size_t size) {
  constexpr uint64_t k_prime = 1099511628211ull;
  const auto *bytes = static_cast<const uint8_t *>(data);
  for (size_t index = 0u; index < size; ++index) {
    hash ^= static_cast<uint64_t>(bytes[index]);
    hash *= k_prime;
  }
  return hash;
}

inline uint64_t fnv1a_seed() { return 14695981039346656037ull; }

inline uint64_t hash_bytes(const void *data, size_t size) {
  return fnv1a_append(fnv1a_seed(), data, size);
}

inline uint64_t hash_string(std::string_view value) {
  return hash_bytes(value.data(), value.size());
}

template <typename T>
uint64_t hash_value(const T &value) {
  if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>) {
    return hash_string(value);
  } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
    return hash_string(value);
  } else if constexpr (std::is_integral_v<std::decay_t<T>> ||
                       std::is_enum_v<std::decay_t<T>>) {
    const auto copied = static_cast<uint64_t>(value);
    return hash_bytes(&copied, sizeof(copied));
  } else {
    return hash_bytes(&value, sizeof(value));
  }
}

inline WidgetId combine(WidgetId parent, uint64_t child_hash) {
  uint64_t seed = fnv1a_seed();
  seed = fnv1a_append(seed, &parent.value, sizeof(parent.value));
  seed = fnv1a_append(seed, &child_hash, sizeof(child_hash));
  if (seed == 0u) {
    seed = 1u;
  }
  return WidgetId{seed};
}

} // namespace detail

inline WidgetId child_id(WidgetId scope, std::string_view local_name) {
  return detail::combine(scope, detail::hash_string(local_name));
}

inline WidgetId auto_child_id(WidgetId scope, uint64_t ordinal) {
  uint64_t seed = detail::fnv1a_seed();
  constexpr std::string_view k_auto_tag = "@auto";
  const uint64_t tag_hash = detail::hash_string(k_auto_tag);
  seed = detail::fnv1a_append(seed, &tag_hash, sizeof(tag_hash));
  seed = detail::fnv1a_append(seed, &ordinal, sizeof(ordinal));
  return detail::combine(scope, seed);
}

template <typename Key>
WidgetId keyed_id(WidgetId scope, std::string_view local_name, const Key &key) {
  uint64_t seed = detail::fnv1a_seed();
  const uint64_t local_hash = detail::hash_string(local_name);
  const uint64_t key_hash = detail::hash_value(key);
  seed = detail::fnv1a_append(seed, &local_hash, sizeof(local_hash));
  seed = detail::fnv1a_append(seed, &key_hash, sizeof(key_hash));
  return detail::combine(scope, seed);
}

} // namespace astralix::ui::im

template <>
struct std::hash<astralix::ui::im::WidgetId> {
  size_t operator()(astralix::ui::im::WidgetId id) const noexcept {
    return static_cast<size_t>(id.value);
  }
};
