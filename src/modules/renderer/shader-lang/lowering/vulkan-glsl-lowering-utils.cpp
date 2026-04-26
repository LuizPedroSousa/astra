#include "shader-lang/lowering/vulkan-glsl-lowering-utils.hpp"

#include <algorithm>
#include <cctype>

namespace astralix {

bool is_vulkan_sampler_type_kind(TokenKind kind) {
  switch (kind) {
    case TokenKind::TypeSampler2D:
    case TokenKind::TypeSamplerCube:
    case TokenKind::TypeSampler2DShadow:
    case TokenKind::TypeIsampler2D:
    case TokenKind::TypeUsampler2D:
      return true;
    default:
      return false;
  }
}

bool has_annotation(const Annotations &annotations, AnnotationKind kind) {
  return std::any_of(annotations.begin(), annotations.end(), [kind](const Annotation &annotation) {
    return annotation.kind == kind;
  });
}

void set_binding_annotations(
    Annotations &annotations,
    uint32_t descriptor_set,
    uint32_t binding
) {
  bool has_set = false;
  bool has_binding = false;

  for (auto &annotation : annotations) {
    if (annotation.kind == AnnotationKind::Set) {
      annotation.slot = static_cast<int32_t>(descriptor_set);
      has_set = true;
    }
    if (annotation.kind == AnnotationKind::Binding) {
      annotation.slot = static_cast<int32_t>(binding);
      has_binding = true;
    }
  }

  if (!has_set) {
    Annotation set_annotation{};
    set_annotation.kind = AnnotationKind::Set;
    set_annotation.slot = static_cast<int32_t>(descriptor_set);
    annotations.insert(annotations.begin(), set_annotation);
  }

  if (!has_binding) {
    Annotation binding_annotation{};
    binding_annotation.kind = AnnotationKind::Binding;
    binding_annotation.slot = static_cast<int32_t>(binding);
    annotations.push_back(binding_annotation);
  }
}

void set_location_annotation(Annotations &annotations, uint32_t location) {
  for (auto &annotation : annotations) {
    if (annotation.kind == AnnotationKind::Location) {
      annotation.slot = static_cast<int32_t>(location);
      return;
    }
  }

  Annotation location_annotation{};
  location_annotation.kind = AnnotationKind::Location;
  location_annotation.slot = static_cast<int32_t>(location);
  annotations.insert(annotations.begin(), location_annotation);
}

std::string strip_array_suffix(std::string_view name) {
  const auto bracket = name.find('[');
  if (bracket == std::string::npos) {
    return std::string(name);
  }
  return std::string(name.substr(0, bracket));
}

std::string extract_leaf_name(std::string_view dotted_name) {
  auto last_dot = dotted_name.rfind('.');
  if (last_dot == std::string::npos) {
    return std::string(dotted_name);
  }
  return std::string(dotted_name.substr(last_dot + 1));
}

std::string extract_first_level(std::string_view dotted_name) {
  auto first_dot = dotted_name.find('.');
  if (first_dot == std::string::npos) {
    return std::string(dotted_name);
  }
  auto second_dot = dotted_name.find('.', first_dot + 1);
  if (second_dot == std::string::npos) {
    return std::string(dotted_name.substr(first_dot + 1));
  }
  return std::string(
      dotted_name.substr(first_dot + 1, second_dot - first_dot - 1)
  );
}

std::string extract_block_prefix(std::string_view dotted_name) {
  auto first_dot = dotted_name.find('.');
  if (first_dot == std::string::npos) {
    return "";
  }
  return std::string(dotted_name.substr(0, first_dot));
}

std::string sanitize_block_field_name(std::string_view name) {
  std::string sanitized;
  sanitized.reserve(name.size() + 8);

  for (char ch : name) {
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_') {
      sanitized.push_back(ch);
    } else {
      sanitized.push_back('_');
    }
  }

  if (sanitized.empty()) {
    sanitized = "field";
  }

  if (std::isdigit(static_cast<unsigned char>(sanitized.front())) != 0) {
    sanitized.insert(sanitized.begin(), '_');
  }

  return sanitized;
}

VulkanBlockFieldLogicalInfo
describe_block_field(std::string_view logical_name) {
  VulkanBlockFieldLogicalInfo info;
  info.first_level = extract_first_level(logical_name);

  auto first_dot = logical_name.find('.');
  if (first_dot != std::string::npos) {
    auto second_dot = logical_name.find('.', first_dot + 1);
    if (second_dot != std::string::npos) {
      info.remainder = std::string(logical_name.substr(second_dot));
    }
  }

  info.aliases.push_back(info.first_level);
  const std::string stripped_first_level = strip_array_suffix(info.first_level);
  if (stripped_first_level != info.first_level) {
    info.aliases.push_back(stripped_first_level);
  }

  const std::string prefix = extract_block_prefix(logical_name);
  if (!prefix.empty()) {
    info.aliases.push_back(prefix + "_" + info.first_level);
    if (stripped_first_level != info.first_level) {
      info.aliases.push_back(prefix + "_" + stripped_first_level);
    }
  }

  return info;
}

bool alias_matches(const std::vector<std::string> &aliases,
                   std::string_view candidate) {
  return std::any_of(aliases.begin(), aliases.end(), [&](const std::string &alias) {
    return alias == candidate;
  });
}

bool remainder_matches_child(std::string_view remainder,
                             std::string_view child_name) {
  if (remainder.empty()) {
    return false;
  }

  const std::string child_prefix = "." + std::string(child_name);
  if (remainder == child_prefix) {
    return true;
  }

  return remainder.rfind(child_prefix + ".", 0) == 0 ||
         remainder.rfind(child_prefix + "[", 0) == 0;
}

std::string make_unused_block_field_name(
    std::string_view logical_name, std::unordered_set<std::string> &used_names
) {
  std::string base = "_unused_" + sanitize_block_field_name(logical_name);
  std::string candidate = base;
  uint32_t suffix = 0;

  while (!used_names.insert(candidate).second) {
    candidate = base + "_" + std::to_string(++suffix);
  }

  return candidate;
}

} // namespace astralix
