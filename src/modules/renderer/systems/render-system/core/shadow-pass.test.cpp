#include "systems/render-system/passes/shadow-pass.hpp"

#include "resources/shader.hpp"
#include "systems/render-system/passes/render-graph-pass.hpp"
#include "vertex-array.hpp"
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
    m_spec.attachments = {FramebufferTextureFormat::DEPTH_ONLY};
  }

  void bind(FramebufferBindType = FramebufferBindType::Default, uint32_t = static_cast<uint32_t>(-1)) override {}
  void unbind() override {}
  void resize(uint32_t width, uint32_t height) override {
    m_spec.width = width;
    m_spec.height = height;
  }
  int read_pixel(uint32_t, int, int) override { return 0; }
  void clear_attachment(uint32_t, int) override {}
  uint32_t get_color_attachment_id(uint32_t = 0) const override { return 0; }
  const std::vector<uint32_t> &get_color_attachments() const override {
    return m_color_attachments;
  }
  uint32_t get_depth_attachment_id() const override { return 91; }
  void blit(uint32_t, uint32_t, FramebufferBlitType = FramebufferBlitType::Color, FramebufferBlitFilter = FramebufferBlitFilter::Nearest) override {}
  const FramebufferSpecification &get_specification() const override {
    return m_spec;
  }

private:
  FramebufferSpecification m_spec;
  std::vector<uint32_t> m_color_attachments;
};

