#include "shader-lang/compiler.hpp"
#include "shader-lang/emitters/opengl-glsl-emitter.hpp"
#include "shader-lang/linker.hpp"
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

  OpenGLGLSLEmitter emitter(link_result.all_nodes);
  for (NodeID sid : program.stages) {
    const auto &func_decl = std::get<FuncDecl>(link_result.all_nodes[sid].data);
    result.stages[func_decl.stage_kind.value()] =
        emitter.emit(program, func_decl.stage_kind.value(),
                     link_result.uniform_usage[func_decl.stage_kind.value()]);
  }

  return result;
}

} // namespace astralix
