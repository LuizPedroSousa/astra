#pragma once

#include <cstdint>
#include <vector>

namespace astralix {

struct CubemapFaceData {
  std::vector<float> pixels;
  uint32_t width;
  uint32_t height;
};

struct CubemapConversionResult {
  CubemapFaceData faces[6];
};

CubemapConversionResult convert_equirectangular_to_cubemap(
    const float *equirectangular_pixels,
    int equirectangular_width,
    int equirectangular_height,
    int equirectangular_channels,
    uint32_t face_resolution
);

CubemapConversionResult convolve_irradiance_cubemap(
    const CubemapConversionResult &source,
    uint32_t output_resolution = 32,
    uint32_t hemisphere_samples = 512
);

} // namespace astralix
