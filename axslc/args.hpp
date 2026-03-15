#pragma once
#include <string>
#include <string_view>
#include <vector>

struct Options {
  std::string input_file;
  std::string output_dir;
  std::string include_path;
  bool verbose = false;
  bool help = false;
  bool version = false;
};

struct Token {
  enum class Kind { Flag, Value };
  Kind kind;
  std::string_view text;
};

std::vector<Token> tokenize(int argc, char **argv);
bool parse(const std::vector<Token> &tokens, Options &opts);
bool parse_args(int argc, char **argv, Options &opts);
