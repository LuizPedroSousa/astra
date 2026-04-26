#include "systems/render-system/passes/grid-pass.hpp"

#include "resources/shader.hpp"
#include "systems/render-system/passes/render-graph-pass.hpp"
#include "vertex-array.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <variant>

namespace astralix {

namespace {

class FakeVertexArray : public VertexArray {
public:
  void bind() const override {}
  void unbind() const override {}

  void add_vertex_buffer(const Ref<VertexBuffer> &vertex_buffer) override {
    vertex_buffers.push_back(vertex_buffer);
  }

  void set_index_buffer(const Ref<IndexBuffer> &index_buffer) override {
    this->index_buffer = index_buffer;
  }

  const std::vector<Ref<VertexBuffer>> &get_vertex_buffers() const override {
    return vertex_buffers;
  }

  const Ref<IndexBuffer> &get_index_buffer() const override {
    return index_buffer;
  }

  std::vector<Ref<VertexBuffer>> vertex_buffers;
  Ref<IndexBuffer> index_buffer = nullptr;
};

class FakeFramebuffer : public Framebuffer {
public:
  FakeFramebuffer(uint32_t width, uint32_t height) {
    m_spec.width = width;
    m_spec.height = height;
    m_spec.attachments = {
        FramebufferTextureFormat::RGBA32F,
        FramebufferTextureFormat::Depth,
    };
  }

  void bind(FramebufferBindType = FramebufferBindType::Default,
            uint32_t = static_cast<uint32_t>(-1)) override {}
  void unbind() override {}
  void resize(uint32_t width, uint32_t height) override {
    m_spec.width = width;
    m_spec.height = height;
  }
  int read_pixel(uint32_t, int, int) override { return 0; }
  void clear_attachment(uint32_t, int) override {}
  uint32_t get_color_attachment_id(uint32_t index = 0) const override {
    return m_color_attachments.at(index);
  }
  const std::vector<uint32_t> &get_color_attachments() const override {
    return m_color_attachments;
  }
  uint32_t get_depth_attachment_id() const override { return m_depth_attachment; }
  void blit(uint32_t, uint32_t,
            FramebufferBlitType = FramebufferBlitType::Color,
            FramebufferBlitFilter = FramebufferBlitFilter::Nearest) override {}
  const FramebufferSpecification &get_specification() const override {
    return m_spec;
  }

private:
  FramebufferSpecification m_spec;
  std::vector<uint32_t> m_color_attachments = {41};
  uint32_t m_depth_attachment = 91;
};

class FakeGridShader : public Shader {
public:
  FakeGridShader() : Shader(ResourceHandle{0, 1}, "shaders::grid") {}

