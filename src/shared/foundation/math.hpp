#pragma once

#include <algorithm>

namespace astralix {

template <typename T>
[[nodiscard]] constexpr T clamp(T value, T minimum, T maximum) {
  return std::clamp(value, minimum, maximum);
}

template <typename T>
[[nodiscard]] constexpr T saturate(T value) {
  return clamp(value, T{0}, T{1});
}

template <typename T>
[[nodiscard]] constexpr T lerp(T compact, T roomy, T t) {
  const T factor = saturate(t);
  return compact + (roomy - compact) * factor;
}

} // namespace astralix
