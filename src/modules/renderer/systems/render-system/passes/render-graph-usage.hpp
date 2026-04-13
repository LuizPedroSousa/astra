#pragma once

#include "systems/render-system/core/render-resource-ref.hpp"
#include "systems/render-system/render-image-export.hpp"
#include <cstdint>

namespace astralix {

enum class RenderUsage : uint8_t {
  ColorAttachmentWrite,
  DepthAttachmentWrite,
  DepthAttachmentRead,
  SampledRead,
  StorageRead,
  StorageWrite,
  ResolveSrc,
  ResolveDst,
  TransferSrc,
  TransferDst,
  Present,
};

enum class ResourceState : uint8_t {
  Undefined,
  ColorAttachment,
  DepthWrite,
  DepthRead,
  ShaderSampled,
  StorageRead,
  StorageWrite,
  ResolveSrc,
  ResolveDst,
  TransferSrc,
  TransferDst,
  PresentSrc,
};

struct RenderResourceRef {
  uint32_t resource_index = 0;

  bool operator==(const RenderResourceRef &) const = default;
};

struct RenderImageSubresourceRef {
  RenderResourceRef resource{};
  ImageAspect aspect = ImageAspect::Color0;
  uint32_t mip = 0;
  uint32_t layer = 0;

  constexpr uint32_t resource_index() const noexcept {
    return resource.resource_index;
  }

  bool operator==(const RenderImageSubresourceRef &) const = default;
};

struct RenderPassImageUsage {
  RenderImageSubresourceRef view{};
  RenderUsage usage = RenderUsage::SampledRead;
};

struct RenderImageExport {
  RenderImageExportKey key{};
  uint32_t resource_index = 0;
  ImageAspect aspect = ImageAspect::Color0;
  uint32_t mip = 0;
  uint32_t layer = 0;

