#include "shader-lang/emitters/binding-cpp-emitter.hpp"

#include <cctype>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astralix {

namespace {

struct MergedUniformInterface {
  std::string logical_name;
  std::string declared_name;
  std::vector<DeclaredFieldReflection> declared_fields;
};

struct PathSegment {
  std::string name;
  std::optional<uint32_t> array_size;
};

struct LeafInfo {
  const DeclaredFieldReflection *field = nullptr;
  std::vector<PathSegment> path;
  std::vector<std::string> tag_segments;
};

std::string indent(int depth) { return std::string(depth * 2, ' '); }

void emit_line(std::ostringstream &out, int depth, const std::string &line) {
  out << indent(depth) << line << '\n';
}

std::string sanitize_identifier(std::string_view value) {
  std::string sanitized;
  sanitized.reserve(value.size() + 1);

  for (char ch : value) {
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_') {
      sanitized.push_back(ch);
    } else {
      sanitized.push_back('_');
    }
  }

  if (sanitized.empty()) {
    return "_";
  }

  if (std::isdigit(static_cast<unsigned char>(sanitized.front())) != 0) {
    sanitized.insert(sanitized.begin(), '_');
  }

  return sanitized;
}

std::string to_pascal_case(std::string_view value) {
  std::string result;
  bool capitalize = true;

  for (char ch : value) {
    if (std::isalnum(static_cast<unsigned char>(ch)) == 0) {
      capitalize = true;
      continue;
    }

    char normalized =
        static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (capitalize) {
      result.push_back(static_cast<char>(
          std::toupper(static_cast<unsigned char>(normalized))));
      capitalize = false;
    } else {
      result.push_back(normalized);
    }
  }

  if (result.empty()) {
    return "_";
  }

  if (std::isdigit(static_cast<unsigned char>(result.front())) != 0) {
    result.insert(result.begin(), '_');
  }

  return result;
}

bool same_type_ref(const TypeRef &lhs, const TypeRef &rhs) {
  return lhs.kind == rhs.kind && lhs.name == rhs.name &&
         lhs.array_size == rhs.array_size &&
         lhs.is_runtime_sized == rhs.is_runtime_sized;
}

bool same_default_value(const std::optional<ShaderDefaultValue> &lhs,
                        const std::optional<ShaderDefaultValue> &rhs) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }

  if (!lhs) {
    return true;
  }

  if (lhs->index() != rhs->index()) {
    return false;
  }

  return *lhs == *rhs;
}

bool same_declared_field_schema(const DeclaredFieldReflection &lhs,
                                const DeclaredFieldReflection &rhs) {
  if (lhs.name != rhs.name || lhs.logical_name != rhs.logical_name ||
      !same_type_ref(lhs.type, rhs.type) || lhs.array_size != rhs.array_size ||
      !same_default_value(lhs.default_value, rhs.default_value) ||
      lhs.binding_id != rhs.binding_id ||
      lhs.fields.size() != rhs.fields.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.fields.size(); ++i) {
    if (!same_declared_field_schema(lhs.fields[i], rhs.fields[i])) {
      return false;
    }
  }

  return true;
}

bool same_declared_field_schema(
    const std::vector<DeclaredFieldReflection> &lhs,
    const std::vector<DeclaredFieldReflection> &rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (!same_declared_field_schema(lhs[i], rhs[i])) {
      return false;
    }
  }

  return true;
}

void merge_declared_field_stage_masks(DeclaredFieldReflection &dst,
                                      const DeclaredFieldReflection &src) {
  dst.active_stage_mask |= src.active_stage_mask;
  for (size_t i = 0; i < dst.fields.size(); ++i) {
    merge_declared_field_stage_masks(dst.fields[i], src.fields[i]);
  }
}

void merge_declared_field_stage_masks(
    std::vector<DeclaredFieldReflection> &dst,
    const std::vector<DeclaredFieldReflection> &src) {
  for (size_t i = 0; i < dst.size(); ++i) {
    merge_declared_field_stage_masks(dst[i], src[i]);
  }
}

