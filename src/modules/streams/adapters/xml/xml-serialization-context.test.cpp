#include "serialization-context.hpp"

#include "arena.hpp"
#include "base.hpp"
#include "stream-buffer.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace astralix;

#if defined(ASTRALIX_SERIALIZATION_ENABLE_XML)

namespace {

Ref<SerializationContext> roundtrip_xml(Ref<SerializationContext> context) {
  ElasticArena arena(4096);
  auto *block = context->to_buffer(arena);

  auto buffer = create_scope<StreamBuffer>(block->size);
  buffer->write(static_cast<char *>(block->data), block->size);

  return SerializationContext::create(SerializationFormat::Xml,
                                      std::move(buffer));
}

Ref<SerializationContext> xml_from_string(const std::string &xml) {
  auto buffer = create_scope<StreamBuffer>(xml.size());
  buffer->write(const_cast<char *>(xml.data()), xml.size());

  return SerializationContext::create(SerializationFormat::Xml,
                                      std::move(buffer));
}

} // namespace

TEST(XmlSerializationContext, WritesAndReadsScalarValues) {
  auto context = SerializationContext::create(SerializationFormat::Xml);

  (*context)["name"] = std::string("astra");
  (*context)["version"] = 42;
  (*context)["pi"] = 3.14f;
  (*context)["active"] = true;

  auto restored = roundtrip_xml(context);

  EXPECT_EQ((*restored)["name"].as<std::string>(), "astra");
  EXPECT_EQ((*restored)["version"].as<int>(), 42);
  EXPECT_FLOAT_EQ((*restored)["pi"].as<float>(), 3.14f);
  EXPECT_EQ((*restored)["active"].as<bool>(), true);
  EXPECT_EQ((*restored)["name"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["version"].kind(), SerializationTypeKind::Int);
  EXPECT_EQ((*restored)["pi"].kind(), SerializationTypeKind::Float);
  EXPECT_EQ((*restored)["active"].kind(), SerializationTypeKind::Bool);
}

TEST(XmlSerializationContext, WritesNestedObjects) {
  auto context = SerializationContext::create(SerializationFormat::Xml);

  (*context)["player"]["name"] = std::string("hero");
  (*context)["player"]["health"] = 100;

  auto restored = roundtrip_xml(context);

  EXPECT_EQ((*restored)["player"].kind(), SerializationTypeKind::Object);
  EXPECT_EQ((*restored)["player"]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["health"].as<int>(), 100);
}

TEST(XmlSerializationContext, ReadsAndWritesAttributes) {
  auto context = SerializationContext::create(SerializationFormat::Xml);

  (*context)["player"]["@name"] = std::string("hero");
  (*context)["player"]["@level"] = 5;

  auto restored = roundtrip_xml(context);

  EXPECT_EQ((*restored)["player"]["@name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["@level"].as<int>(), 5);
  EXPECT_EQ((*restored)["player"]["@name"].kind(),
            SerializationTypeKind::String);
  EXPECT_EQ((*restored)["player"]["@level"].kind(),
            SerializationTypeKind::Int);
}

TEST(XmlSerializationContext, DetectsScalarKindsCorrectlyFromParsedXml) {
  const std::string xml = R"xml(
<root>
  <name>astra</name>
  <version>42</version>
  <pi>3.14</pi>
  <active>true</active>
  <quoted_number>42x</quoted_number>
</root>
)xml";

  auto restored = xml_from_string(xml);

  EXPECT_EQ((*restored)["name"].kind(), SerializationTypeKind::String);
  EXPECT_EQ((*restored)["version"].kind(), SerializationTypeKind::Int);
  EXPECT_EQ((*restored)["pi"].kind(), SerializationTypeKind::Float);
  EXPECT_EQ((*restored)["active"].kind(), SerializationTypeKind::Bool);
  EXPECT_EQ((*restored)["quoted_number"].kind(),
            SerializationTypeKind::String);
}

TEST(XmlSerializationContext, WritesAndReadsRootArraysOfObjects) {
  auto context = SerializationContext::create(SerializationFormat::Xml);

  (*context)[0]["name"] = std::string("hero");
  (*context)[0]["health"] = 100;
  (*context)[1]["name"] = std::string("mage");
  (*context)[1]["health"] = 75;

  auto restored = roundtrip_xml(context);

  EXPECT_EQ(restored->root_size(), 2u);
  EXPECT_EQ(restored->kind(), SerializationTypeKind::Array);
  EXPECT_EQ((*restored)[0]["name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)[0]["health"].as<int>(), 100);
  EXPECT_EQ((*restored)[1]["name"].as<std::string>(), "mage");
  EXPECT_EQ((*restored)[1]["health"].as<int>(), 75);
}

TEST(XmlSerializationContext, DistinguishesArraysFromObjects) {
  auto context = SerializationContext::create(SerializationFormat::Xml);

  (*context)["player"]["name"] = std::string("hero");
  (*context)["inventory"][0] = std::string("sword");
  (*context)["inventory"][1] = std::string("potion");

  auto restored = roundtrip_xml(context);

  EXPECT_EQ((*restored)["player"].kind(), SerializationTypeKind::Object);
  EXPECT_EQ((*restored)["inventory"].kind(), SerializationTypeKind::Array);
}

TEST(XmlSerializationContext, ParsesCommonXmlSubset) {
  const std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<!-- sample -->
<root>
  <title>astra &amp; engine</title>
  <player name="hero" level="5"/>
  <escaped attr="&quot;quoted&quot;">&lt;scene&gt;</escaped>
</root>)xml";

  auto restored = xml_from_string(xml);

  EXPECT_EQ((*restored)["title"].as<std::string>(), "astra & engine");
  EXPECT_EQ((*restored)["player"]["@name"].as<std::string>(), "hero");
  EXPECT_EQ((*restored)["player"]["@level"].as<int>(), 5);
  EXPECT_EQ((*restored)["escaped"].as<std::string>(), "<scene>");
  EXPECT_EQ((*restored)["escaped"]["@attr"].as<std::string>(), "\"quoted\"");
}

TEST(XmlSerializationContext, MissingKeysRemainUnknown) {
  auto context = SerializationContext::create(SerializationFormat::Xml);
  (*context)["present"] = std::string("value");

  auto restored = roundtrip_xml(context);
  auto missing = (*restored)["missing"];

  EXPECT_EQ(missing.kind(), SerializationTypeKind::Unknown);
  EXPECT_EQ(missing.size(), 0u);
  EXPECT_EQ(missing.as<std::string>(), "");
}

TEST(XmlSerializationContext, CopiesContextContentsIntoTargetNode) {
  auto source = SerializationContext::create(SerializationFormat::Xml);
  (*source)["@version"] = std::string("2");
  (*source)["layout"]["kind"] = std::string("split");

  auto target = SerializationContext::create(SerializationFormat::Xml);
  (*target)["snapshot"] = source;

  ElasticArena arena(4096);
  auto *block = target->to_buffer(arena);
  const std::string xml(static_cast<const char *>(block->data), block->size);

  EXPECT_NE(
      xml.find("<snapshot version=\"2\"><layout><kind>split</kind></layout></snapshot>"),
      std::string::npos);
  EXPECT_EQ(xml.find("<snapshot><root"), std::string::npos);

  auto restored = roundtrip_xml(target);
  EXPECT_EQ((*restored)["snapshot"]["@version"].as<std::string>(), "2");
  EXPECT_EQ((*restored)["snapshot"]["layout"]["kind"].as<std::string>(),
            "split");
}

TEST(XmlSerializationContext, ReturnsCorrectExtension) {
  auto context = SerializationContext::create(SerializationFormat::Xml);
  EXPECT_EQ(context->extension(), ".xml");
}

#endif
