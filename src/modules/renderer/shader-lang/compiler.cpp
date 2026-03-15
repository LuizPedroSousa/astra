#include "shader-lang/compiler.hpp"
#include "shader-lang/emitters/opengl-glsl-emitter.hpp"
#include "shader-lang/linker.hpp"
#include "shader-lang/lowering/canonical-lowering.hpp"
#include "shader-lang/lowering/glsl-lowering.hpp"
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
                                std::string_view filename) {
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
    result.errors = link_result.errors;
    return result;
  }

  CanonicalLowering canonical_lowering(link_result.all_nodes);
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

    GlslLoweringResult glsl_result =
        glsl_lowering.lower(canonical_result.stage);
    if (!glsl_result.ok()) {
      result.errors = glsl_result.errors;
      return result;
    }

    result.stages[func_decl.stage_kind.value()] =
        emitter.emit(glsl_result.stage);
  }

  return result;
}

} // namespace astralix
