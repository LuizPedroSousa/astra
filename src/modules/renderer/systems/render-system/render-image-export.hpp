#pragma once

#include <cstdint>

namespace astralix {

enum class RenderImageResource : uint8_t {
  SceneColor,
  GBuffer,
  ShadowMap,
  FinalOutput,
};

enum class RenderImageAspect : uint8_t {
  Color0,
  Color1,
  Color2,
  Depth,
};

struct RenderImageExportKey {
  RenderImageResource resource = RenderImageResource::SceneColor;
  RenderImageAspect aspect = RenderImageAspect::Color0;

  bool operator==(const RenderImageExportKey &) const = default;
};

enum class RenderImageResolveMode : uint8_t {
  DirectColorAttachment,
  DirectDepthAttachment,
  Materialize,
};

struct RenderImageExportBinding {
  RenderImageExportKey key;
  uint32_t resource_index = 0;
  uint32_t attachment_index = 0;
  bool available = false;
  RenderImageResolveMode resolve_mode = RenderImageResolveMode::Materialize;
};

enum class RenderImageTarget : uint8_t {
  Texture2D,
};

struct ResolvedRenderImage {
  bool available = false;
  RenderImageTarget target = RenderImageTarget::Texture2D;
  uint32_t renderer_texture_id = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

} // namespace astralix
