#include "systems/render-system/passes/skybox-pass.hpp"

#include "resources/shader.hpp"
#include "resources/texture.hpp"
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
  FakeFramebuffer(uint32_t width, uint32_t height)
      : m_color_attachments{41u} {
    m_spec.width = width;
    m_spec.height = height;
    m_spec.attachments = {
        FramebufferTextureFormat::RGBA32F,
        FramebufferTextureFormat::Depth,
    };
  }

  void bind(FramebufferBindType = FramebufferBindType::Default, uint32_t = static_cast<uint32_t>(-1)) override {}
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
  uint32_t get_depth_attachment_id() const override { return 91u; }
  void blit(uint32_t, uint32_t, FramebufferBlitType = FramebufferBlitType::Color, FramebufferBlitFilter = FramebufferBlitFilter::Nearest) override {}
  const FramebufferSpecification &get_specification() const override {
    return m_spec;
  }

private:
  FramebufferSpecification m_spec;
  std::vector<uint32_t> m_color_attachments;
};

class FakeCubemap : public Texture3D {
public:
  explicit FakeCubemap(uint32_t renderer_id, uint32_t width = 256, uint32_t height = 256)
      : Texture3D(ResourceHandle{0, renderer_id}),
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

class FakeSkyboxShader : public Shader {
public:
  FakeSkyboxShader() : Shader(ResourceHandle{0, 1}, "shaders::skybox") {}

  void bind() const override {}
  void unbind() const override {}
  void attach() const override {}
  uint32_t renderer_id() const override { return 1u; }

protected:
  void set_typed_value(uint64_t, ShaderValueKind, const void *) const override {}
};

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

rendering::ResolvedMeshDraw make_skybox_draw(const Ref<VertexArray> &vao, uint32_t index_count) {
  rendering::ResolvedMeshDraw draw;
  draw.vertex_array = vao;
  draw.index_count = index_count;
  return draw;
}

TEST(SkyboxPassTest, RecordsCubemapDrawFromSceneFrame) {
  auto scene_color = create_ref<FakeFramebuffer>(1280, 720);
  auto shader = create_ref<FakeSkyboxShader>();
  auto cubemap = create_ref<FakeCubemap>(81u);
  auto vertex_array = create_ref<FakeVertexArray>();

  auto scene_resource = make_graph_image_resource(
      "scene_color", 1280, 720, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto depth_resource = make_graph_image_resource(
      "scene_depth", 1280, 720, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment
  );
  std::vector<const RenderGraphResource *> resources{&scene_resource, &depth_resource};

  auto pass = create_scope<SkyboxPass>(make_skybox_draw(vertex_array, 36u));
  RenderGraphPass graph_pass(std::move(pass));
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  scene_frame.main_camera = rendering::CameraFrame{
      .view = glm::mat4(2.0f),
      .projection = glm::mat4(3.0f),
  };
  scene_frame.skybox = rendering::SkyboxFrame{
      .shader = shader,
      .cubemap = cubemap,
  };

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  ASSERT_EQ(frame.passes.size(), 1u);
  ASSERT_EQ(frame.images.size(), 3u);
  ASSERT_EQ(frame.pipelines.size(), 1u);
  ASSERT_EQ(frame.binding_groups.size(), 1u);
  ASSERT_EQ(frame.buffers.size(), 1u);

  const auto &pipeline = frame.pipelines[0].desc;
  EXPECT_EQ(pipeline.raster.cull_mode, CullMode::None);
  EXPECT_TRUE(pipeline.depth_stencil.depth_test);
  EXPECT_FALSE(pipeline.depth_stencil.depth_write);
  EXPECT_EQ(pipeline.depth_stencil.compare_op, CompareOp::LessEqual);

  const auto &binding_group = frame.binding_groups[0];
  ASSERT_EQ(binding_group.sampled_images.size(), 1u);
  EXPECT_EQ(
      binding_group.sampled_images[0].binding_id,
      shader_bindings::engine_shaders_skybox_axsl::EntityResources::skybox_map
          .binding_id
  );
  EXPECT_EQ(binding_group.sampled_images[0].target, CompiledSampledImageTarget::TextureCube);
  ASSERT_EQ(binding_group.values.size(), 2u);
  EXPECT_EQ(binding_group.values[0].kind, ShaderValueKind::Mat4);
  EXPECT_EQ(binding_group.values[1].kind, ShaderValueKind::Mat4);

  const auto &commands = frame.passes[0].commands;
  ASSERT_EQ(commands.size(), 7u);
  ASSERT_TRUE(std::holds_alternative<BeginRenderingCmd>(commands[0]));
  ASSERT_TRUE(std::holds_alternative<BindPipelineCmd>(commands[1]));
  ASSERT_TRUE(std::holds_alternative<BindBindingsCmd>(commands[2]));
  ASSERT_TRUE(std::holds_alternative<BindVertexBufferCmd>(commands[3]));
  ASSERT_TRUE(std::holds_alternative<BindIndexBufferCmd>(commands[4]));
  ASSERT_TRUE(std::holds_alternative<DrawIndexedCmd>(commands[5]));
  ASSERT_TRUE(std::holds_alternative<EndRenderingCmd>(commands[6]));

  const auto &draw = std::get<DrawIndexedCmd>(commands[5]);
  EXPECT_EQ(draw.args.index_count, 36u);
}

TEST(SkyboxPassTest, SkipsRecordingWithoutSkyboxData) {
  auto scene_color = create_ref<FakeFramebuffer>(800, 600);
  auto vertex_array = create_ref<FakeVertexArray>();

  auto scene_resource = make_graph_image_resource(
      "scene_color", 800, 600, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto depth_resource = make_graph_image_resource(
      "scene_depth", 800, 600, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment
  );
  std::vector<const RenderGraphResource *> resources{&scene_resource, &depth_resource};

  auto pass = create_scope<SkyboxPass>(make_skybox_draw(vertex_array, 36u));
  RenderGraphPass graph_pass(std::move(pass));
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  scene_frame.main_camera = rendering::CameraFrame{};

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  EXPECT_TRUE(frame.passes.empty());
}

} // namespace

} // namespace astralix
