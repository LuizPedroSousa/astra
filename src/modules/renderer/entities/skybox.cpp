
#include "skybox.hpp"
#include "base.hpp"
#include "components/camera/camera-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "entities/ientity.hpp"

#include "glad/glad.h"
#include "guid.hpp"

namespace astralix {

Skybox::Skybox(ENTITY_INIT_PARAMS, const ResourceDescriptorID cubemap_id,
               const ResourceDescriptorID shader_id)
    : ENTITY_INIT(), m_cubemap_id(cubemap_id), m_shader_id(shader_id) {
  add_component<ResourceComponent>();
  add_component<MeshComponent>();
}

void Skybox::start(Ref<RenderTarget> render_target) {
  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  resource->attach_cubemap({m_cubemap_id, "_skybox"});
  resource->set_shader(m_shader_id);

  resource->start();
  mesh->attach_mesh(Mesh::cube(2.0f));
  mesh->start(render_target);
}

void Skybox::pre_update() { glDepthFunc(GL_LEQUAL); }

void Skybox::update(Ref<RenderTarget> render_target) {
  CHECK_ACTIVE(this);
  auto entity_manager = EntityManager::get();

  // if (!entity_manager->has_entity_with_component<CameraComponent>()) {
  //   return;
  // }

  auto resource = get_component<ResourceComponent>();
  auto mesh = get_component<MeshComponent>();

  auto component_manager = ComponentManager::get();

  auto camera = component_manager->get_components<CameraComponent>()[0];

  resource->update();

  if (camera != nullptr) {
    auto shader = resource->shader();

    auto view_without_transformation =
        glm::mat4(glm::mat3(camera->get_view_matrix()));
    shader->set_matrix("view_without_transformation",
                       view_without_transformation);
    shader->set_matrix("projection", camera->get_projection_matrix());
  }

  mesh->update(render_target);
}

void Skybox::post_update() { glDepthFunc(GL_LESS); }

} // namespace astralix
