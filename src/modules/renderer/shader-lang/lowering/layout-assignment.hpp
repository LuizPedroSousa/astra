#pragma once

#include "shader-lang/pipeline-layout.hpp"
#include "shader-lang/reflection.hpp"
#include <set>
#include <unordered_map>
#include <vector>

namespace astralix {

struct LayoutAssignmentResult {
  StageReflection reflection;
  ShaderPipelineLayout layout;
  std::vector<std::string> errors;

  bool ok() const { return errors.empty(); }
};

class LayoutAssignment {
public:
  LayoutAssignmentResult assign(const StageReflection &reflection);

private:
  std::set<std::pair<uint32_t, uint32_t>> m_cross_stage_reserved_slots;
  std::unordered_map<std::string, uint32_t> m_cross_stage_resource_bindings;
  std::unordered_map<std::string, uint32_t> m_cross_stage_block_bindings;
  std::unordered_map<std::string, std::vector<ShaderValueFieldDesc>>
      m_cross_stage_value_block_fields;
};

} // namespace astralix
