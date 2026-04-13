#include "opengl-executor.hpp"

#include "assert.hpp"
#include "glad/glad.h"
#include "trace.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <variant>

namespace astralix {
namespace {

#ifdef ASTRA_TRACE
std::string pass_execute_trace_name(const CompiledPass &pass) {
  return pass.debug_name.empty() ? "RenderPass::execute"
                                 : pass.debug_name + "::execute";
}
#endif

GLenum gl_texture_target(uint32_t samples) {
  return samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
}

bool is_depth_image_format(ImageFormat format) {
  return format == ImageFormat::Depth24Stencil8 || format == ImageFormat::Depth32F;
}

bool same_buffer_layout(const BufferLayout &lhs, const BufferLayout &rhs) {
  if (lhs.get_stride() != rhs.get_stride()) {
    return false;
  }

  const auto &lhs_elements = lhs.get_elements();
  const auto &rhs_elements = rhs.get_elements();
  if (lhs_elements.size() != rhs_elements.size()) {
    return false;
  }

  for (size_t index = 0; index < lhs_elements.size(); ++index) {
    const auto &lhs_element = lhs_elements[index];
    const auto &rhs_element = rhs_elements[index];
    if (lhs_element.name != rhs_element.name ||
        lhs_element.type != rhs_element.type ||
        lhs_element.size != rhs_element.size ||
        lhs_element.offset != rhs_element.offset ||
        lhs_element.normalized != rhs_element.normalized ||
        lhs_element.location != rhs_element.location) {
      return false;
    }
  }

  return true;
}

GLenum gl_internal_format(ImageFormat format) {
  switch (format) {
    case ImageFormat::RGBA8:
      return GL_RGBA8;
    case ImageFormat::RGBA16F:
      return GL_RGBA16F;
    case ImageFormat::RGBA32F:
      return GL_RGBA32F;
    case ImageFormat::R32I:
      return GL_R32I;
    case ImageFormat::Depth24Stencil8:
      return GL_DEPTH24_STENCIL8;
    case ImageFormat::Depth32F:
      return GL_DEPTH_COMPONENT32F;
    case ImageFormat::Undefined:
    default:
      return GL_RGBA8;
  }
}

GLenum gl_upload_format(ImageFormat format) {
  switch (format) {
    case ImageFormat::R32I:
      return GL_RED_INTEGER;
    case ImageFormat::Depth24Stencil8:
      return GL_DEPTH_STENCIL;
    case ImageFormat::Depth32F:
      return GL_DEPTH_COMPONENT;
    case ImageFormat::RGBA8:
    case ImageFormat::RGBA16F:
    case ImageFormat::RGBA32F:
    case ImageFormat::Undefined:
    default:
      return GL_RGBA;
  }
}

GLenum gl_upload_type(ImageFormat format) {
  switch (format) {
    case ImageFormat::R32I:
      return GL_INT;
    case ImageFormat::RGBA16F:
    case ImageFormat::RGBA32F:
    case ImageFormat::Depth32F:
      return GL_FLOAT;
    case ImageFormat::Depth24Stencil8:
      return GL_UNSIGNED_INT_24_8;
    case ImageFormat::RGBA8:
    case ImageFormat::Undefined:
    default:
      return GL_UNSIGNED_BYTE;
  }
}

std::string framebuffer_cache_key(
    const std::vector<OpenGLExecutor::ResolvedImageResource> &color_attachments,
    const std::optional<OpenGLExecutor::ResolvedImageResource> &depth_attachment
) {
  std::ostringstream key;
  key << "c" << color_attachments.size();
  for (const auto &color : color_attachments) {
    key << ':' << color.texture_id << '@' << color.texture_target << '#'
        << static_cast<uint32_t>(color.format);
  }
  key << "|d:";
  if (depth_attachment.has_value()) {
    key << depth_attachment->texture_id << '@' << depth_attachment->texture_target
        << '#' << static_cast<uint32_t>(depth_attachment->format);
  } else {
    key << '0';
  }
  return key.str();
}

} // namespace

void OpenGLExecutor::OpenGLStateCache::reset() {
  draw_binding_valid = false;
  read_binding_valid = false;
  draw_binding = {};
  read_binding = {};
  shader = nullptr;
  vertex_array = nullptr;

  depth_test_valid = false;
  depth_write_valid = false;
  depth_bias_valid = false;
  depth_bias_value_valid = false;
  blend_valid = false;
  cull_valid = false;
  depth_mode_valid = false;
  blend_func_valid = false;
  cull_face_valid = false;
  scissor_valid = false;

  texture2d_slots.clear();
  texture_cube_slots.clear();
  next_texture_unit = 0;
  texture_unit_by_binding_id.clear();
}

OpenGLExecutor::OpenGLExecutor(RenderTarget &render_target)
    : m_api(*render_target.renderer_api()), m_render_target(render_target) {}

OpenGLExecutor::~OpenGLExecutor() {
  destroy_cached_framebuffers();
  for (auto &[_, image] : m_graph_images) {
    if (image.texture_id != 0) {
      glDeleteTextures(1, &image.texture_id);
    }
  }
}

void OpenGLExecutor::execute(const CompiledFrame &frame) {
  ASTRA_PROFILE_N("OpenGLExecutor::execute");
  m_frame = &frame;
  m_bound_pipeline = nullptr;
  m_state.reset();
  m_active_render_extent = {};
  m_api.disable_scissor();
  m_state.scissor_enabled = false;
  m_state.scissor_valid = true;

  for (const auto &pass : frame.passes) {
#ifdef ASTRA_TRACE
    const std::string trace_name = pass_execute_trace_name(pass);
    ASTRA_PROFILE_DYN(trace_name.c_str(), trace_name.size());
#endif
    apply_barriers(pass);
    execute_pass(pass);
  }

  {
    ASTRA_PROFILE_N("OpenGLExecutor::blit_present_edges");
    blit_present_edges(frame);
  }

  m_frame = nullptr;
  m_bound_pipeline = nullptr;
}

std::optional<int>
OpenGLExecutor::read_pixel(const CompiledFrame &frame, ImageHandle src,
                           int x, int y) const {
  const auto *image = frame.find_image(src);
  if (image == nullptr ||
      image->source == CompiledImageSourceKind::DefaultColorTarget) {
    return std::nullopt;
  }

  if (x < 0 || y < 0 || x >= static_cast<int>(image->extent.width) ||
      y >= static_cast<int>(image->extent.height)) {
    return std::nullopt;
  }

  const auto *previous_frame = m_frame;
  const auto *previous_pipeline = m_bound_pipeline;

  int value = 0;
  m_frame = &frame;
  m_bound_pipeline = nullptr;
  invalidate_framebuffer_cache();
  dispatch(ReadbackImageCmd{
      .src = src,
      .x = x,
      .y = y,
      .out_value = &value,
      .out_ready = nullptr,
  });
  invalidate_framebuffer_cache();
  m_frame = previous_frame;
  m_bound_pipeline = previous_pipeline;

  return value;
}

void OpenGLExecutor::execute_pass(const CompiledPass &pass) {
  for (const auto &command : pass.commands) {
    std::visit(
        [this](const auto &typed_command) { dispatch(typed_command); }, command);
  }
}

void OpenGLExecutor::apply_barriers(const CompiledPass &pass) { (void)pass; }

void OpenGLExecutor::dispatch(const BeginRenderingCmd &cmd) {
  ASTRA_ENSURE(
      cmd.info.color_attachments.empty() &&
          !cmd.info.depth_stencil_attachment.has_value(),
      "BeginRenderingCmd requires at least one color or depth attachment");

  std::vector<ResolvedImageResource> color_images;
  color_images.reserve(cmd.info.color_attachments.size());
  for (const auto &attachment : cmd.info.color_attachments) {
    auto image = resolve_image(attachment.view.image);
    if (image.valid()) {
      color_images.push_back(std::move(image));
    }
  }

  std::optional<ResolvedImageResource> depth_image;
  if (cmd.info.depth_stencil_attachment.has_value()) {
    auto image = resolve_image(cmd.info.depth_stencil_attachment->view.image);
    if (image.valid()) {
      depth_image = std::move(image);
    }
  }

  const bool default_target =
      !color_images.empty() ? color_images.front().default_target
                            : (depth_image.has_value() &&
                               depth_image->default_target);
  ASTRA_ENSURE(color_images.empty() && !depth_image.has_value(),
               "BeginRenderingCmd could not resolve a render target");
  ASTRA_ENSURE(default_target &&
                   (color_images.size() > 1 || depth_image.has_value()),
               "Default framebuffer rendering does not support explicit MRT/depth attachments");

  uint32_t framebuffer_id = 0;
  if (!default_target) {
    framebuffer_id = acquire_framebuffer(color_images, depth_image);
  }
  bind_draw_framebuffer(framebuffer_id, default_target);

  m_active_render_extent = cmd.info.extent;
  m_api.set_viewport(0, 0, cmd.info.extent.width, cmd.info.extent.height);

  if (default_target) {
    if (cmd.info.depth_stencil_attachment.has_value() &&
        (cmd.info.depth_stencil_attachment->depth_load_op ==
             AttachmentLoadOp::Clear ||
         cmd.info.depth_stencil_attachment->stencil_load_op ==
             AttachmentLoadOp::Clear)) {
      // BeginRendering clears happen before any pipeline state is rebound.
      // Force depth writes on so depth clears cannot inherit a stale GL_FALSE
      // depth mask from the previous frame.
      m_api.enable_depth_write();
      m_state.depth_write = true;
      m_state.depth_write_valid = true;
    }

    if (!cmd.info.color_attachments.empty() &&
        cmd.info.color_attachments.front().load_op == AttachmentLoadOp::Clear) {
      const auto &first_color = cmd.info.color_attachments.front();
      m_api.clear_color(glm::vec4(first_color.clear_color[0],
                                  first_color.clear_color[1],
                                  first_color.clear_color[2],
                                  first_color.clear_color[3]));
    }

    const auto clear_mask = clear_mask_for(cmd.info);
    if (clear_mask != ClearBufferType::None) {
      m_api.clear_buffers(clear_mask);
    }
    return;
  }

  for (size_t index = 0; index < cmd.info.color_attachments.size() &&
                         index < color_images.size();
       ++index) {
    const auto &attachment_ref = cmd.info.color_attachments[index];
    const auto &resolved = color_images[index];
    if (attachment_ref.load_op != AttachmentLoadOp::Clear) {
      continue;
    }

    if (resolved.format == ImageFormat::R32I) {
      const GLint clear_value[4] = {
          static_cast<GLint>(attachment_ref.clear_color[0]),
          static_cast<GLint>(attachment_ref.clear_color[1]),
          static_cast<GLint>(attachment_ref.clear_color[2]),
          static_cast<GLint>(attachment_ref.clear_color[3]),
      };
      glClearBufferiv(GL_COLOR, static_cast<GLint>(index), clear_value);
    } else {
      const GLfloat clear_value[4] = {
          attachment_ref.clear_color[0],
          attachment_ref.clear_color[1],
          attachment_ref.clear_color[2],
          attachment_ref.clear_color[3],
      };
      glClearBufferfv(GL_COLOR, static_cast<GLint>(index), clear_value);
    }
  }

  if (cmd.info.depth_stencil_attachment.has_value() && depth_image.has_value()) {
    const auto &attachment_ref = *cmd.info.depth_stencil_attachment;
    if (attachment_ref.depth_load_op == AttachmentLoadOp::Clear ||
        attachment_ref.stencil_load_op == AttachmentLoadOp::Clear) {
      m_api.enable_depth_write();
      m_state.depth_write = true;
      m_state.depth_write_valid = true;
    }

    if (depth_image->format == ImageFormat::Depth24Stencil8 &&
        (attachment_ref.depth_load_op == AttachmentLoadOp::Clear ||
         attachment_ref.stencil_load_op == AttachmentLoadOp::Clear)) {
      glClearBufferfi(
          GL_DEPTH_STENCIL, 0,
          attachment_ref.depth_load_op == AttachmentLoadOp::Clear
              ? attachment_ref.clear_depth
              : 1.0f,
          attachment_ref.stencil_load_op == AttachmentLoadOp::Clear
              ? static_cast<GLint>(attachment_ref.clear_stencil)
              : 0
      );
    } else if (attachment_ref.depth_load_op == AttachmentLoadOp::Clear) {
      const GLfloat clear_depth[1] = {attachment_ref.clear_depth};
      glClearBufferfv(GL_DEPTH, 0, clear_depth);
    }
  }
}

void OpenGLExecutor::dispatch(const EndRenderingCmd &cmd) {
  (void)cmd;
  m_active_render_extent = {};
}

void OpenGLExecutor::dispatch(const BindPipelineCmd &cmd) {
  const auto &pipeline = require_pipeline(cmd.pipeline);
  ASTRA_ENSURE(pipeline.shader == nullptr,
               "Compiled pipeline is missing a shader");

  if (m_state.shader != pipeline.shader.get()) {
    pipeline.shader->bind();
    m_state.shader = pipeline.shader.get();
    m_state.next_texture_unit = 0;
    m_state.texture_unit_by_binding_id.clear();
  }

  const bool depth_test = pipeline.desc.depth_stencil.depth_test;
  if (!m_state.depth_test_valid || m_state.depth_test != depth_test) {
    depth_test ? m_api.enable_depth_test() : m_api.disable_depth_test();
    m_state.depth_test = depth_test;
    m_state.depth_test_valid = true;
  }

  const bool depth_write = pipeline.desc.depth_stencil.depth_write;
  if (!m_state.depth_write_valid || m_state.depth_write != depth_write) {
    depth_write ? m_api.enable_depth_write() : m_api.disable_depth_write();
    m_state.depth_write = depth_write;
    m_state.depth_write_valid = true;
  }

  const auto &depth_bias = pipeline.desc.raster.depth_bias;
  if (!m_state.depth_bias_valid || m_state.depth_bias != depth_bias.enabled) {
    depth_bias.enabled ? m_api.enable_depth_bias() : m_api.disable_depth_bias();
    m_state.depth_bias = depth_bias.enabled;
    m_state.depth_bias_valid = true;
  }

  if (depth_bias.enabled &&
      (!m_state.depth_bias_value_valid ||
       m_state.depth_bias_slope != depth_bias.slope_factor ||
       m_state.depth_bias_constant != depth_bias.constant_factor)) {
    m_api.set_depth_bias(
        depth_bias.slope_factor, depth_bias.constant_factor
    );
    m_state.depth_bias_slope = depth_bias.slope_factor;
    m_state.depth_bias_constant = depth_bias.constant_factor;
    m_state.depth_bias_value_valid = true;
  }

  const auto depth_mode =
      map_compare_op(pipeline.desc.depth_stencil.compare_op);
  if (!m_state.depth_mode_valid || m_state.depth_mode != depth_mode) {
    m_api.depth(depth_mode);
    m_state.depth_mode = depth_mode;
    m_state.depth_mode_valid = true;
  }

  const BlendAttachmentState blend_state =
      pipeline.desc.blend_attachments.empty()
          ? BlendAttachmentState::replace()
          : pipeline.desc.blend_attachments.front();

  if (!m_state.blend_valid || m_state.blend != blend_state.enabled) {
    blend_state.enabled ? m_api.enable_blend() : m_api.disable_blend();
    m_state.blend = blend_state.enabled;
    m_state.blend_valid = true;
  }

  const auto blend_src =
      blend_state.src == BlendFactor::SrcAlpha
          ? RendererAPI::BlendFactor::SrcAlpha
          : blend_state.src == BlendFactor::OneMinusSrcAlpha
                ? RendererAPI::BlendFactor::OneMinusSrcAlpha
                : blend_state.src == BlendFactor::Zero
                      ? RendererAPI::BlendFactor::Zero
                      : RendererAPI::BlendFactor::One;
  const auto blend_dst =
      blend_state.dst == BlendFactor::SrcAlpha
          ? RendererAPI::BlendFactor::SrcAlpha
          : blend_state.dst == BlendFactor::OneMinusSrcAlpha
                ? RendererAPI::BlendFactor::OneMinusSrcAlpha
                : blend_state.dst == BlendFactor::Zero
                      ? RendererAPI::BlendFactor::Zero
                      : RendererAPI::BlendFactor::One;

  if (blend_state.enabled &&
      (!m_state.blend_func_valid || m_state.blend_src != blend_src ||
       m_state.blend_dst != blend_dst)) {
    m_api.set_blend_func(blend_src, blend_dst);
    m_state.blend_src = blend_src;
    m_state.blend_dst = blend_dst;
    m_state.blend_func_valid = true;
  }

  const bool cull_enabled = pipeline.desc.raster.cull_mode != CullMode::None;
  if (!m_state.cull_valid || m_state.cull != cull_enabled) {
    cull_enabled ? m_api.enable_cull() : m_api.disable_cull();
    m_state.cull = cull_enabled;
    m_state.cull_valid = true;
  }

  if (cull_enabled) {
    const auto cull_mode = map_cull_mode(pipeline.desc.raster.cull_mode);
    if (!m_state.cull_face_valid || m_state.cull_face != cull_mode) {
      m_api.cull_face(cull_mode);
      m_state.cull_face = cull_mode;
      m_state.cull_face_valid = true;
    }
  }

  m_bound_pipeline = &pipeline;
}

void OpenGLExecutor::dispatch(const BindBindingsCmd &cmd) {
  ASTRA_ENSURE(m_bound_pipeline == nullptr || m_bound_pipeline->shader == nullptr,
               "BindBindingsCmd requires a bound pipeline");

  const auto &binding_group = require_binding_group(cmd.binding_group);
  auto &shader = *m_bound_pipeline->shader;

  for (const auto &value : binding_group.values) {
    apply_value_binding(shader, value);
  }

  for (const auto &image : binding_group.sampled_images) {
    const auto texture_id = resolve_texture_id(image.view);
    const auto [slot_it, inserted] =
        m_state.texture_unit_by_binding_id.try_emplace(
            image.binding_id, m_state.next_texture_unit
        );
    if (inserted) {
      ++m_state.next_texture_unit;
    }
    const uint32_t texture_unit = slot_it->second;

    std::vector<uint32_t> *slots = nullptr;
    switch (image.target) {
      case CompiledSampledImageTarget::TextureCube:
        slots = &m_state.texture_cube_slots;
        break;
      case CompiledSampledImageTarget::Texture2D:
      default:
        slots = &m_state.texture2d_slots;
        break;
    }

    if (slots->size() <= texture_unit) {
      slots->resize(texture_unit + 1, 0);
    }

    if ((*slots)[texture_unit] != texture_id) {
      if (image.target == CompiledSampledImageTarget::TextureCube) {
        m_api.bind_texture_cube(texture_id, texture_unit);
      } else {
        m_api.bind_texture_2d(texture_id, texture_unit);
      }
      (*slots)[texture_unit] = texture_id;
    }

    const int unit = static_cast<int>(texture_unit);
    ASTRA_ENSURE(image.binding_id == 0,
                 "BindBindingsCmd encountered a sampled image binding with "
                 "binding id 0");
    shader.apply_binding_value(image.binding_id, ShaderValueKind::Int, &unit);
  }
}

void OpenGLExecutor::dispatch(const BindVertexBufferCmd &cmd) {
  const auto &buffer = require_buffer(cmd.buffer);
  if (buffer.is_transient) {
    ensure_transient_buffer_uploaded(buffer);
    m_state.vertex_array = m_transient_vertex_array;
  } else {
    m_state.vertex_array = buffer.vertex_array;
  }
}

void OpenGLExecutor::dispatch(const BindIndexBufferCmd &cmd) {
  const auto &buffer = require_buffer(cmd.buffer);
  if (buffer.is_transient) {
    ensure_transient_buffer_uploaded(buffer);
    m_state.vertex_array = m_transient_vertex_array;
  } else {
    m_state.vertex_array = buffer.vertex_array;
  }
  (void)cmd.index_type;
  (void)cmd.offset;
}

void OpenGLExecutor::dispatch(const DrawIndexedCmd &cmd) {
  ASTRA_ENSURE(m_state.vertex_array == nullptr,
               "DrawIndexedCmd requires a bound vertex array");

  if (cmd.args.instance_count > 1) {
    m_state.vertex_array->bind();
    m_api.draw_instanced_indexed(RendererAPI::DrawPrimitive::TRIANGLES,
                                 cmd.args.index_count,
                                 cmd.args.instance_count);
    m_state.vertex_array->unbind();
    return;
  }

  m_api.draw_indexed(m_state.vertex_array,
                     RendererAPI::DrawPrimitive::TRIANGLES,
                     cmd.args.index_count);
}

void OpenGLExecutor::dispatch(const CopyImageCmd &cmd) {
  const auto source = resolve_image(cmd.src);
  const auto destination = resolve_image(cmd.dst);
  if (!source.valid() || !destination.valid() || source.default_target ||
      destination.default_target) {
    return;
  }

  const bool depth_copy = source.aspect == ImageAspect::Depth ||
                          is_depth_image_format(source.format);
  const auto source_framebuffer = acquire_framebuffer(
      depth_copy ? std::vector<ResolvedImageResource>{}
                 : std::vector<ResolvedImageResource>{source},
      depth_copy ? std::optional<ResolvedImageResource>(source) : std::nullopt
  );
  const auto destination_framebuffer = acquire_framebuffer(
      depth_copy ? std::vector<ResolvedImageResource>{}
                 : std::vector<ResolvedImageResource>{destination},
      depth_copy ? std::optional<ResolvedImageResource>(destination)
                 : std::nullopt
  );

  bind_read_framebuffer(source_framebuffer, false);
  bind_draw_framebuffer(destination_framebuffer, false);

  const GLbitfield mask = depth_copy ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT;
  const uint32_t width =
      cmd.region.width != 0
          ? cmd.region.width
          : std::min(source.extent.width, destination.extent.width);
  const uint32_t height =
      cmd.region.height != 0
          ? cmd.region.height
          : std::min(source.extent.height, destination.extent.height);
  glBlitFramebuffer(
      0, 0, static_cast<GLint>(width), static_cast<GLint>(height), 0, 0,
      static_cast<GLint>(width), static_cast<GLint>(height), mask, GL_NEAREST
  );
  invalidate_framebuffer_cache();
}

void OpenGLExecutor::dispatch(const ResolveImageCmd &cmd) {
  const auto source = resolve_image(cmd.src);
  const auto destination = resolve_image(cmd.dst);
  if (!source.valid() || !destination.valid() || source.default_target ||
      destination.default_target) {
    return;
  }

  const bool depth_resolve = source.aspect == ImageAspect::Depth ||
                             is_depth_image_format(source.format);
  const auto source_framebuffer = acquire_framebuffer(
      depth_resolve ? std::vector<ResolvedImageResource>{}
                    : std::vector<ResolvedImageResource>{source},
      depth_resolve ? std::optional<ResolvedImageResource>(source)
                    : std::nullopt
  );
  const auto destination_framebuffer = acquire_framebuffer(
      depth_resolve ? std::vector<ResolvedImageResource>{}
                    : std::vector<ResolvedImageResource>{destination},
      depth_resolve ? std::optional<ResolvedImageResource>(destination)
                    : std::nullopt
  );

  bind_read_framebuffer(source_framebuffer, false);
  bind_draw_framebuffer(destination_framebuffer, false);
  const uint32_t width =
      std::min(source.extent.width, destination.extent.width);
  const uint32_t height =
      std::min(source.extent.height, destination.extent.height);
  glBlitFramebuffer(
      0, 0, static_cast<GLint>(width), static_cast<GLint>(height), 0, 0,
      static_cast<GLint>(width), static_cast<GLint>(height),
      depth_resolve ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT, GL_NEAREST
  );
  invalidate_framebuffer_cache();
}

void OpenGLExecutor::dispatch(const ReadbackImageCmd &cmd) const {
  ASTRA_ENSURE(cmd.out_value == nullptr,
               "ReadbackImageCmd requires an output pointer");

  const auto source = resolve_image(cmd.src);
  ASTRA_ENSURE(source.default_target,
               "Default framebuffer images cannot be bound for readback");
  ASTRA_ENSURE(
      cmd.x < 0 || cmd.y < 0 ||
          cmd.x >= static_cast<int>(source.extent.width) ||
          cmd.y >= static_cast<int>(source.extent.height),
      "ReadbackImageCmd pixel is out of bounds"
  );

  const bool depth_read = source.aspect == ImageAspect::Depth ||
                          is_depth_image_format(source.format);
  const auto source_framebuffer = acquire_framebuffer(
      depth_read ? std::vector<ResolvedImageResource>{}
                 : std::vector<ResolvedImageResource>{source},
      depth_read ? std::optional<ResolvedImageResource>(source) : std::nullopt
  );

  bind_read_framebuffer(source_framebuffer, false);
  if (depth_read) {
    float depth_value = 0.0f;
    glReadPixels(
        cmd.x, cmd.y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth_value
    );
    *cmd.out_value = static_cast<int>(depth_value);
  } else {
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    if (source.format == ImageFormat::R32I) {
      glReadPixels(
          cmd.x, cmd.y, 1, 1, GL_RED_INTEGER, GL_INT, cmd.out_value
      );
    } else {
      GLint pixel = 0;
      glReadPixels(cmd.x, cmd.y, 1, 1, GL_RED, GL_INT, &pixel);
      *cmd.out_value = pixel;
    }
  }
  if (cmd.out_ready != nullptr) {
    *cmd.out_ready = true;
  }
}

const CompiledImage &OpenGLExecutor::require_image(ImageHandle handle) const {
  ASTRA_ENSURE(m_frame == nullptr, "OpenGLExecutor has no active frame");
  const auto *image = m_frame->find_image(handle);
  ASTRA_ENSURE(image == nullptr, "Unknown compiled image handle: ", handle.id);
  return *image;
}

const CompiledPipeline &
OpenGLExecutor::require_pipeline(RenderPipelineHandle handle) const {
  ASTRA_ENSURE(m_frame == nullptr, "OpenGLExecutor has no active frame");
  const auto *pipeline = m_frame->find_pipeline(handle);
  ASTRA_ENSURE(pipeline == nullptr, "Unknown compiled pipeline handle: ",
               handle.id);
  return *pipeline;
}

const CompiledBindingGroup &
OpenGLExecutor::require_binding_group(RenderBindingGroupHandle handle) const {
  ASTRA_ENSURE(m_frame == nullptr, "OpenGLExecutor has no active frame");
  const auto *binding_group = m_frame->find_binding_group(handle);
  ASTRA_ENSURE(binding_group == nullptr,
               "Unknown compiled binding group handle: ", handle.id);
  return *binding_group;
}

const CompiledBuffer &OpenGLExecutor::require_buffer(BufferHandle handle) const {
  ASTRA_ENSURE(m_frame == nullptr, "OpenGLExecutor has no active frame");
  const auto *buffer = m_frame->find_buffer(handle);
  ASTRA_ENSURE(buffer == nullptr, "Unknown compiled buffer handle: ",
               handle.id);
  return *buffer;
}

OpenGLExecutor::ResolvedImageResource
OpenGLExecutor::resolve_image(ImageHandle handle) const {
  const auto &image = require_image(handle);

  switch (image.source) {
    case CompiledImageSourceKind::DefaultColorTarget:
      return ResolvedImageResource{
          .texture_id = 0,
          .texture_target = 0,
          .extent = image.extent,
          .format = ImageFormat::RGBA8,
          .aspect = image.aspect,
          .default_target = true,
      };
    case CompiledImageSourceKind::GraphImage:
      return resolve_graph_image(image);
    case CompiledImageSourceKind::Texture2DResource:
    case CompiledImageSourceKind::TextureCubeResource:
      ASTRA_ENSURE(image.texture == nullptr,
                   "Sampled image handle resolved to a null texture");
      return ResolvedImageResource{
          .texture_id = image.texture->renderer_id(),
          .texture_target =
              image.source == CompiledImageSourceKind::TextureCubeResource
                  ? static_cast<uint32_t>(GL_TEXTURE_CUBE_MAP)
                  : static_cast<uint32_t>(GL_TEXTURE_2D),
          .extent = image.extent,
          .format = ImageFormat::RGBA8,
          .aspect = ImageAspect::Color0,
          .default_target = false,
      };
    case CompiledImageSourceKind::RawTextureId:
      ASTRA_ENSURE(image.raw_renderer_id == 0, "Raw texture id is zero");
      return ResolvedImageResource{
          .texture_id = image.raw_renderer_id,
          .texture_target = GL_TEXTURE_2D,
          .extent = image.extent,
          .format = ImageFormat::RGBA8,
          .aspect = image.aspect,
          .default_target = false,
      };
    default:
      return {};
  }
}

OpenGLExecutor::ResolvedImageResource
OpenGLExecutor::resolve_graph_image(const CompiledImage &image) const {
  ASTRA_ENSURE(image.graph_image == nullptr,
               "Compiled graph image is missing graph-image backing");
  const auto *key = image.graph_image.get();
  auto &cached = m_graph_images[key];

  if (cached.texture_id == 0 ||
      cached.desc_snapshot.width != image.graph_image->desc.width ||
      cached.desc_snapshot.height != image.graph_image->desc.height ||
      cached.desc_snapshot.depth != image.graph_image->desc.depth ||
      cached.desc_snapshot.mip_levels != image.graph_image->desc.mip_levels ||
      cached.desc_snapshot.samples != image.graph_image->desc.samples ||
      cached.desc_snapshot.format != image.graph_image->desc.format) {
    if (cached.texture_id != 0) {
      glDeleteTextures(1, &cached.texture_id);
      destroy_cached_framebuffers();
    }

    cached.desc_snapshot = image.graph_image->desc;
    cached.texture_target = gl_texture_target(cached.desc_snapshot.samples);

    glGenTextures(1, &cached.texture_id);
    glBindTexture(cached.texture_target, cached.texture_id);
    if (cached.texture_target == GL_TEXTURE_2D_MULTISAMPLE) {
      glTexImage2DMultisample(
          GL_TEXTURE_2D_MULTISAMPLE,
          static_cast<GLsizei>(std::max(cached.desc_snapshot.samples, 1u)),
          gl_internal_format(cached.desc_snapshot.format),
          static_cast<GLsizei>(std::max(cached.desc_snapshot.width, 1u)),
          static_cast<GLsizei>(std::max(cached.desc_snapshot.height, 1u)),
          GL_TRUE
      );
    } else {
      glTexImage2D(
          GL_TEXTURE_2D, 0, gl_internal_format(cached.desc_snapshot.format),
          static_cast<GLsizei>(std::max(cached.desc_snapshot.width, 1u)),
          static_cast<GLsizei>(std::max(cached.desc_snapshot.height, 1u)), 0,
          gl_upload_format(cached.desc_snapshot.format),
          gl_upload_type(cached.desc_snapshot.format), nullptr
      );

      const GLint filter =
          cached.desc_snapshot.format == ImageFormat::R32I ||
                  is_depth_image_format(cached.desc_snapshot.format)
              ? GL_NEAREST
              : GL_LINEAR;
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

      if (is_depth_image_format(cached.desc_snapshot.format)) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
      } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      }
    }
  }

  return ResolvedImageResource{
      .texture_id = cached.texture_id,
      .texture_target = cached.texture_target,
      .extent = image.graph_image->extent(),
      .format = image.graph_image->desc.format,
      .aspect = image.aspect,
      .default_target = false,
  };
}