class FakeShadowShader : public Shader {
public:
  FakeShadowShader() : Shader(ResourceHandle{0, 1}, "shaders::shadow_map") {}

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

TEST(ShadowPassTest, RecordsDepthOnlyDrawsFromSceneFrame) {
  auto shader = create_ref<FakeShadowShader>();
  auto vertex_array = create_ref<FakeVertexArray>();

  auto image = std::make_shared<RenderGraphImageResource>();
  image->desc.debug_name = "shadow_map";
  image->desc.width = 1024;
  image->desc.height = 512;
  image->desc.depth = 1;
  image->desc.format = ImageFormat::Depth32F;
  image->desc.usage =
      ImageUsage::DepthStencilAttachment | ImageUsage::Sampled;

  RenderGraphResourceDescriptor shadow_desc;
  shadow_desc.type = RenderGraphResourceType::Image;
  shadow_desc.name = "shadow_map";
  shadow_desc.lifetime = RenderGraphResourceLifetime::Transient;
  shadow_desc.spec = image->desc;
  shadow_desc.external_resource = image;

  RenderGraphResource shadow_resource(shadow_desc);
  std::vector<const RenderGraphResource *> resources{&shadow_resource};

  auto shadow_pass = create_scope<ShadowPass>();
  RenderGraphPass graph_pass(std::move(shadow_pass));
  graph_pass.set_resolved_dependencies(
      {make_shader_dependency("shadow_shader", "shaders::shadow_map", shader)}
  );
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  scene_frame.light_frame.directional.valid = true;
  scene_frame.light_frame.directional.light_space_matrix =
      glm::mat4(3.0f);
  scene_frame.shadow_draws.push_back(rendering::ShadowDrawItem{
      .entity_id = EntityID{1},
      .model = glm::mat4(5.0f),
      .mesh = rendering::ResolvedMeshDraw{
          .vertex_array = vertex_array,
          .index_count = 42,
      },
  });

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  ASSERT_EQ(frame.passes.size(), 1u);
  ASSERT_EQ(frame.images.size(), 1u);
  ASSERT_EQ(frame.pipelines.size(), 1u);
  ASSERT_EQ(frame.binding_groups.size(), 2u);
  ASSERT_EQ(frame.buffers.size(), 1u);

  const auto &pipeline = frame.pipelines[0];
  EXPECT_EQ(pipeline.debug_name, "shadow-pass");
  EXPECT_EQ(pipeline.desc.depth_format, ImageFormat::Depth32F);
  EXPECT_EQ(pipeline.desc.raster.cull_mode, CullMode::None);
  EXPECT_TRUE(pipeline.desc.raster.depth_bias.enabled);
  EXPECT_FLOAT_EQ(pipeline.desc.raster.depth_bias.constant_factor, 1.0f);
  EXPECT_FLOAT_EQ(pipeline.desc.raster.depth_bias.slope_factor, 1.75f);
  EXPECT_TRUE(pipeline.desc.depth_stencil.depth_test);
  EXPECT_TRUE(pipeline.desc.depth_stencil.depth_write);

  const auto &scene_group = frame.binding_groups[0];
  ASSERT_EQ(scene_group.values.size(), 1u);
  EXPECT_EQ(
      scene_group.values[0].binding_id,
      shader_bindings::engine_shaders_shadow_map_axsl::ShadowPassUniform::
          light_space_matrix.binding_id
  );
  EXPECT_EQ(scene_group.values[0].kind, ShaderValueKind::Mat4);

  const auto &binding_group = frame.binding_groups[1];
  ASSERT_EQ(binding_group.values.size(), 1u);
  EXPECT_EQ(
      binding_group.values[0].binding_id,
      shader_bindings::engine_shaders_shadow_map_axsl::ShadowDrawUniform::g_model
          .binding_id
  );
  EXPECT_EQ(binding_group.values[0].kind, ShaderValueKind::Mat4);

  const auto &commands = frame.passes[0].commands;
  ASSERT_EQ(commands.size(), 8u);
  ASSERT_TRUE(std::holds_alternative<BeginRenderingCmd>(commands[0]));
  ASSERT_TRUE(std::holds_alternative<BindPipelineCmd>(commands[1]));
  ASSERT_TRUE(std::holds_alternative<BindBindingsCmd>(commands[2]));
  ASSERT_TRUE(std::holds_alternative<BindBindingsCmd>(commands[3]));
  ASSERT_TRUE(std::holds_alternative<BindVertexBufferCmd>(commands[4]));
  ASSERT_TRUE(std::holds_alternative<BindIndexBufferCmd>(commands[5]));
  ASSERT_TRUE(std::holds_alternative<DrawIndexedCmd>(commands[6]));
  ASSERT_TRUE(std::holds_alternative<EndRenderingCmd>(commands[7]));

  const auto &begin = std::get<BeginRenderingCmd>(commands[0]);
  EXPECT_TRUE(begin.info.color_attachments.empty());
  ASSERT_TRUE(begin.info.depth_stencil_attachment.has_value());
  EXPECT_EQ(begin.info.extent.width, 1024u);
  EXPECT_EQ(begin.info.extent.height, 512u);

  const auto &draw = std::get<DrawIndexedCmd>(commands[6]);
  EXPECT_EQ(draw.args.index_count, 42u);
}

TEST(ShadowPassTest, SkipsRecordingWithoutDirectionalLight) {
  auto shader = create_ref<FakeShadowShader>();
  auto vertex_array = create_ref<FakeVertexArray>();

  auto image = std::make_shared<RenderGraphImageResource>();
  image->desc.debug_name = "shadow_map";
  image->desc.width = 256;
  image->desc.height = 256;
  image->desc.depth = 1;
  image->desc.format = ImageFormat::Depth32F;
  image->desc.usage =
      ImageUsage::DepthStencilAttachment | ImageUsage::Sampled;

  RenderGraphResourceDescriptor shadow_desc;
  shadow_desc.type = RenderGraphResourceType::Image;
  shadow_desc.name = "shadow_map";
  shadow_desc.lifetime = RenderGraphResourceLifetime::Transient;
  shadow_desc.spec = image->desc;
  shadow_desc.external_resource = image;

  RenderGraphResource shadow_resource(shadow_desc);
  std::vector<const RenderGraphResource *> resources{&shadow_resource};

  auto shadow_pass = create_scope<ShadowPass>();
  RenderGraphPass graph_pass(std::move(shadow_pass));
  graph_pass.set_resolved_dependencies(
      {make_shader_dependency("shadow_shader", "shaders::shadow_map", shader)}
  );
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  scene_frame.shadow_draws.push_back(rendering::ShadowDrawItem{
      .entity_id = EntityID{1},
      .mesh = rendering::ResolvedMeshDraw{
          .vertex_array = vertex_array,
          .index_count = 8,
      },
  });

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  EXPECT_TRUE(frame.passes.empty());
  EXPECT_TRUE(frame.images.empty());
}

} // namespace

} // namespace astralix
