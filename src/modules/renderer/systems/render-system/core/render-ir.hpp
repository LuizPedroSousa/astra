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

using RenderCommand = std::variant<
    BeginRenderingCmd,
    EndRenderingCmd,
    BindPipelineCmd,
    BindBindingsCmd,
    BindVertexBufferCmd,
    BindIndexBufferCmd,
    DrawIndexedCmd,
    CopyImageCmd,
    ResolveImageCmd,
    ReadbackImageCmd,
    SetScissorCmd,
    DrawVerticesCmd>;

using RenderCommandBuffer = std::vector<RenderCommand>;

} // namespace astralix
