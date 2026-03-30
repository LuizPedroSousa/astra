#include "serialization-context.hpp"

#include "arena.hpp"
#include "base.hpp"
#include "stream-buffer.hpp"
#include <gtest/gtest.h>
#include <string>
#include <string_view>

using namespace astralix;

#if defined(ASTRALIX_SERIALIZATION_ENABLE_TOML)

namespace {

Ref<SerializationContext> roundtrip_toml(Ref<SerializationContext> context) {
  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  return SerializationContext::create(SerializationFormat::Toml,
                                      std::move(buffer));
}

Ref<SerializationContext> parse_toml(std::string_view toml) {
  auto buffer = create_scope<StreamBuffer>(toml.size());
  buffer->write(const_cast<char *>(toml.data()), toml.size());

  return SerializationContext::create(SerializationFormat::Toml,
                                      std::move(buffer));
}

} // namespace

TEST(TomlSerializationContext, WritesAndReadsScalarValues) {
  auto context = SerializationContext::create(SerializationFormat::Toml);

  (*context)["name"] = std::string("astra");
  (*context)["version"] = 42;
  (*context)["pi"] = 3.14f;
  (*context)["active"] = true;

  auto restored = roundtrip_toml(std::move(context));

  EXPECT_EQ((*restored)["name"].as<std::string>(), "astra");
  EXPECT_EQ((*restored)["version"].as<int>(), 42);
  EXPECT_FLOAT_EQ((*restored)["pi"].as<float>(), 3.14f);
  EXPECT_EQ((*restored)["active"].as<bool>(), true);
}

TEST(TomlSerializationContext, WritesNestedObjects) {
  auto context = SerializationContext::create(SerializationFormat::Toml);

  (*context)["player"]["name"] = std::string("hero");
  (*context)["player"]["health"] = 100;

  auto restored = roundtrip_toml(std::move(context));

  EXPECT_EQ((*restored)["player"]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["health"].as<int>(), 100);
}

TEST(TomlSerializationContext, DetectsScalarKindsCorrectly) {
  const std::string toml = R"toml(
str = "hello"
num = 7
flt = 2.5
flag = false
quoted_number = "42"
quoted_bool = "false"
)toml";

  auto restored = parse_toml(toml);

  EXPECT_EQ((*restored)["str"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["num"].kind(), SerializationTypeKind::Int);
  EXPECT_EQ((*restored)["flt"].kind(), SerializationTypeKind::Float);
  EXPECT_EQ((*restored)["flag"].kind(), SerializationTypeKind::Bool);
  EXPECT_EQ((*restored)["quoted_number"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["quoted_bool"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["quoted_number"].as<std::string>(), "42");
  EXPECT_EQ((*restored)["quoted_bool"].as<std::string>(), "false");
}

TEST(TomlSerializationContext, WritesAndReadsScalarArrays) {
  auto context = SerializationContext::create(SerializationFormat::Toml);

  (*context)["samples"][0] = 1;
  (*context)["samples"][1] = 2;
  (*context)["samples"][2] = 3;

  auto restored = roundtrip_toml(std::move(context));

  EXPECT_EQ((*restored)["samples"].kind(), SerializationTypeKind::Array);
  EXPECT_EQ((*restored)["samples"].size(), 3u);
  EXPECT_EQ((*restored)["samples"][0].as<int>(), 1);
  EXPECT_EQ((*restored)["samples"][1].as<int>(), 2);
  EXPECT_EQ((*restored)["samples"][2].as<int>(), 3);
}

TEST(TomlSerializationContext, WritesAndReadsArraysOfTables) {
  auto context = SerializationContext::create(SerializationFormat::Toml);

  (*context)["resources"][0]["id"] = std::string("shaders::local");
  (*context)["resources"][0]["type"] = std::string("Shader");
  (*context)["resources"][1]["id"] = std::string("shaders::engine");
  (*context)["resources"][1]["type"] = std::string("Shader");

  auto restored = roundtrip_toml(std::move(context));

  EXPECT_EQ((*restored)["resources"].kind(), SerializationTypeKind::Array);
  EXPECT_EQ((*restored)["resources"].size(), 2u);
  EXPECT_EQ((*restored)["resources"][0]["id"].as<std::string>(),
            "shaders::local");
  EXPECT_EQ((*restored)["resources"][0]["type"].as<std::string>(), "Shader");
  EXPECT_EQ((*restored)["resources"][1]["id"].as<std::string>(),
            "shaders::engine");
  EXPECT_EQ((*restored)["resources"][1]["type"].as<std::string>(), "Shader");
}

TEST(TomlSerializationContext, ParsesSectionHeadersAndArraysOfTables) {
  const std::string toml = R"toml(
[project]
name = "Sandbox"

[project.resources]
directory = "assets"

[project.serialization]
format = "toml"

[[resources]]
id = "shaders::local"
type = "Shader"
vertex = "@project/shaders/local.axsl"
fragment = "@project/shaders/local.axsl"

[[resources]]
id = "shaders::engine"
type = "Shader"
vertex = "@engine/shaders/light.axsl"
fragment = "@engine/shaders/light.axsl"
)toml";

  auto restored = parse_toml(toml);

  EXPECT_EQ((*restored)["project"]["name"].as<std::string>(), "Sandbox");
  EXPECT_EQ((*restored)["project"]["resources"]["directory"].as<std::string>(),
            "assets");
  EXPECT_EQ(
      (*restored)["project"]["serialization"]["format"].as<std::string>(),
      "toml");
  EXPECT_EQ((*restored)["resources"].size(), 2u);
  EXPECT_EQ((*restored)["resources"][0]["vertex"].as<std::string>(),
            "@project/shaders/local.axsl");
  EXPECT_EQ((*restored)["resources"][1]["fragment"].as<std::string>(),
            "@engine/shaders/light.axsl");
}

TEST(TomlSerializationContext, MissingKeysRemainUnknown) {
  auto context = SerializationContext::create(SerializationFormat::Toml);
  (*context)["present"] = std::string("value");

  auto restored = roundtrip_toml(std::move(context));

  auto missing = (*restored)["missing"];

  EXPECT_EQ(missing.kind(), SerializationTypeKind::Unknown);
  EXPECT_EQ(missing.size(), 0u);
  EXPECT_EQ(missing.as<std::string>(), "");
}

TEST(TomlSerializationContext, ReturnsCorrectExtension) {
  auto context = SerializationContext::create(SerializationFormat::Toml);
  EXPECT_EQ(context->extension(), ".toml");
}

#endif
