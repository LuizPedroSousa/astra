#include "spirv-builder.hpp"
#include <cstring>

namespace astralix {

static uint64_t hash_combine(uint64_t seed, uint64_t value) {
  return seed ^ (value * 0x9e3779b97f4a7c15ULL + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

SPIRVBuilder::SPIRVBuilder() = default;

uint32_t SPIRVBuilder::allocate_id() {
  return m_next_id++;
}

void SPIRVBuilder::emit_instruction(std::vector<uint32_t> &stream, spv::Op opcode, const std::vector<uint32_t> &operands) {
  uint32_t word_count = static_cast<uint32_t>(1 + operands.size());
  stream.push_back((word_count << 16) | static_cast<uint32_t>(opcode));
  stream.insert(stream.end(), operands.begin(), operands.end());
}

std::vector<uint32_t> SPIRVBuilder::encode_string(const std::string &str) {
  std::vector<uint32_t> words;
  size_t padded_size = (str.size() + 4) & ~3u;
  words.resize(padded_size / 4, 0);
  std::memcpy(words.data(), str.c_str(), str.size() + 1);
  return words;
}

void SPIRVBuilder::emit_instruction_with_string(
    std::vector<uint32_t> &stream, spv::Op opcode,
    const std::vector<uint32_t> &operands_before,
    const std::string &str,
    const std::vector<uint32_t> &operands_after
) {
  auto encoded = encode_string(str);
  uint32_t word_count = static_cast<uint32_t>(
      1 + operands_before.size() + encoded.size() + operands_after.size()
  );
  stream.push_back((word_count << 16) | static_cast<uint32_t>(opcode));
  stream.insert(stream.end(), operands_before.begin(), operands_before.end());
  stream.insert(stream.end(), encoded.begin(), encoded.end());
  stream.insert(stream.end(), operands_after.begin(), operands_after.end());
}

uint32_t SPIRVBuilder::register_void() {
  uint64_t key = hash_combine(0, static_cast<uint64_t>(spv::OpTypeVoid));
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeVoid, {id});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_bool() {
  uint64_t key = hash_combine(0, static_cast<uint64_t>(spv::OpTypeBool));
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeBool, {id});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_int(uint32_t width, bool is_signed) {
  uint64_t key = hash_combine(static_cast<uint64_t>(spv::OpTypeInt), hash_combine(width, is_signed ? 1 : 0));
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeInt, {id, width, is_signed ? 1u : 0u});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_float(uint32_t width) {
  uint64_t key = hash_combine(static_cast<uint64_t>(spv::OpTypeFloat), width);
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeFloat, {id, width});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_vector(uint32_t component_type, uint32_t count) {
  uint64_t key = hash_combine(static_cast<uint64_t>(spv::OpTypeVector), hash_combine(component_type, count));
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeVector, {id, component_type, count});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_matrix(uint32_t column_type, uint32_t column_count) {
  uint64_t key = hash_combine(static_cast<uint64_t>(spv::OpTypeMatrix), hash_combine(column_type, column_count));
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeMatrix, {id, column_type, column_count});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_struct(const std::vector<uint32_t> &member_types) {
  uint32_t id = allocate_id();
  std::vector<uint32_t> operands;
  operands.push_back(id);
  operands.insert(operands.end(), member_types.begin(), member_types.end());
  emit_instruction(m_types_constants_globals, spv::OpTypeStruct, operands);
  return id;
}

uint32_t SPIRVBuilder::register_pointer(spv::StorageClass storage_class, uint32_t pointee_type) {
  uint64_t key = hash_combine(static_cast<uint64_t>(spv::OpTypePointer), hash_combine(static_cast<uint64_t>(storage_class), pointee_type));
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypePointer, {id, static_cast<uint32_t>(storage_class), pointee_type});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_function_type(uint32_t return_type, const std::vector<uint32_t> &param_types) {
  uint64_t key = hash_combine(static_cast<uint64_t>(spv::OpTypeFunction), return_type);
  for (auto param : param_types) {
    key = hash_combine(key, param);
  }
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  std::vector<uint32_t> operands;
  operands.push_back(id);
  operands.push_back(return_type);
  operands.insert(operands.end(), param_types.begin(), param_types.end());
  emit_instruction(m_types_constants_globals, spv::OpTypeFunction, operands);
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_runtime_array(uint32_t element_type) {
  uint64_t key = hash_combine(static_cast<uint64_t>(spv::OpTypeRuntimeArray), element_type);
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeRuntimeArray, {id, element_type});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_array(uint32_t element_type, uint32_t length_id) {
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeArray, {id, element_type, length_id});
  return id;
}

uint32_t SPIRVBuilder::register_image(uint32_t sampled_type, spv::Dim dim, uint32_t depth, uint32_t arrayed, uint32_t ms, uint32_t sampled, spv::ImageFormat format) {
  uint64_t key = hash_combine(static_cast<uint64_t>(spv::OpTypeImage), hash_combine(sampled_type, hash_combine(static_cast<uint64_t>(dim), hash_combine(depth, hash_combine(arrayed, hash_combine(ms, hash_combine(sampled, static_cast<uint64_t>(format))))))));
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeImage, {id, sampled_type, static_cast<uint32_t>(dim), depth, arrayed, ms, sampled, static_cast<uint32_t>(format)});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::register_sampled_image(uint32_t image_type) {
  uint64_t key = hash_combine(static_cast<uint64_t>(spv::OpTypeSampledImage), image_type);
  auto it = m_type_cache.find(key);
  if (it != m_type_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpTypeSampledImage, {id, image_type});
  m_type_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::constant_bool(bool value) {
  uint32_t bool_type = register_bool();
  uint64_t key = hash_combine(999, hash_combine(bool_type, value ? 1u : 0u));
  auto it = m_constant_cache.find(key);
  if (it != m_constant_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, value ? spv::OpConstantTrue : spv::OpConstantFalse, {bool_type, id});
  m_constant_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::constant_int(int32_t value, uint32_t type_id) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  uint64_t key = hash_combine(1000, hash_combine(type_id, bits));
  auto it = m_constant_cache.find(key);
  if (it != m_constant_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpConstant, {type_id, id, bits});
  m_constant_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::constant_uint(uint32_t value, uint32_t type_id) {
  uint64_t key = hash_combine(1001, hash_combine(type_id, value));
  auto it = m_constant_cache.find(key);
  if (it != m_constant_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpConstant, {type_id, id, value});
  m_constant_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::constant_float(float value, uint32_t type_id) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  uint64_t key = hash_combine(1002, hash_combine(type_id, bits));
  auto it = m_constant_cache.find(key);
  if (it != m_constant_cache.end())
    return it->second;
  uint32_t id = allocate_id();
  emit_instruction(m_types_constants_globals, spv::OpConstant, {type_id, id, bits});
  m_constant_cache[key] = id;
  return id;
}

uint32_t SPIRVBuilder::constant_composite(uint32_t type_id, const std::vector<uint32_t> &constituents) {
  uint32_t id = allocate_id();
  std::vector<uint32_t> operands;
  operands.push_back(type_id);
  operands.push_back(id);
  operands.insert(operands.end(), constituents.begin(), constituents.end());
  emit_instruction(m_types_constants_globals, spv::OpConstantComposite, operands);
  return id;
}

void SPIRVBuilder::add_capability(spv::Capability cap) {
  emit_instruction(m_capabilities, spv::OpCapability, {static_cast<uint32_t>(cap)});
}

void SPIRVBuilder::set_memory_model(spv::AddressingModel addressing, spv::MemoryModel memory) {
  emit_instruction(m_memory_model, spv::OpMemoryModel, {static_cast<uint32_t>(addressing), static_cast<uint32_t>(memory)});
}

void SPIRVBuilder::add_entry_point(spv::ExecutionModel model, uint32_t function_id, const std::string &name, const std::vector<uint32_t> &interface_ids) {
  emit_instruction_with_string(m_entry_points, spv::OpEntryPoint, {static_cast<uint32_t>(model), function_id}, name, interface_ids);
}

void SPIRVBuilder::add_execution_mode(uint32_t entry_point, spv::ExecutionMode mode) {
  emit_instruction(m_execution_modes, spv::OpExecutionMode, {entry_point, static_cast<uint32_t>(mode)});
}

uint32_t SPIRVBuilder::import_extended_instruction_set(const std::string &name) {
  uint32_t id = allocate_id();
  emit_instruction_with_string(m_ext_inst_imports, spv::OpExtInstImport, {id}, name);
  return id;
}

void SPIRVBuilder::set_name(uint32_t id, const std::string &name) {
  emit_instruction_with_string(m_debug_names, spv::OpName, {id}, name);
}

void SPIRVBuilder::set_member_name(uint32_t struct_id, uint32_t member, const std::string &name) {
  emit_instruction_with_string(m_debug_names, spv::OpMemberName, {struct_id, member}, name);
}

void SPIRVBuilder::decorate(uint32_t id, spv::Decoration decoration) {
  emit_instruction(m_annotations, spv::OpDecorate, {id, static_cast<uint32_t>(decoration)});
}

void SPIRVBuilder::decorate(uint32_t id, spv::Decoration decoration, uint32_t operand) {
  emit_instruction(m_annotations, spv::OpDecorate, {id, static_cast<uint32_t>(decoration), operand});
}

void SPIRVBuilder::member_decorate(uint32_t struct_id, uint32_t member, spv::Decoration decoration) {
  emit_instruction(m_annotations, spv::OpMemberDecorate, {struct_id, member, static_cast<uint32_t>(decoration)});
}

void SPIRVBuilder::member_decorate(uint32_t struct_id, uint32_t member, spv::Decoration decoration, uint32_t operand) {
  emit_instruction(m_annotations, spv::OpMemberDecorate, {struct_id, member, static_cast<uint32_t>(decoration), operand});
}

uint32_t SPIRVBuilder::variable(uint32_t pointer_type, spv::StorageClass storage_class) {
  uint32_t id = allocate_id();
  if (storage_class == spv::StorageClassFunction) {
    emit_instruction(m_function_bodies, spv::OpVariable, {pointer_type, id, static_cast<uint32_t>(storage_class)});
  } else {
    emit_instruction(m_types_constants_globals, spv::OpVariable, {pointer_type, id, static_cast<uint32_t>(storage_class)});
  }
  return id;
}

void SPIRVBuilder::begin_function(uint32_t result_id, uint32_t return_type, spv::FunctionControlMask control, uint32_t function_type) {
  emit_instruction(m_function_bodies, spv::OpFunction, {return_type, result_id, static_cast<uint32_t>(control), function_type});
}

uint32_t SPIRVBuilder::function_parameter(uint32_t type) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpFunctionParameter, {type, id});
  return id;
}

void SPIRVBuilder::end_function() {
  emit_instruction(m_function_bodies, spv::OpFunctionEnd, {});
}

uint32_t SPIRVBuilder::label() {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpLabel, {id});
  return id;
}

void SPIRVBuilder::label(uint32_t id) {
  emit_instruction(m_function_bodies, spv::OpLabel, {id});
}

uint32_t SPIRVBuilder::op_load(uint32_t result_type, uint32_t pointer) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpLoad, {result_type, id, pointer});
  return id;
}

