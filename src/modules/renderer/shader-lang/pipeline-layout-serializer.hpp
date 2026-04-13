#pragma once

#include "serialization-context.hpp"
#include "shader-lang/pipeline-layout.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace astralix {

std::filesystem::path shader_layout_sidecar_path(
    const std::filesystem::path &source_path,
    SerializationFormat format = SerializationFormat::Json);

std::optional<std::string> serialize_shader_pipeline_layout(
    const ShaderPipelineLayout &layout,
    SerializationFormat format = SerializationFormat::Json,
    std::string *error = nullptr);

std::optional<ShaderPipelineLayout> deserialize_shader_pipeline_layout(
    std::string_view content,
    SerializationFormat format = SerializationFormat::Json,
    std::string *error = nullptr);

} // namespace astralix
