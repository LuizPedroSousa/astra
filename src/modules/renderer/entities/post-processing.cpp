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
    : ENTITY_INIT(), m_shader_descriptor_id(shader_descriptor_id) {
  add_component<PostProcessingComponent>();
  add_component<ResourceComponent>();
  add_component<MeshComponent>();
}

void PostProcessing::start(Ref<RenderTarget> render_target) {
  add_component<MeshComponent>()->attach_mesh(Mesh::quad(1.0f));
  add_component<ResourceComponent>()->attach_shader(m_shader_descriptor_id);
  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();
  auto post_processing = get_component<PostProcessingComponent>();

  resource->start();

  post_processing->start(render_target);
  mesh->start(render_target);
}

void PostProcessing::post_update(Ref<RenderTarget> render_target) {
  render_target->framebuffer()->bind(FramebufferBindType::Default, 0);
  render_target->renderer_api()->disable_buffer_testing();
  render_target->renderer_api()->clear_color();
  render_target->renderer_api()->clear_buffers();

  auto resource = get_component<ResourceComponent>();
  auto post_processing = get_component<PostProcessingComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->update();
  post_processing->post_update(render_target);
  mesh->update(render_target);
}

} // namespace astralix