uint32_t OpenGLExecutor::acquire_framebuffer(
    const std::vector<ResolvedImageResource> &color_attachments,
    const std::optional<ResolvedImageResource> &depth_attachment
) const {
  const auto key = framebuffer_cache_key(color_attachments, depth_attachment);
  if (const auto it = m_framebuffer_cache.find(key);
      it != m_framebuffer_cache.end()) {
    return it->second;
  }

  uint32_t framebuffer = 0;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  for (size_t index = 0; index < color_attachments.size(); ++index) {
    const auto &attachment = color_attachments[index];
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index),
        attachment.texture_target, attachment.texture_id, 0
    );
  }

  if (depth_attachment.has_value()) {
    const auto attachment_enum =
        depth_attachment->format == ImageFormat::Depth24Stencil8
            ? GL_DEPTH_STENCIL_ATTACHMENT
            : GL_DEPTH_ATTACHMENT;
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, attachment_enum, depth_attachment->texture_target,
        depth_attachment->texture_id, 0
    );
  }

  if (color_attachments.empty()) {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  } else {
    std::vector<GLenum> draw_buffers;
    draw_buffers.reserve(color_attachments.size());
    for (size_t index = 0; index < color_attachments.size(); ++index) {
      draw_buffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index));
    }
    glDrawBuffers(
        static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data()
    );
    glReadBuffer(GL_COLOR_ATTACHMENT0);
  }

  ASTRA_ENSURE(
      glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE,
      "Assembled OpenGL framebuffer is incomplete"
  );

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  m_framebuffer_cache.emplace(key, framebuffer);
  return framebuffer;
}

