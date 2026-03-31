#pragma once

#include "serialization-context.hpp"

#include <optional>
#include <string>
#include <utility>

namespace astralix::serialization::context {

inline std::optional<std::string> read_string(ContextProxy proxy) {
  if (proxy.kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  return proxy.as<std::string>();
}

inline std::optional<std::string>
read_string(Ref<SerializationContext> state, const std::string &key) {
  if (state == nullptr) {
    return std::nullopt;
  }

  return read_string((*state)[key]);
}

inline std::optional<int> read_int(ContextProxy proxy) {
  if (proxy.kind() != SerializationTypeKind::Int) {
    return std::nullopt;
  }

  return proxy.as<int>();
}

inline std::optional<int>
read_int(Ref<SerializationContext> state, const std::string &key) {
  if (state == nullptr) {
    return std::nullopt;
  }

  return read_int((*state)[key]);
}

inline std::optional<float> read_float(ContextProxy proxy) {
  const auto kind = proxy.kind();
  if (kind == SerializationTypeKind::Float) {
    return proxy.as<float>();
  }

  if (kind == SerializationTypeKind::Int) {
    return static_cast<float>(proxy.as<int>());
  }

  return std::nullopt;
}

inline std::optional<float>
read_float(Ref<SerializationContext> state, const std::string &key) {
  if (state == nullptr) {
    return std::nullopt;
  }

  return read_float((*state)[key]);
}

inline std::optional<bool> read_bool(ContextProxy proxy) {
  if (proxy.kind() != SerializationTypeKind::Bool) {
    return std::nullopt;
  }

  return proxy.as<bool>();
}

inline std::optional<bool>
read_bool(Ref<SerializationContext> state, const std::string &key) {
  if (state == nullptr) {
    return std::nullopt;
  }

  return read_bool((*state)[key]);
}

inline std::string
read_string_or(ContextProxy proxy, std::string fallback = std::string{}) {
  return read_string(std::move(proxy)).value_or(std::move(fallback));
}

inline int read_int_or(ContextProxy proxy, int fallback) {
  return read_int(std::move(proxy)).value_or(fallback);
}

inline float read_float_or(ContextProxy proxy, float fallback) {
  return read_float(std::move(proxy)).value_or(fallback);
}

inline bool read_bool_or(ContextProxy proxy, bool fallback) {
  return read_bool(std::move(proxy)).value_or(fallback);
}

} // namespace astralix::serialization::context
