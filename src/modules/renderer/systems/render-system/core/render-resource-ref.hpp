#pragma once

#include "render-types.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astralix {

enum class ImageAspect : uint8_t {
  Color0,
  Color1,
  Color2,
  Color3,
  Depth,
  Stencil,
};

enum class AttachmentLoadOp : uint8_t {
  Load,
  Clear,
  DontCare
};

enum class AttachmentStoreOp : uint8_t {
  Store,
  DontCare
};

struct ImageExtent {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 1;
};

struct ImageViewRef {
  ImageHandle image{};
  ImageAspect aspect = ImageAspect::Color0;
  uint32_t mip = 0;
  uint32_t layer = 0;
};

struct BufferSlice {
  BufferHandle buffer{};
  uint32_t offset = 0;
  uint32_t size = 0;
};

struct ColorAttachmentRef {
  ImageViewRef view{};
  AttachmentLoadOp load_op = AttachmentLoadOp::Load;
  AttachmentStoreOp store_op = AttachmentStoreOp::Store;
  std::array<float, 4> clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct DepthStencilAttachmentRef {
  ImageViewRef view{};
  AttachmentLoadOp depth_load_op = AttachmentLoadOp::Load;
  AttachmentStoreOp depth_store_op = AttachmentStoreOp::Store;
  float clear_depth = 1.0f;
  AttachmentLoadOp stencil_load_op = AttachmentLoadOp::Load;
  AttachmentStoreOp stencil_store_op = AttachmentStoreOp::Store;
  uint32_t clear_stencil = 0;
};

struct RenderingInfo {
  std::string debug_name;
  ImageExtent extent{};
  std::vector<ColorAttachmentRef> color_attachments;
  std::optional<DepthStencilAttachmentRef> depth_stencil_attachment;
};

struct CopyRegion {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 1;
  uint32_t src_mip = 0;
  uint32_t dst_mip = 0;
  uint32_t src_layer = 0;
  uint32_t dst_layer = 0;

  static CopyRegion full(const ImageExtent &extent) {
    return CopyRegion{
        .width = extent.width,
        .height = extent.height,
        .depth = extent.depth,
    };
  }
};

} // namespace astralix