void OpenGLExecutor::bind_draw_framebuffer(
    uint32_t framebuffer_id, bool default_target
) {
  FramebufferBindingState desired{
      .framebuffer_id = framebuffer_id,
      .default_target = default_target,
  };

  if (m_state.draw_binding_valid &&
      m_state.draw_binding.framebuffer_id == desired.framebuffer_id &&
      m_state.draw_binding.default_target == desired.default_target) {
    return;
  }

  if (default_target) {
    m_render_target.framebuffer()->bind(FramebufferBindType::Draw, 0);
  } else {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_id);
  }

  m_state.draw_binding = desired;
  m_state.draw_binding_valid = true;
}

void OpenGLExecutor::bind_read_framebuffer(
    uint32_t framebuffer_id, bool default_target
) const {
  FramebufferBindingState desired{
      .framebuffer_id = framebuffer_id,
      .default_target = default_target,
  };

  if (m_state.read_binding_valid &&
      m_state.read_binding.framebuffer_id == desired.framebuffer_id &&
      m_state.read_binding.default_target == desired.default_target) {
    return;
  }

  if (default_target) {
    m_render_target.framebuffer()->bind(FramebufferBindType::Read, 0);
  } else {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_id);
  }

  m_state.read_binding = desired;
  m_state.read_binding_valid = true;
}

