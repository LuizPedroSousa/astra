#include "render-system.hpp"
#include "assert.hpp"
#include "events/event-scheduler.hpp"
#include "framebuffer.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"
#include "path.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/descriptors/svg-descriptor.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/bloom-pass.hpp"
#include "systems/render-system/passes/debug-pass.hpp"
#include "systems/render-system/passes/editor-gizmo-pass.hpp"
#include "systems/render-system/passes/entity-pick-readback-pass.hpp"
#include "systems/render-system/passes/forward-pass.hpp"
#include "systems/render-system/passes/geometry-pass.hpp"
#include "systems/render-system/passes/grid-pass.hpp"
#include "systems/render-system/passes/lighting-pass.hpp"
#include "systems/render-system/passes/post-process-pass.hpp"
#include "systems/render-system/passes/render-graph-builder.hpp"
#include "systems/render-system/passes/shadow-pass.hpp"
#include "systems/render-system/passes/skybox-pass.hpp"
#include "systems/render-system/passes/ssao-blur-pass.hpp"
#include "systems/render-system/passes/ssao-pass.hpp"
#include "systems/render-system/passes/terrain-pass.hpp"
#include "systems/render-system/passes/ui-pass.hpp"
#include "systems/render-system/scene-extraction.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"
#include <array>
#include <random>

