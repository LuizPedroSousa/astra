#pragma once

#include "serialization-context.hpp"
#include "shader-lang/pipeline-layout.hpp"
#include <optional>
#include <string>

namespace astralix {

struct PipelineLayoutIRArtifact {
  SerializationFormat format = SerializationFormat::Json;
  std::string extension;
  std::string content;
};

class PipelineLayoutIREmitter {
public:
  std::optional<PipelineLayoutIRArtifact>
  emit(const ShaderPipelineLayout &layout,
       SerializationFormat format = SerializationFormat::Json,
       std::string *error = nullptr);
};

} // namespace astralix
