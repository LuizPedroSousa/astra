#pragma once

#include "shader-lang/artifacts/artifact-types.hpp"

namespace astralix {

class ShaderArtifactPipeline {
public:
  ShaderArtifactPlan
  build_plan(const std::vector<ShaderArtifactInput> &inputs, const ShaderArtifactBuildOptions &options = {});
};

} // namespace astralix
