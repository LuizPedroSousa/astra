#include "platform/OpenGL/opengl-executor.hpp"

#include "resources/shader.hpp"
#include "targets/render-target.hpp"
#include <gtest/gtest.h>
#include <cstring>
#include <optional>
#include <unordered_map>

namespace astralix {

RenderTarget::RenderTarget(Scope<RendererAPI> renderer_api,
                           Ref<Framebuffer> framebuffer, MSAA msaa,
                           WindowID window_id)
    : m_framebuffer(std::move(framebuffer)),
      m_renderer_api(std::move(renderer_api)), m_msaa(msaa),
      m_window_id(std::move(window_id)) {}

namespace {

class FakeIndexBuffer : public IndexBuffer {
public:
  void bind() const override { ++bind_calls; }
  void unbind() const override { ++unbind_calls; }
  uint32_t get_count() const override { return count; }
  void set_data(const void *data, uint32_t size) const override {
    (void)data;
    last_size = size;
  }

  mutable uint32_t bind_calls = 0;
  mutable uint32_t unbind_calls = 0;
  mutable uint32_t last_size = 0;
  uint32_t count = 6;
};

class FakeVertexBuffer : public VertexBuffer {
public:
  void bind() const override { ++bind_calls; }
  void unbind() const override { ++unbind_calls; }
  void set_data(const void *data, uint32_t size) override {
    (void)data;
    last_size = size;
  }

  const BufferLayout &get_layout() const override { return layout; }
  void set_layout(const BufferLayout &new_layout) override { layout = new_layout; }

  mutable uint32_t bind_calls = 0;
  mutable uint32_t unbind_calls = 0;
  uint32_t last_size = 0;
  BufferLayout layout;
};

class FakeVertexArray : public VertexArray {
public:
  void bind() const override { ++bind_calls; }
  void unbind() const override { ++unbind_calls; }

  void add_vertex_buffer(const Ref<VertexBuffer> &vertex_buffer) override {
    vertex_buffers.push_back(vertex_buffer);
  }

  void set_index_buffer(const Ref<IndexBuffer> &new_index_buffer) override {
    index_buffer = new_index_buffer;
  }

  const std::vector<Ref<VertexBuffer>> &get_vertex_buffers() const override {
    return vertex_buffers;
  }

  const Ref<IndexBuffer> &get_index_buffer() const override {
    return index_buffer;
  }

  mutable uint32_t bind_calls = 0;
  mutable uint32_t unbind_calls = 0;
  std::vector<Ref<VertexBuffer>> vertex_buffers;
  Ref<IndexBuffer> index_buffer = create_ref<FakeIndexBuffer>();
};

class FakeFramebuffer : public Framebuffer {
public:
  explicit FakeFramebuffer(uint32_t width, uint32_t height,
                           std::vector<uint32_t> colors = {11u},
                           uint32_t depth = 99u)
      : color_attachments(std::move(colors)), depth_attachment(depth) {
    specification.width = width;
    specification.height = height;
  }

  void bind(FramebufferBindType type = FramebufferBindType::Default,
            uint32_t id = static_cast<uint32_t>(-1)) override {
    bind_records.push_back({type, id});
  }

  void unbind() override { ++unbind_calls; }

  void resize(uint32_t width, uint32_t height) override {
    specification.width = width;
    specification.height = height;
  }

  int read_pixel(uint32_t attachmentIndex, int x, int y) override {
    last_read_attachment = attachmentIndex;
    last_read_x = x;
    last_read_y = y;
    return read_pixel_result;
  }

  void clear_attachment(uint32_t attachmentIndex, int value) override {
    last_cleared_attachment = attachmentIndex;
    last_cleared_value = value;
  }

  uint32_t get_color_attachment_id(uint32_t index = 0) const override {
    return color_attachments.at(index);
  }

  const std::vector<uint32_t> &get_color_attachments() const override {
    return color_attachments;
  }

