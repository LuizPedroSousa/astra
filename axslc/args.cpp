#include "args.hpp"
#include <iostream>
#include <unordered_map>

std::vector<Token> tokenize(int argc, char **argv) {
  std::vector<Token> tokens;
  tokens.reserve(argc - 1);
  for (int i = 1; i < argc; ++i) {
    std::string_view sv = argv[i];
    Token::Kind kind = (sv.size() > 1 && sv[0] == '-') ? Token::Kind::Flag
                                                       : Token::Kind::Value;
    tokens.push_back({kind, sv});
  }
  return tokens;
}

bool parse(const std::vector<Token> &tokens, Options &opts) {
  enum class Flag { Help, Version, Verbose, Output, IncludePath };

  const std::unordered_map<std::string_view, Flag> flag_aliases = {
      {"-h", Flag::Help},
      {"--help", Flag::Help},
      {"--version", Flag::Version},
      {"-v", Flag::Verbose},
      {"--verbose", Flag::Verbose},
      {"-o", Flag::Output},
      {"--output", Flag::Output},
      {"-I", Flag::IncludePath},
      {"--include-path", Flag::IncludePath},
  };

  for (size_t i = 0; i < tokens.size(); ++i) {
    const Token &token = tokens[i];

    if (token.kind == Token::Kind::Value) {
      if (!opts.input_file.empty()) {
        std::cerr << "error: multiple input files specified\n";
        return false;
      }
      opts.input_file = token.text;
      continue;
    }

    auto it = flag_aliases.find(token.text);
    if (it == flag_aliases.end()) {
      std::cerr << "error: unknown option '" << token.text << "'\n";
      return false;
    }

    auto require_next = [&](std::string &destination) -> bool {
      if (i + 1 >= tokens.size() || tokens[i + 1].kind != Token::Kind::Value) {
        std::cerr << "error: " << token.text << " requires an argument\n";
        return false;
      }
      destination = tokens[++i].text;
      return true;
    };

    switch (it->second) {
    case Flag::Help: {
      opts.help = true;
      return true;
    }
    case Flag::Version: {
      opts.version = true;
      return true;
    }
    case Flag::Verbose: {
      opts.verbose = true;
      break;
    }
    case Flag::Output: {
      if (!require_next(opts.output_dir)) {
        return false;
      }

      break;
    }
    case Flag::IncludePath: {
      if (!require_next(opts.include_path)) {
        return false;
      }

      break;
    }
    }
  }

  if (!opts.help && !opts.version && opts.input_file.empty()) {
    std::cerr << "error: no input file specified\n";
    return false;
  }

  return true;
}

bool parse_args(int argc, char **argv, Options &opts) {
  return parse(tokenize(argc, argv), opts);
}
