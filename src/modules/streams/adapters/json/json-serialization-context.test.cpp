#include "serialization-context.hpp"

#include "arena.hpp"
#include "base.hpp"
#include "exceptions/base-exception.hpp"
#include "stream-buffer.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace astralix;

#if defined(ASTRALIX_SERIALIZATION_ENABLE_JSON)

namespace {

Scope<StreamBuffer> make_buffer(const std::string &content) {
  auto buffer = create_scope<StreamBuffer>(content.size());
  if (!content.empty()) {
    buffer->write(const_cast<char *>(content.data()), content.size());
  }
  return buffer;
}

Ref<SerializationContext> roundtrip_json(Ref<SerializationContext> context) {
  ElasticArena arena(4096);
  auto block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  return SerializationContext::create(SerializationFormat::Json, std::move(buffer));
}

} // namespace

TEST(JsonSerializationContext, WritesAndReadsScalarValues) {
  auto context = SerializationContext::create(SerializationFormat::Json);

  (*context)["name"] = std::string("astra");
  (*context)["version"] = 42;
  (*context)["pi"] = 3.14f;
  (*context)["active"] = true;

  auto restored = roundtrip_json(context);

  EXPECT_EQ((*restored)["name"].as<std::string>(), "astra");
  EXPECT_EQ((*restored)["version"].as<int>(), 42);
  EXPECT_FLOAT_EQ((*restored)["pi"].as<float>(), 3.14f);
  EXPECT_EQ((*restored)["active"].as<bool>(), true);
}

TEST(JsonSerializationContext, WritesNestedObjects) {
  auto context = SerializationContext::create(SerializationFormat::Json);

  (*context)["player"]["name"] = std::string("hero");
  (*context)["player"]["health"] = 100;

  auto restored = roundtrip_json(context);

  EXPECT_EQ((*restored)["player"]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["health"].as<int>(), 100);
}

TEST(JsonSerializationContext, DetectsTypesCorrectly) {
  auto context = SerializationContext::create(SerializationFormat::Json);

  (*context)["str"] = std::string("hello");
  (*context)["num"] = 7;
  (*context)["flt"] = 2.5f;
  (*context)["flag"] = false;

  auto restored = roundtrip_json(context);

  EXPECT_EQ((*restored)["str"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["num"].kind(), SerializationTypeKind::Int);
  EXPECT_EQ((*restored)["flt"].kind(), SerializationTypeKind::Float);
  EXPECT_EQ((*restored)["flag"].kind(), SerializationTypeKind::Bool);
}

TEST(JsonSerializationContext, WritesAndReadsRootArraysOfObjects) {
  auto context = SerializationContext::create(SerializationFormat::Json);

  (*context)[0]["name"] = std::string("hero");
  (*context)[0]["health"] = 100;
  (*context)[1]["name"] = std::string("mage");
  (*context)[1]["health"] = 75;

  auto restored = roundtrip_json(context);

  EXPECT_EQ(restored->root_size(), 2u);
  EXPECT_EQ((*restored)[0]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)[0]["health"].as<int>(), 100);
  EXPECT_EQ((*restored)[1]["name"].as<std::string>(), "mage");
  EXPECT_EQ((*restored)[1]["health"].as<int>(), 75);
}

TEST(JsonSerializationContext, MissingKeysRemainUnknown) {
  auto context = SerializationContext::create(SerializationFormat::Json);
  (*context)["present"] = std::string("value");

  auto restored = roundtrip_json(context);
  auto missing = (*restored)["missing"];

  EXPECT_EQ(missing.kind(), SerializationTypeKind::Unknown);
  EXPECT_EQ(missing.size(), 0u);
  EXPECT_EQ(missing.as<std::string>(), "");
}

TEST(JsonSerializationContext, ParsesNullValuesAsUnknown) {
  const std::string json = R"json({
  "value": null,
  "items": [null, 1]
})json";

  auto restored = SerializationContext::create(SerializationFormat::Json, make_buffer(json));

  EXPECT_EQ((*restored)["value"].kind(), SerializationTypeKind::Unknown);
  EXPECT_EQ((*restored)["value"].as<std::string>(), "");
  EXPECT_EQ((*restored)["items"].kind(), SerializationTypeKind::Array);
  EXPECT_EQ((*restored)["items"].size(), 2u);
  EXPECT_EQ((*restored)["items"][0].kind(), SerializationTypeKind::Unknown);
  EXPECT_EQ((*restored)["items"][1].as<int>(), 1);

  auto reroundtripped = roundtrip_json(restored);
  EXPECT_EQ((*reroundtripped)["value"].kind(), SerializationTypeKind::Unknown);
  EXPECT_EQ((*reroundtripped)["items"][0].kind(), SerializationTypeKind::Unknown);
}

TEST(JsonSerializationContext, ParsesEscapedStringsAndUnicode) {
  const std::string json =
      R"json({"message":"quote: \" line:\n tab:\t slash:\\ latin:\u00E9 rocket:\uD83D\uDE80"})json";

  auto restored = SerializationContext::create(SerializationFormat::Json, make_buffer(json));

  const std::string expected =
      "quote: \" line:\n tab:\t slash:\\ latin:\xC3\xA9 rocket:\xF0\x9F\x9A\x80";
  EXPECT_EQ((*restored)["message"].as<std::string>(), expected);

  auto reroundtripped = roundtrip_json(restored);
  EXPECT_EQ((*reroundtripped)["message"].as<std::string>(), expected);
}

TEST(JsonSerializationContext, IgnoresUtf8BomAndTrailingNullTerminators) {
  const std::string json =
      std::string("\xEF\xBB\xBF", 3) + R"json({"name":"astra"})json" +
      std::string("\0\0", 2);

  auto restored =
      SerializationContext::create(SerializationFormat::Json, make_buffer(json));

  EXPECT_EQ((*restored)["name"].as<std::string>(), "astra");
}

TEST(JsonSerializationContext, RejectsMalformedJsonInput) {
  const std::string truncated = R"json({"name":"astra")json";
  const std::string invalid_escape = R"json({"name":"\u12G4"})json";
  const std::string invalid_number = R"json({"value":01})json";

  EXPECT_THROW(
      SerializationContext::create(SerializationFormat::Json, make_buffer(truncated)),
      BaseException
  );
  EXPECT_THROW(
      SerializationContext::create(SerializationFormat::Json, make_buffer(invalid_escape)),
      BaseException
  );
  EXPECT_THROW(
      SerializationContext::create(SerializationFormat::Json, make_buffer(invalid_number)),
      BaseException
  );
}

TEST(JsonSerializationContext, ReturnsCorrectExtension) {
  auto context = SerializationContext::create(SerializationFormat::Json);
  EXPECT_EQ(context->extension(), ".json");
}

#endif
