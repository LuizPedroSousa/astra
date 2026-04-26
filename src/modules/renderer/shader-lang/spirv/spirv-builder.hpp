#pragma once

#include <cstdint>
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix {

class SPIRVBuilder {
public:
  SPIRVBuilder();

  uint32_t allocate_id();

  uint32_t register_void();
  uint32_t register_bool();
  uint32_t register_int(uint32_t width, bool is_signed);
  uint32_t register_float(uint32_t width);
  uint32_t register_vector(uint32_t component_type, uint32_t count);
  uint32_t register_matrix(uint32_t column_type, uint32_t column_count);
  uint32_t register_struct(const std::vector<uint32_t> &member_types);
  uint32_t register_pointer(spv::StorageClass storage_class, uint32_t pointee_type);
  uint32_t register_function_type(uint32_t return_type, const std::vector<uint32_t> &param_types);
  uint32_t register_runtime_array(uint32_t element_type);
  uint32_t register_array(uint32_t element_type, uint32_t length_id);
  uint32_t register_image(uint32_t sampled_type, spv::Dim dim, uint32_t depth, uint32_t arrayed, uint32_t ms, uint32_t sampled, spv::ImageFormat format);
  uint32_t register_sampled_image(uint32_t image_type);

  uint32_t constant_bool(bool value);
  uint32_t constant_int(int32_t value, uint32_t type_id);
  uint32_t constant_uint(uint32_t value, uint32_t type_id);
  uint32_t constant_float(float value, uint32_t type_id);
  uint32_t constant_composite(uint32_t type_id, const std::vector<uint32_t> &constituents);

  void add_capability(spv::Capability cap);
  void set_memory_model(spv::AddressingModel addressing, spv::MemoryModel memory);
  void add_entry_point(spv::ExecutionModel model, uint32_t function_id, const std::string &name, const std::vector<uint32_t> &interface_ids);
  void add_execution_mode(uint32_t entry_point, spv::ExecutionMode mode);

  uint32_t import_extended_instruction_set(const std::string &name);

  void set_name(uint32_t id, const std::string &name);
  void set_member_name(uint32_t struct_id, uint32_t member, const std::string &name);

  void decorate(uint32_t id, spv::Decoration decoration);
  void decorate(uint32_t id, spv::Decoration decoration, uint32_t operand);
  void member_decorate(uint32_t struct_id, uint32_t member, spv::Decoration decoration);
  void member_decorate(uint32_t struct_id, uint32_t member, spv::Decoration decoration, uint32_t operand);

  uint32_t variable(uint32_t pointer_type, spv::StorageClass storage_class);

  void begin_function(uint32_t result_id, uint32_t return_type, spv::FunctionControlMask control, uint32_t function_type);
  uint32_t function_parameter(uint32_t type);
  void end_function();

  uint32_t label();
  void label(uint32_t id);

  uint32_t op_load(uint32_t result_type, uint32_t pointer);
  void op_store(uint32_t pointer, uint32_t value);
  uint32_t op_access_chain(uint32_t result_type, uint32_t base, const std::vector<uint32_t> &indices);

  uint32_t op_composite_construct(uint32_t result_type, const std::vector<uint32_t> &constituents);
  uint32_t op_composite_extract(uint32_t result_type, uint32_t composite, uint32_t index);
  uint32_t op_vector_shuffle(uint32_t result_type, uint32_t vec1, uint32_t vec2, const std::vector<uint32_t> &components);

  uint32_t op_f_add(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_f_sub(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_f_mul(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_f_div(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_f_negate(uint32_t result_type, uint32_t operand);
  uint32_t op_f_mod(uint32_t result_type, uint32_t a, uint32_t b);

  uint32_t op_i_add(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_i_sub(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_i_mul(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_s_div(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_s_mod(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_s_negate(uint32_t result_type, uint32_t operand);

  uint32_t op_f_ord_equal(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_f_ord_not_equal(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_f_ord_less_than(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_f_ord_greater_than(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_f_ord_less_than_equal(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_f_ord_greater_than_equal(uint32_t result_type, uint32_t a, uint32_t b);

  uint32_t op_i_equal(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_i_not_equal(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_s_less_than(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_s_greater_than(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_s_less_than_equal(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_s_greater_than_equal(uint32_t result_type, uint32_t a, uint32_t b);

  uint32_t op_logical_and(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_logical_or(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_logical_not(uint32_t result_type, uint32_t operand);

  uint32_t op_select(uint32_t result_type, uint32_t condition, uint32_t true_val, uint32_t false_val);
  uint32_t op_phi(uint32_t result_type, const std::vector<std::pair<uint32_t, uint32_t>> &incoming);

  void op_branch(uint32_t target_label);
  void op_branch_conditional(uint32_t condition, uint32_t true_label, uint32_t false_label);
  void op_selection_merge(uint32_t merge_label, spv::SelectionControlMask control);
  void op_loop_merge(uint32_t merge_label, uint32_t continue_label, spv::LoopControlMask control);

  void op_return();
  void op_return_value(uint32_t value);
  void op_kill();
  void op_unreachable();

  uint32_t op_function_call(uint32_t result_type, uint32_t function, const std::vector<uint32_t> &args);

  uint32_t op_matrix_times_vector(uint32_t result_type, uint32_t matrix, uint32_t vector);
  uint32_t op_matrix_times_matrix(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_vector_times_scalar(uint32_t result_type, uint32_t vector, uint32_t scalar);
  uint32_t op_matrix_times_scalar(uint32_t result_type, uint32_t matrix, uint32_t scalar);
  uint32_t op_vector_times_matrix(uint32_t result_type, uint32_t vector, uint32_t matrix);
  uint32_t op_dot(uint32_t result_type, uint32_t a, uint32_t b);
  uint32_t op_transpose(uint32_t result_type, uint32_t matrix);

  uint32_t op_convert_f_to_s(uint32_t result_type, uint32_t value);
  uint32_t op_convert_s_to_f(uint32_t result_type, uint32_t value);
  uint32_t op_convert_f_to_u(uint32_t result_type, uint32_t value);
  uint32_t op_convert_u_to_f(uint32_t result_type, uint32_t value);
  uint32_t op_bitcast(uint32_t result_type, uint32_t value);

  uint32_t op_image_sample_implicit_lod(uint32_t result_type, uint32_t sampled_image, uint32_t coordinate);

  uint32_t op_dpdx(uint32_t result_type, uint32_t operand);
  uint32_t op_dpdy(uint32_t result_type, uint32_t operand);
  uint32_t op_fwidth(uint32_t result_type, uint32_t operand);

  uint32_t op_ext_inst(uint32_t result_type, uint32_t ext_set, uint32_t instruction, const std::vector<uint32_t> &operands);

  std::vector<uint32_t> finalize();

private:
  void emit_instruction(std::vector<uint32_t> &stream, spv::Op opcode, const std::vector<uint32_t> &operands);
  void emit_instruction_with_string(std::vector<uint32_t> &stream, spv::Op opcode, const std::vector<uint32_t> &operands_before, const std::string &str, const std::vector<uint32_t> &operands_after = {});

  static std::vector<uint32_t> encode_string(const std::string &str);

  uint32_t m_next_id = 1;

  std::vector<uint32_t> m_capabilities;
  std::vector<uint32_t> m_extensions;
  std::vector<uint32_t> m_ext_inst_imports;
  std::vector<uint32_t> m_memory_model;
  std::vector<uint32_t> m_entry_points;
  std::vector<uint32_t> m_execution_modes;
  std::vector<uint32_t> m_debug_names;
  std::vector<uint32_t> m_annotations;
  std::vector<uint32_t> m_types_constants_globals;
  std::vector<uint32_t> m_function_bodies;

  std::unordered_map<uint64_t, uint32_t> m_type_cache;
  std::unordered_map<uint64_t, uint32_t> m_constant_cache;
};

} // namespace astralix
