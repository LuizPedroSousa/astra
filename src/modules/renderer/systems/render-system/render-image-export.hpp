#pragma once

#include "systems/render-system/core/render-resource-ref.hpp"
#include <cstdint>

namespace astralix {

enum class RenderImageResource : uint8_t {
  SceneColor,
  GBuffer,
  ShadowMap,
  SSAO,
  SSAOBlur,
  Bloom,
  FinalOutput,
};

enum class RenderImageAspect : uint8_t {
  Color0,
  Color1,
  Color2,
  Color3,
  Depth,
};

enum class GBufferAspect : uint8_t {
  Position,
  Normal,
  Albedo,
  Emissive,
  Depth,
};

struct RenderImageExportKey {
  RenderImageResource resource = RenderImageResource::SceneColor;
  RenderImageAspect aspect = RenderImageAspect::Color0;

  bool operator==(const RenderImageExportKey &) const = default;
};

constexpr RenderImageExportKey make_render_image_export_key(
    RenderImageResource resource,
    RenderImageAspect aspect = RenderImageAspect::Color0
) {
  return RenderImageExportKey{
      .resource = resource,
      .aspect = aspect,
  };
}

constexpr RenderImageAspect to_render_image_aspect(GBufferAspect aspect) {
  switch (aspect) {
    case GBufferAspect::Position:
      return RenderImageAspect::Color0;
    case GBufferAspect::Normal:
      return RenderImageAspect::Color1;
    case GBufferAspect::Albedo:
      return RenderImageAspect::Color2;
    case GBufferAspect::Emissive:
      return RenderImageAspect::Color3;
    case GBufferAspect::Depth:
      return RenderImageAspect::Depth;
  }

  return RenderImageAspect::Color0;
}

constexpr RenderImageExportKey make_g_buffer_export_key(
    GBufferAspect aspect
) {
  return make_render_image_export_key(
      RenderImageResource::GBuffer, to_render_image_aspect(aspect)
  );
}

static_assert(to_render_image_aspect(GBufferAspect::Position) == RenderImageAspect::Color0);
static_assert(to_render_image_aspect(GBufferAspect::Normal) == RenderImageAspect::Color1);
static_assert(to_render_image_aspect(GBufferAspect::Albedo) == RenderImageAspect::Color2);
static_assert(to_render_image_aspect(GBufferAspect::Emissive) == RenderImageAspect::Color3);
static_assert(to_render_image_aspect(GBufferAspect::Depth) == RenderImageAspect::Depth);

struct RenderImageExportBinding {
  RenderImageExportKey key;
  bool available = false;
};

struct ResolvedRenderImage {
  bool available = false;
  ImageViewRef view{};
  uint32_t width = 0;
  uint32_t height = 0;
};

} // namespace astralix
