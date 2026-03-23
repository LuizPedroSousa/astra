#include "adapters/toml/toml-serialization-context.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "context-proxy.hpp"
#include "stream-buffer.hpp"
#include <any>
#include <sstream>
#include <string>
#include <variant>

namespace astralix {

ContextProxy
TomlSerializationContext::operator[](const SerializableKey &key) {
  toml::node *current_node = m_current.top();

  auto parsed_key = std::visit(
      [](auto &&key) -> std::variant<int, std::string> {
        using T = std::decay_t<decltype(key)>;

        if constexpr (std::is_same_v<T, int>) {
          return key;
        } else {
          return convert_key_to_string(key);
        }
      },
      key);

  auto sub_ctx = create_scope<TomlSerializationContext>();

  if (std::holds_alternative<std::string>(parsed_key)) {
    auto str_key = std::get<std::string>(parsed_key);

    if (current_node->is_table()) {
      auto &table = *current_node->as_table();

      if (!table.contains(str_key)) {
        table.insert_or_assign(str_key, toml::table{});
      }

      sub_ctx->m_current.push(table.get(str_key));
      sub_ctx->m_parent_info = ParentInfo{current_node, str_key};

      return ContextProxy(std::move(sub_ctx), str_key);
    }
  }

  auto int_key = std::get<int>(parsed_key);

  if (current_node->is_array()) {
    auto &array = *current_node->as_array();

    while (static_cast<int>(array.size()) <= int_key) {
      array.push_back(toml::table{});
    }

    sub_ctx->m_current.push(array.get(int_key));
    sub_ctx->m_parent_info = ParentInfo{current_node, int_key};

    return ContextProxy(std::move(sub_ctx), std::to_string(int_key));
  }

  ASTRA_EXCEPTION("TOML node is neither table nor array for indexing");
};

void TomlSerializationContext::set_value(const SerializableValue &value) {
  if (!m_parent_info) {
    return;
  }

  auto &parent = m_parent_info->parent;
  auto &parent_key = m_parent_info->key;

  auto assign = [&](auto typed_value) {
    if (std::holds_alternative<std::string>(parent_key)) {
      auto &table = *parent->as_table();
      table.insert_or_assign(std::get<std::string>(parent_key), typed_value);
      m_current = {};
      m_current.push(table.get(std::get<std::string>(parent_key)));
    } else {
      auto &array = *parent->as_array();
      auto index = std::get<int>(parent_key);
      array.replace(array.cbegin() + index, typed_value);
      m_current = {};
      m_current.push(array.get(index));
    }
  };

  if (std::holds_alternative<int>(value)) {
    assign(static_cast<int64_t>(std::get<int>(value)));
  } else if (std::holds_alternative<float>(value)) {
    assign(static_cast<double>(std::get<float>(value)));
  } else if (std::holds_alternative<std::string>(value)) {
    assign(std::get<std::string>(value));
  } else if (std::holds_alternative<bool>(value)) {
    assign(std::get<bool>(value));
  }
}

void TomlSerializationContext::set_value(Ref<SerializationContext> ctx) {
  auto toml_ctx = static_cast<TomlSerializationContext *>(ctx.get());

  if (m_parent_info && std::holds_alternative<std::string>(m_parent_info->key)) {
    auto &table = *m_parent_info->parent->as_table();
    table.insert_or_assign(std::get<std::string>(m_parent_info->key),
                           toml_ctx->m_root);
    m_current = {};
    m_current.push(table.get(std::get<std::string>(m_parent_info->key)));
  } else if (m_current.top()->is_table()) {
    auto &target = *m_current.top()->as_table();
    target = toml_ctx->m_root;
  }
}

ElasticArena::Block *TomlSerializationContext::to_buffer(ElasticArena &arena) {
  std::ostringstream stream;
  stream << toml::toml_formatter(m_root);

  std::string toml_string = stream.str();

  auto block = arena.allocate(toml_string.size());

  std::memcpy(block->data, toml_string.data(), toml_string.size());

  return block;
};

TomlSerializationContext::TomlSerializationContext(Scope<StreamBuffer> buffer) {
  from_buffer(std::move(buffer));
};

void TomlSerializationContext::from_buffer(Scope<StreamBuffer> buffer) {
  std::string content(buffer->data(), buffer->size());

  try {
    auto result = toml::parse(content);
    m_root = std::move(result);
  } catch (const toml::parse_error &error) {
    ASTRA_EXCEPTION("Failed to parse TOML: " + std::string(error.what()));
  }

  m_current = {};
  m_current.push(&m_root);
};

size_t TomlSerializationContext::size() {
  toml::node *node = m_current.top();
  if (node->is_table())
    return node->as_table()->size();
  if (node->is_array())
    return node->as_array()->size();
  return 0;
}

std::string TomlSerializationContext::as_string() {
  return m_current.top()->as_string()->get();
}

int TomlSerializationContext::as_int() {
  return static_cast<int>(m_current.top()->as_integer()->get());
}

float TomlSerializationContext::as_float() {
  return static_cast<float>(m_current.top()->as_floating_point()->get());
}

bool TomlSerializationContext::as_bool() {
  return m_current.top()->as_boolean()->get();
}

std::vector<std::any> TomlSerializationContext::as_array() {
  std::vector<std::any> items;

  if (m_current.top()->is_array()) {
    for (auto &element : *m_current.top()->as_array()) {
      items.push_back(&element);
    }
  }

  return items;
}

bool TomlSerializationContext::is_string() {
  return m_current.top()->is_string();
}

bool TomlSerializationContext::is_int() {
  return m_current.top()->is_integer();
}

bool TomlSerializationContext::is_float() {
  return m_current.top()->is_floating_point();
}

bool TomlSerializationContext::is_bool() {
  return m_current.top()->is_boolean();
}

bool TomlSerializationContext::is_array() {
  return m_current.top()->is_array();
}

bool TomlSerializationContext::is_object() {
  return m_current.top()->is_table();
}

} // namespace astralix
