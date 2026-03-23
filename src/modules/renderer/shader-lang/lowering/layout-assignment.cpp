#include "shader-lang/lowering/layout-assignment.hpp"

namespace astralix {

LayoutAssignmentResult
LayoutAssignment::assign(const StageReflection &reflection) const {
  LayoutAssignmentResult result;
  result.reflection = reflection;

  for (auto &resource : result.reflection.resources) {
    if (!resource.glsl.descriptor_set) {
      resource.glsl.descriptor_set = 0;
    }
  }

  return result;
}

} // namespace astralix
