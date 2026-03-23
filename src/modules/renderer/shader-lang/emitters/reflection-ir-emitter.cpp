#include "shader-lang/emitters/reflection-ir-emitter.hpp"

#include "shader-lang/reflection-serializer.hpp"

namespace astralix {

std::optional<ReflectionIRArtifact>
ReflectionIREmitter::emit(const ShaderReflection &reflection,
                          SerializationFormat format, std::string *error) {
  auto content = serialize_shader_reflection(reflection, format, error);
  if (!content) {
    return std::nullopt;
  }

  auto ctx = SerializationContext::create(format);
  return ReflectionIRArtifact{
      .format = format,
      .extension = ctx->extension(),
      .content = std::move(*content),
  };
}

} // namespace astralix
