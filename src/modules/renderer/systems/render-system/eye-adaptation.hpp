#pragma once

#include <cstdint>
#include <string_view>

namespace astralix {

inline constexpr std::string_view k_eye_adaptation_histogram_resource =
    "__eye_adaptation_histogram";
inline constexpr std::string_view k_eye_adaptation_exposure_resource =
    "__eye_adaptation_exposure";

inline constexpr uint32_t k_eye_adaptation_histogram_bin_count = 256u;
inline constexpr uint32_t k_eye_adaptation_histogram_binding_point = 0u;
inline constexpr uint32_t k_eye_adaptation_exposure_binding_point = 1u;

struct EyeAdaptationExposureData {
  float average_luminance = 1.0f;
  float exposure = 0.7f;
};

struct EyeAdaptationState {
  bool initialized = false;
};

} // namespace astralix