void SPIRVBuilder::op_store(uint32_t pointer, uint32_t value) {
  emit_instruction(m_function_bodies, spv::OpStore, {pointer, value});
}

uint32_t SPIRVBuilder::op_access_chain(uint32_t result_type, uint32_t base, const std::vector<uint32_t> &indices) {
  uint32_t id = allocate_id();
  std::vector<uint32_t> operands = {result_type, id, base};
  operands.insert(operands.end(), indices.begin(), indices.end());
  emit_instruction(m_function_bodies, spv::OpAccessChain, operands);
  return id;
}

uint32_t SPIRVBuilder::op_composite_construct(uint32_t result_type, const std::vector<uint32_t> &constituents) {
  uint32_t id = allocate_id();
  std::vector<uint32_t> operands = {result_type, id};
  operands.insert(operands.end(), constituents.begin(), constituents.end());
  emit_instruction(m_function_bodies, spv::OpCompositeConstruct, operands);
  return id;
}

uint32_t SPIRVBuilder::op_composite_extract(uint32_t result_type, uint32_t composite, uint32_t index) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpCompositeExtract, {result_type, id, composite, index});
  return id;
}

uint32_t SPIRVBuilder::op_vector_shuffle(uint32_t result_type, uint32_t vec1, uint32_t vec2, const std::vector<uint32_t> &components) {
  uint32_t id = allocate_id();
  std::vector<uint32_t> operands = {result_type, id, vec1, vec2};
  operands.insert(operands.end(), components.begin(), components.end());
  emit_instruction(m_function_bodies, spv::OpVectorShuffle, operands);
  return id;
}

