#include "shader-lang/compiler.hpp"
#include "shader-lang/artifacts/shader-artifact-pipeline.hpp"
#include "shader-lang/emitters/binding-cpp-emitter.hpp"
#include "shader-lang/emitters/glsl-text-emitter.hpp"
#include "shader-lang/emitters/pipeline-layout-ir-emitter.hpp"
#include "shader-lang/emitters/reflection-ir-emitter.hpp"
#include "shader-lang/emitters/vulkan-spirv-emitter.hpp"
#include "shader-lang/layout-merge.hpp"
#include "shader-lang/linker.hpp"
#include "shader-lang/lowering/canonical-lowering.hpp"
#include "shader-lang/lowering/glsl-stage-clone.hpp"
#include "shader-lang/lowering/glsl-lowering.hpp"
#include "shader-lang/lowering/layout-assignment.hpp"
#include "shader-lang/lowering/opengl-target-lowering.hpp"
#include "shader-lang/lowering/vulkan-target-lowering.hpp"
#include "shader-lang/parser.hpp"
#include "shader-lang/tokenizer.hpp"

namespace astralix {

static std::pair<std::vector<Token>, std::vector<std::string>>
tokenize_source(std::string_view src, std::string_view filename = "") {
  Tokenizer tokenizer(src, filename);
  std::vector<Token> tokens;

  while (true) {
    Token token = tokenizer.next();
    tokens.push_back(token);
    if (token.kind == TokenKind::EoF)
      break;
  }

  return {std::move(tokens), tokenizer.errors()};
}

static std::string stage_kind_label(StageKind stage) {
  switch (stage) {
    case StageKind::Vertex:
      return "vertex";
    case StageKind::Fragment:
      return "fragment";
    case StageKind::Geometry:
      return "geometry";
    case StageKind::Compute:
      return "compute";
  }

  return "unknown";
}

CompileResult Compiler::compile(std::string_view source, std::string_view base_path, std::string_view filename, const CompileOptions &options) {
  LayoutAssignment layout_assignment;
  return compile_with_layout_assignment(
      layout_assignment, source, base_path, filename, options
  );
}

CompileResult Compiler::compile_with_shared_layout_state(
    std::string_view source,
    std::string_view base_path,
    std::string_view filename,
    const CompileOptions &options
) {
  return compile_with_layout_assignment(
      m_shared_layout_assignment, source, base_path, filename, options
  );
}

CompileResult Compiler::compile_with_layout_assignment(
    LayoutAssignment &layout_assignment,
    std::string_view source,
    std::string_view base_path,
    std::string_view filename,
    const CompileOptions &options
) {
  CompileResult result;

  auto [tokens, token_errors] = tokenize_source(source, filename);

  if (!token_errors.empty()) {
    result.errors = token_errors;
    return result;
  }

  Parser main_parser(std::move(tokens), source);
  Program program = main_parser.parse();

  if (!main_parser.errors().empty()) {
    result.errors = main_parser.errors();
    return result;
  }

  Linker linker;
  LinkResult link_result = linker.link(program, main_parser.nodes(), base_path);

  if (!link_result.ok()) {
    result.dependencies = link_result.dependencies;
    result.errors = link_result.errors;
    return result;
  }

  result.dependencies = link_result.dependencies;

  CanonicalLowering canonical_lowering(link_result.all_nodes);
  GLSLLowering glsl_lowering;
  GLSLTextEmitter text_emitter;
  OpenGLTargetLowering opengl_lowering;
  VulkanTargetLowering vulkan_lowering;
  LayoutMergeState merge_state;

  for (NodeID sid : program.stages) {
    const auto &func_decl = std::get<FuncDecl>(link_result.all_nodes[sid].data);
    const StageKind stage_kind = func_decl.stage_kind.value();
    CanonicalLoweringResult canonical_result = canonical_lowering.lower(
        program, link_result, stage_kind
    );

    if (!canonical_result.ok()) {
      result.errors = canonical_result.errors;
      return result;
    }

    LayoutAssignmentResult layout_result =
        layout_assignment.assign(canonical_result.reflection);
    if (!layout_result.ok()) {
      result.errors = layout_result.errors;
      return result;
    }

    const std::string source_label =
        std::string(filename.empty() ? "<memory>" : filename) + " [" +
        stage_kind_label(stage_kind) + "]";
    if (!merge_pipeline_layout_checked(
            result.merged_layout,
            layout_result.layout,
            source_label,
            merge_state,
            result.errors
        )) {
      return result;
    }

    if (options.emit_spirv) {
      VulkanSPIRVEmitter spirv_emitter;
      auto spirv_result = spirv_emitter.emit(
          canonical_result.stage, layout_result.reflection, layout_result.layout
      );
      if (!spirv_result.ok()) {
        result.errors = spirv_result.errors;
        return result;
      }
      result.spirv_stages[func_decl.stage_kind.value()] =
          std::move(spirv_result.spirv);
    }

    GlslLoweringResult glsl_result =
        glsl_lowering.lower(canonical_result.stage, layout_result.reflection);
    if (!glsl_result.ok()) {
      result.errors = glsl_result.errors;
      return result;
    }

    GLSLStage shared_stage = std::move(glsl_result.stage);
    std::optional<GLSLStage> vk_stage;
    if (options.emit_vulkan_glsl) {
      vk_stage.emplace(clone_glsl_stage(shared_stage));
    }

    GLSLStage gl_stage = std::move(shared_stage);
    opengl_lowering.lower(gl_stage, layout_result.layout, stage_kind);
    result.stages[stage_kind] = text_emitter.emit(gl_stage);

    if (vk_stage) {
      vulkan_lowering.lower(*vk_stage, layout_result.layout, stage_kind);
      result.vulkan_glsl_stages[stage_kind] = text_emitter.emit(*vk_stage);
    }

    result.reflection.stages[stage_kind] =
        std::move(glsl_result.reflection);
  }

  if (options.emit_reflection_ir) {
    ReflectionIREmitter reflection_ir_emitter;
    std::string reflection_error;
    auto reflection_ir = reflection_ir_emitter.emit(
        result.reflection, options.reflection_ir_format, &reflection_error
    );
    if (!reflection_ir) {
      result.errors.push_back(std::move(reflection_error));
      return result;
    }

    result.reflection_ir = std::move(*reflection_ir);
  }

  if (options.emit_pipeline_layout_ir) {
    PipelineLayoutIREmitter layout_ir_emitter;
    std::string layout_error;
    auto layout_ir = layout_ir_emitter.emit(
        result.merged_layout, options.pipeline_layout_ir_format, &layout_error
    );
    if (!layout_ir) {
      result.errors.push_back(std::move(layout_error));
      return result;
    }

    result.pipeline_layout_ir = std::move(*layout_ir);
  }

  if (options.emit_binding_cpp) {
    BindingCppEmitter binding_cpp_emitter;
    std::string binding_error;
    std::string_view binding_input_path =
        options.binding_cpp_input_path.empty()
            ? filename
            : std::string_view(options.binding_cpp_input_path);
    auto header = binding_cpp_emitter.emit(
        result.reflection, binding_input_path, &binding_error
    );
    if (!header) {
      result.errors.push_back(std::move(binding_error));
      return result;
    }

    result.binding_cpp_header = std::move(*header);
  }

  return result;
}

ShaderArtifactPlan
Compiler::build_artifact_plan(const std::vector<ShaderArtifactInput> &inputs, const ShaderArtifactBuildOptions &options) {
  ShaderArtifactPipeline pipeline;
  return pipeline.build_plan(inputs, options);
}

} // namespace astralix
