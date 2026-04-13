#pragma once

#include "shader-lang/ast.hpp"
#include "shader-lang/lowering/glsl-lowering.hpp"
#include "shader-lang/pipeline-layout.hpp"

namespace astralix {

class VulkanTargetLowering {
public:
  void lower(
      GLSLStage &stage, const ShaderPipelineLayout &layout, StageKind stage_kind
  ) const;
};

} // namespace astralix
