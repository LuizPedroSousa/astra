#pragma once

#include "shader-lang/ast.hpp"
#include "shader-lang/lowering/glsl-lowering.hpp"
#include "shader-lang/pipeline-layout.hpp"

namespace astralix {

void annotate_vulkan_layouts(
    GLSLStage &stage, const ShaderPipelineLayout &layout, StageKind stage_kind
);

} // namespace astralix
