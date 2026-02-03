
#include "hdr-system.hpp"
#include "entities/post-processing.hpp"
#include "glad//glad.h"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "path.hpp"
#include "resources/shader.hpp"
#include "targets/render-target.hpp"

namespace astralix {

HDRSystem::HDRSystem(Ref<RenderTarget> render_target)
    : m_render_target(render_target) {}

void HDRSystem::pre_update(double dt) {}

void HDRSystem::fixed_update(double fixed_dt) {}

void HDRSystem::start() {
  Shader::create("shaders::hdr", "shaders/fragment/hdr.glsl"_engine,
                 "shaders/vertex/postprocessing.glsl"_engine);

  resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
      m_render_target->renderer_api()->get_backend(), {"shaders::hdr"});

  EntityManager::get()->add_entity<PostProcessing>("HDR", "shaders::hdr");
}

void HDRSystem::update(double dt) {}

HDRSystem::~HDRSystem() {}

} // namespace astralix
