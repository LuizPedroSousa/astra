#include "systems/render-system/passes/entity-selection-pass.hpp"

#include "components/tags.hpp"
#include "trace.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "path.hpp"
#include "renderer-api.hpp"
#include "systems/render-system/scene-selection.hpp"

namespace astralix {

void EntitySelectionPass::setup(
    Ref<RenderTarget> render_target,
    const std::vector<const RenderGraphResource *> &resources
) {
  m_render_target = render_target;
  m_scene_color = nullptr;
  set_enabled(true);

  for (const auto *resource : resources) {
    if (resource->desc.type == RenderGraphResourceType::Framebuffer &&
        resource->desc.name == "scene_color") {
      m_scene_color = resource->get_framebuffer();
    }
  }

  if (m_scene_color == nullptr || m_pick_id_lut == nullptr) {
    set_enabled(false);
    return;
  }

  Shader::create(
      "shaders::entity_selection",
      "shaders/entity_selection.axsl"_engine,
      "shaders/entity_selection.axsl"_engine
  );

  resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
      m_render_target->renderer_api()->get_backend(),
      {"shaders::entity_selection"}
  );
  m_shader = resource_manager()->get_by_descriptor_id<Shader>(
      "shaders::entity_selection"
  );

  if (m_shader == nullptr) {
    set_enabled(false);
  }
}

void EntitySelectionPass::begin(double) {}

void EntitySelectionPass::execute(double) {
  ASTRA_PROFILE_N("EntitySelectionPass");
  if (m_scene_color == nullptr || m_shader == nullptr ||
      m_pick_id_lut == nullptr) {
    return;
  }

  auto renderer_api = m_render_target->renderer_api();
  m_scene_color->bind();
  m_scene_color->clear_attachment(2, 0);
  m_pick_id_lut->clear();

  auto *scene = SceneManager::get()->get_active_scene();
  if (scene == nullptr) {
    m_scene_color->unbind();
    return;
  }

  auto &world = scene->world();
  auto camera = rendering::select_main_camera(world);
  if (!camera.has_value()) {
    m_scene_color->unbind();
    return;
  }

  renderer_api->enable_buffer_testing();
  renderer_api->enable_depth_test();
  renderer_api->disable_depth_write();
  renderer_api->disable_blend();
  renderer_api->depth(RendererAPI::DepthMode::LessEqual);

  world.each<rendering::Renderable, scene::Transform>(
      [&](EntityID entity_id, rendering::Renderable &, scene::Transform &transform) {
        if (!world.active(entity_id)) {
          return;
        }

        auto entity = world.entity(entity_id);
        auto *model_ref = entity.get<rendering::ModelRef>();
        auto *mesh_set = entity.get<rendering::MeshSet>();
        if (model_ref == nullptr && mesh_set == nullptr) {
          return;
        }

        m_pick_id_lut->push_back(entity_id);
        const int pick_id = static_cast<int>(m_pick_id_lut->size());

        m_shader->bind();
        m_shader->set_matrix("transform.model", transform.matrix);
        m_shader->set_matrix("camera.view", camera->camera->view_matrix);
        m_shader->set_matrix(
            "camera.projection", camera->camera->projection_matrix
        );
        m_shader->set_int("selection.entity_id", pick_id);

        rendering::for_each_render_mesh(
            model_ref,
            mesh_set,
            m_render_target,
            [&](Mesh &mesh) {
              renderer_api->draw_indexed(mesh.vertex_array, mesh.draw_type);
            }
        );

        m_shader->unbind();
      }
  );

  renderer_api->enable_depth_write();
  renderer_api->depth(RendererAPI::DepthMode::Less);
  m_scene_color->unbind();
}

void EntitySelectionPass::end(double) {}

void EntitySelectionPass::cleanup() {
  if (m_pick_id_lut != nullptr) {
    m_pick_id_lut->clear();
  }

  m_scene_color = nullptr;
  m_shader.reset();
}

} // namespace astralix
