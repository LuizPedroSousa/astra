#pragma once

#include "shader-lang/lowering/glsl-lowering.hpp"

namespace astralix {

void normalize_vulkan_fragment_coordinates(GLSLStage &stage);
void normalize_vulkan_vertex_clip_space(GLSLStage &stage);

} // namespace astralix