uint32_t OpenGLExecutor::resolve_texture_id(ImageViewRef view) const {
  const auto image = resolve_image(view.image);
  ASTRA_ENSURE(image.default_target,
               "Default framebuffer cannot be sampled as a texture");
  ASTRA_ENSURE(!image.valid(), "Unsupported sampled image source");
  return image.texture_id;
}

ClearBufferType OpenGLExecutor::clear_mask_for(const RenderingInfo &info) const {
  ClearBufferType mask = ClearBufferType::None;

  for (const auto &attachment : info.color_attachments) {
    if (attachment.load_op == AttachmentLoadOp::Clear) {
      mask |= ClearBufferType::Color;
      break;
    }
  }

  if (info.depth_stencil_attachment.has_value()) {
    const auto &depth = *info.depth_stencil_attachment;
    if (depth.depth_load_op == AttachmentLoadOp::Clear) {
      mask |= ClearBufferType::Depth;
    }
    if (depth.stencil_load_op == AttachmentLoadOp::Clear) {
      mask |= ClearBufferType::Stencil;
    }
  }

  return mask;
}

RendererAPI::DepthMode OpenGLExecutor::map_compare_op(CompareOp op) const {
  switch (op) {
    case CompareOp::Equal:
      return RendererAPI::DepthMode::Equal;
    case CompareOp::LessEqual:
      return RendererAPI::DepthMode::LessEqual;
    case CompareOp::Less:
    case CompareOp::Never:
    case CompareOp::Greater:
    case CompareOp::Always:
    default:
      return RendererAPI::DepthMode::Less;
  }
}

