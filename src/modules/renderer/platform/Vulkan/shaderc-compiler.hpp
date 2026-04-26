#pragma once

#include "shader-lang/ast.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace astralix {

struct ShadercCompileResult {
  std::vector<uint32_t> spirv;
  std::vector<std::string> errors;
  bool ok() const { return errors.empty(); }
};

ShadercCompileResult compile_glsl_to_spirv(std::string_view glsl_source,
                                           StageKind stage,
                                           std::string_view filename = "shader");

} // namespace astralix
