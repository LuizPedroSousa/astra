#pragma once

#include "serialization-context.hpp"
#include "shader-lang/reflection.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace astralix {

std::filesystem::path shader_reflection_sidecar_path(
    const std::filesystem::path &source_path,
    SerializationFormat format = SerializationFormat::Json);

std::optional<std::string> serialize_shader_reflection(
    const ShaderReflection &reflection,
    SerializationFormat format = SerializationFormat::Json,
    std::string *error = nullptr);

std::optional<ShaderReflection> deserialize_shader_reflection(
    std::string_view content,
    SerializationFormat format = SerializationFormat::Json,
    std::string *error = nullptr);

std::optional<ShaderReflection>
read_shader_reflection(const std::filesystem::path &path,
                       SerializationFormat format = SerializationFormat::Json,
                       std::string *error = nullptr);

} // namespace astralix
