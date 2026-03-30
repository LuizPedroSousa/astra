#include "serialization-context.hpp"

#include "arena.hpp"
#include "base.hpp"
#include "stream-buffer.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace astralix;

#if defined(ASTRALIX_SERIALIZATION_ENABLE_YAML)

TEST(YamlSerializationContext, WritesAndReadsScalarValues) {
  auto context = SerializationContext::create(SerializationFormat::Yaml);

  (*context)["name"] = std::string("astra");
  (*context)["version"] = 42;
  (*context)["pi"] = 3.14f;
  (*context)["active"] = true;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Yaml, std::move(buffer));

  EXPECT_EQ((*restored)["name"].as<std::string>(), "astra");
  EXPECT_EQ((*restored)["version"].as<int>(), 42);
  EXPECT_FLOAT_EQ((*restored)["pi"].as<float>(), 3.14f);
  EXPECT_EQ((*restored)["active"].as<bool>(), true);
}

TEST(YamlSerializationContext, WritesNestedObjects) {
  auto context = SerializationContext::create(SerializationFormat::Yaml);

  (*context)["player"]["name"] = std::string("hero");
  (*context)["player"]["health"] = 100;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Yaml, std::move(buffer));

  EXPECT_EQ((*restored)["player"]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["health"].as<int>(), 100);
}

TEST(YamlSerializationContext, DetectsScalarKindsCorrectly) {
  const std::string yaml = R"yaml(
name: "astra"
version: 42
pi: 3.14
active: true
quoted_number: "42"
quoted_bool: "false"
)yaml";

  auto buffer = create_scope<StreamBuffer>(yaml.size());
  buffer->write(const_cast<char *>(yaml.data()), yaml.size());

  auto restored =
      SerializationContext::create(SerializationFormat::Yaml, std::move(buffer));

  EXPECT_EQ((*restored)["name"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["version"].kind(), SerializationTypeKind::Int);
  EXPECT_EQ((*restored)["pi"].kind(), SerializationTypeKind::Float);
  EXPECT_EQ((*restored)["active"].kind(), SerializationTypeKind::Bool);
  EXPECT_EQ((*restored)["quoted_number"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["quoted_bool"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["quoted_number"].as<std::string>(), "42");
  EXPECT_EQ((*restored)["quoted_bool"].as<std::string>(), "false");
}

TEST(YamlSerializationContext, WritesAndReadsRootArraysOfObjects) {
  auto context = SerializationContext::create(SerializationFormat::Yaml);

  (*context)[0]["name"] = std::string("hero");
  (*context)[0]["health"] = 100;
  (*context)[1]["name"] = std::string("mage");
  (*context)[1]["health"] = 75;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Yaml, std::move(buffer));

  EXPECT_EQ(restored->root_size(), 2u);
  EXPECT_EQ((*restored)[0]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)[0]["health"].as<int>(), 100);
  EXPECT_EQ((*restored)[1]["name"].as<std::string>(), "mage");
  EXPECT_EQ((*restored)[1]["health"].as<int>(), 75);
}

TEST(YamlSerializationContext, MissingKeysRemainUnknown) {
  auto context = SerializationContext::create(SerializationFormat::Yaml);
  (*context)["present"] = std::string("value");

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Yaml, std::move(buffer));

  auto missing = (*restored)["missing"];

  EXPECT_EQ(missing.kind(), SerializationTypeKind::Unknown);
  EXPECT_EQ(missing.size(), 0u);
  EXPECT_EQ(missing.as<std::string>(), "");
}

TEST(YamlSerializationContext, ReturnsCorrectExtension) {
  auto context = SerializationContext::create(SerializationFormat::Yaml);
  EXPECT_EQ(context->extension(), ".yaml");
}

#endif