#define SPIRV_BINARY_OP(name, opcode)                                         \
  uint32_t SPIRVBuilder::name(uint32_t result_type, uint32_t a, uint32_t b) { \
    uint32_t id = allocate_id();                                              \
    emit_instruction(m_function_bodies, opcode, {result_type, id, a, b});     \
    return id;                                                                \
  }

#define SPIRV_UNARY_OP(name, opcode)                                         \
  uint32_t SPIRVBuilder::name(uint32_t result_type, uint32_t operand) {      \
    uint32_t id = allocate_id();                                             \
    emit_instruction(m_function_bodies, opcode, {result_type, id, operand}); \
    return id;                                                               \
  }

SPIRV_BINARY_OP(op_f_add, spv::OpFAdd)
SPIRV_BINARY_OP(op_f_sub, spv::OpFSub)
SPIRV_BINARY_OP(op_f_mul, spv::OpFMul)
SPIRV_BINARY_OP(op_f_div, spv::OpFDiv)
SPIRV_UNARY_OP(op_f_negate, spv::OpFNegate)
SPIRV_BINARY_OP(op_f_mod, spv::OpFMod)

SPIRV_BINARY_OP(op_i_add, spv::OpIAdd)
SPIRV_BINARY_OP(op_i_sub, spv::OpISub)
SPIRV_BINARY_OP(op_i_mul, spv::OpIMul)
SPIRV_BINARY_OP(op_s_div, spv::OpSDiv)
SPIRV_BINARY_OP(op_s_mod, spv::OpSMod)
SPIRV_UNARY_OP(op_s_negate, spv::OpSNegate)

