#include "context-proxy.hpp"

#include "guid.hpp"
#include "serialization-context.hpp"
#include <string>
#include <type_traits>

namespace astralix {

ContextProxy ContextProxy::operator[](const SerializableKey &sub_key) {
  return (*m_serialization_ctx)[sub_key];
}

void ContextProxy::operator=(const SerializableValue &value) {
  m_serialization_ctx->set_value(value);
};

void ContextProxy::operator=(Ref<SerializationContext> ctx) {
  m_serialization_ctx->set_value(ctx);
};

size_t ContextProxy::size() { return m_serialization_ctx->size(); };

SerializationTypeKind ContextProxy::kind() {
  return m_serialization_ctx->kind();
};

template <typename T> T ContextProxy::as() {
  if constexpr (std::is_same_v<T, std::string>) {
    return m_serialization_ctx->as_string();
  } else if constexpr (std::is_same_v<T, int>) {
    return m_serialization_ctx->as_int();
  } else if constexpr (std::is_same_v<T, float>) {
    return m_serialization_ctx->as_float();
  } else if constexpr (std::is_same_v<T, bool>) {
    return m_serialization_ctx->as_bool();
  } else if constexpr (std::is_same_v<T, std::vector<std::any>>) {
    return m_serialization_ctx->as_array();
  }
}

template int ContextProxy::as<int>();
template std::string ContextProxy::as<std::string>();
template float ContextProxy::as<float>();
template bool ContextProxy::as<bool>();

} // namespace astralix
