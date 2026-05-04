#include "args.hpp"

#include <iostream>
#include <unordered_map>

namespace axgen {

std::vector<Token> tokenize(int argc, char **argv) {
  std::vector<Token> tokens;
  tokens.reserve(argc > 1 ? static_cast<size_t>(argc - 1) : 0u);

  for (int i = 1; i < argc; ++i) {
    std::string_view value = argv[i];
    Token::Kind kind = (value.size() > 1 && value[0] == '-')
                           ? Token::Kind::Flag
                           : Token::Kind::Value;
    tokens.push_back({kind, std::string(value)});
  }

  return tokens;
}

bool parse(const std::vector<Token> &tokens, Options &opts) {
  enum class Flag { Help, Manifest, Root, Watch };

  const std::unordered_map<std::string_view, Flag> flags = {
      {"-h", Flag::Help},
      {"--help", Flag::Help},
      {"--manifest", Flag::Manifest},
      {"--root", Flag::Root},
      {"--watch", Flag::Watch},
  };

  bool command_seen = false;

  for (size_t i = 0; i < tokens.size(); ++i) {
    const auto &token = tokens[i];

    if (token.kind == Token::Kind::Value) {
      if (!command_seen) {
        if (token.text != "sync-shaders" && token.text != "cook-assets") {
          std::cerr << "error: unknown command '" << token.text << "'\n";
          return false;
        }

        opts.command = token.text == "sync-shaders"
                           ? Options::Command::SyncShaders
                           : Options::Command::CookAssets;
        command_seen = true;
        continue;
      }

      std::cerr << "error: unexpected argument '" << token.text << "'\n";
      return false;
    }

    auto it = flags.find(token.text);
    if (it == flags.end()) {
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
    case Flag::Help:
      opts.help = true;
      break;
    case Flag::Manifest:
      if (!require_next(opts.manifest_path)) {
        return false;
      }
      break;
    case Flag::Root:
      if (!require_next(opts.root_path)) {
        return false;
      }
      break;
    case Flag::Watch:
      opts.watch = true;
      break;
    }
  }

  if (!opts.help && opts.command == Options::Command::None) {
    std::cerr << "error: no command specified\n";
    return false;
  }

  if (opts.command == Options::Command::CookAssets && opts.watch) {
    std::cerr << "error: --watch is only supported by sync-shaders\n";
    return false;
  }

  return true;
}

bool parse_args(int argc, char **argv, Options &opts) {
  return parse(tokenize(argc, argv), opts);
}

} // namespace axgen
