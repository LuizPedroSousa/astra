#pragma once

#include "shader-lang/lowering/glsl-lowering.hpp"

#include <string>
#include <unordered_map>

namespace astralix {

void remap_struct_fields_in_stmt(
    GLSLStmt &stmt,
    const std::unordered_map<std::string, std::string> &field_rewrites
);

void remap_struct_field_accesses(
    GLSLStage &stage,
    const std::unordered_map<std::string, std::string> &field_rewrites
);

} // namespace astralix
