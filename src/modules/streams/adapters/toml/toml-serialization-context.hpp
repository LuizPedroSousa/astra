#pragma once

#include "arena.hpp"
#include "assert.hpp"
#include "guid.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"

#include <memory>
#include <string>
#include <vector>

namespace astralix::toml_detail {

enum class NodeKind {
  Undefined,
  String,
  Int,
  Float,
  Bool,
  Table,
  Array,
};

struct Node;

struct TableEntry {
  std::string key;
  std::unique_ptr<Node> value;
};

struct Node {
  NodeKind kind = NodeKind::Undefined;
  std::string string_value;
  int int_value = 0;
  float float_value = 0.0f;
  bool bool_value = false;
  std::vector<TableEntry> table_items;
  std::vector<std::unique_ptr<Node>> array_items;
};

} // namespace astralix::toml_detail

namespace astralix {

class TomlSerializationContext : public SerializationContext {
public:
  TomlSerializationContext();
  TomlSerializationContext(Scope<StreamBuffer> buffer);

  ~TomlSerializationContext() = default;
  void set_value(const SerializableValue &value) override;
  void set_value(Ref<SerializationContext> ctx) override;

  std::any get_root() override { return m_root.get(); }

  size_t root_size() override;
  size_t size() override;

  ElasticArena::Block *to_buffer(ElasticArena &arena) override;

  std::string as_string() override;
  int as_int() override;
  float as_float() override;
  bool as_bool() override;

  std::vector<std::any> as_array() override;
  std::vector<std::string> object_keys() override;

  SerializationTypeKind kind() override;

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

private:
  using Node = toml_detail::Node;

  TomlSerializationContext(std::shared_ptr<Node> root, Node *current);

  static size_t node_size(const Node *node);

  std::shared_ptr<Node> m_root;
  Node *m_current = nullptr;
};
} // namespace astralix
