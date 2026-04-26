#include "shader-lang/lowering/vulkan-glsl-layout-pass.hpp"

#include "shader-lang/lowering/vulkan-glsl-lowering-utils.hpp"

#include <type_traits>
#include <unordered_map>

namespace astralix {

void annotate_vulkan_layouts(
    GLSLStage &stage, const ShaderPipelineLayout &layout, StageKind stage_kind
) {
  (void)stage_kind;

  std::unordered_map<std::string, VulkanBindingLocation> resource_bindings;
  for (const auto &resource : layout.resource_layout.resources) {
    resource_bindings[resource.logical_name] = {
        resource.descriptor_set, resource.binding
    };
  }

  std::unordered_map<std::string, VulkanBindingLocation> block_bindings;
  for (const auto &block : layout.resource_layout.value_blocks) {
    if (block.descriptor_set && block.binding) {
      block_bindings[block.logical_name] = {
          *block.descriptor_set, *block.binding
      };
    }
  }

  std::unordered_map<std::string, VulkanBindingLocation> resource_leaf_bindings;
  for (const auto &resource : layout.resource_layout.resources) {
    std::string leaf = extract_leaf_name(resource.logical_name);
    VulkanBindingLocation location = {
        resource.descriptor_set, resource.binding
    };
    resource_leaf_bindings[leaf] = location;

    std::string prefix = extract_block_prefix(resource.logical_name);
    if (!prefix.empty()) {
      resource_leaf_bindings[prefix + "_" + leaf] = location;
    }

    auto first_dot = resource.logical_name.find('.');
    if (first_dot != std::string::npos) {
      std::string without_prefix = resource.logical_name.substr(first_dot + 1);
      std::string underscore_name;
      for (char c : without_prefix) {
        underscore_name += (c == '.') ? '_' : c;
      }
      resource_leaf_bindings[underscore_name] = location;
    }
  }

  uint32_t next_in_location = 0;
  uint32_t next_out_location = 0;

  for (auto &declaration : stage.declarations) {
    std::visit(
        [&](auto &decl) {
          using T = std::decay_t<decltype(decl)>;

          if constexpr (std::is_same_v<T, GLSLInterfaceBlockDecl>) {
            if (decl.storage == "uniform" || decl.storage == "buffer") {
              auto find_binding =
                  [&](const std::string &name) -> const VulkanBindingLocation * {
                auto block_it = block_bindings.find(name);
                if (block_it != block_bindings.end()) {
                  return &block_it->second;
                }
                auto resource_it = resource_bindings.find(name);
                if (resource_it != resource_bindings.end()) {
                  return &resource_it->second;
                }
                return nullptr;
              };

              const VulkanBindingLocation *location = find_binding(decl.block_name);
              if (!location && decl.instance_name) {
                location = find_binding(*decl.instance_name);
              }

              if (location) {
                set_binding_annotations(
                    decl.annotations, location->descriptor_set, location->binding
                );
              }
            }

            if (decl.storage == "in") {
              if (!has_annotation(decl.annotations, AnnotationKind::Location)) {
                set_location_annotation(decl.annotations, next_in_location);
              }
              next_in_location += static_cast<uint32_t>(decl.fields.size());
            }

            if (decl.storage == "out") {
              if (!has_annotation(decl.annotations, AnnotationKind::Location)) {
                set_location_annotation(decl.annotations, next_out_location);
              }
              next_out_location += static_cast<uint32_t>(decl.fields.size());
            }
          }

          if constexpr (std::is_same_v<T, GLSLGlobalVarDecl>) {
            if (decl.storage == "in") {
              if (!has_annotation(decl.annotations, AnnotationKind::Location)) {
                set_location_annotation(decl.annotations, next_in_location);
              }
              next_in_location++;
            }

            if (decl.storage == "out") {
              if (!has_annotation(decl.annotations, AnnotationKind::Location)) {
                set_location_annotation(decl.annotations, next_out_location);
              }
              next_out_location++;
            }

            if (decl.storage == "uniform" &&
                is_vulkan_sampler_type_kind(decl.type.kind)) {
              std::string bare_name =
                  (!decl.name.empty() && decl.name[0] == '_')
                      ? decl.name.substr(1)
                      : decl.name;
              auto leaf_it = resource_leaf_bindings.find(bare_name);
              if (leaf_it != resource_leaf_bindings.end()) {
                set_binding_annotations(
                    decl.annotations,
                    leaf_it->second.descriptor_set,
                    leaf_it->second.binding
                );
              }
            }
          }
        },
        declaration
    );
  }
}

} // namespace astralix