  uint32_t get_depth_attachment_id() const override { return depth_attachment; }

  void blit(uint32_t width, uint32_t height,
            FramebufferBlitType type = FramebufferBlitType::Color,
            FramebufferBlitFilter filter = FramebufferBlitFilter::Nearest)
      override {
    ++blit_calls;
    last_blit_width = width;
    last_blit_height = height;
    last_blit_type = type;
    last_blit_filter = filter;
  }

  const FramebufferSpecification &get_specification() const override {
    return specification;
  }

  struct BindRecord {
    FramebufferBindType type;
    uint32_t id;
  };

  FramebufferSpecification specification;
  std::vector<uint32_t> color_attachments;
  uint32_t depth_attachment = 0;
  std::vector<BindRecord> bind_records;
  uint32_t unbind_calls = 0;
  uint32_t blit_calls = 0;
  uint32_t last_blit_width = 0;
  uint32_t last_blit_height = 0;
  FramebufferBlitType last_blit_type = FramebufferBlitType::None;
  FramebufferBlitFilter last_blit_filter = FramebufferBlitFilter::Nearest;
  uint32_t last_read_attachment = 0;
  int last_read_x = 0;
  int last_read_y = 0;
  int read_pixel_result = 7;
  uint32_t last_cleared_attachment = 0;
  int last_cleared_value = 0;
};

class FakeTexture : public Texture {
public:
  explicit FakeTexture(uint32_t renderer_id, uint32_t width = 128,
                       uint32_t height = 128)
      : Texture(ResourceHandle{0, renderer_id}),
        m_renderer_id(renderer_id),
        m_width(width),
        m_height(height) {}

  void bind() const override {}
  void active(uint32_t) const override {}
  uint32_t renderer_id() const override { return m_renderer_id; }
  uint32_t width() const override { return m_width; }
  uint32_t height() const override { return m_height; }

private:
  uint32_t m_renderer_id = 0;
  uint32_t m_width = 0;
  uint32_t m_height = 0;
};

class FakeRendererAPI : public RendererAPI {
public:
  FakeRendererAPI() { m_backend = RendererBackend::OpenGL; }

  void init() override {}

  void set_viewport(uint32_t x, uint32_t y, uint32_t width,
                    uint32_t height) override {
    viewport = {x, y, width, height};
  }

  void clear_color(glm::vec4 color) override {
    clear_color_value = color;
    ++clear_color_calls;
  }

  void clear_buffers(ClearBufferType type) override {
    last_clear_mask = type;
    ++clear_buffer_calls;
  }

  void disable_buffer_testing() override { ++disable_buffer_testing_calls; }
  void enable_buffer_testing() override { ++enable_buffer_testing_calls; }
  void enable_depth_test() override { ++enable_depth_test_calls; }
  void disable_depth_test() override { ++disable_depth_test_calls; }
  void enable_depth_write() override { ++enable_depth_write_calls; }
  void disable_depth_write() override { ++disable_depth_write_calls; }
  void enable_depth_bias() override { ++enable_depth_bias_calls; }
  void disable_depth_bias() override { ++disable_depth_bias_calls; }
  void set_depth_bias(float slope_factor, float constant_factor) override {
    last_depth_bias_slope = slope_factor;
    last_depth_bias_constant = constant_factor;
    ++set_depth_bias_calls;
  }
  void enable_blend() override { ++enable_blend_calls; }
  void disable_blend() override { ++disable_blend_calls; }

  void set_blend_func(BlendFactor src, BlendFactor dst) override {
    last_blend_src = src;
    last_blend_dst = dst;
    ++set_blend_func_calls;
  }

  void enable_scissor() override { ++enable_scissor_calls; }
  void disable_scissor() override { ++disable_scissor_calls; }

  void set_scissor_rect(uint32_t x, uint32_t y, uint32_t width,
                        uint32_t height) override {
    scissor = {x, y, width, height};
  }

