#pragma once

#include <cstddef>

namespace astralix {

struct AllocatorMetrics {
  float heap_used_mb = 0.0f;
  float mmap_used_mb = 0.0f;
  float allocation_rate_mb_per_sec = 0.0f;
};

class AllocatorMetricsSampler {
public:
  AllocatorMetrics sample(double wall_delta);

private:
  size_t m_previous_uordblks = 0u;
  bool m_has_previous = false;
};

} // namespace astralix
