#pragma once

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace astralix {

inline constexpr uint64_t k_fnv1a64_offset_basis = 14695981039346656037ull;
inline constexpr uint64_t k_fnv1a64_prime = 1099511628211ull;

[[nodiscard]] constexpr uint64_t
fnv1a64_append_string(std::string_view value, uint64_t hash = k_fnv1a64_offset_basis) noexcept {
  for (unsigned char ch : value) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= k_fnv1a64_prime;
  }

  return hash;
}

[[nodiscard]] inline uint64_t fnv1a64_append_bytes(uint64_t hash, const void *data, size_t size) noexcept {
  const auto *bytes = static_cast<const uint8_t *>(data);
  for (size_t index = 0; index < size; ++index) {
    hash ^= static_cast<uint64_t>(bytes[index]);
    hash *= k_fnv1a64_prime;
  }

  return hash;
}

template <typename T>
[[nodiscard]] inline uint64_t fnv1a64_append_value(uint64_t hash, const T &value) noexcept {
  static_assert(std::is_trivially_copyable_v<T>, "fnv1a64_append_value requires a trivially copyable value");
  return fnv1a64_append_bytes(hash, &value, sizeof(value));
}

[[nodiscard]] inline std::string fnv1a64_hex_digest(uint64_t hash) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

} // namespace astralix