RendererAPI::CullFaceMode OpenGLExecutor::map_cull_mode(CullMode mode) const {
  switch (mode) {
    case CullMode::Front:
      return RendererAPI::CullFaceMode::Front;
    case CullMode::Back:
    case CullMode::None:
    default:
      return RendererAPI::CullFaceMode::Back;
  }
}

void OpenGLExecutor::apply_value_binding(
    const Shader &shader, const CompiledValueBinding &binding) const {
  ASTRA_ENSURE(binding.bytes.empty(),
               "Compiled value binding is missing payload bytes");
  ASTRA_ENSURE(binding.binding_id == 0,
               "Compiled value binding is missing binding id");
  shader.apply_binding_value(binding.binding_id, binding.kind,
                             binding.bytes.data());
}

void OpenGLExecutor::dispatch(const SetScissorCmd &cmd) {
  if (!cmd.enabled) {
    if (!m_state.scissor_valid || m_state.scissor_enabled) {
      m_api.disable_scissor();
      m_state.scissor_enabled = false;
      m_state.scissor_valid = true;
    }
    return;
  }

  if (!m_state.scissor_valid || !m_state.scissor_enabled) {
    m_api.enable_scissor();
    m_state.scissor_enabled = true;
    m_state.scissor_valid = true;
  }

  const uint32_t bottom_y =
      m_active_render_extent.height > cmd.y + cmd.height
          ? m_active_render_extent.height - (cmd.y + cmd.height)
          : 0u;
  m_api.set_scissor_rect(cmd.x, bottom_y, cmd.width, cmd.height);
}

