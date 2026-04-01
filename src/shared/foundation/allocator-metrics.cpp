#include "allocator-metrics.hpp"

#include <cstddef>

#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
#define ASTRA_HAS_MALLINFO2 1
#include <malloc.h>
#else
#define ASTRA_HAS_MALLINFO2 0
#endif

namespace astralix {

AllocatorMetrics AllocatorMetricsSampler::sample(double wall_delta) {
  AllocatorMetrics metrics;

#if ASTRA_HAS_MALLINFO2
  const struct mallinfo2 info = mallinfo2();
  const size_t current_uordblks = info.uordblks;

  metrics.heap_used_mb =
      static_cast<float>(info.uordblks) / (1024.0f * 1024.0f);
  metrics.mmap_used_mb =
      static_cast<float>(info.hblkhd) / (1024.0f * 1024.0f);

  if (m_has_previous && wall_delta > 0.0) {
    const double delta_bytes = current_uordblks > m_previous_uordblks
                                   ? static_cast<double>(current_uordblks - m_previous_uordblks)
                                   : 0.0;
    metrics.allocation_rate_mb_per_sec =
        static_cast<float>(delta_bytes / (1024.0 * 1024.0) / wall_delta);
  }

  m_previous_uordblks = current_uordblks;
  m_has_previous = true;
#else
  (void)wall_delta;
#endif

  return metrics;
}

} // namespace astralix
