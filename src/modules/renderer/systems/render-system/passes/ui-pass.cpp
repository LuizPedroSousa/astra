#include "systems/render-system/passes/ui-pass.hpp"

#include "components/ui.hpp"
#include "framebuffer.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "log.hpp"
#include "resources/font.hpp"
#include "resources/svg.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/render-frame.hpp"
#include "vector/path-tessellator.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"
#include "vertex-buffer.hpp"
#include <algorithm>
#include <cmath>

#include ASTRALIX_ENGINE_BINDINGS_HEADER
using namespace astralix::shader_bindings;

namespace astralix {

UIPass::UIPass(rendering::ResolvedMeshDraw quad) : m_quad(std::move(quad)) {}

void UIPass::setup(PassSetupContext &ctx) {
  m_shaders.solid = ctx.find_shader("ui_solid");
  m_shaders.image = ctx.find_shader("ui_image");
  m_shaders.text = ctx.find_shader("ui_text");
  m_shaders.polyline = ctx.find_shader("ui_polyline");
  m_shaders.vector = ctx.find_shader("ui_vector");
  m_render_image_sample_flip_y = ui_pass_detail::render_image_sample_flip_y(
      ctx.target() != nullptr ? ctx.target()->backend() : RendererBackend::None
  );
}

void UIPass::record(PassRecordContext &ctx, PassRecorder &recorder) {
  ASTRA_PROFILE_N("UIPass::record");
  const auto *scene_frame = ctx.scene();
  if (scene_frame == nullptr || scene_frame->ui_roots.empty()) {
    return;
  }

  const auto *present_target = ctx.find_graph_image("present");
  if (present_target == nullptr || m_quad.vertex_array == nullptr ||
      m_quad.index_count == 0) {
    return;
  }

  auto &frame = ctx.frame();
  const auto &spec = present_target->get_graph_image()->desc;
  const auto extent = ctx.graph_image_extent(*present_target);
  const float ui_width = static_cast<float>(spec.width);
  const float ui_height = static_cast<float>(spec.height);

  const glm::mat4 projection = glm::ortho(
      0.0f, ui_width, ui_height, 0.0f
  );
  const auto rect_to_vec4 = [](const ui::UIRect &rect) {
    return glm::vec4(rect.x, rect.y, rect.width, rect.height);
  };
  const auto colored_vertex_layout = [] {
    return BufferLayout(
        {BufferElement(ShaderDataType::Float2, "a_position").at_location(0),
         BufferElement(ShaderDataType::Float4, "a_color").at_location(1)}
    );
  };

  const auto present_color =
      ctx.register_graph_image("ui.present", *present_target);
  const auto quad_buffer = frame.register_vertex_array(
      "ui.quad", m_quad.vertex_array
  );

  RenderPipelineDesc solid_pipeline_desc;
  solid_pipeline_desc.debug_name = "ui.solid";
  solid_pipeline_desc.raster.cull_mode = CullMode::None;
  solid_pipeline_desc.depth_stencil.depth_test = false;
  solid_pipeline_desc.depth_stencil.depth_write = false;
  solid_pipeline_desc.blend_attachments = {BlendAttachmentState::alpha_blend()};

  RenderPipelineDesc image_pipeline_desc = solid_pipeline_desc;
  image_pipeline_desc.debug_name = "ui.image";

  RenderPipelineDesc render_image_pipeline_desc = solid_pipeline_desc;
  render_image_pipeline_desc.debug_name = "ui.render-image";
  render_image_pipeline_desc.blend_attachments = {BlendAttachmentState::replace()};

  RenderPipelineDesc text_pipeline_desc = solid_pipeline_desc;
  text_pipeline_desc.debug_name = "ui.text";

  RenderPipelineDesc polyline_pipeline_desc = solid_pipeline_desc;
  polyline_pipeline_desc.debug_name = "ui.polyline";

  RenderPipelineDesc vector_pipeline_desc = solid_pipeline_desc;
  vector_pipeline_desc.debug_name = "ui.vector";

  RenderPipelineHandle solid_pipeline{};
  RenderPipelineHandle image_pipeline{};
  RenderPipelineHandle render_image_pipeline{};
  RenderPipelineHandle text_pipeline{};
  RenderPipelineHandle polyline_pipeline{};
  RenderPipelineHandle vector_pipeline{};

  if (m_shaders.solid != nullptr) {
    solid_pipeline = frame.register_pipeline(solid_pipeline_desc, m_shaders.solid);
  }
  if (m_shaders.image != nullptr) {
    image_pipeline = frame.register_pipeline(image_pipeline_desc, m_shaders.image);
    render_image_pipeline = frame.register_pipeline(render_image_pipeline_desc, m_shaders.image);
  }
  if (m_shaders.text != nullptr) {
    text_pipeline = frame.register_pipeline(text_pipeline_desc, m_shaders.text);
  }
  if (m_shaders.polyline != nullptr) {
    polyline_pipeline = frame.register_pipeline(polyline_pipeline_desc, m_shaders.polyline);
  }
  if (m_shaders.vector != nullptr) {
    vector_pipeline = frame.register_pipeline(vector_pipeline_desc, m_shaders.vector);
  }

  RenderingInfo info;
  info.debug_name = "ui-pass";
  info.extent = extent;
  info.color_attachments.push_back(ColorAttachmentRef{
      .view = ImageViewRef{.image = present_color},
      .load_op = AttachmentLoadOp::Load,
      .store_op = AttachmentStoreOp::Store,
  });

  recorder.begin_rendering(info);

  RenderPipelineHandle bound_pipeline{};
  BufferHandle bound_vertex_buffer{};
  uint32_t bound_vertex_slot = 0;
  uint32_t bound_vertex_offset = 0;
  BufferHandle bound_index_buffer{};
  IndexType bound_index_type = IndexType::Uint32;
  uint32_t bound_index_offset = 0;
  bool scissor_initialized = false;
  ui_pass_detail::ScissorRect active_scissor{};

  RenderBindingGroupHandle solid_scene_bindings{};
  RenderBindingGroupHandle image_scene_bindings{};
  RenderBindingGroupHandle text_scene_bindings{};
  RenderBindingGroupHandle polyline_scene_bindings{};
  RenderBindingGroupHandle vector_scene_bindings{};

  const auto record_quad_draw_params = [&](RenderBindingGroupHandle bindings,
                                           const glm::vec4 &rect) {
    rendering::record_shader_params(
        frame, bindings, engine_shaders_ui_quad_axsl::QuadParams{
            .rect = rect,
        }
    );
  };

  const auto ensure_quad_scene_bindings =
      [&](RenderBindingGroupHandle &bindings, Ref<Shader> shader,
          std::string_view debug_name) {
        if (bindings.valid()) {
          return;
        }

        bindings = frame.register_binding_group(
            make_binding_group_desc(
                std::string(debug_name),
                "ui-pass",
                shader,
                0,
                std::string(debug_name),
                RenderBindingScope::Pass,
                RenderBindingCachePolicy::Reuse,
                RenderBindingSharing::LocalOnly,
                0,
                RenderBindingStability::FrameLocal
            )
        );
        rendering::record_shader_params(
            frame, bindings, engine_shaders_ui_quad_axsl::CameraParams{
                .projection = projection,
            }
        );
      };

  const auto ensure_polyline_scene_bindings = [&] {
    if (polyline_scene_bindings.valid()) {
      return;
    }

    polyline_scene_bindings = frame.register_binding_group(
        make_binding_group_desc(
            "ui.polyline-scene",
            "ui-pass",
            m_shaders.polyline,
            0,
            "ui.polyline-scene",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );
    rendering::record_shader_params(
        frame,
        polyline_scene_bindings,
        engine_shaders_ui_polyline_axsl::CameraParams{
            .projection = projection,
        }
    );
  };

  const auto ensure_vector_scene_bindings = [&] {
    if (vector_scene_bindings.valid()) {
      return;
    }

    vector_scene_bindings = frame.register_binding_group(
        make_binding_group_desc(
            "ui.vector-scene",
            "ui-pass",
            m_shaders.vector,
            0,
            "ui.vector-scene",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );
    rendering::record_shader_params(
        frame,
        vector_scene_bindings,
        engine_shaders_ui_vector_axsl::CameraParams{
            .projection = projection,
        }
    );
  };

  const auto bind_pipeline = [&](RenderPipelineHandle pipeline) {
    if (bound_pipeline == pipeline) {
      return;
    }
    recorder.bind_pipeline(pipeline);
    bound_pipeline = pipeline;
  };

  const auto bind_vertex_buffer = [&](BufferHandle buffer,
                                      uint32_t slot = 0,
                                      uint32_t offset = 0) {
    if (bound_vertex_buffer == buffer && bound_vertex_slot == slot &&
        bound_vertex_offset == offset) {
      return;
    }
    recorder.bind_vertex_buffer(buffer, slot, offset);
    bound_vertex_buffer = buffer;
    bound_vertex_slot = slot;
    bound_vertex_offset = offset;
  };

  const auto bind_index_buffer = [&](BufferHandle buffer,
                                     IndexType index_type = IndexType::Uint32,
                                     uint32_t offset = 0) {
    if (bound_index_buffer == buffer && bound_index_type == index_type &&
        bound_index_offset == offset) {
      return;
    }
    recorder.bind_index_buffer(buffer, index_type, offset);
    bound_index_buffer = buffer;
    bound_index_type = index_type;
    bound_index_offset = offset;
  };

  const auto bind_quad_geometry = [&]() {
    bind_vertex_buffer(quad_buffer);
    bind_index_buffer(quad_buffer, IndexType::Uint32);
  };

  const auto bind_quad_pipeline = [&](RenderPipelineHandle pipeline,
                                      RenderBindingGroupHandle scene_bindings) {
    bind_pipeline(pipeline);
    bind_quad_geometry();
    recorder.bind_binding_group(scene_bindings);
  };

  const auto apply_scissor = [&](const ui_pass_detail::ScissorRect &next_scissor) {
    if (scissor_initialized && active_scissor.enabled == next_scissor.enabled &&
        (!next_scissor.enabled ||
         (active_scissor.x == next_scissor.x &&
          active_scissor.y == next_scissor.y &&
          active_scissor.width == next_scissor.width &&
          active_scissor.height == next_scissor.height))) {
      return;
    }

    scissor_initialized = true;
    active_scissor = next_scissor;

    if (next_scissor.enabled) {
      recorder.set_scissor(
          true,
          next_scissor.x,
          next_scissor.y,
          next_scissor.width,
          next_scissor.height
      );
    } else {
      recorder.set_scissor(false);
    }
  };

  for (const auto &root : scene_frame->ui_roots) {
    for (const auto &command : root.commands) {
      if (command.has_clip) {
        apply_scissor(ui_pass_detail::resolve_scissor_rect(
            command.clip_rect, ui_width, ui_height, extent
        ));
      } else {
        apply_scissor({});
      }

      switch (command.type) {
        case ui::DrawCommandType::Rect: {
          if (!solid_pipeline.valid() || command.rect.width <= 0.0f ||
              command.rect.height <= 0.0f) {
            break;
          }

          ensure_quad_scene_bindings(
              solid_scene_bindings, m_shaders.solid, "ui.solid-scene"
          );
          const auto bindings = frame.register_binding_group(
              make_binding_group_desc(
                  "ui.rect",
                  "ui-pass",
                  m_shaders.solid,
                  1,
                  "ui.rect",
                  RenderBindingScope::Draw,
                  RenderBindingCachePolicy::Ephemeral,
                  RenderBindingSharing::LocalOnly,
                  0,
                  RenderBindingStability::Transient
              )
          );
          const glm::vec4 rect = rect_to_vec4(command.rect);
          record_quad_draw_params(bindings, rect);
          rendering::record_shader_params(
              frame, bindings, engine_shaders_ui_solid_axsl::SolidParams{
                  .rect = rect,
                  .fill_color = command.color,
              }
          );
          rendering::record_shader_params(
              frame, bindings,
              engine_shaders_ui_solid_axsl::SolidBorderParams{
                  .color = command.border_color,
                  .width = command.border_width,
                  .radius = command.border_radius,
              }
          );

          bind_quad_pipeline(solid_pipeline, solid_scene_bindings);
          recorder.bind_binding_group(bindings);
          recorder.draw_indexed(DrawIndexedArgs{
              .index_count = m_quad.index_count,
          });
          break;
        }

        case ui::DrawCommandType::Image: {
          if (!image_pipeline.valid() || command.rect.width <= 0.0f ||
              command.rect.height <= 0.0f) {
            break;
          }

          auto texture_iterator =
              scene_frame->ui_resources.textures.find(command.texture_id);
          if (texture_iterator == scene_frame->ui_resources.textures.end() ||
              texture_iterator->second == nullptr) {
            break;
          }

          const auto texture_image = frame.register_texture_2d(
              "ui.image-texture", texture_iterator->second
          );
          ensure_quad_scene_bindings(
              image_scene_bindings, m_shaders.image, "ui.image-scene"
          );
          const auto bindings = frame.register_binding_group(
              make_binding_group_desc(
                  "ui.image",
                  "ui-pass",
                  m_shaders.image,
                  1,
                  "ui.image",
                  RenderBindingScope::Draw,
                  RenderBindingCachePolicy::Ephemeral,
                  RenderBindingSharing::LocalOnly,
                  0,
                  RenderBindingStability::Transient
              )
          );
          record_quad_draw_params(bindings, rect_to_vec4(command.rect));
          rendering::record_shader_params(
              frame, bindings, engine_shaders_ui_image_axsl::ImageParams{
                  .tint = command.tint,
                  .sample_flip_y = 0.0f,
              }
          );
          frame.add_sampled_image_binding(
              bindings, engine_shaders_ui_image_axsl::ImageResources::texture.binding_id,
              ImageViewRef{.image = texture_image}
          );

          bind_quad_pipeline(image_pipeline, image_scene_bindings);
          recorder.bind_binding_group(bindings);
          recorder.draw_indexed(DrawIndexedArgs{
              .index_count = m_quad.index_count,
          });
          break;
        }

        case ui::DrawCommandType::SvgImage: {
          if (!polyline_pipeline.valid() || command.rect.width <= 0.0f ||
              command.rect.height <= 0.0f) {
            break;
          }

          auto svg_iterator =
              scene_frame->ui_resources.svgs.find(command.texture_id);
          if (svg_iterator == scene_frame->ui_resources.svgs.end() ||
              svg_iterator->second == nullptr ||
              svg_iterator->second->width() <= 0.0f ||
              svg_iterator->second->height() <= 0.0f) {
            break;
          }

          const auto &svg = svg_iterator->second;
          std::vector<ui::UIPolylineVertex> triangle_vertices;
          for (const auto &batch : svg->batches()) {
            triangle_vertices.reserve(
                triangle_vertices.size() + batch.vertices.size()
            );
            for (const SvgColorVertex &vertex : batch.vertices) {
              const glm::vec2 normalized(
                  vertex.position.x / svg->width(),
                  vertex.position.y / svg->height()
              );
              const glm::vec2 transformed(
                  command.rect.x + normalized.x * command.rect.width,
                  command.rect.y + normalized.y * command.rect.height
              );
              triangle_vertices.push_back(
                  {transformed, vertex.color * command.tint}
              );
            }
          }

          if (triangle_vertices.empty()) {
            break;
          }

          const auto transient_buffer = frame.register_transient_vertices(
              "ui.svg-triangles", triangle_vertices.data(), static_cast<uint32_t>(triangle_vertices.size() * sizeof(ui::UIPolylineVertex)), static_cast<uint32_t>(triangle_vertices.size()), colored_vertex_layout()
          );

          ensure_polyline_scene_bindings();
          bind_pipeline(polyline_pipeline);
          recorder.bind_binding_group(polyline_scene_bindings);
          bind_vertex_buffer(transient_buffer);
          recorder.draw_vertices(
              static_cast<uint32_t>(triangle_vertices.size())
          );
          break;
        }

        case ui::DrawCommandType::RenderImageView: {
          if (!render_image_pipeline.valid() || command.rect.width <= 0.0f ||
              command.rect.height <= 0.0f ||
              !command.render_image_key.has_value()) {
            break;
          }

          const auto *export_entry =
              frame.find_export(*command.render_image_key);
          if (export_entry == nullptr) {
            break;
          }

          ensure_quad_scene_bindings(
              image_scene_bindings, m_shaders.image, "ui.image-scene"
          );
          const auto bindings = frame.register_binding_group(
              make_binding_group_desc(
                  "ui.render-image-view",
                  "ui-pass",
                  m_shaders.image,
                  1,
                  "ui.render-image-view",
                  RenderBindingScope::Draw,
                  RenderBindingCachePolicy::Ephemeral,
                  RenderBindingSharing::LocalOnly,
                  0,
                  RenderBindingStability::Transient
              )
          );
          record_quad_draw_params(bindings, rect_to_vec4(command.rect));
          rendering::record_shader_params(
              frame, bindings, engine_shaders_ui_image_axsl::ImageParams{
                  .tint = command.tint,
                  .sample_flip_y = m_render_image_sample_flip_y,
              }
          );
          frame.add_sampled_image_binding(
              bindings, engine_shaders_ui_image_axsl::ImageResources::texture.binding_id,
              ImageViewRef{.image = export_entry->image}
          );

          bind_quad_pipeline(render_image_pipeline, image_scene_bindings);
          recorder.bind_binding_group(bindings);
          recorder.draw_indexed(DrawIndexedArgs{
              .index_count = m_quad.index_count,
          });
          break;
        }

        case ui::DrawCommandType::Text: {
          if (!text_pipeline.valid() || command.text.empty() ||
              command.font_id.empty()) {
            break;
          }

          auto font_iterator =
              scene_frame->ui_resources.fonts.find(command.font_id);
          if (font_iterator == scene_frame->ui_resources.fonts.end() ||
              font_iterator->second == nullptr) {
            break;
          }

          const auto &font = font_iterator->second;
          const uint32_t font_size = static_cast<uint32_t>(
              std::max(1.0f, std::round(command.font_size))
          );
          const auto &glyphs = font->glyphs(font_size);
          const auto &glyph_lut = font->glyph_lut(font_size);
          const float baseline_y =
              command.text_origin.y + font->ascent(font_size);

          float current_x = command.text_origin.x;

          ensure_quad_scene_bindings(
              text_scene_bindings, m_shaders.text, "ui.text-scene"
          );
          bind_quad_pipeline(text_pipeline, text_scene_bindings);

          for (const char character : command.text) {
            const GlyphHandle handle =
                glyph_lut[static_cast<unsigned char>(character)];
            if (handle == k_invalid_glyph_handle) {
              continue;
            }

            const auto &glyph = glyphs[handle];
            auto glyph_texture_iterator =
                scene_frame->ui_resources.textures.find(glyph.texture_id);
            if (glyph_texture_iterator ==
                    scene_frame->ui_resources.textures.end() ||
                glyph_texture_iterator->second == nullptr) {
              current_x += static_cast<float>(glyph.advance >> 6);
              continue;
            }

            const float xpos =
                current_x + static_cast<float>(glyph.bearing.x);
            const float ypos =
                baseline_y - static_cast<float>(glyph.bearing.y);
            const float glyph_width = static_cast<float>(glyph.size.x);
            const float glyph_height = static_cast<float>(glyph.size.y);

            if (command.has_clip) {
              const ui::UIRect glyph_rect{
                  .x = xpos,
                  .y = ypos,
                  .width = glyph_width,
                  .height = glyph_height,
              };
              if (!ui::intersects(glyph_rect, command.clip_rect)) {
                current_x += static_cast<float>(glyph.advance >> 6);
                continue;
              }
            }

            const auto glyph_image = frame.register_texture_2d(
                "ui.text-glyph", glyph_texture_iterator->second
            );
            const auto bindings = frame.register_binding_group(
                make_binding_group_desc(
                    "ui.text-glyph",
                    "ui-pass",
                    m_shaders.text,
                    1,
                    "ui.text-glyph",
                    RenderBindingScope::Draw,
                    RenderBindingCachePolicy::Ephemeral,
                    RenderBindingSharing::LocalOnly,
                    0,
                    RenderBindingStability::Transient
                )
            );

            record_quad_draw_params(
                bindings, glm::vec4(xpos, ypos, glyph_width, glyph_height)
            );
            rendering::record_shader_params(
                frame, bindings, engine_shaders_ui_text_axsl::TextParams{
                    .color = command.color,
                }
            );
            frame.add_sampled_image_binding(
                bindings, engine_shaders_ui_text_axsl::TextResources::glyph.binding_id,
                ImageViewRef{.image = glyph_image}
            );

            recorder.bind_binding_group(bindings);
            recorder.draw_indexed(DrawIndexedArgs{
                .index_count = m_quad.index_count,
            });

            current_x += static_cast<float>(glyph.advance >> 6);
          }
          break;
        }

        case ui::DrawCommandType::Polyline: {
          if (!polyline_pipeline.valid() || command.polyline_series.empty()) {
            break;
          }

          std::vector<ui::UIPolylineVertex> triangle_vertices;
          for (const auto &series : command.polyline_series) {
            if (series.vertices.size() < 2u) {
              continue;
            }

            const float half_thickness = series.thickness * 0.5f;
            const size_t segment_count = series.vertices.size() - 1u;
            triangle_vertices.reserve(
                triangle_vertices.size() + segment_count * 6u
            );

            std::vector<glm::vec2> perpendiculars(series.vertices.size());
            for (size_t i = 0u; i < segment_count; ++i) {
              const glm::vec2 direction = glm::normalize(
                  series.vertices[i + 1u].position -
                  series.vertices[i].position
              );
              perpendiculars[i] = glm::vec2(-direction.y, direction.x);
            }
            perpendiculars[segment_count] =
                perpendiculars[segment_count - 1u];

            std::vector<glm::vec2> miter_normals(series.vertices.size());
            miter_normals[0u] = perpendiculars[0u];
            miter_normals[segment_count] =
                perpendiculars[segment_count - 1u];

            for (size_t i = 1u; i < segment_count; ++i) {
              glm::vec2 averaged = glm::normalize(
                  perpendiculars[i - 1u] + perpendiculars[i]
              );
              float dot_product = glm::dot(averaged, perpendiculars[i]);
              if (dot_product < 0.5f) {
                averaged = perpendiculars[i];
                dot_product = 1.0f;
              }
              miter_normals[i] = averaged / dot_product;
            }

            for (size_t i = 0u; i < segment_count; ++i) {
              const auto &point_a = series.vertices[i];
              const auto &point_b = series.vertices[i + 1u];

              const glm::vec2 offset_a =
                  miter_normals[i] * half_thickness;
              const glm::vec2 offset_b =
                  miter_normals[i + 1u] * half_thickness;

              const glm::vec2 top_left = point_a.position + offset_a;
              const glm::vec2 bottom_left = point_a.position - offset_a;
              const glm::vec2 top_right = point_b.position + offset_b;
              const glm::vec2 bottom_right = point_b.position - offset_b;

              triangle_vertices.push_back({top_left, point_a.color});
              triangle_vertices.push_back({bottom_left, point_a.color});
              triangle_vertices.push_back({top_right, point_b.color});
              triangle_vertices.push_back({top_right, point_b.color});
              triangle_vertices.push_back({bottom_left, point_a.color});
              triangle_vertices.push_back({bottom_right, point_b.color});
            }
          }

          if (triangle_vertices.empty()) {
            break;
          }

          const auto transient_buffer = frame.register_transient_vertices(
              "ui.polyline-triangles", triangle_vertices.data(), static_cast<uint32_t>(triangle_vertices.size() * sizeof(ui::UIPolylineVertex)), static_cast<uint32_t>(triangle_vertices.size()), colored_vertex_layout()
          );

          ensure_polyline_scene_bindings();
          bind_pipeline(polyline_pipeline);
          recorder.bind_binding_group(polyline_scene_bindings);
          bind_vertex_buffer(transient_buffer);
          recorder.draw_vertices(
              static_cast<uint32_t>(triangle_vertices.size())
          );
          break;
        }

        case ui::DrawCommandType::Path: {
          if (!vector_pipeline.valid() || command.path_commands.empty()) {
            break;
          }

          std::vector<ui::UIPolylineVertex> triangle_vertices;
          for (const auto &path_command : command.path_commands) {
            ui::UIPathCommand effective_command = path_command;
            effective_command.style.fill_color.a *= command.color.a;
            effective_command.style.stroke_color.a *= command.color.a;

            const auto tessellated = ui::tessellate_path(effective_command);
            triangle_vertices.insert(
                triangle_vertices.end(),
                tessellated.triangle_vertices.begin(),
                tessellated.triangle_vertices.end()
            );
          }

          if (triangle_vertices.empty()) {
            break;
          }

          const auto transient_buffer = frame.register_transient_vertices(
              "ui.path-triangles",
              triangle_vertices.data(),
              static_cast<uint32_t>(
                  triangle_vertices.size() * sizeof(ui::UIPolylineVertex)
              ),
              static_cast<uint32_t>(triangle_vertices.size()),
              colored_vertex_layout()
          );

          ensure_vector_scene_bindings();
          bind_pipeline(vector_pipeline);
          recorder.bind_binding_group(vector_scene_bindings);
          bind_vertex_buffer(transient_buffer);
          recorder.draw_vertices(
              static_cast<uint32_t>(triangle_vertices.size())
          );
          break;
        }
      }
    }
  }

  apply_scissor({});
  recorder.end_rendering();
}

} // namespace astralix
