#pragma once

#include "shader-lang/reflection.hpp"
#include <vector>

namespace astralix {

struct LayoutAssignmentResult {
  StageReflection reflection;
  std::vector<std::string> errors;

  bool ok() const { return errors.empty(); }
};

class LayoutAssignment {
public:
  LayoutAssignmentResult assign(const StageReflection &reflection) const;
};

} // namespace astralix
