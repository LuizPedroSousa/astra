#pragma once

#include <cstdint>

namespace astralix {

[[nodiscard]] constexpr uint8_t to_bit(bool value) noexcept {
  return static_cast<uint8_t>(value);
}

[[nodiscard]] constexpr uint8_t bit_is_set(uint8_t value,
                                           uint8_t mask) noexcept {
  return static_cast<uint8_t>((value & mask) != 0u);
}

// Valid for normalized 0/1 values only.
[[nodiscard]] constexpr uint8_t not_one(uint8_t value) noexcept {
  return value ^ 1u;
}

} // namespace astralix