  void bind() const override {}
  void unbind() const override {}
  void attach() const override {}
  uint32_t renderer_id() const override { return 1; }

protected:
  void set_typed_value(uint64_t, ShaderValueKind, const void *) const override {}
};

ResolvedRenderPassDependency make_shader_dependency(
    std::string slot, ResourceDescriptorID descriptor_id, Ref<Shader> shader
) {
  return ResolvedRenderPassDependency{
      .declaration =
          RenderPassDependencyDeclaration{
              .type = RenderPassDependencyType::Shader,
              .slot = std::move(slot),
              .descriptor_id = std::move(descriptor_id),
          },
      .resource = std::move(shader),
  };
}

RenderGraphResource make_graph_image_resource(
    const std::string &name, uint32_t width, uint32_t height,
    ImageFormat format, ImageUsage usage
) {
  auto image = std::make_shared<RenderGraphImageResource>();
  image->desc.debug_name = name;
  image->desc.width = width;
  image->desc.height = height;
  image->desc.depth = 1;
  image->desc.format = format;
  image->desc.usage = usage;

  RenderGraphResourceDescriptor desc;
  desc.type = RenderGraphResourceType::Image;
  desc.name = name;
  desc.lifetime = RenderGraphResourceLifetime::Persistent;
  desc.spec = image->desc;
  desc.external_resource = image;
  return RenderGraphResource(desc);
}

rendering::ResolvedMeshDraw make_grid_quad(const Ref<VertexArray> &vao,
                                           uint32_t index_count) {
  rendering::ResolvedMeshDraw draw;
  draw.vertex_array = vao;
  draw.index_count = index_count;
  return draw;
}

TEST(GridPassTest, RecordsSurfaceAndAxisOverlayDraws) {
  auto shader = create_ref<FakeGridShader>();
  auto quad = create_ref<FakeVertexArray>();
  auto scene_color = create_ref<FakeFramebuffer>(800, 600);

  auto scene_resource = make_graph_image_resource(
      "scene_color", 800, 600, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto depth_resource = make_graph_image_resource(
      "scene_depth", 800, 600, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment
  );
  std::vector<const RenderGraphResource *> resources{&scene_resource, &depth_resource};

  auto pass = create_scope<GridPass>(make_grid_quad(quad, 6));
  RenderGraphPass graph_pass(std::move(pass));
  graph_pass.set_resolved_dependencies(
      {make_shader_dependency("grid_shader", "shaders::grid", shader)}
  );
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  scene_frame.main_camera = rendering::CameraFrame{
      .view = glm::mat4(2.0f),
      .projection = glm::mat4(3.0f),
  };

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  ASSERT_EQ(frame.passes.size(), 1u);
  ASSERT_EQ(frame.images.size(), 2u);
  ASSERT_EQ(frame.pipelines.size(), 2u);
  ASSERT_EQ(frame.binding_groups.size(), 2u);
  ASSERT_EQ(frame.buffers.size(), 1u);

  const auto &surface_pipeline = frame.pipelines[0].desc;
  EXPECT_TRUE(surface_pipeline.depth_stencil.depth_test);
  EXPECT_FALSE(surface_pipeline.depth_stencil.depth_write);
  EXPECT_EQ(surface_pipeline.depth_stencil.compare_op, CompareOp::LessEqual);
  ASSERT_EQ(surface_pipeline.blend_attachments.size(), 1u);
  EXPECT_TRUE(surface_pipeline.blend_attachments[0].enabled);

  const auto &axis_pipeline = frame.pipelines[1].desc;
  EXPECT_FALSE(axis_pipeline.depth_stencil.depth_test);
  EXPECT_FALSE(axis_pipeline.depth_stencil.depth_write);

  const auto &commands = frame.passes[0].commands;
  ASSERT_EQ(commands.size(), 10u);
  ASSERT_TRUE(std::holds_alternative<BeginRenderingCmd>(commands[0]));
  ASSERT_TRUE(std::holds_alternative<BindVertexBufferCmd>(commands[1]));
  ASSERT_TRUE(std::holds_alternative<BindIndexBufferCmd>(commands[2]));
  ASSERT_TRUE(std::holds_alternative<BindPipelineCmd>(commands[3]));
  ASSERT_TRUE(std::holds_alternative<BindBindingsCmd>(commands[4]));
  ASSERT_TRUE(std::holds_alternative<DrawIndexedCmd>(commands[5]));
  ASSERT_TRUE(std::holds_alternative<BindPipelineCmd>(commands[6]));
  ASSERT_TRUE(std::holds_alternative<BindBindingsCmd>(commands[7]));
  ASSERT_TRUE(std::holds_alternative<DrawIndexedCmd>(commands[8]));
  ASSERT_TRUE(std::holds_alternative<EndRenderingCmd>(commands[9]));

  const auto &binding_group = frame.binding_groups[0];
  ASSERT_EQ(binding_group.values.size(), 5u);
  EXPECT_EQ(
      binding_group.values[0].binding_id,
      shader_bindings::engine_shaders_grid_axsl::GridUniform::view.binding_id
  );
  EXPECT_EQ(binding_group.values[0].kind, ShaderValueKind::Mat4);
  EXPECT_EQ(
      binding_group.values[1].binding_id,
      shader_bindings::engine_shaders_grid_axsl::GridUniform::projection
          .binding_id
  );
  EXPECT_EQ(binding_group.values[1].kind, ShaderValueKind::Mat4);
  EXPECT_EQ(
      binding_group.values[2].binding_id,
      shader_bindings::engine_shaders_grid_axsl::GridUniform::inverse_view
          .binding_id
  );
  EXPECT_EQ(binding_group.values[2].kind, ShaderValueKind::Mat4);
  EXPECT_EQ(
      binding_group.values[3].binding_id,
      shader_bindings::engine_shaders_grid_axsl::GridUniform::inverse_projection
          .binding_id
  );
  EXPECT_EQ(binding_group.values[3].kind, ShaderValueKind::Mat4);
  EXPECT_EQ(
      binding_group.values[4].binding_id,
      shader_bindings::engine_shaders_grid_axsl::GridUniform::render_mode
          .binding_id
  );
  EXPECT_EQ(binding_group.values[4].kind, ShaderValueKind::Int);

  const auto &axis_binding_group = frame.binding_groups[1];
  ASSERT_EQ(axis_binding_group.values.size(), 5u);
  int render_mode = -1;
  std::memcpy(
      &render_mode, axis_binding_group.values[4].bytes.data(), sizeof(int)
  );
  EXPECT_EQ(render_mode, 1);
}

TEST(GridPassTest, SkipsRecordingWithoutCamera) {
  auto shader = create_ref<FakeGridShader>();
  auto quad = create_ref<FakeVertexArray>();
  auto scene_color = create_ref<FakeFramebuffer>(800, 600);

  auto scene_resource = make_graph_image_resource(
      "scene_color", 800, 600, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto depth_resource = make_graph_image_resource(
      "scene_depth", 800, 600, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment
  );
  std::vector<const RenderGraphResource *> resources{&scene_resource, &depth_resource};

  auto pass = create_scope<GridPass>(make_grid_quad(quad, 6));
  RenderGraphPass graph_pass(std::move(pass));
  graph_pass.set_resolved_dependencies(
      {make_shader_dependency("grid_shader", "shaders::grid", shader)}
  );
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  EXPECT_TRUE(frame.passes.empty());
}

} // namespace

} // namespace astralix
