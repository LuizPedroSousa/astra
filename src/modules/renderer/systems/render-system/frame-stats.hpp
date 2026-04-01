#pragma once

#include <cstdint>

namespace astralix {

struct FrameStats {
  uint32_t draw_call_count = 0u;
  uint32_t state_change_count = 0u;
  float gpu_frame_time_ms = 0.0f;
  float gpu_memory_used_mb = 0.0f;
  float gpu_memory_total_mb = 0.0f;
};

} // namespace astralix
