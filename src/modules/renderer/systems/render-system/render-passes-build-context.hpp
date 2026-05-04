#pragma once

#include "systems/render-system/eye-adaptation.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix {

class RenderGraphBuilder;
struct RenderGraphPassConfig;

namespace rendering {
struct ResolvedMeshDraw;
struct EntityPickReadbackRequest;
} // namespace rendering

struct RenderPassesBuildContext {
  RenderGraphBuilder *builder;
  const std::vector<RenderGraphPassConfig> *pass_configs;
  const std::unordered_map<std::string, uint32_t> *resource_indices;
  const rendering::ResolvedMeshDraw *skybox_cube;
  const rendering::ResolvedMeshDraw *fullscreen_quad;
  EyeAdaptationState *eye_adaptation_state;
  rendering::EntityPickReadbackRequest *entity_pick_request;
};

} // namespace astralix