  void enable_cull() override { ++enable_cull_calls; }
  void disable_cull() override { ++disable_cull_calls; }

  void bind_texture_2d(uint32_t texture_id, uint32_t slot) override {
    texture_2d_binds.push_back({texture_id, slot});
  }

  void bind_texture_cube(uint32_t texture_id, uint32_t slot) override {
    texture_cube_binds.push_back({texture_id, slot});
  }

  void cull_face(CullFaceMode mode) override {
    last_cull_face = mode;
    ++cull_face_calls;
  }

  void depth(DepthMode mode) override {
    last_depth_mode = mode;
    ++depth_mode_calls;
  }

  void draw_indexed(const Ref<VertexArray> &vertex_array,
                    DrawPrimitive primitive_type,
                    uint32_t index_count) override {
    last_vertex_array = vertex_array.get();
    last_primitive = primitive_type;
    last_index_count = index_count;
    ++draw_indexed_calls;
    m_frame_stats.draw_call_count++;
  }

  void draw_instanced_indexed(DrawPrimitive primitive_type,
                              uint32_t index_count,
                              uint32_t instance_count) override {
    last_primitive = primitive_type;
    last_index_count = index_count;
    last_instance_count = instance_count;
    ++draw_instanced_calls;
    m_frame_stats.draw_call_count++;
  }

  void draw_lines(const Ref<VertexArray> &vertex_array,
                  uint32_t vertex_count) override {
    last_vertex_array = vertex_array.get();
    last_vertex_count = vertex_count;
    ++draw_lines_calls;
  }

  void draw_triangles(const Ref<VertexArray> &vertex_array,
                      uint32_t vertex_count) override {
    last_vertex_array = vertex_array.get();
    last_vertex_count = vertex_count;
    ++draw_triangles_calls;
  }

  struct TextureBind {
    uint32_t texture_id = 0;
    uint32_t slot = 0;
  };

  glm::uvec4 viewport{};
  glm::uvec4 scissor{};
  glm::vec4 clear_color_value{};
  ClearBufferType last_clear_mask = ClearBufferType::None;
  BlendFactor last_blend_src = BlendFactor::One;
  BlendFactor last_blend_dst = BlendFactor::Zero;
  CullFaceMode last_cull_face = CullFaceMode::Back;
  DepthMode last_depth_mode = DepthMode::Less;
  float last_depth_bias_slope = 0.0f;
  float last_depth_bias_constant = 0.0f;
  const VertexArray *last_vertex_array = nullptr;
  DrawPrimitive last_primitive = DrawPrimitive::TRIANGLES;
  uint32_t last_index_count = 0;
  uint32_t last_instance_count = 0;
  uint32_t last_vertex_count = 0;

  uint32_t clear_color_calls = 0;
  uint32_t clear_buffer_calls = 0;
  uint32_t enable_buffer_testing_calls = 0;
  uint32_t disable_buffer_testing_calls = 0;
  uint32_t enable_depth_test_calls = 0;
  uint32_t disable_depth_test_calls = 0;
  uint32_t enable_depth_write_calls = 0;
  uint32_t disable_depth_write_calls = 0;
  uint32_t enable_depth_bias_calls = 0;
  uint32_t disable_depth_bias_calls = 0;
  uint32_t set_depth_bias_calls = 0;
  uint32_t enable_blend_calls = 0;
  uint32_t disable_blend_calls = 0;
  uint32_t set_blend_func_calls = 0;
  uint32_t enable_scissor_calls = 0;
  uint32_t disable_scissor_calls = 0;
  uint32_t enable_cull_calls = 0;
  uint32_t disable_cull_calls = 0;
  uint32_t cull_face_calls = 0;
  uint32_t depth_mode_calls = 0;
  uint32_t draw_indexed_calls = 0;
  uint32_t draw_instanced_calls = 0;
  uint32_t draw_lines_calls = 0;
  uint32_t draw_triangles_calls = 0;
  std::vector<TextureBind> texture_2d_binds;
  std::vector<TextureBind> texture_cube_binds;
};

class FakeShader : public Shader {
public:
  FakeShader() : Shader(ResourceHandle{1, 1}, "fake-shader") {}

