#include "shader-lang/lowering/vulkan-glsl-resource-pass.hpp"

#include "shader-lang/lowering/vulkan-glsl-lowering-utils.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace astralix {

VulkanResourceLegalizationResult
legalize_vulkan_resources(GLSLStage &stage, const ShaderPipelineLayout &layout) {
  struct PendingBlockFieldDecl {
    size_t declaration_index = 0;
    std::vector<const ShaderValueFieldDesc *> layout_fields;
    uint32_t first_offset = std::numeric_limits<uint32_t>::max();
  };

  std::unordered_map<std::string, VulkanBindingLocation> block_bindings;
  std::unordered_map<std::string, const ShaderValueBlockDesc *>
      layout_blocks_by_name;
  for (const auto &block : layout.resource_layout.value_blocks) {
    layout_blocks_by_name[block.logical_name] = &block;
    if (block.descriptor_set && block.binding) {
      block_bindings[block.logical_name] = {*block.descriptor_set, *block.binding};
    }
  }

  std::unordered_map<std::string, std::string> field_name_to_block;
  for (const auto &[block_name, _] : block_bindings) {
    for (const auto &block : layout.resource_layout.value_blocks) {
      if (block.logical_name != block_name) {
        continue;
      }
      for (const auto &field : block.fields) {
        std::string first_level = extract_first_level(field.logical_name);
        if (!first_level.empty()) {
          field_name_to_block[first_level] = block_name;
          const std::string stripped_first_level =
              strip_array_suffix(first_level);
          if (!stripped_first_level.empty()) {
            field_name_to_block[stripped_first_level] = block_name;
          }
          std::string prefix = extract_block_prefix(field.logical_name);
          if (!prefix.empty()) {
            field_name_to_block[prefix + "_" + first_level] = block_name;
            if (!stripped_first_level.empty()) {
              field_name_to_block[prefix + "_" + stripped_first_level] =
                  block_name;
            }
          }
        }
      }
    }
  }

  std::unordered_map<std::string, VulkanBindingLocation> resource_leaf_bindings;
  for (const auto &resource : layout.resource_layout.resources) {
    std::string leaf = extract_leaf_name(resource.logical_name);
    VulkanBindingLocation location = {resource.descriptor_set, resource.binding};
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

  struct StructFieldInfo {
    std::string name;
    TypeRef type;
  };
  std::unordered_map<std::string, std::vector<StructFieldInfo>>
      struct_field_infos;
  for (const auto &declaration : stage.declarations) {
    auto *struct_decl = std::get_if<GLSLStructDecl>(&declaration);
    if (struct_decl) {
      auto &field_infos = struct_field_infos[struct_decl->name];
      for (const auto &field : struct_decl->fields) {
        field_infos.push_back({field.name, field.type});
      }
    }
  }

  std::vector<GLSLDecl> patched_declarations;
  std::unordered_map<std::string, std::vector<PendingBlockFieldDecl>>
      pending_block_fields;
  std::unordered_map<std::string, std::string> struct_field_rewrites;

  auto queue_block_decl =
      [&](const std::string &block_name, size_t declaration_index, auto matcher) {
        auto layout_it = layout_blocks_by_name.find(block_name);
        if (layout_it == layout_blocks_by_name.end()) {
          return;
        }

        PendingBlockFieldDecl pending;
        pending.declaration_index = declaration_index;
        for (const auto &field : layout_it->second->fields) {
          if (!matcher(field)) {
            continue;
          }

          pending.layout_fields.push_back(&field);
          pending.first_offset = std::min(pending.first_offset, field.offset);
        }

        pending_block_fields[block_name].push_back(std::move(pending));
      };

  for (auto &declaration : stage.declarations) {
    bool handled = false;

    std::visit(
        [&](auto &decl) {
          using T = std::decay_t<decltype(decl)>;

          if constexpr (std::is_same_v<T, GLSLInterfaceBlockDecl>) {
            if (decl.storage == "buffer") {
              decl.storage = "readonly buffer";
            }
          }

          if constexpr (std::is_same_v<T, GLSLGlobalVarDecl>) {
            if (decl.storage == "uniform") {
              if (is_vulkan_sampler_type_kind(decl.type.kind)) {
                return;
              }

              if (decl.type.kind == TokenKind::Identifier) {
                auto struct_it = struct_field_infos.find(decl.type.name);
                if (struct_it != struct_field_infos.end()) {
                  bool has_sampler_fields = std::any_of(
                      struct_it->second.begin(),
                      struct_it->second.end(),
                      [](const StructFieldInfo &info) {
                        return is_vulkan_sampler_type_kind(info.type.kind);
                      }
                  );

                  if (has_sampler_fields) {
                    std::string parent_bare_name =
                        (!decl.name.empty() && decl.name[0] == '_')
                            ? decl.name.substr(1)
                            : decl.name;
                    std::string target_block_name;
                    auto parent_block_it =
                        field_name_to_block.find(parent_bare_name);
                    if (parent_block_it != field_name_to_block.end()) {
                      target_block_name = parent_block_it->second;
                    }

                    for (const auto &field : struct_it->second) {
                      std::string new_name = decl.name + "_" + field.name;
                      std::string bare_field = new_name;
                      if (!bare_field.empty() && bare_field[0] == '_') {
                        bare_field = bare_field.substr(1);
                      }

                      struct_field_rewrites[decl.name + "." + field.name] =
                          new_name;

                      if (is_vulkan_sampler_type_kind(field.type.kind)) {
                        GLSLGlobalVarDecl sampler_var;
                        sampler_var.type = field.type;
                        sampler_var.name = new_name;
                        sampler_var.array_size = decl.array_size;
                        sampler_var.storage = "uniform";
                        auto leaf_it = resource_leaf_bindings.find(bare_field);
                        if (leaf_it != resource_leaf_bindings.end()) {
                          set_binding_annotations(
                              sampler_var.annotations,
                              leaf_it->second.descriptor_set,
                              leaf_it->second.binding
                          );
                        }
                        patched_declarations.push_back(std::move(sampler_var));
                      } else {
                        size_t target_index = patched_declarations.size();
                        if (!target_block_name.empty()) {
                          queue_block_decl(
                              target_block_name,
                              target_index,
                              [&](const ShaderValueFieldDesc &layout_field) {
                                const auto info = describe_block_field(
                                    layout_field.logical_name
                                );
                                return alias_matches(
                                           info.aliases, parent_bare_name
                                       ) &&
                                       remainder_matches_child(
                                           info.remainder, field.name
                                       );
                              }
                          );
                        }
                        GLSLGlobalVarDecl value_var;
                        value_var.type = field.type;
                        value_var.name = new_name;
                        value_var.array_size = decl.array_size;
                        value_var.storage = "uniform";
                        patched_declarations.push_back(std::move(value_var));
                      }
                    }
                    handled = true;
                    return;
                  }
                }
              }

              std::string bare_name =
                  (!decl.name.empty() && decl.name[0] == '_')
                      ? decl.name.substr(1)
                      : decl.name;
              auto field_it = field_name_to_block.find(bare_name);
              if (field_it != field_name_to_block.end()) {
                queue_block_decl(
                    field_it->second,
                    patched_declarations.size(),
                    [&](const ShaderValueFieldDesc &layout_field) {
                      return alias_matches(
                          describe_block_field(layout_field.logical_name).aliases,
                          bare_name
                      );
                    }
                );
              }
            }
          }
        },
        declaration
    );

    if (!handled) {
      patched_declarations.push_back(std::move(declaration));
    }
  }

  std::unordered_map<std::string, GLSLInterfaceBlockDecl> ubo_blocks;
  std::unordered_set<size_t> indices_to_remove;

  for (const auto &[block_name, field_entries] : pending_block_fields) {
    auto block_it = block_bindings.find(block_name);
    if (block_it == block_bindings.end()) {
      continue;
    }
    auto layout_it = layout_blocks_by_name.find(block_name);
    if (layout_it == layout_blocks_by_name.end()) {
      continue;
    }

    GLSLInterfaceBlockDecl ubo_block;
    ubo_block.storage = "uniform";
    ubo_block.block_name = "_UBO_" + block_name;

    Annotation std140_annotation{};
    std140_annotation.kind = AnnotationKind::Std140;
    ubo_block.annotations.push_back(std140_annotation);
    set_binding_annotations(
        ubo_block.annotations,
        block_it->second.descriptor_set,
        block_it->second.binding
    );

    std::vector<PendingBlockFieldDecl> ordered_entries = field_entries;
    std::sort(
        ordered_entries.begin(),
        ordered_entries.end(),
        [](const PendingBlockFieldDecl &lhs, const PendingBlockFieldDecl &rhs) {
          if (lhs.first_offset != rhs.first_offset) {
            return lhs.first_offset < rhs.first_offset;
          }
          return lhs.declaration_index < rhs.declaration_index;
        }
    );

    std::unordered_set<std::string> covered_layout_fields;
    std::unordered_set<std::string> used_field_names;

    for (const auto &entry : ordered_entries) {
      auto *var_decl =
          std::get_if<GLSLGlobalVarDecl>(&patched_declarations[entry.declaration_index]);
      if (var_decl) {
        used_field_names.insert(var_decl->name);
      }
    }

    auto append_dummy_field = [&](const ShaderValueFieldDesc &layout_field) {
      if (!covered_layout_fields.insert(layout_field.logical_name).second) {
        return;
      }

      GLSLFieldDecl field;
      field.type = layout_field.type;
      field.name = make_unused_block_field_name(
          layout_field.logical_name, used_field_names
      );
      ubo_block.fields.push_back(std::move(field));
    };

    auto append_uncovered_prefix = [&](uint32_t limit_offset) {
      for (const auto &layout_field : layout_it->second->fields) {
        if (layout_field.offset >= limit_offset) {
          break;
        }

        append_dummy_field(layout_field);
      }
    };

    for (const auto &entry : ordered_entries) {
      if (entry.first_offset != std::numeric_limits<uint32_t>::max()) {
        append_uncovered_prefix(entry.first_offset);
      }

      auto *var_decl =
          std::get_if<GLSLGlobalVarDecl>(&patched_declarations[entry.declaration_index]);
      if (!var_decl) {
        continue;
      }

      GLSLFieldDecl field;
      field.type = var_decl->type;
      field.name = var_decl->name;
      field.array_size = var_decl->array_size;
      ubo_block.fields.push_back(std::move(field));
      indices_to_remove.insert(entry.declaration_index);

      for (const auto *layout_field : entry.layout_fields) {
        covered_layout_fields.insert(layout_field->logical_name);
      }
    }

    for (const auto &layout_field : layout_it->second->fields) {
      append_dummy_field(layout_field);
    }

    ubo_blocks[block_name] = std::move(ubo_block);
  }

  std::unordered_map<size_t, std::string> first_index_per_block;
  for (const auto &[block_name, field_entries] : pending_block_fields) {
    if (ubo_blocks.find(block_name) == ubo_blocks.end()) {
      continue;
    }

    size_t earliest = std::numeric_limits<size_t>::max();
    for (const auto &entry : field_entries) {
      earliest = std::min(earliest, entry.declaration_index);
    }

    if (earliest != std::numeric_limits<size_t>::max()) {
      first_index_per_block[earliest] = block_name;
    }
  }

  std::vector<GLSLDecl> final_declarations;
  for (size_t i = 0; i < patched_declarations.size(); ++i) {
    auto block_it = first_index_per_block.find(i);
    if (block_it != first_index_per_block.end()) {
      final_declarations.push_back(std::move(ubo_blocks.at(block_it->second)));
    }

    if (indices_to_remove.count(i) == 0) {
      final_declarations.push_back(std::move(patched_declarations[i]));
    }
  }

  stage.declarations = std::move(final_declarations);

  return VulkanResourceLegalizationResult{
      .field_rewrites = std::move(struct_field_rewrites),
  };
}

} // namespace astralix
