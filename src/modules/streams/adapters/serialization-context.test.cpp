#include "serialization-context.hpp"

#include "arena.hpp"
#include "base.hpp"
#include "stream-buffer.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace astralix;

#if defined(ASTRALIX_SERIALIZATION_ENABLE_JSON)
// -- JSON Adapter Tests --

TEST(JsonSerializationContext, WritesAndReadsScalarValues) {
  auto context = SerializationContext::create(SerializationFormat::Json);

  (*context)["name"] = std::string("astra");
  (*context)["version"] = 42;
  (*context)["pi"] = 3.14f;
  (*context)["active"] = true;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Json, std::move(buffer));

  EXPECT_EQ((*restored)["name"].as<std::string>(), "astra");
  EXPECT_EQ((*restored)["version"].as<int>(), 42);
  EXPECT_FLOAT_EQ((*restored)["pi"].as<float>(), 3.14f);
  EXPECT_EQ((*restored)["active"].as<bool>(), true);
}

TEST(JsonSerializationContext, WritesNestedObjects) {
  auto context = SerializationContext::create(SerializationFormat::Json);

  (*context)["player"]["name"] = std::string("hero");
  (*context)["player"]["health"] = 100;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Json, std::move(buffer));

  EXPECT_EQ((*restored)["player"]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["health"].as<int>(), 100);
}

TEST(JsonSerializationContext, DetectsTypesCorrectly) {
  auto context = SerializationContext::create(SerializationFormat::Json);

  (*context)["str"] = std::string("hello");
  (*context)["num"] = 7;
  (*context)["flt"] = 2.5f;
  (*context)["flag"] = false;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Json, std::move(buffer));

  EXPECT_EQ((*restored)["str"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["num"].kind(), SerializationTypeKind::Int);
  EXPECT_EQ((*restored)["flt"].kind(), SerializationTypeKind::Float);
  EXPECT_EQ((*restored)["flag"].kind(), SerializationTypeKind::Bool);
}

TEST(JsonSerializationContext, ReturnsCorrectExtension) {
  auto context = SerializationContext::create(SerializationFormat::Json);
  EXPECT_EQ(context->extension(), ".json");
}
#endif

#if defined(ASTRALIX_SERIALIZATION_ENABLE_YAML)
// -- YAML Adapter Tests --

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

TEST(YamlSerializationContext, ReturnsCorrectExtension) {
  auto context = SerializationContext::create(SerializationFormat::Yaml);
  EXPECT_EQ(context->extension(), ".yaml");
}
#endif

#if defined(ASTRALIX_SERIALIZATION_ENABLE_TOML)
// -- TOML Adapter Tests --

TEST(TomlSerializationContext, WritesAndReadsScalarValues) {
  auto context = SerializationContext::create(SerializationFormat::Toml);

  (*context)["name"] = std::string("astra");
  (*context)["version"] = 42;
  (*context)["pi"] = 3.14f;
  (*context)["active"] = true;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Toml, std::move(buffer));

  EXPECT_EQ((*restored)["name"].as<std::string>(), "astra");
  EXPECT_EQ((*restored)["version"].as<int>(), 42);
  EXPECT_FLOAT_EQ((*restored)["pi"].as<float>(), 3.14f);
  EXPECT_EQ((*restored)["active"].as<bool>(), true);
}

TEST(TomlSerializationContext, WritesNestedObjects) {
  auto context = SerializationContext::create(SerializationFormat::Toml);

  (*context)["player"]["name"] = std::string("hero");
  (*context)["player"]["health"] = 100;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Toml, std::move(buffer));

  EXPECT_EQ((*restored)["player"]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["health"].as<int>(), 100);
}

TEST(TomlSerializationContext, DetectsTypesCorrectly) {
  auto context = SerializationContext::create(SerializationFormat::Toml);

  (*context)["str"] = std::string("hello");
  (*context)["num"] = 7;
  (*context)["flt"] = 2.5f;
  (*context)["flag"] = false;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Toml, std::move(buffer));

  EXPECT_EQ((*restored)["str"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["num"].kind(), SerializationTypeKind::Int);
  EXPECT_EQ((*restored)["flt"].kind(), SerializationTypeKind::Float);
  EXPECT_EQ((*restored)["flag"].kind(), SerializationTypeKind::Bool);
}

TEST(TomlSerializationContext, ReturnsCorrectExtension) {
  auto context = SerializationContext::create(SerializationFormat::Toml);
  EXPECT_EQ(context->extension(), ".toml");
}
#endif

#if defined(ASTRALIX_SERIALIZATION_ENABLE_XML)
// -- XML Adapter Tests --

TEST(XmlSerializationContext, WritesAndReadsScalarValues) {
  auto context = SerializationContext::create(SerializationFormat::Xml);

  (*context)["name"] = std::string("astra");
  (*context)["version"] = 42;
  (*context)["pi"] = 3.14f;
  (*context)["active"] = true;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Xml, std::move(buffer));

  EXPECT_EQ((*restored)["name"].as<std::string>(), "astra");
  EXPECT_EQ((*restored)["version"].as<int>(), 42);
  EXPECT_FLOAT_EQ((*restored)["pi"].as<float>(), 3.14f);
  EXPECT_EQ((*restored)["active"].as<bool>(), true);
}

TEST(XmlSerializationContext, WritesNestedObjects) {
  auto context = SerializationContext::create(SerializationFormat::Xml);

  (*context)["player"]["name"] = std::string("hero");
  (*context)["player"]["health"] = 100;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Xml, std::move(buffer));

  EXPECT_EQ((*restored)["player"]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["health"].as<int>(), 100);
}

TEST(XmlSerializationContext, ReadsAndWritesAttributes) {
  auto context = SerializationContext::create(SerializationFormat::Xml);

  (*context)["player"]["@name"] = std::string("hero");
  (*context)["player"]["@level"] = 5;

  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  auto restored =
      SerializationContext::create(SerializationFormat::Xml, std::move(buffer));

  EXPECT_EQ((*restored)["player"]["@name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["@level"].as<int>(), 5);
}

TEST(XmlSerializationContext, ReturnsCorrectExtension) {
  auto context = SerializationContext::create(SerializationFormat::Xml);
  EXPECT_EQ(context->extension(), ".xml");
}
#endif
