#pragma once

#include "shader-lang/lowering/glsl-lowering.hpp"
#include "shader-lang/pipeline-layout.hpp"

#include <string>
#include <unordered_map>

namespace astralix {

struct VulkanResourceLegalizationResult {
  std::unordered_map<std::string, std::string> field_rewrites;
};

VulkanResourceLegalizationResult
legalize_vulkan_resources(GLSLStage &stage, const ShaderPipelineLayout &layout);

} // namespace astralix
