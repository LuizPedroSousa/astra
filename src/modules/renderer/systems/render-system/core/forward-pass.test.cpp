#include "systems/render-system/passes/forward-pass.hpp"

#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include "systems/render-system/passes/render-graph-pass.hpp"
#include "vertex-array.hpp"
#include <gtest/gtest.h>
#include <cstring>
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
      : m_color_attachments{41u, 42u, 43u} {
    m_spec.width = width;
    m_spec.height = height;
    m_spec.attachments = {
        FramebufferTextureFormat::RGBA32F,
        FramebufferTextureFormat::RGBA16F,
        FramebufferTextureFormat::RED_INTEGER,
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
  uint32_t get_depth_attachment_id() const override { return 91u; }
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

class FakeForwardShader : public Shader {
public:
  FakeForwardShader()
      : Shader(ResourceHandle{0, 1}, "shaders::lighting_forward") {}

  void bind() const override {}
  void unbind() const override {}
  void attach() const override {}
  uint32_t renderer_id() const override { return 1u; }

protected:
  void set_typed_value(uint64_t, ShaderValueKind, const void *) const override {}
};

class FakeCanonicalForwardShader : public Shader {
public:
  FakeCanonicalForwardShader()
      : Shader(ResourceHandle{0, 2}, "shaders::lighting_forward") {}

  void bind() const override {}
  void unbind() const override {}
  void attach() const override {}
  uint32_t renderer_id() const override { return 2u; }

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

const CompiledValueBinding *find_value_binding(
    const CompiledBindingGroup &group,
    uint64_t binding_id
) {
  for (const auto &binding : group.values) {
    if (binding.binding_id == binding_id) {
      return &binding;
    }
  }

  return nullptr;
}

template <typename T>
T decode_binding_value(const CompiledValueBinding &binding) {
  T value{};
  std::memcpy(&value, binding.bytes.data(), sizeof(T));
  return value;
}

TEST(ForwardPassTest, RecordsOpaqueSurfaceDrawsWithMaterialAndShadowBindings) {
  auto scene_color = create_ref<FakeFramebuffer>(1280, 720);
  auto shadow_map = create_ref<FakeFramebuffer>(1024, 1024);
  auto shader = create_ref<FakeForwardShader>();
  auto vertex_array = create_ref<FakeVertexArray>();
  auto base_color = create_ref<FakeTexture>(71u);
  auto normal = create_ref<FakeTexture>(72u);

  auto scene_resource = make_graph_image_resource(
      "scene_color", 1280, 720, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto shadow_resource = make_graph_image_resource(
      "shadow_map", 1024, 1024, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment | ImageUsage::Sampled
  );
  auto bloom_resource = make_graph_image_resource(
      "bloom_extract", 1280, 720, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto entity_id_resource = make_graph_image_resource(
      "entity_pick", 1280, 720, ImageFormat::R32I,
      ImageUsage::ColorAttachment | ImageUsage::TransferSrc
  );
  auto depth_resource = make_graph_image_resource(
      "scene_depth", 1280, 720, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment
  );
  std::vector<const RenderGraphResource *> resources{
      &scene_resource,
      &shadow_resource,
      &bloom_resource,
      &entity_id_resource,
      &depth_resource,
  };

  auto pass = create_scope<ForwardPass>();
  RenderGraphPass graph_pass(std::move(pass));
  graph_pass.set_resolved_dependencies({make_shader_dependency(
      "forward_shader", "shaders::lighting_forward", shader
  )});
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  scene_frame.main_camera = rendering::CameraFrame{
      .position = glm::vec3(1.0f, 2.0f, 3.0f),
      .view = glm::mat4(2.0f),
      .projection = glm::mat4(3.0f),
  };
  scene_frame.light_frame.directional.valid = true;
  scene_frame.opaque_surfaces.push_back(rendering::SurfaceDrawItem{
      .entity_id = EntityID{1},
      .pick_id = 13u,
      .shader = shader,
      .material =
          rendering::ResolvedMaterialData{
              .base_color_descriptor_id = "textures::test::base_color",
              .normal_descriptor_id = "textures::test::normal",
              .base_color = base_color,
              .normal = normal,
          },
      .mesh =
          rendering::ResolvedMeshDraw{
              .vertex_array = vertex_array,
              .index_count = 36u,
          },
      .model = glm::mat4(4.0f),
      .bloom_enabled = true,
      .bloom_layer = 2,
  });

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  ASSERT_EQ(frame.passes.size(), 1u);
  ASSERT_EQ(frame.pipelines.size(), 1u);
  ASSERT_EQ(frame.binding_groups.size(), 3u);
  ASSERT_EQ(frame.buffers.size(), 1u);

  size_t texture_resource_count = 0u;
  size_t graph_image_count = 0u;
  for (const auto &image : frame.images) {
    if (image.source == CompiledImageSourceKind::Texture2DResource) {
      ++texture_resource_count;
    }
    if (image.source == CompiledImageSourceKind::GraphImage) {
      ++graph_image_count;
    }
  }
  EXPECT_EQ(texture_resource_count, 2u);
  EXPECT_EQ(graph_image_count, 5u);

  const auto &commands = frame.passes[0].commands;
  ASSERT_EQ(commands.size(), 9u);
  ASSERT_TRUE(std::holds_alternative<BeginRenderingCmd>(commands[0]));
  ASSERT_TRUE(std::holds_alternative<BindPipelineCmd>(commands[1]));
  ASSERT_TRUE(std::holds_alternative<BindBindingsCmd>(commands[2]));
  ASSERT_TRUE(std::holds_alternative<BindBindingsCmd>(commands[3]));
  ASSERT_TRUE(std::holds_alternative<BindBindingsCmd>(commands[4]));
  ASSERT_TRUE(std::holds_alternative<BindVertexBufferCmd>(commands[5]));
  ASSERT_TRUE(std::holds_alternative<BindIndexBufferCmd>(commands[6]));
  ASSERT_TRUE(std::holds_alternative<DrawIndexedCmd>(commands[7]));
  ASSERT_TRUE(std::holds_alternative<EndRenderingCmd>(commands[8]));

  const auto &begin = std::get<BeginRenderingCmd>(commands[0]);
  ASSERT_EQ(begin.info.color_attachments.size(), 3u);
  ASSERT_TRUE(begin.info.depth_stencil_attachment.has_value());
  EXPECT_EQ(begin.info.extent.width, 1280u);
  EXPECT_EQ(begin.info.extent.height, 720u);

  const auto &scene_group = frame.binding_groups[0];
  ASSERT_EQ(scene_group.sampled_images.size(), 1u);
  EXPECT_EQ(
      scene_group.sampled_images[0].binding_id,
      shader_bindings::engine_shaders_lighting_forward_axsl::SceneLightResources::
          shadow_map.binding_id
  );
  EXPECT_EQ(scene_group.sampled_images[0].view.aspect, ImageAspect::Depth);
  EXPECT_GT(scene_group.values.size(), 1u);

  const auto &material_group = frame.binding_groups[1];
  ASSERT_EQ(material_group.sampled_images.size(), 2u);
  EXPECT_EQ(
      material_group.sampled_images[0].binding_id,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialResources::
          base_color.binding_id
  );
  EXPECT_EQ(
      material_group.sampled_images[1].binding_id,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialResources::
          normal.binding_id
  );
  EXPECT_GT(material_group.values.size(), 0u);

  const auto &draw_group = frame.binding_groups[2];
  EXPECT_GT(draw_group.values.size(), 0u);

  const auto &draw = std::get<DrawIndexedCmd>(commands[7]);
  EXPECT_EQ(draw.args.index_count, 36u);
}

TEST(ForwardPassTest, RecordsWithFallbackTexturesWhenMaterialsAreIncomplete) {
  auto scene_color = create_ref<FakeFramebuffer>(640, 360);
  auto shadow_map = create_ref<FakeFramebuffer>(1024, 1024);
  auto shader = create_ref<FakeForwardShader>();
  auto vertex_array = create_ref<FakeVertexArray>();

  auto scene_resource = make_graph_image_resource(
      "scene_color", 640, 360, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto shadow_resource = make_graph_image_resource(
      "shadow_map", 1024, 1024, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment | ImageUsage::Sampled
  );
  auto bloom_resource = make_graph_image_resource(
      "bloom_extract", 640, 360, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto entity_id_resource = make_graph_image_resource(
      "entity_pick", 640, 360, ImageFormat::R32I,
      ImageUsage::ColorAttachment | ImageUsage::TransferSrc
  );
  auto depth_resource = make_graph_image_resource(
      "scene_depth", 640, 360, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment
  );
  std::vector<const RenderGraphResource *> resources{
      &scene_resource,
      &shadow_resource,
      &bloom_resource,
      &entity_id_resource,
      &depth_resource,
  };

  auto pass = create_scope<ForwardPass>();
  RenderGraphPass graph_pass(std::move(pass));
  graph_pass.set_resolved_dependencies({make_shader_dependency(
      "forward_shader", "shaders::lighting_forward", shader
  )});
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  scene_frame.main_camera = rendering::CameraFrame{};
  scene_frame.light_frame.directional.valid = true;
  scene_frame.opaque_surfaces.push_back(rendering::SurfaceDrawItem{
      .shader = shader,
      .mesh =
          rendering::ResolvedMeshDraw{
              .vertex_array = vertex_array,
              .index_count = 12u,
          },
  });

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  ASSERT_EQ(frame.passes.size(), 1u);
  ASSERT_EQ(frame.binding_groups.size(), 3u);
  ASSERT_EQ(frame.binding_groups[0].sampled_images.size(), 1u);
  EXPECT_EQ(
      frame.binding_groups[0].sampled_images[0].binding_id,
      shader_bindings::engine_shaders_lighting_forward_axsl::SceneLightResources::
          shadow_map.binding_id
  );
  EXPECT_TRUE(frame.binding_groups[1].sampled_images.empty());
}

TEST(ForwardPassTest, AcceptsCanonicalMaterialSamplerNames) {
  auto scene_color = create_ref<FakeFramebuffer>(1280, 720);
  auto shadow_map = create_ref<FakeFramebuffer>(1024, 1024);
  auto shader = create_ref<FakeCanonicalForwardShader>();
  auto vertex_array = create_ref<FakeVertexArray>();
  auto base_color = create_ref<FakeTexture>(71u);
  auto metallic_roughness = create_ref<FakeTexture>(72u);
  auto normal = create_ref<FakeTexture>(73u);

  auto scene_resource = make_graph_image_resource(
      "scene_color", 1280, 720, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto shadow_resource = make_graph_image_resource(
      "shadow_map", 1024, 1024, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment | ImageUsage::Sampled
  );
  auto bloom_resource = make_graph_image_resource(
      "bloom_extract", 1280, 720, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto entity_id_resource = make_graph_image_resource(
      "entity_pick", 1280, 720, ImageFormat::R32I,
      ImageUsage::ColorAttachment | ImageUsage::TransferSrc
  );
  auto depth_resource = make_graph_image_resource(
      "scene_depth", 1280, 720, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment
  );
  std::vector<const RenderGraphResource *> resources{
      &scene_resource,
      &shadow_resource,
      &bloom_resource,
      &entity_id_resource,
      &depth_resource,
  };

  auto pass = create_scope<ForwardPass>();
  RenderGraphPass graph_pass(std::move(pass));
  graph_pass.set_resolved_dependencies({make_shader_dependency(
      "forward_shader", "shaders::lighting_forward", shader
  )});
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  scene_frame.main_camera = rendering::CameraFrame{
      .position = glm::vec3(1.0f, 2.0f, 3.0f),
      .view = glm::mat4(2.0f),
      .projection = glm::mat4(3.0f),
  };
  scene_frame.light_frame.directional.valid = true;
  scene_frame.opaque_surfaces.push_back(rendering::SurfaceDrawItem{
      .entity_id = EntityID{1},
      .pick_id = 7u,
      .shader = shader,
      .material =
          rendering::ResolvedMaterialData{
              .base_color_descriptor_id = "textures::test::base_color",
              .normal_descriptor_id = "textures::test::normal",
              .metallic_roughness_descriptor_id =
                  "textures::test::metallic_roughness",
              .base_color = base_color,
              .normal = normal,
              .metallic_roughness = metallic_roughness,
          },
      .mesh =
          rendering::ResolvedMeshDraw{
              .vertex_array = vertex_array,
              .index_count = 12u,
          },
      .model = glm::mat4(4.0f),
  });

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  ASSERT_EQ(frame.passes.size(), 1u);
  ASSERT_EQ(frame.binding_groups.size(), 3u);
  ASSERT_EQ(frame.binding_groups[1].sampled_images.size(), 4u);
  EXPECT_EQ(
      frame.binding_groups[1].sampled_images[0].binding_id,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialResources::
          base_color.binding_id
  );
  EXPECT_EQ(
      frame.binding_groups[1].sampled_images[1].binding_id,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialResources::
          normal.binding_id
  );
  EXPECT_EQ(
      frame.binding_groups[1].sampled_images[2].binding_id,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialResources::
          metallic.binding_id
  );
  EXPECT_EQ(
      frame.binding_groups[1].sampled_images[3].binding_id,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialResources::
          roughness.binding_id
  );
  const auto *metallic_channel = find_value_binding(
      frame.binding_groups[1],
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__metallic_channel.binding_ids[0]
  );
  ASSERT_NE(metallic_channel, nullptr);
  EXPECT_EQ(decode_binding_value<int>(*metallic_channel), 2);
  const auto *roughness_channel = find_value_binding(
      frame.binding_groups[1],
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__roughness_channel.binding_ids[0]
  );
  ASSERT_NE(roughness_channel, nullptr);
  EXPECT_EQ(decode_binding_value<int>(*roughness_channel), 1);
  ASSERT_EQ(frame.binding_groups[0].sampled_images.size(), 1u);
}

TEST(ForwardPassTest, RecordsMaterialScalarUniformValues) {
  auto scene_color = create_ref<FakeFramebuffer>(1280, 720);
  auto shadow_map = create_ref<FakeFramebuffer>(1024, 1024);
  auto shader = create_ref<FakeForwardShader>();
  auto vertex_array = create_ref<FakeVertexArray>();
  auto base_color = create_ref<FakeTexture>(71u);

  auto scene_resource = make_graph_image_resource(
      "scene_color", 1280, 720, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto shadow_resource = make_graph_image_resource(
      "shadow_map", 1024, 1024, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment | ImageUsage::Sampled
  );
  auto bloom_resource = make_graph_image_resource(
      "bloom_extract", 1280, 720, ImageFormat::RGBA16F,
      ImageUsage::ColorAttachment | ImageUsage::Sampled
  );
  auto entity_id_resource = make_graph_image_resource(
      "entity_pick", 1280, 720, ImageFormat::R32I,
      ImageUsage::ColorAttachment | ImageUsage::TransferSrc
  );
  auto depth_resource = make_graph_image_resource(
      "scene_depth", 1280, 720, ImageFormat::Depth32F,
      ImageUsage::DepthStencilAttachment
  );
  std::vector<const RenderGraphResource *> resources{
      &scene_resource,
      &shadow_resource,
      &bloom_resource,
      &entity_id_resource,
      &depth_resource,
  };

  auto pass = create_scope<ForwardPass>();
  RenderGraphPass graph_pass(std::move(pass));
  graph_pass.set_resolved_dependencies({make_shader_dependency(
      "forward_shader", "shaders::lighting_forward", shader
  )});
  graph_pass.setup(nullptr, resources);

  rendering::SceneFrame scene_frame;
  scene_frame.main_camera = rendering::CameraFrame{
      .position = glm::vec3(1.0f, 2.0f, 3.0f),
      .view = glm::mat4(2.0f),
      .projection = glm::mat4(3.0f),
  };
  scene_frame.light_frame.directional.valid = true;
  scene_frame.opaque_surfaces.push_back(rendering::SurfaceDrawItem{
      .shader = shader,
      .material =
          rendering::ResolvedMaterialData{
              .base_color_descriptor_id = "textures::test::base_color",
              .base_color = base_color,
              .base_color_factor = glm::vec4(0.2f, 0.4f, 0.6f, 0.8f),
              .emissive_factor = glm::vec3(0.1f, 0.2f, 0.3f),
              .metallic_factor = 0.15f,
              .roughness_factor = 0.75f,
              .occlusion_strength = 0.55f,
              .normal_scale = 0.35f,
              .bloom_intensity = 1.25f,
          },
      .mesh =
          rendering::ResolvedMeshDraw{
              .vertex_array = vertex_array,
              .index_count = 12u,
          },
      .model = glm::mat4(4.0f),
  });

  CompiledFrame frame;
  graph_pass.record(0.0, frame, resources, &scene_frame);

  ASSERT_EQ(frame.binding_groups.size(), 3u);
  const auto &material_group = frame.binding_groups[1];

  const auto *base_color_factor = find_value_binding(
      material_group,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__base_color_factor.binding_ids[0]
  );
  ASSERT_NE(base_color_factor, nullptr);
  EXPECT_EQ(base_color_factor->kind, ShaderValueKind::Vec4);
  const auto base_color_value = decode_binding_value<glm::vec4>(*base_color_factor);
  EXPECT_FLOAT_EQ(base_color_value.x, 0.2f);
  EXPECT_FLOAT_EQ(base_color_value.y, 0.4f);
  EXPECT_FLOAT_EQ(base_color_value.z, 0.6f);
  EXPECT_FLOAT_EQ(base_color_value.w, 0.8f);

  const auto *emissive_factor = find_value_binding(
      material_group,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__emissive_factor.binding_ids[0]
  );
  ASSERT_NE(emissive_factor, nullptr);
  EXPECT_EQ(emissive_factor->kind, ShaderValueKind::Vec3);
  const auto emissive_value = decode_binding_value<glm::vec3>(*emissive_factor);
  EXPECT_FLOAT_EQ(emissive_value.x, 0.1f);
  EXPECT_FLOAT_EQ(emissive_value.y, 0.2f);
  EXPECT_FLOAT_EQ(emissive_value.z, 0.3f);

  const auto *metallic_factor = find_value_binding(
      material_group,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__metallic_factor.binding_ids[0]
  );
  ASSERT_NE(metallic_factor, nullptr);
  EXPECT_EQ(metallic_factor->kind, ShaderValueKind::Float);
  EXPECT_FLOAT_EQ(decode_binding_value<float>(*metallic_factor), 0.15f);

  const auto *roughness_factor = find_value_binding(
      material_group,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__roughness_factor.binding_ids[0]
  );
  ASSERT_NE(roughness_factor, nullptr);
  EXPECT_EQ(roughness_factor->kind, ShaderValueKind::Float);
  EXPECT_FLOAT_EQ(decode_binding_value<float>(*roughness_factor), 0.75f);

  const auto *metallic_channel = find_value_binding(
      material_group,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__metallic_channel.binding_ids[0]
  );
  ASSERT_NE(metallic_channel, nullptr);
  EXPECT_EQ(metallic_channel->kind, ShaderValueKind::Int);
  EXPECT_EQ(decode_binding_value<int>(*metallic_channel), 0);

  const auto *roughness_channel = find_value_binding(
      material_group,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__roughness_channel.binding_ids[0]
  );
  ASSERT_NE(roughness_channel, nullptr);
  EXPECT_EQ(roughness_channel->kind, ShaderValueKind::Int);
  EXPECT_EQ(decode_binding_value<int>(*roughness_channel), 0);

  const auto *occlusion_strength = find_value_binding(
      material_group,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__occlusion_strength.binding_ids[0]
  );
  ASSERT_NE(occlusion_strength, nullptr);
  EXPECT_EQ(occlusion_strength->kind, ShaderValueKind::Float);
  EXPECT_FLOAT_EQ(decode_binding_value<float>(*occlusion_strength), 0.55f);

  const auto *normal_scale = find_value_binding(
      material_group,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__normal_scale.binding_ids[0]
  );
  ASSERT_NE(normal_scale, nullptr);
  EXPECT_EQ(normal_scale->kind, ShaderValueKind::Float);
  EXPECT_FLOAT_EQ(decode_binding_value<float>(*normal_scale), 0.35f);

  const auto *bloom_intensity = find_value_binding(
      material_group,
      shader_bindings::engine_shaders_lighting_forward_axsl::MaterialUniform::
          materials__bloom_intensity.binding_ids[0]
  );
  ASSERT_NE(bloom_intensity, nullptr);
  EXPECT_EQ(bloom_intensity->kind, ShaderValueKind::Float);
  EXPECT_FLOAT_EQ(decode_binding_value<float>(*bloom_intensity), 1.25f);
}

} // namespace

} // namespace astralix
