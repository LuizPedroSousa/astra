#pragma once

#include "shader-lang/pipeline-layout.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astralix {

struct LayoutMergeState {
  std::unordered_map<std::string, std::string> value_block_sources;
  std::unordered_map<std::string, std::string> value_field_sources;
  std::unordered_map<std::string, std::string> resource_sources;
  std::unordered_map<std::string, std::string> attribute_sources;
};

bool merge_pipeline_layout_checked(
    ShaderPipelineLayout &merged,
    const ShaderPipelineLayout &incoming,
    std::string_view incoming_source,
    LayoutMergeState &state,
    std::vector<std::string> &errors
);

} // namespace astralix
