#include "shader-lang/lowering/vulkan-target-lowering.hpp"

#include "shader-lang/lowering/glsl-ast-rewrite.hpp"
#include "shader-lang/lowering/vulkan-glsl-builtin-pass.hpp"
#include "shader-lang/lowering/vulkan-glsl-coordinate-pass.hpp"
#include "shader-lang/lowering/vulkan-glsl-layout-pass.hpp"
#include "shader-lang/lowering/vulkan-glsl-resource-pass.hpp"

namespace astralix {

void VulkanTargetLowering::lower(
    GLSLStage &stage,
    const ShaderPipelineLayout &layout,
    StageKind stage_kind
) const {
  annotate_vulkan_layouts(stage, layout, stage_kind);

  auto legalization = legalize_vulkan_resources(stage, layout);

  if (stage_kind == StageKind::Fragment) {
    normalize_vulkan_fragment_coordinates(stage);
  }

  rename_vulkan_builtins(stage);
  remap_struct_field_accesses(stage, legalization.field_rewrites);

  if (stage_kind == StageKind::Vertex) {
    normalize_vulkan_vertex_clip_space(stage);
  }
}

} // namespace astralix
