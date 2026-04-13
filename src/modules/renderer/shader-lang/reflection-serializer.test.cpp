#include "shader-lang/reflection-serializer.hpp"
#include "shader-lang/compiler.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace astralix {

TEST(ShaderReflectionSerializer, SidecarPathUsesSourceStem) {
  const auto path = shader_reflection_sidecar_path("/tmp/shaders/light.axsl");
  EXPECT_EQ(path.string(), "/tmp/shaders/light.reflection.json");
}

TEST(ShaderReflectionSerializer, ReflectionRoundTripsThroughJsonSidecar) {
  static constexpr std::string_view src = R"axsl(
@version 450;

@uniform
interface Entity {
    mat4 view;
    mat4 projection;
    bool use_instancing = false;
}

interface VertexOutput {
    @location(0) vec4 color;
}

@vertex
fn main(Entity entity) -> VertexOutput {
    return VertexOutput(vec4(entity.view[0][0]));
}
)axsl";

  Compiler compiler;
  auto compile_result =
      compiler.compile(src, {}, "/tmp/reflection-roundtrip.axsl");
  ASSERT_TRUE(compile_result.ok());
  ASSERT_FALSE(compile_result.reflection.empty());

  std::string serialize_error;
  auto serialized = serialize_shader_reflection(
      compile_result.reflection, SerializationFormat::Json, &serialize_error);
  ASSERT_TRUE(serialized.has_value()) << serialize_error;
  EXPECT_NE(serialized->find("\"version\""), std::string::npos);

  std::string deserialize_error;
  auto from_content = deserialize_shader_reflection(
      *serialized, SerializationFormat::Json, &deserialize_error);
  ASSERT_TRUE(from_content.has_value()) << deserialize_error;
  EXPECT_EQ(from_content->version, 3);

  const auto path = std::filesystem::temp_directory_path() /
                    "astralix-reflection-roundtrip.json";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out);
  out << *serialized;
  out.close();

  std::string read_error;
  auto loaded =
      read_shader_reflection(path, SerializationFormat::Json, &read_error);
  ASSERT_TRUE(loaded.has_value()) << read_error;
  EXPECT_EQ(loaded->version, 3);

  auto stage_it = loaded->stages.find(StageKind::Vertex);
  ASSERT_NE(stage_it, loaded->stages.end());
  ASSERT_EQ(stage_it->second.resources.size(), 1u);

  const auto &resource = stage_it->second.resources.front();
  EXPECT_EQ(resource.logical_name, "entity");
  ASSERT_EQ(resource.members.size(), 1u);
  EXPECT_EQ(resource.members.front().logical_name, "entity.view");
  EXPECT_EQ(resource.members.front().binding_id,
            shader_binding_id("entity.view"));
  ASSERT_TRUE(resource.members.front().compatibility_alias.has_value());
  EXPECT_EQ(*resource.members.front().compatibility_alias, "view");
  ASSERT_TRUE(resource.members.front().glsl.emitted_name.has_value());
  EXPECT_EQ(*resource.members.front().glsl.emitted_name, "_view");

  ASSERT_EQ(resource.declared_fields.size(), 3u);
  EXPECT_EQ(resource.declared_fields[0].logical_name, "entity.view");
  EXPECT_EQ(resource.declared_fields[0].active_stage_mask,
            shader_stage_mask(StageKind::Vertex));
  EXPECT_EQ(resource.declared_fields[0].binding_id,
            shader_binding_id("entity.view"));
  EXPECT_EQ(resource.declared_fields[1].logical_name, "entity.projection");
  EXPECT_EQ(resource.declared_fields[1].active_stage_mask, 0u);
  EXPECT_EQ(resource.declared_fields[2].logical_name, "entity.use_instancing");
  ASSERT_TRUE(resource.declared_fields[2].default_value.has_value());
  EXPECT_EQ(std::get<bool>(*resource.declared_fields[2].default_value), false);
  EXPECT_EQ(resource.declared_fields[2].active_stage_mask, 0u);

  std::filesystem::remove(path);
}

} // namespace astralix
