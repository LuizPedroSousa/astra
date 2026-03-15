#pragma once
#include "shader-lang/ast.hpp"
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace astralix {

struct CompileResult {
  std::map<StageKind, std::string> stages;
  std::vector<std::string> errors;
  bool ok() const { return errors.empty(); }
};

class Compiler {
public:
  CompileResult compile(std::string_view source,
                        std::string_view base_path = {},
                        std::string_view filename = {});
};

} // namespace astralix
