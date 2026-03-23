#pragma once

#include "shader-lang/artifacts/artifact-types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace axgen {

struct ArtifactPlanApplyFailure {
  std::filesystem::path path;
  std::string message;
};

struct ArtifactPlanApplyResult {
  int written_files = 0;
  int removed_files = 0;
  std::vector<ArtifactPlanApplyFailure> failures;

  bool ok() const { return failures.empty(); }
};

ArtifactPlanApplyResult
apply_shader_artifact_plan(const astralix::ShaderArtifactPlan &plan);

std::string format_sync_summary(const astralix::ShaderArtifactPlan &plan,
                                const ArtifactPlanApplyResult &apply_result);

} // namespace axgen
