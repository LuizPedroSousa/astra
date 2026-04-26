#pragma once

#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "guid.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astralix::xml_detail {
  struct Attribute {
    std::string name;
    std::string value;
  };

  struct Node {
    std::string name;
    std::string text;
    std::vector<Attribute> attributes;
    std::vector<Scope<Node>> children;
  };

  Attribute* find_attribute(Node& node, std::string_view name);
  const Attribute* find_attribute(const Node& node, std::string_view name);
  Node parse_document(std::string_view input);
}

namespace astralix {

  class XmlSerializationContext : public SerializationContext {
  public:
    XmlSerializationContext();
    XmlSerializationContext(Scope<StreamBuffer> buffer);

    ~XmlSerializationContext() = default;
    void set_value(const SerializableValue& value) override;
    void set_value(Ref<SerializationContext> ctx) override;

    std::any get_root() override { return m_root.get(); }

    size_t root_size() override;
    size_t size() override;

    ElasticArena::Block* to_buffer(ElasticArena& arena) override;

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

    ContextProxy operator[](const SerializableKey& key) override;

    std::string extension() const override { return ".xml"; }
    void from_buffer(Scope<StreamBuffer> buffer) override;

    template <typename T> static std::string convert_key_to_string(const T& key) {
      if constexpr (std::is_same_v<T, int>) {
        return std::to_string(key);
      }
      else if constexpr (std::is_same_v<T, float>) {
        return std::to_string(key);
      }
      else if constexpr (std::is_same_v<T, std::string>) {
        return key;
      }
      else if constexpr (std::is_same_v<T, bool>) {
        return key ? "true" : "false";
      }

      ASTRA_EXCEPTION("NO SUITABLE TYPE STRING CASTING FOR KEY");
    }

  private:
    using Node = xml_detail::Node;

    XmlSerializationContext(std::shared_ptr<Node> root, Node* current,
      std::optional<std::string> attribute_name =
      std::nullopt);

    static size_t node_size(const Node* node);

    std::shared_ptr<Node> m_root;
    Node* m_current = nullptr;
    std::optional<std::string> m_attribute_name;
  };
} // namespace astralix
