#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

namespace astralix::ui {

template <size_t Capacity>
class TimeSeries {
public:
  void push(float value) {
    m_samples[m_head] = value;
    m_head = (m_head + 1u) % Capacity;
    if (m_count < Capacity) {
      ++m_count;
    }
  }

  size_t size() const { return m_count; }
  bool empty() const { return m_count == 0u; }
  size_t capacity() const { return Capacity; }

  float operator[](size_t index) const {
    const size_t physical =
        (m_head + Capacity - m_count + index) % Capacity;
    return m_samples[physical];
  }

  float latest() const {
    if (m_count == 0u) {
      return 0.0f;
    }
    return m_samples[(m_head + Capacity - 1u) % Capacity];
  }

  float min_value() const {
    if (m_count == 0u) {
      return 0.0f;
    }
    float result = std::numeric_limits<float>::max();
    for (size_t i = 0u; i < m_count; ++i) {
      result = std::min(result, (*this)[i]);
    }
    return result;
  }

  float max_value() const {
    if (m_count == 0u) {
      return 0.0f;
    }
    float result = std::numeric_limits<float>::lowest();
    for (size_t i = 0u; i < m_count; ++i) {
      result = std::max(result, (*this)[i]);
    }
    return result;
  }

  void clear() {
    m_head = 0u;
    m_count = 0u;
  }

private:
  std::array<float, Capacity> m_samples{};
  size_t m_head = 0u;
  size_t m_count = 0u;
};

} // namespace astralix::ui
