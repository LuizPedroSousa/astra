#pragma once

#include <cstddef>

namespace astralix {

struct ProcessMetrics {
  float cpu_usage_percent = 0.0f;
  float memory_rss_mb = 0.0f;
  float memory_virtual_mb = 0.0f;
  float binary_size_mb = 0.0f;
  size_t thread_count = 0u;
  size_t open_fd_count = 0u;
  size_t minor_page_faults = 0u;
  size_t major_page_faults = 0u;
  size_t voluntary_context_switches = 0u;
  size_t involuntary_context_switches = 0u;
  float disk_read_bytes = 0.0f;
  float disk_write_bytes = 0.0f;
};

class ProcessMetricsSampler {
public:
  ProcessMetrics sample();

private:
  unsigned long long m_previous_utime = 0u;
  unsigned long long m_previous_stime = 0u;
  double m_previous_wall_seconds = 0.0;
  bool m_has_previous_sample = false;
  float m_cached_binary_size_mb = 0.0f;
  bool m_has_binary_size = false;
};

} // namespace astralix
