#pragma once

#include "shader-lang/ast.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace astralix {

struct VulkanBindingLocation {
  uint32_t descriptor_set = 0;
  uint32_t binding = 0;
};

struct VulkanBlockFieldLogicalInfo {
  std::string first_level;
  std::string remainder;
  std::vector<std::string> aliases;
};

bool is_vulkan_sampler_type_kind(TokenKind kind);
bool has_annotation(const Annotations &annotations, AnnotationKind kind);

void set_binding_annotations(
    Annotations &annotations, uint32_t descriptor_set, uint32_t binding
);
void set_location_annotation(Annotations &annotations, uint32_t location);

std::string strip_array_suffix(std::string_view name);
std::string extract_leaf_name(std::string_view dotted_name);
std::string extract_first_level(std::string_view dotted_name);
std::string extract_block_prefix(std::string_view dotted_name);
std::string sanitize_block_field_name(std::string_view name);

VulkanBlockFieldLogicalInfo describe_block_field(std::string_view logical_name);
bool alias_matches(const std::vector<std::string> &aliases,
                   std::string_view candidate);
bool remainder_matches_child(std::string_view remainder,
                             std::string_view child_name);

std::string make_unused_block_field_name(
    std::string_view logical_name, std::unordered_set<std::string> &used_names
);

} // namespace astralix
