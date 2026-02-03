#pragma once

#include "arena.hpp"
#include "assert.hpp"
#include "guid.hpp"
#include "log.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"

#include <json/value.h>
#include <stack>

namespace astralix {

class JsonSerializationContext : public SerializationContext {
public:
  JsonSerializationContext() { m_current.push(&m_root); }
  JsonSerializationContext(Scope<StreamBuffer> buffer);

  ~JsonSerializationContext() = default;
  void set_value(const SerializableValue &value) override;
  void set_value(Ref<SerializationContext> ctx) override;

  std::any get_root() override { return m_root; }

  size_t root_size() override { return m_root.size(); };
  size_t size() override { return m_current.top()->size(); };

  ElasticArena::Block *to_buffer(ElasticArena &arena) override;

  std::string as_string() override { return m_current.top()->asString(); };
  int as_int() override { return m_current.top()->asInt(); };
  float as_float() override { return m_current.top()->asFloat(); };
  bool as_bool() override { return m_current.top()->asBool(); };

  std::vector<std::any> as_array() override {
    auto size = this->size();
    std::vector<std::any> items;

    for (int i = 0; i < size; i++) {
      items.push_back(m_current.top()[i]);
    }

    return items;
  };

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

  bool is_string() override { return m_current.top()->isString(); };
  bool is_int() override { return m_current.top()->isInt(); };
  bool is_float() override { return m_current.top()->isDouble(); };
  bool is_bool() override { return m_current.top()->isBool(); };
  bool is_array() override { return m_current.top()->isArray(); };
  bool is_object() override { return m_current.top()->isObject(); };

  ContextProxy operator[](const SerializableKey &key) override;

  std::string extension() const override { return ".json"; }
  void from_buffer(Scope<StreamBuffer> buffer) override;

  template <typename T> static std::string convert_key_to_string(const T &key) {
    if constexpr (std::is_same_v<T, int>) {
      return std::to_string(key); // int to string
    } else if constexpr (std::is_same_v<T, float>) {
      return std::to_string(key); // float to string
    } else if constexpr (std::is_same_v<T, std::string>) {
      return key; // already a string
    } else if constexpr (std::is_same_v<T, bool>) {
      return key ? "true" : "false"; // bool to "true" or "false"
    }

    ASTRA_EXCEPTION("NO SUITABLE TYPE STRING CASTING FOR KEY");
  }

protected:
  JsonSerializationContext(Json::Value &root) : m_root(root) {
    m_current.push(&m_root);
  }
  Json::Value m_root;
  std::stack<Json::Value *> m_current;
};
} // namespace astralix