void OpenGLExecutor::dispatch(const DrawVerticesCmd &cmd) {
  ASTRA_ENSURE(m_state.vertex_array == nullptr,
               "DrawVerticesCmd requires a bound vertex array");

  m_api.draw_triangles(m_state.vertex_array, cmd.vertex_count);
}

void OpenGLExecutor::ensure_transient_buffer_uploaded(
    const CompiledBuffer &buffer) {
  if (!buffer.is_transient || buffer.transient_data.empty()) {
    return;
  }

  const auto backend = m_render_target.renderer_api()->get_backend();
  const size_t required_bytes = buffer.transient_data.size();
  const bool layout_changed =
      !m_transient_layout.has_value() ||
      !same_buffer_layout(*m_transient_layout, buffer.transient_layout);

  if (m_transient_vertex_array == nullptr ||
      required_bytes > m_transient_capacity || layout_changed) {
    m_transient_capacity = std::max(required_bytes, size_t(4096u));
    m_transient_vertex_array = VertexArray::create(backend);
    m_transient_vertex_buffer = VertexBuffer::create(
        backend, static_cast<uint32_t>(m_transient_capacity));
    m_transient_vertex_buffer->set_layout(buffer.transient_layout);
    m_transient_vertex_array->add_vertex_buffer(m_transient_vertex_buffer);
    m_transient_vertex_array->unbind();
    m_transient_layout = buffer.transient_layout;
  }

  m_transient_vertex_buffer->set_data(
      buffer.transient_data.data(),
      static_cast<uint32_t>(required_bytes));
}