namespace astralix {

namespace {

const std::array<unsigned char, 4 * 4 * 3> &ssao_noise_seed() {
  static const auto k_seed = [] {
    std::array<unsigned char, 4 * 4 * 3> seed{};
    std::mt19937 generator(1337u);
    std::uniform_int_distribution<int> distribution(0, 255);

    for (auto &value : seed) {
      value = static_cast<unsigned char>(distribution(generator));
    }

    return seed;
  }();

  return k_seed;
}

} // namespace

RenderSystem::RenderSystem(RenderSystemConfig &config) : m_config(config) {};

void RenderSystem::reset_render_graph_state() {
  m_g_position_resource_index = 0;
  m_g_normal_resource_index = 0;
  m_g_albedo_resource_index = 0;
  m_g_emissive_resource_index = 0;
  m_g_entity_id_resource_index = 0;
  m_ssao_resource_index = 0;
  m_ssao_blur_resource_index = 0;
  m_pending_entity_pick_request.reset();
  m_pending_entity_pick_submissions.clear();
  m_latest_entity_pick_result.reset();
  m_entity_pick_readback_request = {};
  m_render_frame_serial = 0;
}

void RenderSystem::start() {
  ASTRA_PROFILE_N("RenderSystem::start");
  m_render_target = RenderTarget::create(
      m_config.backend_to_api(), m_config.msaa_to_render_target_msaa(), m_config.window_id
  );

  m_render_target->init();

  ensure_pass_dependency_descriptors();

  const auto render_backend = m_render_target->backend();

  switch (render_backend) {
    case RendererBackend::OpenGL: {
      resource_manager()->load_from_descriptors<ModelDescriptor, ShaderDescriptor, Texture2DDescriptor, Texture3DDescriptor, MaterialDescriptor, FontDescriptor, SvgDescriptor>(render_backend);
      break;
    }
    case RendererBackend::Vulkan: {
      resource_manager()->load_from_descriptors<ModelDescriptor, ShaderDescriptor, MaterialDescriptor, FontDescriptor, SvgDescriptor>(render_backend);
      break;
    }

    default: {
      ASTRA_EXCEPTION("You must define a renderer backend first")
    }
  }

  RenderGraphBuilder target_graph;

  auto window =
      window_manager()->get_window_by_id(m_render_target->window_id());

  auto window_width = static_cast<uint32_t>(window->width());
  auto window_height = static_cast<uint32_t>(window->height());

  m_shadow_map_resource_index = target_graph.resolve_resource_index(
      target_graph.declare_window_relative_image(
          "shadow_map", window_width, window_height, ImageFormat::Depth32F, ImageUsage::DepthStencilAttachment | ImageUsage::Sampled
      )
  );

  m_scene_color_resource_index = target_graph.resolve_resource_index(
      target_graph.declare_window_relative_image(
          "scene_color", window_width, window_height, ImageFormat::RGBA16F, ImageUsage::ColorAttachment | ImageUsage::Sampled
      )
  );

  m_scene_depth_resource_index = target_graph.resolve_resource_index(
      target_graph.declare_window_relative_image(
          "scene_depth", window_width, window_height, ImageFormat::Depth32F, ImageUsage::DepthStencilAttachment
      )
  );

  m_bloom_extract_resource_index = target_graph.resolve_resource_index(
      target_graph.declare_window_relative_image(
          "bloom_extract", window_width, window_height, ImageFormat::RGBA16F, ImageUsage::ColorAttachment | ImageUsage::Sampled
      )
  );

  m_entity_pick_resource_index = target_graph.resolve_resource_index(
      target_graph.declare_window_relative_image(
          "entity_pick", window_width, window_height, ImageFormat::R32I, ImageUsage::ColorAttachment | ImageUsage::TransferSrc
      )
  );

  m_bloom_resource_index = target_graph.resolve_resource_index(
      target_graph.declare_window_relative_image(
          "bloom", window_width, window_height, ImageFormat::RGBA16F, ImageUsage::ColorAttachment | ImageUsage::Sampled
      )
  );

  m_present_resource_index = target_graph.resolve_resource_index(
      target_graph.declare_window_relative_image(
          "present", window_width, window_height, ImageFormat::RGBA8, ImageUsage::ColorAttachment | ImageUsage::TransferSrc | ImageUsage::Sampled
      )
  );

  const bool use_forward_strategy = m_config.strategy == "forward";
  reset_render_graph_state();

  if (!use_forward_strategy && !m_config.strategy.empty() &&
      m_config.strategy != "deferred") {
    LOG_WARN("Unknown render strategy '", m_config.strategy, "', defaulting to deferred");
  }

  rendering::ResolvedMeshDraw skybox_cube{};
  rendering::ResolvedMeshDraw fullscreen_quad{};
  skybox_cube = rendering::create_skybox_cube_mesh(render_backend);
  fullscreen_quad = rendering::create_fullscreen_quad_mesh(render_backend);

  target_graph.add_pass(create_scope<ShadowPass>())
      .use_shader("shadow_shader", "shaders::shadow_map")
      .use_image(
          m_shadow_map_resource_index,
          ImageAspect::Depth,
          RenderUsage::DepthAttachmentWrite
      )
      .export_image(make_shadow_map_render_image_export(m_shadow_map_resource_index));

  if (use_forward_strategy) {
    target_graph.add_pass(create_scope<ForwardPass>())
        .use_shader("forward_shader", "shaders::lighting_forward")
        .use_image(m_shadow_map_resource_index, ImageAspect::Depth, RenderUsage::SampledRead)
        .use_image(m_scene_color_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_bloom_extract_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_entity_pick_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_scene_depth_resource_index, ImageAspect::Depth, RenderUsage::DepthAttachmentWrite)
        .export_image(
            make_scene_color_render_image_export(m_scene_color_resource_index)
        );
  } else {
    m_g_position_resource_index = target_graph.resolve_resource_index(
        target_graph.declare_window_relative_image(
            "g_position", window_width, window_height, ImageFormat::RGBA16F, ImageUsage::ColorAttachment | ImageUsage::Sampled
        )
    );
    m_g_normal_resource_index = target_graph.resolve_resource_index(
        target_graph.declare_window_relative_image(
            "g_normal", window_width, window_height, ImageFormat::RGBA16F, ImageUsage::ColorAttachment | ImageUsage::Sampled
        )
    );
    m_g_albedo_resource_index = target_graph.resolve_resource_index(
        target_graph.declare_window_relative_image(
            "g_albedo", window_width, window_height, ImageFormat::RGBA16F, ImageUsage::ColorAttachment | ImageUsage::Sampled
        )
    );
    m_g_emissive_resource_index = target_graph.resolve_resource_index(
        target_graph.declare_window_relative_image(
            "g_emissive", window_width, window_height, ImageFormat::RGBA16F, ImageUsage::ColorAttachment | ImageUsage::Sampled
        )
    );
    m_g_entity_id_resource_index = target_graph.resolve_resource_index(
        target_graph.declare_window_relative_image(
            "g_entity_id", window_width, window_height, ImageFormat::R32I, ImageUsage::ColorAttachment | ImageUsage::Sampled
        )
    );

    uint32_t ao_width = window_width / 2;
    uint32_t ao_height = window_height / 2;
    m_ssao_resource_index = target_graph.resolve_resource_index(
        target_graph.declare_window_relative_image(
            "ssao", ao_width, ao_height, ImageFormat::RGBA8, ImageUsage::ColorAttachment | ImageUsage::Sampled
        )
    );

    m_ssao_blur_resource_index = target_graph.resolve_resource_index(
        target_graph.declare_window_relative_image(
            "ssao_blur", ao_width, ao_height, ImageFormat::RGBA8, ImageUsage::ColorAttachment | ImageUsage::Sampled
        )
    );

    target_graph.add_pass(create_scope<GeometryPass>())
        .use_shader("geometry_shader", "shaders::g_buffer")
        .use_image(m_g_position_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_g_normal_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_g_albedo_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_g_emissive_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_g_entity_id_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_scene_depth_resource_index, ImageAspect::Depth, RenderUsage::DepthAttachmentWrite)
        .export_image(
            make_g_buffer_render_image_export(
                GBufferAspect::Position, m_g_position_resource_index
            )
        )
        .export_image(
            make_g_buffer_render_image_export(
                GBufferAspect::Normal, m_g_normal_resource_index
            )
        )
        .export_image(
            make_g_buffer_render_image_export(
                GBufferAspect::Albedo, m_g_albedo_resource_index
            )
        )
        .export_image(
            make_g_buffer_render_image_export(
                GBufferAspect::Emissive, m_g_emissive_resource_index
            )
        );

    target_graph.add_pass(create_scope<SSAOPass>(fullscreen_quad))
        .use_shader("ssao_shader", "shaders::ssao")
        .use_texture_2d("noise_texture", "noise_texture")
        .use_image(m_g_position_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_normal_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_albedo_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_ssao_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .export_image(make_ssao_render_image_export(m_ssao_resource_index));

    target_graph.add_pass(create_scope<SSAOBlurPass>(fullscreen_quad))
        .use_shader("ssao_blur_shader", "shaders::ssao_blur")
        .use_image(m_g_position_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_normal_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_ssao_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_ssao_blur_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .export_image(
            make_ssao_blur_render_image_export(m_ssao_blur_resource_index)
        );

    target_graph.add_pass(create_scope<LightingPass>(fullscreen_quad))
        .use_shader("lighting_shader", "shaders::lighting")
        .use_image(m_shadow_map_resource_index, ImageAspect::Depth, RenderUsage::SampledRead)
        .use_image(m_g_position_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_normal_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_albedo_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_emissive_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_entity_id_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_ssao_blur_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_scene_color_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_bloom_extract_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .use_image(m_entity_pick_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
        .export_image(
            make_scene_color_render_image_export(m_scene_color_resource_index)
        );

    target_graph.add_pass(create_scope<DebugGBufferPass>(fullscreen_quad))
        .use_shader("debug_g_buffer_shader", "shaders::debug_g_buffer")
        .use_image(m_g_position_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_normal_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_albedo_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_g_emissive_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
        .use_image(m_scene_color_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite);
  }


  target_graph.add_pass(create_scope<SkyboxPass>(std::move(skybox_cube)))
      .use_image(m_scene_color_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
      .use_image(m_scene_depth_resource_index, ImageAspect::Depth, RenderUsage::DepthAttachmentRead);

  target_graph.add_pass(create_scope<GridPass>(fullscreen_quad))
      .use_shader("grid_shader", "shaders::grid")
      .use_image(m_scene_color_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
      .use_image(m_scene_depth_resource_index, ImageAspect::Depth, RenderUsage::DepthAttachmentRead);

  target_graph.add_pass(create_scope<DebugOverlayPass>(fullscreen_quad))
      .use_shader("debug_depth_shader", "shaders::debug_depth")
      .use_image(m_shadow_map_resource_index, ImageAspect::Depth, RenderUsage::SampledRead)
      .use_image(m_scene_color_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite);

  target_graph.add_pass(
                  create_scope<EntityPickReadbackPass>(
                      &m_entity_pick_readback_request
                  )
  )
      .use_image(
          m_entity_pick_resource_index, ImageAspect::Color0, RenderUsage::TransferSrc
      );

  target_graph.add_pass(create_scope<BloomPass>(fullscreen_quad))
      .use_shader("bloom_shader", "shaders::bloom")
      .use_image(m_bloom_extract_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
      .use_image(m_bloom_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
      .export_image(make_bloom_render_image_export(m_bloom_resource_index));

  target_graph.add_pass(create_scope<PostProcessPass>(fullscreen_quad))
      .use_shader("hdr_shader", "shaders::hdr")
      .use_image(m_bloom_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
      .use_image(m_scene_color_resource_index, ImageAspect::Color0, RenderUsage::SampledRead)
      .use_image(m_present_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
      .export_image(
          make_final_output_render_image_export(m_present_resource_index)
      );

  target_graph.add_pass(create_scope<EditorGizmoPass>())
      .use_shader("editor_gizmo_shader", "shaders::editor_gizmo")
      .use_image(m_present_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite);

  target_graph.add_pass(create_scope<UIPass>(fullscreen_quad))
      .use_shader("ui_solid", "shaders::ui_solid")
      .use_shader("ui_image", "shaders::ui_image")
      .use_shader("ui_text", "shaders::ui_text")
      .use_shader("ui_polyline", "shaders::ui_polyline")
      .use_image(m_present_resource_index, ImageAspect::Color0, RenderUsage::ColorAttachmentWrite)
      .present(m_present_resource_index, ImageAspect::Color0);

  m_render_graph = target_graph.build();
  m_render_graph->compile(m_render_target);
  rebuild_render_image_exports();
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

  drain_completed_entity_picks();

  if (m_render_graph) {
    auto *active_scene = SceneManager::get()->get_active_scene();
    if (active_scene) {
      ++m_render_frame_serial;
      auto scene_frame = rendering::build_scene_frame(
          active_scene->world(), m_render_target, m_render_runtime_store
      );

      m_entity_pick_readback_request = {};

      if (m_pending_entity_pick_request.has_value()) {
        const auto extent = entity_selection_extent();
        const auto &pixel = m_pending_entity_pick_request->pixel;

        if (extent.has_value() && pixel.x >= 0 && pixel.y >= 0 &&
            pixel.x < extent->x && pixel.y < extent->y) {
          m_pending_entity_pick_submissions.push_back(
              PendingEntityPickSubmission{
                  .frame_serial = m_render_frame_serial,
                  .pixel = pixel,
                  .pick_id_lut = std::make_shared<const std::vector<EntityID>>(
                      scene_frame.pick_id_lut
                  ),
              }
          );

          auto &submission = m_pending_entity_pick_submissions.back();
          m_entity_pick_readback_request = rendering::EntityPickReadbackRequest{
              .armed = true,
              .pixel = pixel,
              .out_value = &submission.raw_value,
              .out_ready = &submission.ready,
          };
        }

        m_pending_entity_pick_request.reset();
      }

      m_render_graph->execute(dt, &scene_frame);
      m_entity_pick_readback_request = {};
      drain_completed_entity_picks();
    }
  }

  auto scheduler = EventScheduler::get();
  scheduler->bind(SchedulerType::REALTIME);
};

std::optional<ResolvedRenderImage>
RenderSystem::resolve_render_image(RenderImageExportKey key) const {
  if (m_render_graph == nullptr) {
    return std::nullopt;
  }

  const auto &frame = m_render_graph->latest_compiled_frame();
  const auto *entry = frame.find_export(key);
  if (entry == nullptr) {
    return std::nullopt;
  }

  return ResolvedRenderImage{
      .available = true,
      .view = ImageViewRef{.image = entry->image},
      .width = entry->extent.width,
      .height = entry->extent.height,
  };
}

std::optional<glm::ivec2> RenderSystem::entity_selection_extent() const {
  if (m_render_graph == nullptr) {
    return std::nullopt;
  }

  const auto *resource = m_render_graph->resource_at(m_entity_pick_resource_index);
  if (resource == nullptr || resource->get_graph_image() == nullptr) {
    return std::nullopt;
  }

  const auto &desc = resource->get_graph_image()->desc;
  if (desc.width == 0 || desc.height == 0) {
    return std::nullopt;
  }

  return glm::ivec2(
      static_cast<int>(desc.width),
      static_cast<int>(desc.height)
  );
}

void RenderSystem::request_entity_pick(glm::ivec2 pixel) {
  m_pending_entity_pick_request = PendingEntityPickRequest{
      .pixel = pixel,
  };
}

std::optional<EntityPickResult> RenderSystem::consume_latest_entity_pick() {
  auto result = m_latest_entity_pick_result;
  m_latest_entity_pick_result.reset();
  return result;
}

void RenderSystem::rebuild_render_image_exports() {
  m_render_image_exports.clear();

  if (m_render_graph == nullptr) {
    return;
  }

  for (const auto &compiled_export : m_render_graph->compiled_exports()) {
        m_render_image_exports.push_back(RenderImageExportBinding{
        .key = compiled_export.key,
        .available = true,
        });
  }
}

void RenderSystem::drain_completed_entity_picks() {
  for (auto it = m_pending_entity_pick_submissions.begin();
       it != m_pending_entity_pick_submissions.end();) {
    if (!it->ready) {
      ++it;
      continue;
    }

    std::optional<EntityID> entity_id;
    if (it->raw_value > 0 && it->pick_id_lut != nullptr &&
        static_cast<size_t>(it->raw_value) <= it->pick_id_lut->size()) {
      entity_id = (*it->pick_id_lut)[static_cast<size_t>(it->raw_value - 1)];
    }

    m_latest_entity_pick_result = EntityPickResult{
        .frame_serial = it->frame_serial,
        .pixel = it->pixel,
        .entity_id = entity_id,
    };
    it = m_pending_entity_pick_submissions.erase(it);
  }
}

RenderSystem::~RenderSystem() {}

void RenderSystem::ensure_pass_dependency_descriptors() {
  if (resource_manager()->get_descriptor_by_id<ShaderDescriptor>(
          "shaders::lighting_forward"
      ) == nullptr) {
    Shader::create(
        "shaders::lighting_forward",
        "shaders/lighting-forward.axsl"_engine,
        "shaders/lighting-forward.axsl"_engine
    );
  }

  if (resource_manager()->get_descriptor_by_id<ShaderDescriptor>(
          "shaders::editor_gizmo"
      ) == nullptr) {
    Shader::create(
        "shaders::editor_gizmo",
        "shaders/editor_gizmo.axsl"_engine,
        "shaders/editor_gizmo.axsl"_engine
  );
}

  if (resource_manager()->get_descriptor_by_id<ShaderDescriptor>(
          "shaders::ui_polyline"
      ) == nullptr) {
    Shader::create(
        "shaders::ui_polyline",
        "shaders/ui/polyline.axsl"_engine,
        "shaders/ui/polyline.axsl"_engine
    );
  }

  if (resource_manager()->get_descriptor_by_id<Texture2DDescriptor>(
          "noise_texture"
      ) == nullptr) {
    TextureConfig texture_config;
    texture_config.width = 4;
    texture_config.height = 4;
    texture_config.format = TextureFormat::RGB;
    texture_config.buffer =
        const_cast<unsigned char *>(ssao_noise_seed().data());
    texture_config.parameters = {
        {TextureParameter::WrapS, TextureValue::Linear},
        {TextureParameter::WrapT, TextureValue::Linear},
        {TextureParameter::MagFilter, TextureValue::Nearest},
        {TextureParameter::MinFilter, TextureValue::Nearest},
  };
    Texture2D::create("noise_texture", texture_config);
}
}

} // namespace astralix
