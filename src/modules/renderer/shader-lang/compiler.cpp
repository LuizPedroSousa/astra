#include "shader-lang/compiler.hpp"
#include "shader-lang/artifacts/shader-artifact-pipeline.hpp"
#include "shader-lang/emitters/binding-cpp-emitter.hpp"
#include "shader-lang/emitters/opengl-glsl-emitter.hpp"
#include "shader-lang/emitters/reflection-ir-emitter.hpp"
#include "shader-lang/linker.hpp"
#include "shader-lang/lowering/canonical-lowering.hpp"
#include "shader-lang/lowering/glsl-lowering.hpp"
#include "shader-lang/lowering/layout-assignment.hpp"
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

CompileResult Compiler::compile(std::string_view source,
                                std::string_view base_path,
                                std::string_view filename,
                                const CompileOptions &options) {
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
  LayoutAssignment layout_assignment;
  GLSLLowering glsl_lowering;
  OpenGLGLSLEmitter emitter;

  for (NodeID sid : program.stages) {
    const auto &func_decl = std::get<FuncDecl>(link_result.all_nodes[sid].data);
    CanonicalLoweringResult canonical_result = canonical_lowering.lower(
        program, link_result, func_decl.stage_kind.value());

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

    GlslLoweringResult glsl_result =
        glsl_lowering.lower(canonical_result.stage, layout_result.reflection);
    if (!glsl_result.ok()) {
      result.errors = glsl_result.errors;
      return result;
    }

    result.stages[func_decl.stage_kind.value()] =
        emitter.emit(glsl_result.stage);
    result.reflection.stages[func_decl.stage_kind.value()] =
        std::move(glsl_result.reflection);
  }

  if (options.emit_reflection_ir) {
    ReflectionIREmitter reflection_ir_emitter;
    std::string reflection_error;
    auto reflection_ir = reflection_ir_emitter.emit(
        result.reflection, options.reflection_ir_format, &reflection_error);
    if (!reflection_ir) {
      result.errors.push_back(std::move(reflection_error));
      return result;
    }

    result.reflection_ir = std::move(*reflection_ir);
  }

  if (options.emit_binding_cpp) {
    BindingCppEmitter binding_cpp_emitter;
    std::string binding_error;
    std::string_view binding_input_path =
        options.binding_cpp_input_path.empty()
            ? filename
            : std::string_view(options.binding_cpp_input_path);
    auto header = binding_cpp_emitter.emit(result.reflection,
                                           binding_input_path, &binding_error);
    if (!header) {
      result.errors.push_back(std::move(binding_error));
      return result;
    }

    result.binding_cpp_header = std::move(*header);
  }

  return result;
}

ShaderArtifactPlan
Compiler::build_artifact_plan(const std::vector<ShaderArtifactInput> &inputs,
                              const ShaderArtifactBuildOptions &options) {
  ShaderArtifactPipeline pipeline;
  return pipeline.build_plan(inputs, options);
}

} // namespace astralix
