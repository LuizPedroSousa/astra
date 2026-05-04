#pragma once

#include "render-resource-ref.hpp"
#include "render-types.hpp"
#include <cstdint>
#include <variant>
#include <vector>

namespace astralix {

struct BeginRenderingCmd {
  RenderingInfo info;
};

struct EndRenderingCmd {};

struct BindPipelineCmd {
  RenderPipelineHandle pipeline{};
};

struct BindComputePipelineCmd {
  RenderPipelineHandle pipeline{};
};

struct BindBindingsCmd {
  RenderBindingGroupHandle binding_group{};
};

struct BindVertexBufferCmd {
  BufferHandle buffer{};
  uint32_t slot = 0;
  uint32_t offset = 0;
};

struct BindIndexBufferCmd {
  BufferHandle buffer{};
  IndexType index_type = IndexType::Uint32;
  uint32_t offset = 0;
};

struct DrawIndexedArgs {
  uint32_t index_count = 0;
  uint32_t instance_count = 1;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  uint32_t first_instance = 0;
};

struct DrawIndexedCmd {
  DrawIndexedArgs args{};
};

enum class MemoryBarrierBit : uint32_t {
  None = 0,
  ShaderStorage = 1u << 0u,
};

inline constexpr MemoryBarrierBit
operator|(MemoryBarrierBit lhs, MemoryBarrierBit rhs) {
  return static_cast<MemoryBarrierBit>(
      static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)
  );
}

inline constexpr bool has_memory_barrier_bit(
    MemoryBarrierBit mask,
    MemoryBarrierBit bit
) {
  return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(bit)) != 0u;
}

struct DispatchComputeCmd {
  uint32_t group_count_x = 1;
  uint32_t group_count_y = 1;
  uint32_t group_count_z = 1;
};

struct MemoryBarrierCmd {
  MemoryBarrierBit barriers = MemoryBarrierBit::None;
};

struct CopyImageCmd {
  ImageHandle src{};
  ImageHandle dst{};
  CopyRegion region{};
};

struct ResolveImageCmd {
  ImageHandle src{};
  ImageHandle dst{};
};

struct ReadbackImageCmd {
  ImageHandle src{};
  int x = 0;
  int y = 0;
  int *out_value = nullptr;
  bool *out_ready = nullptr;
};

struct SetScissorCmd {
  bool enabled = true;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

struct DrawVerticesCmd {
  uint32_t vertex_count = 0;
  uint32_t first_vertex = 0;
};

struct SetViewportCmd {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

using RenderCommand = std::variant<
    BeginRenderingCmd,
    EndRenderingCmd,
    BindPipelineCmd,
    BindComputePipelineCmd,
    BindBindingsCmd,
    BindVertexBufferCmd,
    BindIndexBufferCmd,
    DrawIndexedCmd,
    DispatchComputeCmd,
    MemoryBarrierCmd,
    CopyImageCmd,
    ResolveImageCmd,
    ReadbackImageCmd,
    SetScissorCmd,
    DrawVerticesCmd,
    SetViewportCmd>;

using RenderCommandBuffer = std::vector<RenderCommand>;

} // namespace astralix
