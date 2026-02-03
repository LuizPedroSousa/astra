#pragma once

#include "base.hpp"
#include "guid.hpp"
#include <string>

namespace astralix {

class SerializationContext;
class DeserializationContext;
enum class SerializationTypeKind;

class ContextProxy {

public:
  ContextProxy(Scope<SerializationContext> ctx, const std::string &key)
      : m_serialization_ctx(std::move(ctx)), m_key(key) {}

  ContextProxy operator[](const SerializableKey &sub_key);
  void operator=(const SerializableValue &value);

  void operator=(Ref<SerializationContext> ctx);

  template <typename T> T as();

  SerializationTypeKind kind();

  size_t size();

private:
  Scope<SerializationContext> m_serialization_ctx;
  std::string m_key;
};

} // namespace astralix
