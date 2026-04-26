#pragma once
#include "shader-lang/ast.hpp"
#include "shader-lang/artifacts/artifact-types.hpp"
#include "shader-lang/emitters/pipeline-layout-ir-emitter.hpp"
#include "shader-lang/emitters/reflection-ir-emitter.hpp"
#include "shader-lang/lowering/layout-assignment.hpp"
#include "shader-lang/pipeline-layout.hpp"
#include "shader-lang/reflection.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astralix {

struct CompileOptions {
  bool emit_binding_cpp = false;
  std::string binding_cpp_input_path;
  bool emit_reflection_ir = false;
  SerializationFormat reflection_ir_format = SerializationFormat::Json;
  bool emit_pipeline_layout_ir = false;
  SerializationFormat pipeline_layout_ir_format = SerializationFormat::Json;
  bool emit_spirv = false;
  bool emit_vulkan_glsl = false;
};

struct CompileResult {
  std::map<StageKind, std::string> stages;
  std::map<StageKind, std::vector<uint32_t>> spirv_stages;
  std::map<StageKind, std::string> vulkan_glsl_stages;
  ShaderReflection reflection;
  ShaderPipelineLayout merged_layout;
  std::vector<std::filesystem::path> dependencies;
  std::optional<ReflectionIRArtifact> reflection_ir;
  std::optional<PipelineLayoutIRArtifact> pipeline_layout_ir;
  std::optional<std::string> binding_cpp_header;
  std::vector<std::string> errors;
  bool ok() const { return errors.empty(); }
};

class Compiler {
public:
  CompileResult compile(std::string_view source,
                        std::string_view base_path = {},
                        std::string_view filename = {},
                        const CompileOptions &options = {});
  CompileResult compile_with_shared_layout_state(
      std::string_view source,
      std::string_view base_path = {},
      std::string_view filename = {},
      const CompileOptions &options = {}
  );

  ShaderArtifactPlan
  build_artifact_plan(const std::vector<ShaderArtifactInput> &inputs,
                      const ShaderArtifactBuildOptions &options = {});

private:
  CompileResult compile_with_layout_assignment(
      LayoutAssignment &layout_assignment,
      std::string_view source,
      std::string_view base_path,
      std::string_view filename,
      const CompileOptions &options
  );

  LayoutAssignment m_shared_layout_assignment;
};

} // namespace astralix
