#pragma once

#include "shader-lang/lowering/canonical-lowering.hpp"
#include "shader-lang/pipeline-layout.hpp"
#include "shader-lang/reflection.hpp"
#include "shader-lang/spirv/spirv-builder.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix {

struct VulkanSPIRVEmitResult {
  std::vector<uint32_t> spirv;
  std::vector<std::string> errors;
  bool ok() const { return errors.empty(); }
};

class VulkanSPIRVEmitter {
public:
  VulkanSPIRVEmitResult emit(const CanonicalStage &stage,
                              const StageReflection &reflection,
                              const ShaderPipelineLayout &layout);

private:
  struct VariableInfo {
    uint32_t pointer_id = 0;
    uint32_t type_id = 0;
    spv::StorageClass storage = spv::StorageClassFunction;
  };

  struct UBOInfo {
    uint32_t variable_id = 0;
    uint32_t struct_type_id = 0;
    uint32_t pointer_type_id = 0;
    std::vector<std::string> field_logical_names;
    std::vector<uint32_t> field_type_ids;
    spv::StorageClass storage = spv::StorageClassUniform;
  };

  void setup_module(StageKind stage);
  uint32_t resolve_type(const TypeRef &type);
  uint32_t resolve_element_type(const TypeRef &type);
  void emit_struct_types(const std::vector<CanonicalStructDecl> &structs);
  void emit_input_variables(const StageReflection &reflection,
                            const CanonicalEntryPoint &entry);
  void emit_output_variables(const StageReflection &reflection,
                             const CanonicalEntryPoint &entry);
  void emit_resource_variables(const StageReflection &reflection,
                               const ShaderPipelineLayout &layout,
                               const CanonicalEntryPoint &entry);
  void emit_builtin_variables(StageKind stage, const CanonicalEntryPoint &entry);
  void emit_global_constants(const std::vector<CanonicalDecl> &declarations);
  void emit_helper_functions(const std::vector<CanonicalDecl> &declarations);
  void emit_entry_point(const CanonicalEntryPoint &entry);
  void add_error(std::string message);

  void collect_local_variables(const CanonicalStmt &stmt,
                               std::vector<std::pair<std::string, TypeRef>> &out);

  std::optional<uint32_t> try_emit_constant_expr(const CanonicalExpr &expr);
  uint32_t emit_expr(const CanonicalExpr &expr);
  uint32_t emit_lvalue(const CanonicalExpr &expr);
  void emit_stmt(const CanonicalStmt &stmt);

  uint32_t emit_binary_op(TokenKind op, const TypeRef &result_type,
                           const TypeRef &lhs_type, const TypeRef &rhs_type,
                           uint32_t lhs, uint32_t rhs);
  uint32_t emit_unary_op(TokenKind op, const TypeRef &type, uint32_t operand);
  uint32_t emit_builtin_call(const std::string &name, const TypeRef &result_type,
                              const CanonicalExpr &call_expr);
  uint32_t emit_construct(const TypeRef &target_type,
                           const std::vector<CanonicalExprPtr> &args);

  static bool uses_builtin_output(const CanonicalStmt &stmt,
                                  const std::string &field);

  bool resolve_resource_field_path(const CanonicalExpr &expr,
                                   std::string &ubo_name,
                                   std::string &field_path) const;

  spv::StorageClass infer_lvalue_storage_class(const CanonicalExpr &expr) const;

  TypeRef infer_expr_type(const CanonicalExpr &expr) const;

  bool is_float_type(const TypeRef &type) const;
  bool is_int_type(const TypeRef &type) const;
  bool is_uint_type(const TypeRef &type) const;
  bool is_vector_type(const TypeRef &type) const;
  bool is_matrix_type(const TypeRef &type) const;
  bool is_sampler_type(const TypeRef &type) const;
  bool is_scalar_type(const TypeRef &type) const;
  uint32_t vector_component_count(const TypeRef &type) const;
  uint32_t swizzle_index(char component) const;
  bool is_swizzle(const std::string &field, const TypeRef &object_type) const;

  SPIRVBuilder m_builder;
  uint32_t m_glsl_ext = 0;
  uint32_t m_void_type = 0;
  uint32_t m_bool_type = 0;
  uint32_t m_int_type = 0;
  uint32_t m_uint_type = 0;
  uint32_t m_float_type = 0;

  std::unordered_map<std::string, VariableInfo> m_input_variables;
  std::unordered_map<std::string, VariableInfo> m_output_variables;
  std::unordered_map<std::string, VariableInfo> m_sampler_variables;
  std::unordered_map<std::string, UBOInfo> m_ubo_map;
  std::unordered_map<std::string, VariableInfo> m_local_variables;
  std::unordered_map<std::string, uint32_t> m_global_constants;
  std::vector<std::pair<std::string, const CanonicalExpr *>> m_deferred_constants;
  std::unordered_map<std::string, uint32_t> m_function_ids;
  std::unordered_map<std::string, TypeRef> m_named_type_refs;

  std::unordered_map<std::string, uint32_t> m_struct_type_ids;
  std::unordered_map<std::string, std::vector<std::string>> m_struct_field_names;
  std::unordered_map<std::string, std::vector<uint32_t>> m_struct_field_type_ids;

  VariableInfo m_gl_position{};
  VariableInfo m_gl_vertex_id{};
  VariableInfo m_gl_instance_id{};
  VariableInfo m_gl_frag_depth{};
  VariableInfo m_gl_frag_coord{};
  bool m_has_gl_position = false;
  bool m_has_gl_vertex_id = false;
  bool m_has_gl_instance_id = false;
  bool m_has_gl_frag_depth = false;
  bool m_has_gl_frag_coord = false;

  std::vector<uint32_t> m_interface_variable_ids;
  std::vector<uint32_t> m_loop_merge_stack;
  std::vector<uint32_t> m_loop_continue_stack;

  StageKind m_stage_kind = StageKind::Vertex;
  uint32_t m_entry_function_id = 0;
  std::vector<std::string> m_errors;
};

} // namespace astralix
