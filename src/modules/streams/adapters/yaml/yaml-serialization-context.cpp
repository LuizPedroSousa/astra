#include "adapters/yaml/yaml-serialization-context.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "context-proxy.hpp"
#include "stream-buffer.hpp"
#include <any>
#include <string>
#include <variant>
#include <yaml-cpp/yaml.h>

namespace astralix {

ContextProxy

YamlSerializationContext::operator[](const SerializableKey &key) {
  YAML::Node current_node = m_current.top();

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

  auto sub_ctx = create_scope<YamlSerializationContext>();

  if (std::holds_alternative<std::string>(parsed_key)) {
    auto str_key = std::get<std::string>(parsed_key);

    sub_ctx->m_current.push(current_node[str_key]);

    return ContextProxy(std::move(sub_ctx), str_key);
  }

  auto int_key = std::get<int>(parsed_key);

  sub_ctx->m_current.push(current_node[int_key]);

  return ContextProxy(std::move(sub_ctx), std::to_string(int_key));
};

void YamlSerializationContext::set_value(const SerializableValue &value) {
  YAML::Node current_value = m_current.top();

  if (std::holds_alternative<int>(value)) {
    current_value = std::get<int>(value);
  } else if (std::holds_alternative<float>(value)) {
    current_value = std::get<float>(value);
  } else if (std::holds_alternative<std::string>(value)) {
    current_value = std::get<std::string>(value);
  } else if (std::holds_alternative<bool>(value)) {
    current_value = std::get<bool>(value);
  }
}

void YamlSerializationContext::set_value(Ref<SerializationContext> ctx) {
  YAML::Node current_value = m_current.top();

  auto yaml_ctx = static_cast<YamlSerializationContext *>(ctx.get());

  current_value = yaml_ctx->m_root;
}

ElasticArena::Block *YamlSerializationContext::to_buffer(ElasticArena &arena) {
  YAML::Emitter emitter;
  emitter << m_root;

  std::string yaml_string = emitter.c_str();

  auto block = arena.allocate(yaml_string.size());

  std::memcpy(block->data, yaml_string.data(), yaml_string.size());

  return block;
};

YamlSerializationContext::YamlSerializationContext(Scope<StreamBuffer> buffer) {
  from_buffer(std::move(buffer));
};

void YamlSerializationContext::from_buffer(Scope<StreamBuffer> buffer) {
  std::string content(buffer->data(), buffer->size());

  m_root = YAML::Load(content);

  m_current = {};
  m_current.push(m_root);
};

bool YamlSerializationContext::is_int() {
  if (!m_current.top().IsScalar()) {
    return false;
  }

  try {
    m_current.top().as<int>();
    return true;
  } catch (...) {
    return false;
  }
}

bool YamlSerializationContext::is_float() {
  if (!m_current.top().IsScalar()) {
    return false;
  }

  try {
    m_current.top().as<float>();
    return true;
  } catch (...) {
    return false;
  }
}

bool YamlSerializationContext::is_bool() {
  if (!m_current.top().IsScalar()) {
    return false;
  }

  try {
    m_current.top().as<bool>();
    return true;
  } catch (...) {
    return false;
  }
}

} // namespace astralix
