#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace axgen {

struct Options {
  enum class Command { None, SyncShaders, CookAssets };

  Command command = Command::None;
  std::string manifest_path;
  std::string root_path;
  bool watch = false;
  bool help = false;
};

struct Token {
  enum class Kind { Flag, Value };
  Kind kind;
  std::string text;
};

std::vector<Token> tokenize(int argc, char **argv);
bool parse(const std::vector<Token> &tokens, Options &opts);
bool parse_args(int argc, char **argv, Options &opts);

} // namespace axgen