  void bind() const override { ++bind_calls; }
  void unbind() const override { ++unbind_calls; }
  void attach() const override {}

  uint32_t renderer_id() const override { return 17; }

  mutable uint32_t bind_calls = 0;
  mutable uint32_t unbind_calls = 0;

  struct TypedUpload {
    uint64_t binding_id = 0;
    ShaderValueKind kind = ShaderValueKind::Float;
    std::vector<uint8_t> bytes;
  };

  mutable std::vector<TypedUpload> typed_uploads;

protected:
  void set_typed_value(uint64_t binding_id, ShaderValueKind kind,
                       const void *value) const override {
    TypedUpload upload{
        .binding_id = binding_id,
        .kind = kind,
    };

    size_t value_size = 0;
    switch (kind) {
      case ShaderValueKind::Bool:
        value_size = sizeof(bool);
        break;
      case ShaderValueKind::Int:
        value_size = sizeof(int);
        break;
      case ShaderValueKind::Float:
        value_size = sizeof(float);
        break;
      case ShaderValueKind::Vec2:
        value_size = sizeof(glm::vec2);
        break;
      case ShaderValueKind::Vec3:
        value_size = sizeof(glm::vec3);
        break;
      case ShaderValueKind::Vec4:
        value_size = sizeof(glm::vec4);
        break;
      case ShaderValueKind::Mat3:
        value_size = sizeof(glm::mat3);
        break;
      case ShaderValueKind::Mat4:
        value_size = sizeof(glm::mat4);
        break;
    }

    upload.bytes.resize(value_size);
    std::memcpy(upload.bytes.data(), value, value_size);
    typed_uploads.push_back(std::move(upload));
  }
};

CompiledPass make_fullscreen_pass(const CompiledFrame &frame,
                                  RenderPipelineHandle pipeline,
                                  RenderBindingGroupHandle binding_group,
                                  BufferHandle geometry,
                                  ImageHandle backbuffer) {
  (void)frame;
  RenderingInfo info;
  info.debug_name = "fullscreen";
  info.extent = ImageExtent{.width = 64, .height = 32, .depth = 1};
  info.color_attachments.push_back(ColorAttachmentRef{
      .view = ImageViewRef{.image = backbuffer},
      .load_op = AttachmentLoadOp::Clear,
      .store_op = AttachmentStoreOp::Store,
      .clear_color = {0.1f, 0.2f, 0.3f, 1.0f},
  });

  return CompiledPass{
      .debug_name = "fullscreen",
      .commands =
          {
              BeginRenderingCmd{info},
              BindPipelineCmd{pipeline},
              BindBindingsCmd{binding_group},
              BindVertexBufferCmd{geometry},
              BindIndexBufferCmd{geometry},
              DrawIndexedCmd{DrawIndexedArgs{.index_count = 6}},
              EndRenderingCmd{},
          },
  };
}

TEST(OpenGLExecutorTest, ExecutesRecordedPassAndCachesStateAcrossPasses) {
  auto renderer_api = create_scope<FakeRendererAPI>();
  auto *api = renderer_api.get();
  auto helper_framebuffer = create_ref<FakeFramebuffer>(64, 32, std::vector<uint32_t>{77u});
  RenderTarget target(std::move(renderer_api), helper_framebuffer,
                      RenderTarget::MSAA{.samples = 1, .is_enabled = false},
                      "test-window");

  auto shader = create_ref<FakeShader>();

  auto vertex_array = create_ref<FakeVertexArray>();

  CompiledFrame frame;
  auto backbuffer =
      frame.register_default_color_target("backbuffer", ImageExtent{64, 32, 1});
  auto screen_image =
      frame.register_raw_texture_2d("scene-color", 11u, 64u, 32u);
  auto bloom_image = frame.register_raw_texture_2d("bloom", 22u, 64u, 32u);

  RenderPipelineDesc pipeline_desc;
  pipeline_desc.debug_name = "post-process";
  pipeline_desc.raster.cull_mode = CullMode::None;
  pipeline_desc.depth_stencil.depth_test = false;
  pipeline_desc.depth_stencil.depth_write = false;
  pipeline_desc.blend_attachments = {BlendAttachmentState::replace()};
  const auto pipeline = frame.register_pipeline(pipeline_desc, shader);

  const auto bindings = frame.register_binding_group(
      make_binding_group_desc(
          "post-process",
          "opengl-executor-test",
          shader,
          0,
          "post-process",
          RenderBindingScope::Pass,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );
  frame.add_sampled_image_binding(bindings, 101,
                                  ImageViewRef{.image = screen_image});
  frame.add_sampled_image_binding(bindings, 102,
                                  ImageViewRef{.image = bloom_image});
  constexpr float kBloomStrength = 0.12f;
  frame.add_value_binding(bindings, 103, ShaderValueKind::Float,
                          kBloomStrength);

  const auto geometry = frame.register_vertex_array("quad", vertex_array);
  frame.passes.push_back(
      make_fullscreen_pass(frame, pipeline, bindings, geometry, backbuffer));
  frame.passes.push_back(
      make_fullscreen_pass(frame, pipeline, bindings, geometry, backbuffer));

  OpenGLExecutor executor(target);
  executor.execute(frame);

  ASSERT_EQ(helper_framebuffer->bind_records.size(), 1u);
  EXPECT_EQ(helper_framebuffer->bind_records[0].type,
            FramebufferBindType::Draw);
  EXPECT_EQ(helper_framebuffer->bind_records[0].id, 0u);

  EXPECT_EQ(api->clear_color_calls, 2u);
  EXPECT_EQ(api->clear_buffer_calls, 2u);
  EXPECT_EQ(api->disable_depth_test_calls, 1u);
  EXPECT_EQ(api->disable_depth_write_calls, 1u);
  EXPECT_EQ(api->disable_blend_calls, 1u);
  EXPECT_EQ(api->disable_cull_calls, 1u);
  EXPECT_EQ(api->depth_mode_calls, 1u);
  EXPECT_EQ(api->draw_indexed_calls, 2u);
  EXPECT_EQ(api->texture_2d_binds.size(), 2u);
  ASSERT_EQ(api->texture_2d_binds[0].texture_id, 11u);
  EXPECT_EQ(api->texture_2d_binds[0].slot, 0u);
  ASSERT_EQ(api->texture_2d_binds[1].texture_id, 22u);
  EXPECT_EQ(api->texture_2d_binds[1].slot, 1u);

  EXPECT_EQ(shader->bind_calls, 1u);
  ASSERT_EQ(shader->typed_uploads.size(), 6u);
  EXPECT_EQ(shader->typed_uploads[0].binding_id, 103u);
  EXPECT_EQ(shader->typed_uploads[1].binding_id, 101u);
  EXPECT_EQ(shader->typed_uploads[2].binding_id, 102u);
}

TEST(OpenGLExecutorTest, AppliesDepthBiasStateOncePerPipelineConfiguration) {
  auto renderer_api = create_scope<FakeRendererAPI>();
  auto *api = renderer_api.get();
  auto helper_framebuffer =
      create_ref<FakeFramebuffer>(64, 32, std::vector<uint32_t>{77u});
  RenderTarget target(std::move(renderer_api), helper_framebuffer,
                      RenderTarget::MSAA{.samples = 1, .is_enabled = false},
                      "test-window");

  auto shader = create_ref<FakeShader>();
  auto vertex_array = create_ref<FakeVertexArray>();

  CompiledFrame frame;
  const auto backbuffer = frame.register_default_color_target(
      "backbuffer", ImageExtent{.width = 64, .height = 32, .depth = 1}
  );

  RenderPipelineDesc pipeline_desc;
  pipeline_desc.debug_name = "shadow-like-pass";
  pipeline_desc.raster.cull_mode = CullMode::None;
  pipeline_desc.raster.depth_bias.enabled = true;
  pipeline_desc.raster.depth_bias.slope_factor = 1.75f;
  pipeline_desc.raster.depth_bias.constant_factor = 1.0f;
  pipeline_desc.depth_stencil.depth_test = true;
  pipeline_desc.depth_stencil.depth_write = true;
  pipeline_desc.blend_attachments = {BlendAttachmentState::replace()};
  const auto pipeline = frame.register_pipeline(pipeline_desc, shader);

  const auto bindings = frame.register_binding_group(
      make_binding_group_desc(
          "depth-bias-pass",
          "opengl-executor-test",
          shader,
          0,
          "depth-bias-pass",
          RenderBindingScope::Pass,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );

  const auto geometry = frame.register_vertex_array("quad", vertex_array);
  frame.passes.push_back(
      make_fullscreen_pass(frame, pipeline, bindings, geometry, backbuffer)
  );
  frame.passes.push_back(
      make_fullscreen_pass(frame, pipeline, bindings, geometry, backbuffer)
  );

  OpenGLExecutor executor(target);
  executor.execute(frame);

  EXPECT_EQ(api->enable_depth_bias_calls, 1u);
  EXPECT_EQ(api->disable_depth_bias_calls, 0u);
  EXPECT_EQ(api->set_depth_bias_calls, 1u);
  EXPECT_FLOAT_EQ(api->last_depth_bias_slope, 1.75f);
  EXPECT_FLOAT_EQ(api->last_depth_bias_constant, 1.0f);
}

TEST(OpenGLExecutorTest, ConvertsTopLeftScissorCoordsToOpenGLCoords) {
  auto renderer_api = create_scope<FakeRendererAPI>();
  auto *api = renderer_api.get();
  auto helper_framebuffer =
      create_ref<FakeFramebuffer>(64, 32, std::vector<uint32_t>{77u});
  RenderTarget target(std::move(renderer_api), helper_framebuffer,
                      RenderTarget::MSAA{.samples = 1, .is_enabled = false},
                      "test-window");

  CompiledFrame frame;
  const auto backbuffer = frame.register_default_color_target(
      "backbuffer", ImageExtent{.width = 64, .height = 32, .depth = 1}
  );

  RenderingInfo info;
  info.debug_name = "scissor-test";
  info.extent = ImageExtent{.width = 64, .height = 32, .depth = 1};
  info.color_attachments.push_back(ColorAttachmentRef{
      .view = ImageViewRef{.image = backbuffer},
      .load_op = AttachmentLoadOp::Load,
      .store_op = AttachmentStoreOp::Store,
  });

  frame.passes.push_back(CompiledPass{
      .debug_name = "scissor-pass",
      .commands =
          {
              BeginRenderingCmd{info},
              SetScissorCmd{
                  .enabled = true,
                  .x = 5,
                  .y = 7,
                  .width = 11,
                  .height = 13,
              },
              EndRenderingCmd{},
          },
  });

  OpenGLExecutor executor(target);
  executor.execute(frame);

  EXPECT_EQ(api->enable_scissor_calls, 1u);
  EXPECT_EQ(api->scissor, glm::uvec4(5u, 12u, 11u, 13u));
}

TEST(OpenGLExecutorTest, SamplesTextureResourceImages) {
  auto renderer_api = create_scope<FakeRendererAPI>();
  auto *api = renderer_api.get();
  auto helper_framebuffer =
      create_ref<FakeFramebuffer>(64, 32, std::vector<uint32_t>{77u});
  RenderTarget target(std::move(renderer_api), helper_framebuffer,
                      RenderTarget::MSAA{.samples = 1, .is_enabled = false},
                      "test-window");

  auto shader = create_ref<FakeShader>();

  auto texture_2d = create_ref<FakeTexture>(31u, 64u, 32u);
  auto texture_cube = create_ref<FakeTexture>(47u, 16u, 16u);
  auto vertex_array = create_ref<FakeVertexArray>();

  CompiledFrame frame;
  const auto backbuffer = frame.register_default_color_target(
      "backbuffer", ImageExtent{.width = 64, .height = 32, .depth = 1}
  );
  const auto albedo = frame.register_texture_2d("albedo", texture_2d);
  const auto environment =
      frame.register_texture_cube("environment", texture_cube);

  RenderPipelineDesc pipeline_desc;
  pipeline_desc.debug_name = "texture-sampling";
  pipeline_desc.raster.cull_mode = CullMode::None;
  pipeline_desc.depth_stencil.depth_test = false;
  pipeline_desc.depth_stencil.depth_write = false;
  pipeline_desc.blend_attachments = {BlendAttachmentState::replace()};
  const auto pipeline = frame.register_pipeline(pipeline_desc, shader);

  const auto bindings = frame.register_binding_group(
      make_binding_group_desc(
          "texture-bindings",
          "opengl-executor-test",
          shader,
          0,
          "texture-bindings",
          RenderBindingScope::Pass,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );
  frame.add_sampled_image_binding(bindings, 201u,
                                  ImageViewRef{.image = albedo});
  frame.add_sampled_image_binding(
      bindings, 202u, ImageViewRef{.image = environment},
      CompiledSampledImageTarget::TextureCube
  );

  const auto geometry = frame.register_vertex_array("quad", vertex_array);
  frame.passes.push_back(
      make_fullscreen_pass(frame, pipeline, bindings, geometry, backbuffer)
  );

  OpenGLExecutor executor(target);
  executor.execute(frame);

  ASSERT_EQ(api->texture_2d_binds.size(), 1u);
  EXPECT_EQ(api->texture_2d_binds[0].texture_id, 31u);
  EXPECT_EQ(api->texture_2d_binds[0].slot, 0u);
  ASSERT_EQ(api->texture_cube_binds.size(), 1u);
  EXPECT_EQ(api->texture_cube_binds[0].texture_id, 47u);
  EXPECT_EQ(api->texture_cube_binds[0].slot, 1u);

  ASSERT_EQ(shader->typed_uploads.size(), 2u);
  EXPECT_EQ(shader->typed_uploads[0].binding_id, 201u);
  EXPECT_EQ(shader->typed_uploads[1].binding_id, 202u);

  int albedo_slot = -1;
  int environment_slot = -1;
  std::memcpy(&albedo_slot, shader->typed_uploads[0].bytes.data(),
              sizeof(int));
  std::memcpy(&environment_slot, shader->typed_uploads[1].bytes.data(),
              sizeof(int));
  EXPECT_EQ(albedo_slot, 0);
  EXPECT_EQ(environment_slot, 1);
}

TEST(OpenGLExecutorTest, KeepsSamplerUnitsStableAcrossBindingGroups) {
  auto renderer_api = create_scope<FakeRendererAPI>();
  auto *api = renderer_api.get();
  auto helper_framebuffer =
      create_ref<FakeFramebuffer>(64, 32, std::vector<uint32_t>{77u});
  RenderTarget target(std::move(renderer_api), helper_framebuffer,
                      RenderTarget::MSAA{.samples = 1, .is_enabled = false},
                      "test-window");

  auto shader = create_ref<FakeShader>();
  auto first_texture = create_ref<FakeTexture>(61u, 64u, 32u);
  auto second_texture = create_ref<FakeTexture>(73u, 64u, 32u);
  auto vertex_array = create_ref<FakeVertexArray>();

  CompiledFrame frame;
  const auto backbuffer = frame.register_default_color_target(
      "backbuffer", ImageExtent{.width = 64, .height = 32, .depth = 1}
  );
  const auto first_image = frame.register_texture_2d("first", first_texture);
  const auto second_image = frame.register_texture_2d("second", second_texture);

  RenderPipelineDesc pipeline_desc;
  pipeline_desc.debug_name = "multi-set-textures";
  pipeline_desc.raster.cull_mode = CullMode::None;
  pipeline_desc.depth_stencil.depth_test = false;
  pipeline_desc.depth_stencil.depth_write = false;
  pipeline_desc.blend_attachments = {BlendAttachmentState::replace()};
  const auto pipeline = frame.register_pipeline(pipeline_desc, shader);

  const auto scene_bindings = frame.register_binding_group(
      make_binding_group_desc(
          "scene-bindings",
          "opengl-executor-test",
          shader,
          0,
          "scene-bindings",
          RenderBindingScope::Pass,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );
  frame.add_sampled_image_binding(scene_bindings, 301u,
                                  ImageViewRef{.image = first_image});

  const auto material_bindings = frame.register_binding_group(
      make_binding_group_desc(
          "material-bindings",
          "opengl-executor-test",
          shader,
          1,
          "material-bindings",
          RenderBindingScope::Draw,
          RenderBindingCachePolicy::Reuse,
          RenderBindingSharing::LocalOnly,
          0,
          RenderBindingStability::FrameLocal
      )
  );
  frame.add_sampled_image_binding(material_bindings, 302u,
                                  ImageViewRef{.image = second_image});

  const auto geometry = frame.register_vertex_array("quad", vertex_array);

  RenderingInfo info;
  info.debug_name = "multi-set-textures";
  info.extent = ImageExtent{.width = 64, .height = 32, .depth = 1};
  info.color_attachments.push_back(ColorAttachmentRef{
      .view = ImageViewRef{.image = backbuffer},
      .load_op = AttachmentLoadOp::Clear,
      .store_op = AttachmentStoreOp::Store,
      .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
  });

  frame.passes.push_back(CompiledPass{
      .debug_name = "multi-set-textures",
      .commands =
          {
              BeginRenderingCmd{info},
              BindPipelineCmd{pipeline},
              BindBindingsCmd{scene_bindings},
              BindBindingsCmd{material_bindings},
              BindVertexBufferCmd{geometry},
              BindIndexBufferCmd{geometry},
              DrawIndexedCmd{DrawIndexedArgs{.index_count = 6}},
              EndRenderingCmd{},
          },
  });

  OpenGLExecutor executor(target);
  executor.execute(frame);

  ASSERT_EQ(api->texture_2d_binds.size(), 2u);
  EXPECT_EQ(api->texture_2d_binds[0].texture_id, 61u);
  EXPECT_EQ(api->texture_2d_binds[0].slot, 0u);
  EXPECT_EQ(api->texture_2d_binds[1].texture_id, 73u);
  EXPECT_EQ(api->texture_2d_binds[1].slot, 1u);

  ASSERT_EQ(shader->typed_uploads.size(), 2u);
  EXPECT_EQ(shader->typed_uploads[0].binding_id, 301u);
  EXPECT_EQ(shader->typed_uploads[1].binding_id, 302u);

  int first_slot = -1;
  int second_slot = -1;
  std::memcpy(&first_slot, shader->typed_uploads[0].bytes.data(), sizeof(int));
  std::memcpy(&second_slot, shader->typed_uploads[1].bytes.data(),
              sizeof(int));
  EXPECT_EQ(first_slot, 0);
  EXPECT_EQ(second_slot, 1);
}

} // namespace

} // namespace astralix
