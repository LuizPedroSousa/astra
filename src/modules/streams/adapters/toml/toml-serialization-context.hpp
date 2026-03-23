#pragma once

#include "arena.hpp"
#include "assert.hpp"
#include "guid.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"

#include <optional>
#include <stack>
#include <string>
#include <variant>

#include <toml++/toml.hpp>

namespace astralix {

class TomlSerializationContext : public SerializationContext {
public:
  TomlSerializationContext() { m_current.push(&m_root); }
  TomlSerializationContext(Scope<StreamBuffer> buffer);

  ~TomlSerializationContext() = default;
  void set_value(const SerializableValue &value) override;
  void set_value(Ref<SerializationContext> ctx) override;

  std::any get_root() override { return m_root; }

  size_t root_size() override { return m_root.size(); };
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

  std::string extension() const override { return ".toml"; }
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
  TomlSerializationContext(toml::table root) : m_root(std::move(root)) {
    m_current.push(&m_root);
  }

  toml::table m_root;
  std::stack<toml::node *> m_current;

  // Parent tracking for set_value — toml++ nodes are strongly typed and
  // cannot be replaced in-place, so we need to re-insert into the parent.
  struct ParentInfo {
    toml::node *parent;
    std::variant<std::string, int> key;
  };
  std::optional<ParentInfo> m_parent_info;
};
} // namespace astralix
