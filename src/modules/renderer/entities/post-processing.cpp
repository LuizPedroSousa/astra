#include "post-processing.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/post-processing/post-processing-component.hpp"
#include "components/resource/resource-component.hpp"
#include "glad/glad.h"
#include "guid.hpp"
#include "renderer-api.hpp"

namespace astralix {

PostProcessing::PostProcessing(ENTITY_INIT_PARAMS,
                               ResourceDescriptorID shader_descriptor_id)
    : ENTITY_INIT(), shader_descriptor_id(shader_descriptor_id) {
  add_component<PostProcessingComponent>();
  add_component<ResourceComponent>();
  add_component<MeshComponent>();
}

void PostProcessing::start(Ref<RenderTarget> render_target) {}

void PostProcessing::post_update(Ref<RenderTarget> render_target) {}

} // namespace astralix