void OpenGLExecutor::invalidate_framebuffer_cache() const {
  m_state.draw_binding_valid = false;
  m_state.read_binding_valid = false;
}

void OpenGLExecutor::destroy_cached_framebuffers() const {
  for (const auto &[_, framebuffer] : m_framebuffer_cache) {
    if (framebuffer != 0) {
      glDeleteFramebuffers(1, &framebuffer);
    }
  }
  m_framebuffer_cache.clear();
  invalidate_framebuffer_cache();
}

void OpenGLExecutor::blit_present_edges(const CompiledFrame &frame) {
  for (const auto &edge : frame.present_edges) {
    if (!edge.source.image.valid() || edge.extent.width == 0 ||
        edge.extent.height == 0) {
      continue;
    }

    const auto source = resolve_image(edge.source.image);
    if (!source.valid() || source.default_target) {
      continue;
    }

    const bool depth_source = source.aspect == ImageAspect::Depth ||
                              is_depth_image_format(source.format);
    const auto source_framebuffer = acquire_framebuffer(
        depth_source ? std::vector<ResolvedImageResource>{}
                     : std::vector<ResolvedImageResource>{source},
        depth_source ? std::optional<ResolvedImageResource>(source)
                     : std::nullopt
    );

    bind_read_framebuffer(source_framebuffer, false);
    bind_draw_framebuffer(0, true);

    const auto &present_spec = m_render_target.framebuffer()->get_specification();
    glBlitFramebuffer(
        0, 0, static_cast<GLint>(edge.extent.width),
        static_cast<GLint>(edge.extent.height), 0, 0,
        static_cast<GLint>(present_spec.width),
        static_cast<GLint>(present_spec.height),
        depth_source ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT, GL_NEAREST
    );
    invalidate_framebuffer_cache();
  }
}

} // namespace astralix
