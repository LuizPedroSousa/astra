#pragma once

#include "serialization-context.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace astralix {

enum class ShaderUnitArtifactKind {
  BindingCppHeader,
  ReflectionIR,
  PipelineLayoutIR,
  CacheMetadata,
};

enum class ShaderBatchArtifactKind {
  UmbrellaHeader,
};

struct ShaderUnitArtifactSpec {
  ShaderUnitArtifactKind kind = ShaderUnitArtifactKind::BindingCppHeader;
  SerializationFormat format = SerializationFormat::Json;
};

struct ShaderBatchArtifactSpec {
  ShaderBatchArtifactKind kind = ShaderBatchArtifactKind::UmbrellaHeader;
};

struct ShaderArtifactInput {
  std::string canonical_id;
  std::filesystem::path source_path;
  std::filesystem::path output_root;
  std::string umbrella_name;
};

struct ShaderArtifactBuildOptions {
  std::vector<ShaderUnitArtifactSpec> unit_artifacts = {
      {.kind = ShaderUnitArtifactKind::BindingCppHeader},
      {.kind = ShaderUnitArtifactKind::PipelineLayoutIR},
      {.kind = ShaderUnitArtifactKind::CacheMetadata},
  };
  std::vector<ShaderBatchArtifactSpec> batch_artifacts = {
      {.kind = ShaderBatchArtifactKind::UmbrellaHeader},
  };
  bool use_cache = true;
  bool prune_stale = true;
  bool preserve_last_good_outputs = true;
  bool memoize_compiles = true;
};

struct ShaderPlannedWrite {
  std::filesystem::path path;
  std::string content;
};

struct ShaderArtifactFailure {
  std::string canonical_id;
  std::filesystem::path source_path;
  std::string message;
};

struct ShaderArtifactPlan {
  int total_shaders = 0;
  int generated_shaders = 0;
  int unchanged_shaders = 0;
  int failed_shaders = 0;
  int planned_removals = 0;
  std::vector<std::filesystem::path> watched_paths;
  std::vector<ShaderPlannedWrite> writes;
  std::vector<std::filesystem::path> deletes;
  std::vector<ShaderArtifactFailure> failures;

  bool ok() const { return failed_shaders == 0; }
};

std::string sanitize_generated_shader_name(std::string_view canonical_id);

} // namespace astralix
