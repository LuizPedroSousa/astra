#include "shader-lang/emitters/pipeline-layout-ir-emitter.hpp"

#include "shader-lang/pipeline-layout-serializer.hpp"

namespace astralix {

std::optional<PipelineLayoutIRArtifact>
PipelineLayoutIREmitter::emit(const ShaderPipelineLayout &layout,
                              SerializationFormat format,
                              std::string *error) {
  auto content = serialize_shader_pipeline_layout(layout, format, error);
  if (!content) {
    return std::nullopt;
  }

  auto ctx = SerializationContext::create(format);

  return PipelineLayoutIRArtifact{
      .format = format,
      .extension = ctx->extension(),
      .content = std::move(*content),
  };
}

} // namespace astralix