SPIRV_BINARY_OP(op_f_ord_equal, spv::OpFOrdEqual)
SPIRV_BINARY_OP(op_f_ord_not_equal, spv::OpFOrdNotEqual)
SPIRV_BINARY_OP(op_f_ord_less_than, spv::OpFOrdLessThan)
SPIRV_BINARY_OP(op_f_ord_greater_than, spv::OpFOrdGreaterThan)
SPIRV_BINARY_OP(op_f_ord_less_than_equal, spv::OpFOrdLessThanEqual)
SPIRV_BINARY_OP(op_f_ord_greater_than_equal, spv::OpFOrdGreaterThanEqual)

SPIRV_BINARY_OP(op_i_equal, spv::OpIEqual)
SPIRV_BINARY_OP(op_i_not_equal, spv::OpINotEqual)
SPIRV_BINARY_OP(op_s_less_than, spv::OpSLessThan)
SPIRV_BINARY_OP(op_s_greater_than, spv::OpSGreaterThan)
SPIRV_BINARY_OP(op_s_less_than_equal, spv::OpSLessThanEqual)
SPIRV_BINARY_OP(op_s_greater_than_equal, spv::OpSGreaterThanEqual)

SPIRV_BINARY_OP(op_logical_and, spv::OpLogicalAnd)
SPIRV_BINARY_OP(op_logical_or, spv::OpLogicalOr)
SPIRV_UNARY_OP(op_logical_not, spv::OpLogicalNot)

