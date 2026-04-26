#pragma once

#include "render-graph-usage.hpp"
#include <cstdint>
#include <optional>

namespace astralix {

struct CompiledTransition {
  RenderImageSubresourceRef view{};
  ResourceState before = ResourceState::Undefined;
  ResourceState after = ResourceState::Undefined;
};

struct CompiledExportImage {
  RenderImageExportKey key{};
  RenderImageSubresourceRef source{};
  bool direct_bind = false;
  bool needs_materialize = false;
  std::optional<uint32_t> materialized_resource_index{};
};

struct CompiledPresentEdge {
  uint32_t resource_index = 0;
  ImageAspect aspect = ImageAspect::Color0;
  uint32_t producer_pass_index = 0;
};

} // namespace astralix
