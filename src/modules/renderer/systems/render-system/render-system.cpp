#include "render-system.hpp"
#include "components/post-processing/post-processing-component.hpp"
#include "entities/object.hpp"
#include "entities/post-processing.hpp"
#include "events/event-scheduler.hpp"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "managers/window-manager.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "systems/render-system/collectors/mesh-collector.hpp"
#include "systems/render-system/passes/debug-pass.hpp"
#include "systems/render-system/passes/exporters/ascii-exporter.hpp"
#include "systems/render-system/passes/exporters/graphviz-exporter.hpp"
#include "systems/render-system/passes/exporters/mermaid-exporter.hpp"
#include "systems/render-system/passes/geometry-pass.hpp"
#include "systems/render-system/passes/mesh-pass.hpp"
#include "systems/render-system/passes/post-process-pass.hpp"
#include "systems/render-system/passes/render-graph-builder.hpp"
#include "systems/render-system/passes/shadow-pass.hpp"
#include "systems/render-system/passes/skybox-pass.hpp"
#include "systems/render-system/passes/text-pass.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"

namespace astralix {
RenderSystem::RenderSystem(RenderSystemConfig &config) : m_config(config) {};

void RenderSystem::start() {
  m_render_target = RenderTarget::create(m_config.backend_to_api(),
                                         m_config.msaa_to_render_target_msaa(),
                                         m_config.window_id);

  auto entity_manager = EntityManager::get();

  m_render_target->init();

  resource_manager()
      ->load_from_descriptors<ShaderDescriptor, Texture2DDescriptor,
                              Texture3DDescriptor, MaterialDescriptor,
                              FontDescriptor>(
          m_render_target->renderer_api()->get_backend());

  RenderGraphBuilder target_graph;

  auto window =
      window_manager()->get_window_by_id(m_render_target->window_id());

  auto mesh_context =
      target_graph.declare_logical_buffer<MeshContext>("mesh_context");

  auto mesh_collector_storage = target_graph.declare_storage_buffer(
      "mesh_collector_storage", 1024 * sizeof(glm::mat4));

  auto shadow_map = target_graph.declare_framebuffer(
      "shadow_map", window->width(), window->height(),
      FramebufferTextureFormat::DEPTH_ONLY);

  auto scene_color = target_graph.import_persistent_framebuffer(
      "scene_color", m_render_target->framebuffer().get());

  target_graph.add_pass(create_scope<SkyboxPass>()).write(scene_color);
  target_graph.add_pass(create_scope<ShadowPass>()).write(shadow_map);
  target_graph.add_pass(create_scope<GeometryPass>())
      .read(shadow_map)
      .write(scene_color);
  target_graph.add_pass(create_scope<MeshPass>())
      .read_write(mesh_context)
      .read_write(mesh_collector_storage)
      .read_write(scene_color);
  target_graph.add_pass(create_scope<TextPass>()).read_write(scene_color);
  target_graph.add_pass(create_scope<DebugPass>())
      .read(shadow_map)
      .read_write(mesh_context)
      .read_write(mesh_collector_storage)
      .read_write(scene_color);
  target_graph.add_pass(create_scope<PostProcessPass>())
      .read_write(mesh_context)
      .read_write(mesh_collector_storage)
      .read_write(scene_color);

  m_render_graph = target_graph.build();
  m_render_graph->compile(m_render_target);

  GraphvizExporter graphviz_exporter;
  m_render_graph->export_graph(graphviz_exporter, "render_graph.dot");
  MermaidExporter mermaid_exporter;
  m_render_graph->export_graph(mermaid_exporter, "render_graph.md");
  AsciiExporter ascii_exporter;
  m_render_graph->export_graph(ascii_exporter, "render_graph.txt");
}

void RenderSystem::fixed_update(double fixed_dt) {
  ASTRA_PROFILE_N("RenderSystem FixedUpdate");
};

void RenderSystem::pre_update(double dt) {
  ASTRA_PROFILE_N("RenderSystem PreUpdate");

  auto entity_manager = EntityManager::get();
  auto post_processings = entity_manager->get_entities<PostProcessing>();

  bool has_post_processing = false;
  for (auto post_processing : post_processings) {
    if (!post_processing->is_active()) {
      continue;
    }

    auto comp = post_processing->get_component<PostProcessingComponent>();

    if (comp != nullptr && comp->is_active()) {
      has_post_processing = true;
      break;
    }
  }

  m_render_target->bind(has_post_processing);
};

void RenderSystem::update(double dt) {
  ASTRA_PROFILE_N("RenderSystem Update");

  if (m_render_graph) {
    m_render_graph->execute(dt);
  }

  auto scheduler = EventScheduler::get();
  scheduler->bind(SchedulerType::REALTIME);
};

RenderSystem::~RenderSystem() {}

} // namespace astralix