std::optional<std::string> cpp_scalar_type(const TypeRef &type) {
  switch (type.kind) {
    case TokenKind::TypeBool:
      return "bool";
    case TokenKind::TypeInt:
      return "int";
    case TokenKind::TypeFloat:
      return "float";
    case TokenKind::TypeVec2:
      return "glm::vec2";
    case TokenKind::TypeVec3:
      return "glm::vec3";
    case TokenKind::TypeVec4:
      return "glm::vec4";
    case TokenKind::TypeMat3:
      return "glm::mat3";
    case TokenKind::TypeMat4:
      return "glm::mat4";
    case TokenKind::TypeSampler2D:
    case TokenKind::TypeSamplerCube:
    case TokenKind::TypeSampler2DShadow:
    case TokenKind::TypeIsampler2D:
    case TokenKind::TypeUsampler2D:
      return "int";
    default:
      return std::nullopt;
  }
}

bool supports_typed_uniform_field(const DeclaredFieldReflection &field) {
  if (field.fields.empty()) {
    return cpp_scalar_type(field.type).has_value();
  }

  for (const auto &child : field.fields) {
    if (supports_typed_uniform_field(child)) {
      return true;
    }
  }

  return false;
}

std::string wrap_array_type(std::string type_name,
                            const std::vector<uint32_t> &array_extents) {
  for (auto it = array_extents.rbegin(); it != array_extents.rend(); ++it) {
    type_name = "std::array<" + type_name + ", " + std::to_string(*it) + ">";
  }

  return type_name;
}

std::string format_default_value(const ShaderDefaultValue &default_value) {
  return std::visit(
      [](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, bool>) {
          return value ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int>) {
          return std::to_string(value);
        } else {
          std::ostringstream out;
          out << value;
          auto formatted = out.str();
          if (formatted.find('.') == std::string::npos) {
            formatted += ".0";
          }
          formatted += "f";
          return formatted;
        }
      },
      default_value);
}

std::string float_nan_expression() {
  return "std::numeric_limits<float>::quiet_NaN()";
}

std::string invalid_scalar_initializer(const TypeRef &type) {
  switch (type.kind) {
    case TokenKind::TypeSampler2D:
    case TokenKind::TypeSamplerCube:
    case TokenKind::TypeSampler2DShadow:
    case TokenKind::TypeIsampler2D:
    case TokenKind::TypeUsampler2D:
      return "-1";
    case TokenKind::TypeInt:
      return "std::numeric_limits<int>::min()";
    case TokenKind::TypeFloat:
      return float_nan_expression();
    case TokenKind::TypeVec2:
      return "glm::vec2(" + float_nan_expression() + ")";
    case TokenKind::TypeVec3:
      return "glm::vec3(" + float_nan_expression() + ")";
    case TokenKind::TypeVec4:
      return "glm::vec4(" + float_nan_expression() + ")";
    case TokenKind::TypeMat3: {
      const std::string nan_vec3 =
          "glm::vec3(" + float_nan_expression() + ")";
      return "glm::mat3(" + nan_vec3 + ", " + nan_vec3 + ", " + nan_vec3 +
             ")";
    }
    case TokenKind::TypeMat4: {
      const std::string nan_vec4 =
          "glm::vec4(" + float_nan_expression() + ")";
      return "glm::mat4(" + nan_vec4 + ", " + nan_vec4 + ", " + nan_vec4 +
             ", " + nan_vec4 + ")";
    }
    default:
      return {};
  }
}

std::string wrap_array_initializer(std::string base_initializer,
                                   const std::vector<uint32_t> &array_extents) {
  for (auto it = array_extents.rbegin(); it != array_extents.rend(); ++it) {
    base_initializer = "binding_detail::filled_array<" +
                       std::to_string(*it) + ">(" + base_initializer + ")";
  }

  return base_initializer;
}

