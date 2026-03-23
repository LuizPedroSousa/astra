#include "serialization-context.hpp"

#if defined(ASTRALIX_SERIALIZATION_ENABLE_JSON)
#include "adapters/json/json-serialization-context.hpp"
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_TOML)
#include "adapters/toml/toml-serialization-context.hpp"
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_XML)
#include "adapters/xml/xml-serialization-context.hpp"
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_YAML)
#include "adapters/yaml/yaml-serialization-context.hpp"
#endif

#include "assert.hpp"
#include "stream-buffer.hpp"

namespace astralix {

Ref<SerializationContext>
SerializationContext::create(SerializationFormat format) {
  switch (format) {
#if defined(ASTRALIX_SERIALIZATION_ENABLE_JSON)
  case astralix::SerializationFormat::Json:
    return create_ref<JsonSerializationContext>();
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_YAML)
  case astralix::SerializationFormat::Yaml:
    return create_ref<YamlSerializationContext>();
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_TOML)
  case astralix::SerializationFormat::Toml:
    return create_ref<TomlSerializationContext>();
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_XML)
  case astralix::SerializationFormat::Xml:
    return create_ref<XmlSerializationContext>();
#endif
  default:
    break;
  }

  ASTRA_EXCEPTION("Serialization format not implemented");
}

Ref<SerializationContext>
SerializationContext::create(SerializationFormat format,
                             Scope<StreamBuffer> buffer) {
  switch (format) {
#if defined(ASTRALIX_SERIALIZATION_ENABLE_JSON)
  case astralix::SerializationFormat::Json:
    return create_ref<JsonSerializationContext>(std::move(buffer));
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_YAML)
  case astralix::SerializationFormat::Yaml:
    return create_ref<YamlSerializationContext>(std::move(buffer));
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_TOML)
  case astralix::SerializationFormat::Toml:
    return create_ref<TomlSerializationContext>(std::move(buffer));
#endif
#if defined(ASTRALIX_SERIALIZATION_ENABLE_XML)
  case astralix::SerializationFormat::Xml:
    return create_ref<XmlSerializationContext>(std::move(buffer));
#endif
  default:
    break;
  }

  ASTRA_EXCEPTION("Serialization format not implemented");
}

} // namespace astralix