  constexpr RenderImageSubresourceRef to_subresource() const {
    return RenderImageSubresourceRef{
        .resource = RenderResourceRef{.resource_index = resource_index},
        .aspect = aspect,
        .mip = mip,
        .layer = layer,
    };
  }
};

constexpr RenderImageExport make_render_image_export(
    RenderImageExportKey key,
    uint32_t resource_index,
    ImageAspect aspect = ImageAspect::Color0,
    uint32_t mip = 0,
    uint32_t layer = 0
) {
  return RenderImageExport{
      .key = key,
      .resource_index = resource_index,
      .aspect = aspect,
      .mip = mip,
      .layer = layer,
  };
}

constexpr RenderImageExport make_shadow_map_render_image_export(
    uint32_t resource_index,
    ImageAspect aspect = ImageAspect::Depth,
    uint32_t mip = 0,
    uint32_t layer = 0
) {
  auto key = make_render_image_export_key(
      RenderImageResource::ShadowMap, RenderImageAspect::Depth
  );

  return make_render_image_export(
      key,
      resource_index,
      aspect,
      mip,
      layer
  );
}

constexpr RenderImageExport make_scene_color_render_image_export(
    uint32_t resource_index,
    ImageAspect aspect = ImageAspect::Color0,
    uint32_t mip = 0,
    uint32_t layer = 0
) {
  auto key = make_render_image_export_key(RenderImageResource::SceneColor);

  return make_render_image_export(
      key,
      resource_index,
      aspect,
      mip,
      layer
  );
}

constexpr RenderImageExport make_g_buffer_render_image_export(
    GBufferAspect g_buffer_aspect,
    uint32_t resource_index,
    ImageAspect aspect = ImageAspect::Color0,
    uint32_t mip = 0,
    uint32_t layer = 0
) {
  auto key = make_render_image_export_key(
      RenderImageResource::GBuffer, to_render_image_aspect(g_buffer_aspect)
  );

  return make_render_image_export(
      key,
      resource_index,
      aspect,
      mip,
      layer
  );
}

constexpr RenderImageExport make_ssao_render_image_export(
    uint32_t resource_index,
    ImageAspect aspect = ImageAspect::Color0,
    uint32_t mip = 0,
    uint32_t layer = 0
) {
  auto key = make_render_image_export_key(RenderImageResource::SSAO);

  return make_render_image_export(
      key,
      resource_index,
      aspect,
      mip,
      layer
  );
}

constexpr RenderImageExport make_ssao_blur_render_image_export(
    uint32_t resource_index,
    ImageAspect aspect = ImageAspect::Color0,
    uint32_t mip = 0,
    uint32_t layer = 0
) {
  auto key = make_render_image_export_key(RenderImageResource::SSAOBlur);

  return make_render_image_export(
      key,
      resource_index,
      aspect,
      mip,
      layer
  );
}

constexpr RenderImageExport make_bloom_render_image_export(
    uint32_t resource_index,
    ImageAspect aspect = ImageAspect::Color0,
    uint32_t mip = 0,
    uint32_t layer = 0
) {
  auto key = make_render_image_export_key(RenderImageResource::Bloom);

  return make_render_image_export(
      key,
      resource_index,
      aspect,
      mip,
      layer
  );
}

constexpr RenderImageExport make_final_output_render_image_export(
    uint32_t resource_index,
    ImageAspect aspect = ImageAspect::Color0,
    uint32_t mip = 0,
    uint32_t layer = 0
) {
  auto key = make_render_image_export_key(RenderImageResource::FinalOutput);

  return make_render_image_export(
      key,
      resource_index,
      aspect,
      mip,
      layer
  );
}

struct RenderPassPresentRequest {
  RenderImageSubresourceRef source{};
};

inline ResourceState usage_to_state(RenderUsage usage) {
  switch (usage) {
    case RenderUsage::ColorAttachmentWrite:
      return ResourceState::ColorAttachment;
    case RenderUsage::DepthAttachmentWrite:
      return ResourceState::DepthWrite;
    case RenderUsage::DepthAttachmentRead:
      return ResourceState::DepthRead;
    case RenderUsage::SampledRead:
      return ResourceState::ShaderSampled;
    case RenderUsage::StorageRead:
      return ResourceState::StorageRead;
    case RenderUsage::StorageWrite:
      return ResourceState::StorageWrite;
    case RenderUsage::ResolveSrc:
      return ResourceState::ResolveSrc;
    case RenderUsage::ResolveDst:
      return ResourceState::ResolveDst;
    case RenderUsage::TransferSrc:
      return ResourceState::TransferSrc;
    case RenderUsage::TransferDst:
      return ResourceState::TransferDst;
    case RenderUsage::Present:
      return ResourceState::PresentSrc;
  }
  return ResourceState::Undefined;
}

inline bool is_write_usage(RenderUsage usage) {
  switch (usage) {
    case RenderUsage::ColorAttachmentWrite:
    case RenderUsage::DepthAttachmentWrite:
    case RenderUsage::StorageWrite:
    case RenderUsage::ResolveDst:
    case RenderUsage::TransferDst:
      return true;
    default:
      return false;
  }
}

inline bool is_read_usage(RenderUsage usage) {
  switch (usage) {
    case RenderUsage::DepthAttachmentRead:
    case RenderUsage::SampledRead:
    case RenderUsage::StorageRead:
    case RenderUsage::ResolveSrc:
    case RenderUsage::TransferSrc:
      return true;
    default:
      return false;
  }
}

inline const char *render_usage_label(RenderUsage usage) {
  switch (usage) {
    case RenderUsage::ColorAttachmentWrite:
      return "color_write";
    case RenderUsage::DepthAttachmentWrite:
      return "depth_write";
    case RenderUsage::DepthAttachmentRead:
      return "depth_read";
    case RenderUsage::SampledRead:
      return "sampled";
    case RenderUsage::StorageRead:
      return "storage_read";
    case RenderUsage::StorageWrite:
      return "storage_write";
    case RenderUsage::ResolveSrc:
      return "resolve_src";
    case RenderUsage::ResolveDst:
      return "resolve_dst";
    case RenderUsage::TransferSrc:
      return "transfer_src";
    case RenderUsage::TransferDst:
      return "transfer_dst";
    case RenderUsage::Present:
      return "present";
  }
  return "unknown";
}

inline const char *resource_state_label(ResourceState state) {
  switch (state) {
    case ResourceState::Undefined:
      return "undefined";
    case ResourceState::ColorAttachment:
      return "color_attachment";
    case ResourceState::DepthWrite:
      return "depth_write";
    case ResourceState::DepthRead:
      return "depth_read";
    case ResourceState::ShaderSampled:
      return "shader_sampled";
    case ResourceState::StorageRead:
      return "storage_read";
    case ResourceState::StorageWrite:
      return "storage_write";
    case ResourceState::ResolveSrc:
      return "resolve_src";
    case ResourceState::ResolveDst:
      return "resolve_dst";
    case ResourceState::TransferSrc:
      return "transfer_src";
    case ResourceState::TransferDst:
      return "transfer_dst";
    case ResourceState::PresentSrc:
      return "present_src";
  }
  return "unknown";
}

} // namespace astralix
