#pragma once

#include "arena.hpp"
#include "context-proxy.hpp"
#include "guid.hpp"
#include "stream-buffer.hpp"

#include <string>

namespace astralix {

enum SerializationFormat { Json };

enum class SerializationTypeKind {
  Unknown = 0,
  String = 1,
  Int = 2,
  Float = 3,
  Bool = 4,
  Array = 5,
  Object = 6,
};

class SerializationContext {
public:
  virtual ~SerializationContext() = default;
  virtual ContextProxy operator[](const SerializableKey &key) = 0;

  virtual void set_value(const SerializableValue &value) = 0;
  virtual void set_value(Ref<SerializationContext> ctx) = 0;

  virtual std::any get_root() = 0;

  virtual ElasticArena::Block *to_buffer(ElasticArena &arena) = 0;
  virtual void from_buffer(Scope<StreamBuffer> buffer) = 0;
  virtual size_t root_size() = 0;
  virtual size_t size() = 0;

  virtual std::string extension() const = 0;

  virtual std::string as_string() = 0;
  virtual int as_int() = 0;
  virtual float as_float() = 0;
  virtual bool as_bool() = 0;
  virtual std::vector<std::any> as_array() = 0;

  virtual bool is_string() = 0;
  virtual bool is_int() = 0;
  virtual bool is_float() = 0;
  virtual bool is_bool() = 0;
  virtual bool is_array() = 0;
  virtual bool is_object() = 0;

  virtual SerializationTypeKind kind() = 0;

  static Ref<SerializationContext> create(SerializationFormat format);
  static Ref<SerializationContext> create(SerializationFormat format,
                                          Scope<StreamBuffer> buffer);
};

} // namespace astralix
