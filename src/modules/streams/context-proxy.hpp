#pragma once

#include "base.hpp"
#include "guid.hpp"
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace astralix {

class SerializationContext;
class DeserializationContext;
enum class SerializationTypeKind;

#define ASTRA_ASSIGN_CONTEXT_MEMBER(ctx, obj, member)                          \
  ((ctx)[#member] = (obj).member)
#define ASTRA_ASSIGN_CONTEXT_MEMBER_IF_DEFINED(ctx, obj, member)               \
  ((ctx)[#member].assign_if_defined((obj).member))
#define ASTRA_ASSIGN_CONTEXT_MEMBER_IF_KIND(ctx, obj, member, Source, Kind)    \
  ((ctx)[#member].assign_if_kind<Source>((obj).member, Kind))

class ContextProxy {

public:
  ContextProxy(Scope<SerializationContext> ctx, const std::string &key)
      : m_serialization_ctx(std::move(ctx)), m_key(key) {}

  ContextProxy operator[](const SerializableKey &sub_key);
  void operator=(const SerializableValue &value);

  void operator=(Ref<SerializationContext> ctx);

  template <typename Cast = void, typename T>
  void assign_if_defined(const std::optional<T> &value) {
    if (!value) {
      return;
    }

    if constexpr (std::is_void_v<Cast>) {
      *this = *value;
    } else {
      *this = static_cast<Cast>(*value);
    }
  }

  template <typename Source, typename T>
  void assign_if_kind(std::optional<T> &value,
                      SerializationTypeKind expected_kind) {
    if (kind() != expected_kind) {
      return;
    }

    if constexpr (std::is_same_v<Source, T>) {
      value = as<Source>();
    } else {
      value = static_cast<T>(as<Source>());
    }
  }

  template <typename T> T as();

  SerializationTypeKind kind();

  size_t size();
  std::vector<std::string> object_keys();

private:
  Scope<SerializationContext> m_serialization_ctx;
  std::string m_key;
};

} // namespace astralix
