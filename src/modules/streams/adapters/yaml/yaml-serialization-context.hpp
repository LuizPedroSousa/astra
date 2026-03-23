#pragma once

#include "arena.hpp"
#include "assert.hpp"
#include "guid.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"

#include <stack>
#include <yaml-cpp/yaml.h>

namespace astralix {

class YamlSerializationContext : public SerializationContext {
public:
  YamlSerializationContext() : m_root(YAML::NodeType::Map) {
    m_current.push(m_root);
  }
  YamlSerializationContext(Scope<StreamBuffer> buffer);

  ~YamlSerializationContext() = default;
  void set_value(const SerializableValue &value) override;
  void set_value(Ref<SerializationContext> ctx) override;

  std::any get_root() override { return m_root; }

  size_t root_size() override { return m_root.size(); };
  size_t size() override { return m_current.top().size(); };

  ElasticArena::Block *to_buffer(ElasticArena &arena) override;

  std::string as_string() override { return m_current.top().as<std::string>(); };
  int as_int() override { return m_current.top().as<int>(); };
  float as_float() override { return m_current.top().as<float>(); };
  bool as_bool() override { return m_current.top().as<bool>(); };

  std::vector<std::any> as_array() override {
    std::vector<std::any> items;

    for (const auto &element : m_current.top()) {
      items.push_back(element);
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

  bool is_string() override { return m_current.top().IsScalar(); };
  bool is_int() override;
  bool is_float() override;
  bool is_bool() override;
  bool is_array() override { return m_current.top().IsSequence(); };
  bool is_object() override { return m_current.top().IsMap(); };

  ContextProxy operator[](const SerializableKey &key) override;

  std::string extension() const override { return ".yaml"; }
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
  YamlSerializationContext(YAML::Node root) : m_root(root) {
    m_current.push(m_root);
  }
  YAML::Node m_root;
  std::stack<YAML::Node> m_current;
};
} // namespace astralix
