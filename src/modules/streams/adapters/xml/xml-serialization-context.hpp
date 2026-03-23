#pragma once

#include "arena.hpp"
#include "assert.hpp"
#include "guid.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"

#include <stack>
#include <string>

#include <pugixml.hpp>

namespace astralix {

class XmlSerializationContext : public SerializationContext {
public:
  XmlSerializationContext();
  XmlSerializationContext(Scope<StreamBuffer> buffer);

  ~XmlSerializationContext() = default;
  void set_value(const SerializableValue &value) override;
  void set_value(Ref<SerializationContext> ctx) override;

  std::any get_root() override { return m_current.top(); }

  size_t root_size() override;
  size_t size() override;

  ElasticArena::Block *to_buffer(ElasticArena &arena) override;

  std::string as_string() override;
  int as_int() override;
  float as_float() override;
  bool as_bool() override;

  std::vector<std::any> as_array() override;

  SerializationTypeKind kind() override {
#define MAP_WHEN_KIND(t, k)                                                    \
  if (is_##t())                                                                \
    return SerializationTypeKind::k;

    MAP_WHEN_KIND(int, Int)
    MAP_WHEN_KIND(string, String)
    MAP_WHEN_KIND(float, Float)
    MAP_WHEN_KIND(bool, Bool)
    MAP_WHEN_KIND(array, Array)
    MAP_WHEN_KIND(object, Object)

#undef MAP_WHEN_KIND
    return SerializationTypeKind::Unknown;
  }

  bool is_string() override;
  bool is_int() override;
  bool is_float() override;
  bool is_bool() override;
  bool is_array() override;
  bool is_object() override;

  ContextProxy operator[](const SerializableKey &key) override;

  std::string extension() const override { return ".xml"; }
  void from_buffer(Scope<StreamBuffer> buffer) override;

  template <typename T> static std::string convert_key_to_string(const T &key) {
    if constexpr (std::is_same_v<T, int>) {
      return std::to_string(key);
    } else if constexpr (std::is_same_v<T, float>) {
      return std::to_string(key);
    } else if constexpr (std::is_same_v<T, std::string>) {
      return key;
    } else if constexpr (std::is_same_v<T, bool>) {
      return key ? "true" : "false";
    }

    ASTRA_EXCEPTION("NO SUITABLE TYPE STRING CASTING FOR KEY");
  }

protected:
  XmlSerializationContext(pugi::xml_node node) { m_current.push(node); }

  pugi::xml_document m_document;
  std::stack<pugi::xml_node> m_current;

  // Tracks whether current node was accessed via @-prefix (attribute mode)
  bool m_is_attribute = false;
  std::string m_attribute_name;

private:
  static size_t count_children(pugi::xml_node node);
  bool has_text_only() const;
};
} // namespace astralix
