#pragma once

#include "serialization-context.hpp"
#include "shader-lang/reflection.hpp"
#include <optional>
#include <string>

namespace astralix {

struct ReflectionIRArtifact {
  SerializationFormat format = SerializationFormat::Json;
  std::string extension;
  std::string content;
};

class ReflectionIREmitter {
public:
  std::optional<ReflectionIRArtifact>
  emit(const ShaderReflection &reflection,
       SerializationFormat format = SerializationFormat::Json,
       std::string *error = nullptr);
};

} // namespace astralix