#undef SPIRV_BINARY_OP
#undef SPIRV_UNARY_OP

uint32_t SPIRVBuilder::op_select(uint32_t result_type, uint32_t condition, uint32_t true_val, uint32_t false_val) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpSelect, {result_type, id, condition, true_val, false_val});
  return id;
}

uint32_t SPIRVBuilder::op_phi(uint32_t result_type, const std::vector<std::pair<uint32_t, uint32_t>> &incoming) {
  uint32_t id = allocate_id();
  std::vector<uint32_t> operands = {result_type, id};
  for (const auto &[value, block] : incoming) {
    operands.push_back(value);
    operands.push_back(block);
  }
  emit_instruction(m_function_bodies, spv::OpPhi, operands);
  return id;
}

void SPIRVBuilder::op_branch(uint32_t target_label) {
  emit_instruction(m_function_bodies, spv::OpBranch, {target_label});
}

void SPIRVBuilder::op_branch_conditional(uint32_t condition, uint32_t true_label, uint32_t false_label) {
  emit_instruction(m_function_bodies, spv::OpBranchConditional, {condition, true_label, false_label});
}

void SPIRVBuilder::op_selection_merge(uint32_t merge_label, spv::SelectionControlMask control) {
  emit_instruction(m_function_bodies, spv::OpSelectionMerge, {merge_label, static_cast<uint32_t>(control)});
}

void SPIRVBuilder::op_loop_merge(uint32_t merge_label, uint32_t continue_label, spv::LoopControlMask control) {
  emit_instruction(m_function_bodies, spv::OpLoopMerge, {merge_label, continue_label, static_cast<uint32_t>(control)});
}

void SPIRVBuilder::op_return() {
  emit_instruction(m_function_bodies, spv::OpReturn, {});
}

void SPIRVBuilder::op_return_value(uint32_t value) {
  emit_instruction(m_function_bodies, spv::OpReturnValue, {value});
}

void SPIRVBuilder::op_kill() {
  emit_instruction(m_function_bodies, spv::OpKill, {});
}

void SPIRVBuilder::op_unreachable() {
  emit_instruction(m_function_bodies, spv::OpUnreachable, {});
}

uint32_t SPIRVBuilder::op_function_call(uint32_t result_type, uint32_t function, const std::vector<uint32_t> &args) {
  uint32_t id = allocate_id();
  std::vector<uint32_t> operands = {result_type, id, function};
  operands.insert(operands.end(), args.begin(), args.end());
  emit_instruction(m_function_bodies, spv::OpFunctionCall, operands);
  return id;
}

uint32_t SPIRVBuilder::op_matrix_times_vector(uint32_t result_type, uint32_t matrix, uint32_t vector) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpMatrixTimesVector, {result_type, id, matrix, vector});
  return id;
}

uint32_t SPIRVBuilder::op_matrix_times_matrix(uint32_t result_type, uint32_t a, uint32_t b) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpMatrixTimesMatrix, {result_type, id, a, b});
  return id;
}

uint32_t SPIRVBuilder::op_vector_times_scalar(uint32_t result_type, uint32_t vector, uint32_t scalar) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpVectorTimesScalar, {result_type, id, vector, scalar});
  return id;
}

uint32_t SPIRVBuilder::op_matrix_times_scalar(uint32_t result_type, uint32_t matrix, uint32_t scalar) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpMatrixTimesScalar, {result_type, id, matrix, scalar});
  return id;
}

