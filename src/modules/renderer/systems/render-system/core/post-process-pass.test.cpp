#include "systems/render-system/passes/post-process-pass.hpp"

#include "resources/shader.hpp"
#include "systems/render-system/passes/render-graph-pass.hpp"
#include "vertex-array.hpp"
#include <gtest/gtest.h>
#include <cmath>
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
  FakeFramebuffer(uint32_t width, uint32_t height, uint32_t color_attachment)
      : m_color_attachments{color_attachment} {
    m_spec.width = width;
    m_spec.height = height;
    m_spec.attachments = {FramebufferTextureFormat::RGBA32F};
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
  uint32_t get_depth_attachment_id() const override { return 0; }
  void blit(uint32_t, uint32_t,
            FramebufferBlitType = FramebufferBlitType::Color,
            FramebufferBlitFilter = FramebufferBlitFilter::Nearest) override {}
  const FramebufferSpecification &get_specification() const override {
    return m_spec;
  }

private:
  FramebufferSpecification m_spec;
  std::vector<uint32_t> m_color_attachments;
};

class FakePostProcessShader : public Shader {
public:
  FakePostProcessShader() : Shader(ResourceHandle{0, 1}, "shaders::hdr") {}

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
  desc.lifetime = RenderGraphResourceLifetime::Transient;
  desc.spec = image->desc;
  desc.external_resource = image;
  return RenderGraphResource(desc);
}

rendering::ResolvedMeshDraw make_fullscreen_quad_draw(const Ref<VertexArray> &vao,
                                                      uint32_t index_count) {
  rendering::ResolvedMeshDraw draw;
  draw.vertex_array = vao;
  draw.index_count = index_count;
  return draw;
}

TEST(PostProcessPassTest, RecordsFullscreenCompositeToPresent) {
  auto shader = create_ref<FakePostProcessShader>();
  auto quad = create_ref<FakeVertexArray>();
  auto scene_color = create_ref<FakeFramebuffer>(1280, 720, 21);
  auto bloom = create_ref<FakeFramebuffer>(1280, 720, 22);

  auto present = create_ref<FakeFramebuffer>(1280, 720, 23);

  auto scene_resource = make_graph_image_resource(
      "scene_color", 1280, 720, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto bloom_resource = make_graph_image_resource(
      "bloom", 1280, 720, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto present_resource = make_graph_image_resource(
      "present", 1280, 720, ImageFormat::RGBA8,
      ImageUsage::ColorAttachment | ImageUsage::TransferSrc |
          ImageUsage::Sampled
  );
  std::vector<const RenderGraphResource *> resources{
      &scene_resource,
      &bloom_resource,
      &present_resource,
  };

  auto pass = create_scope<PostProcessPass>(make_fullscreen_quad_draw(quad, 6));
  RenderGraphPass graph_pass(std::move(pass));
  graph_pass.set_resolved_dependencies(
      {make_shader_dependency("hdr_shader", "shaders::hdr", shader)}
  );
  graph_pass.setup(nullptr, resources);

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, nullptr);

  ASSERT_EQ(frame.passes.size(), 1u);
  ASSERT_EQ(frame.images.size(), 3u);
  ASSERT_EQ(frame.binding_groups.size(), 1u);
  ASSERT_EQ(frame.buffers.size(), 1u);

  const auto &commands = frame.passes[0].commands;
  ASSERT_EQ(commands.size(), 7u);
  ASSERT_TRUE(std::holds_alternative<BeginRenderingCmd>(commands[0]));
  ASSERT_TRUE(std::holds_alternative<BindPipelineCmd>(commands[1]));
  ASSERT_TRUE(std::holds_alternative<BindBindingsCmd>(commands[2]));
  ASSERT_TRUE(std::holds_alternative<BindVertexBufferCmd>(commands[3]));
  ASSERT_TRUE(std::holds_alternative<BindIndexBufferCmd>(commands[4]));
  ASSERT_TRUE(std::holds_alternative<DrawIndexedCmd>(commands[5]));
  ASSERT_TRUE(std::holds_alternative<EndRenderingCmd>(commands[6]));

  EXPECT_EQ(frame.images[2].source, CompiledImageSourceKind::GraphImage);

  const auto &binding_group = frame.binding_groups[0];
  ASSERT_EQ(binding_group.sampled_images.size(), 2u);
  EXPECT_EQ(
      binding_group.sampled_images[0].binding_id,
      shader_bindings::engine_shaders_hdr_axsl::GlobalsResources::screen_texture
          .binding_id
  );
  EXPECT_EQ(
      binding_group.sampled_images[1].binding_id,
      shader_bindings::engine_shaders_hdr_axsl::GlobalsResources::bloom_texture
          .binding_id
  );
  ASSERT_EQ(binding_group.values.size(), 3u);
  EXPECT_EQ(
      binding_group.values[0].binding_id,
      shader_bindings::engine_shaders_hdr_axsl::GlobalsUniform::bloom_strength
          .binding_ids[0]
  );
  EXPECT_EQ(binding_group.values[0].kind, ShaderValueKind::Float);
  EXPECT_EQ(
      binding_group.values[1].binding_id,
      shader_bindings::engine_shaders_hdr_axsl::GlobalsUniform::gamma
          .binding_ids[0]
  );
  EXPECT_EQ(binding_group.values[1].kind, ShaderValueKind::Float);
  EXPECT_EQ(
      binding_group.values[2].binding_id,
      shader_bindings::engine_shaders_hdr_axsl::GlobalsUniform::exposure
          .binding_ids[0]
  );
  EXPECT_EQ(binding_group.values[2].kind, ShaderValueKind::Float);

  float bloom_strength = 0.0f;
  float gamma = 0.0f;
  float exposure = 0.0f;
  std::memcpy(
      &bloom_strength,
      binding_group.values[0].bytes.data(),
      sizeof(bloom_strength)
  );
  std::memcpy(&gamma, binding_group.values[1].bytes.data(), sizeof(gamma));
  std::memcpy(
      &exposure, binding_group.values[2].bytes.data(), sizeof(exposure)
  );

  EXPECT_FLOAT_EQ(bloom_strength, 0.12f);
  EXPECT_FLOAT_EQ(gamma, 2.2f);
  EXPECT_FLOAT_EQ(exposure, 0.7f);

  const auto &draw = std::get<DrawIndexedCmd>(commands[5]);
  EXPECT_EQ(draw.args.index_count, 6u);
}

} // namespace

} // namespace astralix
