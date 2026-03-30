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
#include "systems/render-system/passes/ssao-pass.hpp"
#include "systems/render-system/passes/text-pass.hpp"
#include "systems/render-system/passes/ui-pass.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"

namespace astralix {
RenderSystem::RenderSystem(RenderSystemConfig &config) : m_config(config) {};

void RenderSystem::start() {
  m_render_target = RenderTarget::create(m_config.backend_to_api(), m_config.msaa_to_render_target_msaa(), m_config.window_id);

  m_render_target->init();

  resource_manager()
      ->load_from_descriptors<ModelDescriptor, ShaderDescriptor, Texture2DDescriptor, Texture3DDescriptor, MaterialDescriptor, FontDescriptor>(
          m_render_target->renderer_api()->get_backend()
      );

  RenderGraphBuilder target_graph;

  auto window =
      window_manager()->get_window_by_id(m_render_target->window_id());

  auto window_width = static_cast<uint32_t>(window->width());
  auto window_height = static_cast<uint32_t>(window->height());

  m_shadow_map_resource_index = target_graph.declare_framebuffer(
      "shadow_map", {.width = window_width, .height = window_height, .attachments = {FramebufferTextureFormat::DEPTH_ONLY}, .extent = {.mode = RenderExtentMode::WindowRelative}}
  );

  m_scene_color_resource_index = target_graph.import_persistent_framebuffer(
      "scene_color", m_render_target->framebuffer().get(), {.mode = RenderExtentMode::WindowRelative}
  );

  const bool use_forward_strategy = m_config.strategy == "forward";
  m_g_buffer_resource_index = 0;
  m_has_g_buffer = false;

  if (!use_forward_strategy && !m_config.strategy.empty() &&
      m_config.strategy != "deferred") {
    LOG_WARN("Unknown render strategy '", m_config.strategy, "', defaulting to deferred");
  }

  target_graph.add_pass(create_scope<ShadowPass>())
      .write(m_shadow_map_resource_index);

  if (use_forward_strategy) {
    target_graph.add_pass(create_scope<ForwardPass>())
        .read(m_shadow_map_resource_index)
        .read_write(m_scene_color_resource_index);
  } else {
    m_g_buffer_resource_index = target_graph.declare_framebuffer(
        "g_buffer",
        {

            .width = window_width,
            .height = window_height,
            .attachments =
                {
                    FramebufferTextureSpecification(
                        "g_position", FramebufferTextureFormat::RGBA16F
                    ),
                    FramebufferTextureSpecification(
                        "g_normal", FramebufferTextureFormat::RGBA16F
                    ),
                    FramebufferTextureSpecification(
                        "g_albedo", FramebufferTextureFormat::RGBA16F
                    ),
                    FramebufferTextureFormat::Depth,
                },
            .extent = {.mode = RenderExtentMode::WindowRelative}
        }
    );
    m_has_g_buffer = true;

    target_graph.add_pass(create_scope<GeometryPass>())
        .read_write(m_g_buffer_resource_index)
        .write(m_scene_color_resource_index);

    target_graph.add_pass(create_scope<SSAOPass>())
        .read(m_g_buffer_resource_index)
        .read_write(m_scene_color_resource_index);

    target_graph.add_pass(create_scope<LightingPass>())
        .read(m_shadow_map_resource_index)
        .read(m_g_buffer_resource_index)
        .read_write(m_scene_color_resource_index);
    target_graph.add_pass(create_scope<DebugGBufferPass>())
        .read(m_g_buffer_resource_index)
        .read_write(m_scene_color_resource_index);
  }

  target_graph.add_pass(create_scope<SkyboxPass>())
      .read_write(m_scene_color_resource_index);
  target_graph.add_pass(create_scope<GridPass>())
      .read_write(m_scene_color_resource_index);
  target_graph.add_pass(create_scope<TextPass>())
      .read_write(m_scene_color_resource_index);
  target_graph.add_pass(create_scope<DebugOverlayPass>())
      .read(m_shadow_map_resource_index)
      .read_write(m_scene_color_resource_index);

  target_graph.add_pass(create_scope<PostProcessPass>())
      .read_write(m_scene_color_resource_index);

  target_graph.add_pass(create_scope<UIPass>())
      .read(m_scene_color_resource_index);

  m_render_graph = target_graph.build();
  m_render_graph->compile(m_render_target);
  rebuild_render_image_exports();

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
    m_render_graph->resize(static_cast<uint32_t>(window->width()), static_cast<uint32_t>(window->height()));
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

std::optional<ResolvedRenderImage>
RenderSystem::resolve_render_image(RenderImageExportKey key) const {
  const auto *binding = find_render_image_export(key);
  if (binding == nullptr || !binding->available || m_render_graph == nullptr) {
    return std::nullopt;
  }

  const auto *resource = m_render_graph->resource_at(binding->resource_index);
  if (resource == nullptr) {
    return std::nullopt;
  }

  switch (binding->resolve_mode) {
    case RenderImageResolveMode::DirectColorAttachment:
      return resolve_direct_color_attachment(
          *resource, binding->attachment_index
      );
    case RenderImageResolveMode::DirectDepthAttachment:
      return resolve_direct_depth_attachment(*resource);
    case RenderImageResolveMode::Materialize:
      return resolve_materialized_render_image(*binding, *resource);
  }

  return std::nullopt;
}

void RenderSystem::rebuild_render_image_exports() {
  m_render_image_exports.clear();

  auto add_export =
      [this](RenderImageExportKey key, uint32_t resource_index, uint32_t attachment_index, RenderImageResolveMode resolve_mode, bool available = true) {
        m_render_image_exports.push_back(RenderImageExportBinding{
            .key = key,
            .resource_index = resource_index,
            .attachment_index = attachment_index,
            .available = available,
            .resolve_mode = resolve_mode,
        });
      };

  add_export(
      RenderImageExportKey{
          .resource = RenderImageResource::SceneColor,
          .aspect = RenderImageAspect::Color0,
      },
      m_scene_color_resource_index,
      0,
      RenderImageResolveMode::DirectColorAttachment
  );

  if (m_has_g_buffer) {
    add_export(
        RenderImageExportKey{
            .resource = RenderImageResource::GBuffer,
            .aspect = RenderImageAspect::Color0,
        },
        m_g_buffer_resource_index,
        0,
        RenderImageResolveMode::DirectColorAttachment
    );
    add_export(
        RenderImageExportKey{
            .resource = RenderImageResource::GBuffer,
            .aspect = RenderImageAspect::Color1,
        },
        m_g_buffer_resource_index,
        1,
        RenderImageResolveMode::DirectColorAttachment
    );
    add_export(
        RenderImageExportKey{
            .resource = RenderImageResource::GBuffer,
            .aspect = RenderImageAspect::Color2,
        },
        m_g_buffer_resource_index,
        2,
        RenderImageResolveMode::DirectColorAttachment
    );
  }

  add_export(
      RenderImageExportKey{
          .resource = RenderImageResource::ShadowMap,
          .aspect = RenderImageAspect::Depth,
      },
      m_shadow_map_resource_index,
      0,
      RenderImageResolveMode::DirectDepthAttachment
  );

  add_export(
      RenderImageExportKey{
          .resource = RenderImageResource::FinalOutput,
          .aspect = RenderImageAspect::Color0,
      },
      0,
      0,
      RenderImageResolveMode::Materialize,
      false
  );
}

const RenderImageExportBinding *
RenderSystem::find_render_image_export(RenderImageExportKey key) const {
  for (const auto &binding : m_render_image_exports) {
    if (binding.key == key) {
      return &binding;
    }
  }

  return nullptr;
}

std::optional<ResolvedRenderImage>
RenderSystem::resolve_direct_color_attachment(
    const RenderGraphResource &resource, uint32_t attachment_index
) const {
  auto *framebuffer = resource.get_framebuffer();
  if (framebuffer == nullptr) {
    return std::nullopt;
  }

  const auto &attachments = framebuffer->get_color_attachments();
  if (attachment_index >= attachments.size()) {
    return std::nullopt;
  }

  const uint32_t texture_id = attachments[attachment_index];
  if (texture_id == 0) {
    return std::nullopt;
  }

  const auto &spec = framebuffer->get_specification();
  return ResolvedRenderImage{
      .available = true,
      .target = RenderImageTarget::Texture2D,
      .renderer_texture_id = texture_id,
      .width = spec.width,
      .height = spec.height,
  };
}

std::optional<ResolvedRenderImage>
RenderSystem::resolve_direct_depth_attachment(
    const RenderGraphResource &resource
) const {
  auto *framebuffer = resource.get_framebuffer();
  if (framebuffer == nullptr) {
    return std::nullopt;
  }

  const uint32_t texture_id = framebuffer->get_depth_attachment_id();
  if (texture_id == 0) {
    return std::nullopt;
  }

  const auto &spec = framebuffer->get_specification();
  return ResolvedRenderImage{
      .available = true,
      .target = RenderImageTarget::Texture2D,
      .renderer_texture_id = texture_id,
      .width = spec.width,
      .height = spec.height,
  };
}

std::optional<ResolvedRenderImage> RenderSystem::resolve_materialized_render_image(
    const RenderImageExportBinding &, const RenderGraphResource &
) const {
  return std::nullopt;
}

RenderSystem::~RenderSystem() {}

} // namespace astralix