uint32_t SPIRVBuilder::op_vector_times_matrix(uint32_t result_type, uint32_t vector, uint32_t matrix) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpVectorTimesMatrix, {result_type, id, vector, matrix});
  return id;
}

uint32_t SPIRVBuilder::op_dot(uint32_t result_type, uint32_t a, uint32_t b) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpDot, {result_type, id, a, b});
  return id;
}

uint32_t SPIRVBuilder::op_transpose(uint32_t result_type, uint32_t matrix) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpTranspose, {result_type, id, matrix});
  return id;
}

uint32_t SPIRVBuilder::op_convert_f_to_s(uint32_t result_type, uint32_t value) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpConvertFToS, {result_type, id, value});
  return id;
}

uint32_t SPIRVBuilder::op_convert_s_to_f(uint32_t result_type, uint32_t value) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpConvertSToF, {result_type, id, value});
  return id;
}

uint32_t SPIRVBuilder::op_convert_f_to_u(uint32_t result_type, uint32_t value) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpConvertFToU, {result_type, id, value});
  return id;
}

uint32_t SPIRVBuilder::op_convert_u_to_f(uint32_t result_type, uint32_t value) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpConvertUToF, {result_type, id, value});
  return id;
}

uint32_t SPIRVBuilder::op_bitcast(uint32_t result_type, uint32_t value) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpBitcast, {result_type, id, value});
  return id;
}

uint32_t SPIRVBuilder::op_image_sample_implicit_lod(uint32_t result_type, uint32_t sampled_image, uint32_t coordinate) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpImageSampleImplicitLod, {result_type, id, sampled_image, coordinate});
  return id;
}

uint32_t SPIRVBuilder::op_dpdx(uint32_t result_type, uint32_t operand) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpDPdx, {result_type, id, operand});
  return id;
}

uint32_t SPIRVBuilder::op_dpdy(uint32_t result_type, uint32_t operand) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpDPdy, {result_type, id, operand});
  return id;
}

uint32_t SPIRVBuilder::op_fwidth(uint32_t result_type, uint32_t operand) {
  uint32_t id = allocate_id();
  emit_instruction(m_function_bodies, spv::OpFwidth, {result_type, id, operand});
  return id;
}

uint32_t SPIRVBuilder::op_ext_inst(uint32_t result_type, uint32_t ext_set, uint32_t instruction, const std::vector<uint32_t> &operands) {
  uint32_t id = allocate_id();
  std::vector<uint32_t> all_operands = {result_type, id, ext_set, instruction};
  all_operands.insert(all_operands.end(), operands.begin(), operands.end());
  emit_instruction(m_function_bodies, spv::OpExtInst, all_operands);
  return id;
}

std::vector<uint32_t> SPIRVBuilder::finalize() {
  std::vector<uint32_t> module;

  module.push_back(spv::MagicNumber);
  module.push_back(0x00010600);
  module.push_back(0);
  module.push_back(m_next_id);
  module.push_back(0);

  module.insert(module.end(), m_capabilities.begin(), m_capabilities.end());
  module.insert(module.end(), m_extensions.begin(), m_extensions.end());
  module.insert(module.end(), m_ext_inst_imports.begin(), m_ext_inst_imports.end());
  module.insert(module.end(), m_memory_model.begin(), m_memory_model.end());
  module.insert(module.end(), m_entry_points.begin(), m_entry_points.end());
  module.insert(module.end(), m_execution_modes.begin(), m_execution_modes.end());
  module.insert(module.end(), m_debug_names.begin(), m_debug_names.end());
  module.insert(module.end(), m_annotations.begin(), m_annotations.end());
  module.insert(module.end(), m_types_constants_globals.begin(), m_types_constants_globals.end());
  module.insert(module.end(), m_function_bodies.begin(), m_function_bodies.end());

  return module;
}

} // namespace astralix