bool supports_runtime_validation(const DeclaredFieldReflection &field) {
  if (!field.fields.empty()) {
    for (const auto &child : field.fields) {
      if (supports_runtime_validation(child)) {
        return true;
      }
    }

    return false;
  }

  if (!supports_typed_uniform_field(field) || field.default_value) {
    return false;
  }

  switch (field.type.kind) {
    case TokenKind::TypeBool:
      return false;
    case TokenKind::TypeInt:
    case TokenKind::TypeFloat:
    case TokenKind::TypeVec2:
    case TokenKind::TypeVec3:
    case TokenKind::TypeVec4:
    case TokenKind::TypeMat3:
    case TokenKind::TypeMat4:
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

std::string matrix_invalid_check(const std::string &access, int dimension) {
  std::ostringstream out;

  for (int column = 0; column < dimension; ++column) {
    for (int row = 0; row < dimension; ++row) {
      if (column != 0 || row != 0) {
        out << " || ";
      }

      out << "!std::isfinite(" << access << "[" << column << "][" << row
          << "])";
    }
  }

  return out.str();
}

std::string invalid_value_check(const std::string &access,
                                const TypeRef &type) {
  switch (type.kind) {
    case TokenKind::TypeSampler2D:
    case TokenKind::TypeSamplerCube:
    case TokenKind::TypeSampler2DShadow:
    case TokenKind::TypeIsampler2D:
    case TokenKind::TypeUsampler2D:
      return access + " < 0";
    case TokenKind::TypeInt:
      return access + " == std::numeric_limits<int>::min()";
    case TokenKind::TypeFloat:
      return "!std::isfinite(" + access + ")";
    case TokenKind::TypeVec2:
      return "!std::isfinite(" + access + ".x) || !std::isfinite(" + access +
             ".y)";
    case TokenKind::TypeVec3:
      return "!std::isfinite(" + access + ".x) || !std::isfinite(" + access +
             ".y) || !std::isfinite(" + access + ".z)";
    case TokenKind::TypeVec4:
      return "!std::isfinite(" + access + ".x) || !std::isfinite(" + access +
             ".y) || !std::isfinite(" + access + ".z) || !std::isfinite(" +
             access + ".w)";
    case TokenKind::TypeMat3:
      return matrix_invalid_check(access, 3);
    case TokenKind::TypeMat4:
      return matrix_invalid_check(access, 4);
    default:
      return "false";
  }
}

std::string
unique_nested_type_name(const DeclaredFieldReflection &field,
                        std::unordered_set<std::string> &used_names) {
  std::string candidate = field.type.name.empty()
                              ? to_pascal_case(field.name)
                              : sanitize_identifier(field.type.name);

  if (used_names.insert(candidate).second) {
    return candidate;
  }

  candidate += to_pascal_case(field.name);
  used_names.insert(candidate);
  return candidate;
}

void collect_leaf_infos(const DeclaredFieldReflection &field,
                        std::vector<PathSegment> path,
                        std::vector<std::string> tag_segments,
                        std::vector<LeafInfo> &leaves) {
  path.push_back(PathSegment{field.name, field.array_size});
  tag_segments.push_back(sanitize_identifier(field.name));

  if (field.fields.empty()) {
    if (supports_typed_uniform_field(field)) {
      leaves.push_back(
          LeafInfo{&field, std::move(path), std::move(tag_segments)});
    }
    return;
  }

  for (const auto &child : field.fields) {
    collect_leaf_infos(child, path, tag_segments, leaves);
  }
}

std::vector<uint32_t> collect_array_extents(const LeafInfo &leaf) {
  std::vector<uint32_t> array_extents;

  for (const auto &segment : leaf.path) {
    if (segment.array_size) {
      array_extents.push_back(*segment.array_size);
    }
  }

  return array_extents;
}

std::string build_leaf_tag_name(const LeafInfo &leaf) {
  std::string tag_name;

  for (size_t i = 0; i < leaf.tag_segments.size(); ++i) {
    if (i > 0) {
      tag_name += "__";
    }
    tag_name += leaf.tag_segments[i];
  }

  return tag_name;
}

void expand_exact_logical_names(const std::vector<PathSegment> &path,
                                size_t index, std::string prefix,
                                std::vector<std::string> &logical_names) {
  if (index >= path.size()) {
    logical_names.push_back(std::move(prefix));
    return;
  }

  const auto &segment = path[index];
  std::string base =
      prefix.empty() ? segment.name : prefix + "." + segment.name;

  if (segment.array_size) {
    for (uint32_t i = 0; i < *segment.array_size; ++i) {
      expand_exact_logical_names(
          path, index + 1, base + "[" + std::to_string(i) + "]", logical_names);
    }
    return;
  }

  expand_exact_logical_names(path, index + 1, std::move(base), logical_names);
}

std::vector<std::string> exact_logical_names(std::string_view resource_name,
                                             const LeafInfo &leaf) {
  std::vector<PathSegment> full_path = {
      PathSegment{std::string(resource_name), std::nullopt}};
  full_path.insert(full_path.end(), leaf.path.begin(), leaf.path.end());

  std::vector<std::string> logical_names;
  expand_exact_logical_names(full_path, 0, {}, logical_names);
  return logical_names;
}

std::string build_params_access(std::string_view root,
                                const std::vector<PathSegment> &path,
                                const std::vector<std::string> &indices) {
  std::string access(root);
  size_t array_index = 0;

  for (const auto &segment : path) {
    access += ".";
    access += sanitize_identifier(segment.name);
    if (segment.array_size) {
      access += "[";
      access += indices[array_index++];
      access += "]";
    }
  }

  return access;
}

std::string build_values_access(const std::vector<std::string> &indices) {
  std::string access = "values";
  for (const auto &index : indices) {
    access += "[";
    access += index;
    access += "]";
  }
  return access;
}

bool emit_params_struct_body(std::ostringstream &out,
                             const std::vector<DeclaredFieldReflection> &fields,
                             int depth, std::string *error) {
  std::unordered_set<std::string> used_nested_type_names;

  for (const auto &field : fields) {
    if (!supports_typed_uniform_field(field)) {
      continue;
    }

    std::string field_name = sanitize_identifier(field.name);

    if (!field.fields.empty()) {
      std::string nested_type_name =
          unique_nested_type_name(field, used_nested_type_names);
      emit_line(out, depth, "struct " + nested_type_name + " {");
      if (!emit_params_struct_body(out, field.fields, depth + 1, error)) {
        return false;
      }
      emit_line(out, depth, "};");

      std::vector<uint32_t> array_extents;
      if (field.array_size) {
        array_extents.push_back(*field.array_size);
      }
      emit_line(out, depth,
                wrap_array_type(nested_type_name, array_extents) + " " +
                    field_name + "{};");
      continue;
    }

    auto scalar_type = cpp_scalar_type(field.type);
    if (!scalar_type) {
      continue;
    }

    std::vector<uint32_t> array_extents;
    if (field.array_size) {
      array_extents.push_back(*field.array_size);
    }

    std::string initializer;
    if (field.default_value) {
      initializer = format_default_value(*field.default_value);
      if (field.array_size) {
        initializer = wrap_array_initializer(initializer, array_extents);
      }
    } else {
      initializer = invalid_scalar_initializer(field.type);
      if (!initializer.empty() && field.array_size) {
        initializer = wrap_array_initializer(initializer, array_extents);
      }
    }

    if (!initializer.empty()) {
      emit_line(out, depth,
                wrap_array_type(*scalar_type, array_extents) + " " +
                    field_name + " = " + initializer + ";");
    } else {
      emit_line(out, depth,
                wrap_array_type(*scalar_type, array_extents) + " " +
                    field_name + "{};");
    }
  }

  return true;
}

void emit_binding_detail_helpers(std::ostringstream &out) {
  emit_line(out, 1, "namespace binding_detail {");
  emit_line(out, 2, "template <size_t N, typename T>");
  emit_line(out, 2,
            "inline std::array<T, N> filled_array(const T &value) {");
  emit_line(out, 3, "std::array<T, N> values{};");
  emit_line(out, 3, "values.fill(value);");
  emit_line(out, 3, "return values;");
  emit_line(out, 2, "}");
  emit_line(out, 1, "} // namespace binding_detail");
}

bool emit_uniform_container(std::ostringstream &out,
                            const MergedUniformInterface &resource,
                            const std::string &container_name,
                            std::string *error) {
  emit_line(out, 1, "struct " + container_name + " {");

  std::vector<LeafInfo> leaves;
  for (const auto &field : resource.declared_fields) {
    collect_leaf_infos(field, {}, {}, leaves);
  }

  for (const auto &leaf : leaves) {
    auto scalar_type = cpp_scalar_type(leaf.field->type);
    if (!scalar_type) {
      continue;
    }

    const std::string tag_name = build_leaf_tag_name(leaf);
    const std::string tag_type_name = tag_name + "_t";
    const auto array_extents = collect_array_extents(leaf);
    const auto logical_names = exact_logical_names(resource.logical_name, leaf);

    std::ostringstream binding_ids;
    for (size_t i = 0; i < logical_names.size(); ++i) {
      if (i > 0) {
        binding_ids << ", ";
      }
      binding_ids << shader_binding_id(logical_names[i]) << "ull";
    }

    emit_line(out, 2, "struct " + tag_type_name + " {");
    emit_line(out, 3,
              "using value_type = " +
                  wrap_array_type(*scalar_type, array_extents) + ";");
    emit_line(out, 3,
              "static constexpr uint64_t binding_id = " +
                  std::to_string(leaf.field->binding_id) + "ull;");
    emit_line(out, 3,
              "static constexpr std::string_view logical_name = \"" +
                  leaf.field->logical_name + "\";");
    emit_line(out, 3,
              "static constexpr uint32_t stage_mask = " +
                  std::to_string(leaf.field->active_stage_mask) + "u;");
    emit_line(out, 3,
              "static constexpr std::array<uint64_t, " +
                  std::to_string(logical_names.size()) + "> binding_ids = {" +
                  binding_ids.str() + "};");
    emit_line(out, 2, "};");
    emit_line(out, 2,
              "static inline constexpr " + tag_type_name + " " + tag_name +
                  "{};");
  }

  emit_line(out, 1, "};");
  return true;
}

bool emit_params_struct(std::ostringstream &out,
                        const MergedUniformInterface &resource,
                        const std::string &params_name, std::string *error) {
  emit_line(out, 1, "struct " + params_name + " {");
  if (!emit_params_struct_body(out, resource.declared_fields, 2, error)) {
    return false;
  }
  emit_line(out, 1, "};");
  return true;
}

void emit_apply_leaf(std::ostringstream &out, const std::string &container_name,
                     const LeafInfo &leaf, int depth) {
  const std::string tag_name = build_leaf_tag_name(leaf);
  const std::vector<uint32_t> array_extents = collect_array_extents(leaf);

  if (array_extents.empty()) {
    emit_line(out, depth,
              "shader.set(" + container_name + "::" + tag_name + ", " +
                  build_params_access("params", leaf.path, {}) + ");");
    return;
  }

  emit_line(out, depth, "{");
  emit_line(out, depth + 1,
            "decltype(" + container_name + "::" + tag_name +
                ")::value_type values{};");

  std::vector<std::string> indices;
  for (size_t i = 0; i < array_extents.size(); ++i) {
    std::string index_name = "i" + std::to_string(i);
    indices.push_back(index_name);
    emit_line(out, depth + 1 + static_cast<int>(i),
              "for (size_t " + index_name + " = 0; " + index_name + " < " +
                  std::to_string(array_extents[i]) + "; ++" + index_name +
                  ") {");
  }

  emit_line(out, depth + 1 + static_cast<int>(array_extents.size()),
            build_values_access(indices) + " = " +
                build_params_access("params", leaf.path, indices) + ";");

  for (size_t i = array_extents.size(); i > 0; --i) {
    emit_line(out, depth + static_cast<int>(i), "}");
  }

  emit_line(out, depth + 1,
            "shader.set(" + container_name + "::" + tag_name + ", values);");
  emit_line(out, depth, "}");
}

void emit_validation_failure(std::ostringstream &out, int depth,
                             const std::string &params_name,
                             const std::string &logical_name) {
  emit_line(out, depth, "if (error != nullptr) {");
  emit_line(out, depth + 1,
            "*error = \"" + params_name + " is missing required field '" +
                logical_name + "'\";");
  emit_line(out, depth, "}");
  emit_line(out, depth, "return false;");
}

void emit_validate_leaf(std::ostringstream &out, const LeafInfo &leaf,
                        int depth, const std::string &params_name) {
  if (!supports_runtime_validation(*leaf.field)) {
    return;
  }

  const auto array_extents = collect_array_extents(leaf);
  if (array_extents.empty()) {
    emit_line(out, depth,
              "if (" +
                  invalid_value_check(build_params_access("params", leaf.path, {}),
                                      leaf.field->type) +
                  ") {");
    emit_validation_failure(out, depth + 1, params_name,
                            leaf.field->logical_name);
    emit_line(out, depth, "}");
    return;
  }

  emit_line(out, depth, "{");

  std::vector<std::string> indices;
  for (size_t i = 0; i < array_extents.size(); ++i) {
    std::string index_name = "i" + std::to_string(i);
    indices.push_back(index_name);
    emit_line(out, depth + 1 + static_cast<int>(i),
              "for (size_t " + index_name + " = 0; " + index_name + " < " +
                  std::to_string(array_extents[i]) + "; ++" + index_name +
                  ") {");
  }

  emit_line(out, depth + 1 + static_cast<int>(array_extents.size()),
            "if (" +
                invalid_value_check(
                    build_params_access("params", leaf.path, indices),
                    leaf.field->type) +
                ") {");
  emit_validation_failure(out, depth + 2 + static_cast<int>(array_extents.size()),
                          params_name, leaf.field->logical_name);
  emit_line(out, depth + 1 + static_cast<int>(array_extents.size()), "}");

  for (size_t i = array_extents.size(); i > 0; --i) {
    emit_line(out, depth + static_cast<int>(i), "}");
  }

  emit_line(out, depth, "}");
}

void emit_validate_shader_params(std::ostringstream &out,
                                 const MergedUniformInterface &resource,
                                 const std::string &params_name) {
  emit_line(out, 1,
            "inline bool validate_shader_params(const " + params_name +
                " &params, std::string *error = nullptr) {");

  std::vector<LeafInfo> leaves;
  for (const auto &field : resource.declared_fields) {
    collect_leaf_infos(field, {}, {}, leaves);
  }

  for (const auto &leaf : leaves) {
    emit_validate_leaf(out, leaf, 2, params_name);
  }

  emit_line(out, 2, "return true;");
  emit_line(out, 1, "}");
}

void emit_apply_shader_params(std::ostringstream &out,
                              const MergedUniformInterface &resource,
                              const std::string &container_name,
                              const std::string &params_name) {
  emit_line(out, 1,
            "inline void apply_shader_params(const astralix::Shader &shader,");
  emit_line(out, 2, "const " + params_name + " &params) {");

  std::vector<LeafInfo> leaves;
  for (const auto &field : resource.declared_fields) {
    collect_leaf_infos(field, {}, {}, leaves);
  }

  for (const auto &leaf : leaves) {
    emit_apply_leaf(out, container_name, leaf, 2);
  }

  emit_line(out, 1, "}");
}

std::optional<std::vector<MergedUniformInterface>>
merge_uniform_interfaces(const ShaderReflection &reflection,
                         std::string *error) {
  std::vector<MergedUniformInterface> merged;
  std::unordered_map<std::string, size_t> index_by_logical_name;

  for (const auto &[stage_kind, stage] : reflection.stages) {
    (void)stage_kind;

    for (const auto &resource : stage.resources) {
      if (resource.kind != ShaderResourceKind::UniformInterface) {
        continue;
      }

      auto [it, inserted] =
          index_by_logical_name.emplace(resource.logical_name, merged.size());
      if (inserted) {
        merged.push_back(MergedUniformInterface{resource.logical_name,
                                                resource.declared_name,
                                                resource.declared_fields});
        continue;
      }

      auto &existing = merged[it->second];
      if (existing.declared_name != resource.declared_name ||
          !same_declared_field_schema(existing.declared_fields,
                                      resource.declared_fields)) {
        if (error) {
          *error = "conflicting typed uniform schema for resource '" +
                   resource.logical_name + "'";
        }
        return std::nullopt;
      }

      merge_declared_field_stage_masks(existing.declared_fields,
                                       resource.declared_fields);
    }
  }

  return merged;
}

} // namespace

std::string BindingCppEmitter::sanitize_namespace(std::string_view input_path) {
  const auto normalized =
      std::filesystem::path(input_path).lexically_normal().generic_string();
  return sanitize_identifier(normalized);
}

std::optional<std::string>
BindingCppEmitter::emit(const ShaderReflection &reflection,
                        std::string_view input_path, std::string *error) {
  auto merged_uniforms = merge_uniform_interfaces(reflection, error);
  if (!merged_uniforms) {
    return std::nullopt;
  }

  std::unordered_map<std::string, size_t> interface_counts;
  for (const auto &resource : *merged_uniforms) {
    interface_counts[resource.declared_name]++;
  }

  std::ostringstream out;
  out << "#pragma once\n\n";
  out << "#include \"resources/shader.hpp\"\n";
  out << "#include <array>\n";
  out << "#include <cmath>\n";
  out << "#include <limits>\n";
  out << "#include <string>\n";
  out << "#include <cstdint>\n";
  out << "#include <string_view>\n\n";
  out << "namespace astralix::shader_bindings::"
      << sanitize_namespace(input_path) << " {\n\n";

  emit_binding_detail_helpers(out);
  out << '\n';

  for (const auto &resource : *merged_uniforms) {
    std::string base_name = sanitize_identifier(resource.declared_name);
    if (interface_counts[resource.declared_name] > 1) {
      base_name += to_pascal_case(resource.logical_name);
    }

    const std::string uniform_name = base_name + "Uniform";
    const std::string params_name = base_name + "Params";

    if (!emit_uniform_container(out, resource, uniform_name, error)) {
      return std::nullopt;
    }
    out << '\n';

    if (!emit_params_struct(out, resource, params_name, error)) {
      return std::nullopt;
    }
    out << '\n';

    emit_validate_shader_params(out, resource, params_name);
    out << '\n';

    emit_apply_shader_params(out, resource, uniform_name, params_name);
    out << '\n';
  }

  out << "} // namespace astralix::shader_bindings::"
      << sanitize_namespace(input_path) << "\n";
  return out.str();
}

} // namespace astralix
