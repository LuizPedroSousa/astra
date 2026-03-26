#include "render-system.hpp"
#include "events/event-scheduler.hpp"
#include "framebuffer.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/window-manager.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "systems/render-system/passes/debug-pass.hpp"
#include "systems/render-system/passes/exporters/ascii-exporter.hpp"
#include "systems/render-system/passes/exporters/graphviz-exporter.hpp"
#include "systems/render-system/passes/exporters/mermaid-exporter.hpp"
#include "systems/render-system/passes/forward-pass.hpp"
#include "systems/render-system/passes/geometry-pass.hpp"
#include "systems/render-system/passes/grid-pass.hpp"
#include "systems/render-system/passes/lighting-pass.hpp"
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

  m_render_target->init();

  resource_manager()
      ->load_from_descriptors<ModelDescriptor, ShaderDescriptor,
                              Texture2DDescriptor, Texture3DDescriptor,
                              MaterialDescriptor, FontDescriptor>(
          m_render_target->renderer_api()->get_backend());

  RenderGraphBuilder target_graph;

  auto window =
      window_manager()->get_window_by_id(m_render_target->window_id());

  auto window_width = static_cast<uint32_t>(window->width());
  auto window_height = static_cast<uint32_t>(window->height());

  auto shadow_map = target_graph.declare_framebuffer(
      "shadow_map", {.width = window_width,
                     .height = window_height,
                     .attachments = {FramebufferTextureFormat::DEPTH_ONLY},
                     .extent = {.mode = RenderExtentMode::WindowRelative}});

  auto scene_color = target_graph.import_persistent_framebuffer(
      "scene_color", m_render_target->framebuffer().get(),
      {.mode = RenderExtentMode::WindowRelative});

  const bool use_forward_strategy = m_config.strategy == "forward";
  uint32_t g_buffer = 0;
  bool has_g_buffer = false;

  if (!use_forward_strategy && !m_config.strategy.empty() &&
      m_config.strategy != "deferred") {
    LOG_WARN("Unknown render strategy '", m_config.strategy,
             "', defaulting to deferred");
  }

  target_graph.add_pass(create_scope<ShadowPass>()).write(shadow_map);

  if (use_forward_strategy) {
    target_graph.add_pass(create_scope<ForwardPass>())
        .read(shadow_map)
        .read_write(scene_color);
  } else {
    g_buffer = target_graph.declare_framebuffer(
        "g_buffer",
        {

            .width = window_width,
            .height = window_height,
            .attachments =
                {
                    FramebufferTextureSpecification(
                        "g_position", FramebufferTextureFormat::RGBA16F),
                    FramebufferTextureSpecification(
                        "g_normal", FramebufferTextureFormat::RGBA16F),
                    FramebufferTextureSpecification(
                        "g_albedo", FramebufferTextureFormat::RGBA16F),
                    FramebufferTextureFormat::Depth,
                },
            .extent = {.mode = RenderExtentMode::WindowRelative}});
    has_g_buffer = true;

    target_graph.add_pass(create_scope<GeometryPass>())
        .read_write(g_buffer)
        .write(scene_color);
    target_graph.add_pass(create_scope<LightingPass>())
        .read(shadow_map)
        .read(g_buffer)
        .read_write(scene_color);
    target_graph.add_pass(create_scope<DebugGBufferPass>())
        .read(g_buffer)
        .read_write(scene_color);
  }

  target_graph.add_pass(create_scope<SkyboxPass>()).read_write(scene_color);
  target_graph.add_pass(create_scope<GridPass>()).read_write(scene_color);
  target_graph.add_pass(create_scope<TextPass>()).read_write(scene_color);
  target_graph.add_pass(create_scope<DebugOverlayPass>())
      .read(shadow_map)
      .read_write(scene_color);

  target_graph.add_pass(create_scope<PostProcessPass>())
      .read_write(scene_color);

  m_render_graph = target_graph.build();
  m_render_graph->compile(m_render_target);

  // GraphvizExporter graphviz_exporter;
  // m_render_graph->export_graph(graphviz_exporter, "render_graph.dot");
  // MermaidExporter mermaid_exporter;
  // m_render_graph->export_graph(mermaid_exporter, "render_graph.md");
  // AsciiExporter ascii_exporter;
  // m_render_graph->export_graph(ascii_exporter, "render_graph.txt");
}

void RenderSystem::fixed_update(double fixed_dt) {
  ASTRA_PROFILE_N("RenderSystem FixedUpdate");
};

void RenderSystem::pre_update(double dt) {
  ASTRA_PROFILE_N("RenderSystem PreUpdate");

  auto window = window_manager()->active_window();

  if (window->was_resized && m_render_graph != nullptr) {
    m_render_graph->resize(static_cast<uint32_t>(window->width()),
                           static_cast<uint32_t>(window->height()));
  }

  m_render_target->bind(true);
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
