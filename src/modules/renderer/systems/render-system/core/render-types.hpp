#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astralix {

template <typename Tag>
struct RenderHandle {
  uint32_t id = 0;

  [[nodiscard]] constexpr bool valid() const noexcept { return id != 0; }
  constexpr explicit operator bool() const noexcept { return valid(); }

  friend constexpr bool operator==(RenderHandle, RenderHandle) = default;
};

using ImageHandle = RenderHandle<struct ImageTag>;
using BufferHandle = RenderHandle<struct BufferTag>;
using SamplerHandle = RenderHandle<struct SamplerTag>;
using ShaderProgramHandle = RenderHandle<struct ShaderProgramTag>;
using RenderPipelineHandle = RenderHandle<struct RenderPipelineTag>;
using RenderBindingLayoutHandle = RenderHandle<struct RenderBindingLayoutTag>;
using RenderBindingGroupHandle = RenderHandle<struct RenderBindingGroupTag>;
using VertexInputLayoutHandle = RenderHandle<struct VertexInputLayoutTag>;

enum class RenderBindingScope : uint8_t {
  Frame,
  Pass,
  Material,
  Draw,
};

enum class RenderBindingCachePolicy : uint8_t {
  Auto,
  Reuse,
  Ephemeral,
};

enum class RenderBindingSharing : uint8_t {
  LocalOnly,
  PipelineLayoutCompatible,
};

enum class RenderBindingStability : uint8_t {
  Stable,
  FrameLocal,
  Transient,
};

struct RenderBindingLayoutKey {
  std::string shader_descriptor_id;
  uint32_t descriptor_set_index = 0;

  friend bool operator==(const RenderBindingLayoutKey &,
                         const RenderBindingLayoutKey &) = default;
};

struct RenderBindingReuseIdentity {
  RenderBindingSharing sharing = RenderBindingSharing::LocalOnly;
  std::string cache_namespace;
  uint64_t stable_tag = 0;

  friend bool operator==(const RenderBindingReuseIdentity &,
                         const RenderBindingReuseIdentity &) = default;
};

enum class RenderGraphPassType : uint8_t {
  Graphics,
  Compute,
  Transfer
};

enum class ShaderStage : uint32_t {
  None = 0,
  Vertex = 1u << 0u,
  Fragment = 1u << 1u,
  Compute = 1u << 2u,
};

inline ShaderStage operator|(ShaderStage lhs, ShaderStage rhs) {
  return static_cast<ShaderStage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline ShaderStage &operator|=(ShaderStage &lhs, ShaderStage rhs) {
  lhs = lhs | rhs;
  return lhs;
}

enum class ImageFormat : uint16_t {
  Undefined,
  RGBA8,
  RGBA16F,
  RGBA32F,
  R32I,
  Depth24Stencil8,
  Depth32F,
};

enum class ImageUsage : uint32_t {
  None = 0,
  Sampled = 1u << 0u,
  ColorAttachment = 1u << 1u,
  DepthStencilAttachment = 1u << 2u,
  TransferSrc = 1u << 3u,
  TransferDst = 1u << 4u,
  Readback = 1u << 5u,
};

inline ImageUsage operator|(ImageUsage lhs, ImageUsage rhs) {
  return static_cast<ImageUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline ImageUsage &operator|=(ImageUsage &lhs, ImageUsage rhs) {
  lhs = lhs | rhs;
  return lhs;
}

enum class ImageExtentMode : uint8_t {
  Absolute,
  WindowRelative,
};

struct ImageExtentDesc {
  ImageExtentMode mode = ImageExtentMode::Absolute;
  uint32_t width = 0;
  uint32_t height = 0;
  float scale_x = 1.0f;
  float scale_y = 1.0f;
};

enum class BufferUsage : uint32_t {
  None = 0,
  Vertex = 1u << 0u,
  Index = 1u << 1u,
  Uniform = 1u << 2u,
  Storage = 1u << 3u,
  TransferSrc = 1u << 4u,
  TransferDst = 1u << 5u,
  Readback = 1u << 6u,
};

inline BufferUsage operator|(BufferUsage lhs, BufferUsage rhs) {
  return static_cast<BufferUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline BufferUsage &operator|=(BufferUsage &lhs, BufferUsage rhs) {
  lhs = lhs | rhs;
  return lhs;
}

enum class BindingResourceType : uint8_t {
  UniformBuffer,
  StorageBuffer,
  SampledImage,
  StorageImage,
  Sampler,
};

enum class CullMode : uint8_t {
  None,
  Front,
  Back
};

enum class FrontFace : uint8_t {
  Clockwise,
  CounterClockwise
};

enum class CompareOp : uint8_t {
  Never,
  Less,
  LessEqual,
  Equal,
  Greater,
  Always,
};

enum class BlendFactor : uint8_t {
  Zero,
  One,
  SrcAlpha,
  OneMinusSrcAlpha,
};

enum class IndexType : uint8_t {
  Uint16,
  Uint32
};

struct ImageDesc {
  std::string debug_name;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 1;
  uint32_t mip_levels = 1;
  uint32_t samples = 1;
  ImageFormat format = ImageFormat::Undefined;
  ImageUsage usage = ImageUsage::None;
  bool persistent = false;
  ImageExtentDesc extent{};
};

struct BufferDesc {
  std::string debug_name;
  uint32_t size = 0;
  BufferUsage usage = BufferUsage::None;
  bool persistent = false;
};

struct RenderBindingElementDesc {
  std::string debug_name;
  BindingResourceType type = BindingResourceType::UniformBuffer;
  uint32_t binding = 0;
  uint32_t array_count = 1;
  ShaderStage stages = ShaderStage::None;
};

struct RenderBindingLayoutDesc {
  std::string debug_name;
  std::vector<RenderBindingElementDesc> elements;
};

struct RenderBindingBufferDesc {
  uint32_t binding = 0;
  BufferHandle buffer{};
  uint32_t offset = 0;
  uint32_t size = 0;
};

struct RenderBindingImageDesc {
  uint32_t binding = 0;
  ImageHandle image{};
  SamplerHandle sampler{};
};

struct RenderBindingSetDesc {
  std::string debug_name;
  RenderBindingLayoutHandle layout{};
  std::vector<RenderBindingBufferDesc> buffers;
  std::vector<RenderBindingImageDesc> images;
};

struct DepthBiasState {
  bool enabled = false;
  float constant_factor = 0.0f;
  float slope_factor = 0.0f;
};

struct RasterState {
  CullMode cull_mode = CullMode::Back;
  FrontFace front_face = FrontFace::CounterClockwise;
  DepthBiasState depth_bias{};
};

struct DepthStencilState {
  bool depth_test = true;
  bool depth_write = true;
  CompareOp compare_op = CompareOp::Less;
};

struct BlendAttachmentState {
  bool enabled = false;
  BlendFactor src = BlendFactor::One;
  BlendFactor dst = BlendFactor::Zero;

  static BlendAttachmentState replace() { return {}; }

  static BlendAttachmentState alpha_blend() {
    BlendAttachmentState state;
    state.enabled = true;
    state.src = BlendFactor::SrcAlpha;
    state.dst = BlendFactor::OneMinusSrcAlpha;
    return state;
  }
};

struct RenderPipelineDesc {
  std::string debug_name;
  ShaderProgramHandle shader{};
  VertexInputLayoutHandle vertex_input{};
  std::vector<ImageFormat> color_formats;
  ImageFormat depth_format = ImageFormat::Undefined;
  RasterState raster{};
  DepthStencilState depth_stencil{};
  std::vector<BlendAttachmentState> blend_attachments;
};

} // namespace astralix
