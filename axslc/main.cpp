#include "args.hpp"
#include "output.hpp"
#include "shader-lang/compiler.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

void print_usage(const char *prog) {
  std::cout << "Usage: " << prog << " [options] <input.axsl>\n\n"
            << "Options:\n"
            << "  -o, --output <dir>         Output directory (default: "
               "current directory)\n"
            << "  -I, --include-path <path>  Include search path for @include "
               "directives\n"
            << "  -v, --verbose              Enable verbose output\n"
            << "  -h, --help                 Display this help message\n"
            << "  --version                  Display version information\n\n"
            << "Examples:\n"
            << "  " << prog << " shader.axsl\n"
            << "  " << prog << " -o build/ shader.axsl\n"
            << "  " << prog << " -I ./shaders -o build/ light.axsl\n";
}

void print_version() {
  std::cout << "axslc - Astralix Shader Language Compiler\n"
            << "Version 0.1.0\n";
}

int main(int argc, char **argv) {
  Options opts;

  if (!parse_args(argc, argv, opts)) {
    std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
    return 1;
  }

  if (opts.help) {
    print_usage(argv[0]);
    return 0;
  }

  if (opts.version) {
    print_version();
    return 0;
  }

  std::ifstream file(opts.input_file);
  if (!file) {
    std::cerr << "error: cannot open '" << opts.input_file << "'\n";
    return 1;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();

  if (opts.verbose) {
    std::cout << "Compiling " << opts.input_file << "...\n";
    if (!opts.include_path.empty())
      std::cout << "Include path: " << opts.include_path << '\n';
  }

  std::string base_path;
  if (!opts.include_path.empty()) {
    base_path = opts.include_path;
  } else {
    fs::path input_path(opts.input_file);
    if (input_path.has_parent_path())
      base_path = input_path.parent_path().string();
  }

  astralix::Compiler compiler;
  auto result = compiler.compile(
      buffer.str(), base_path, opts.input_file,
      {
          .emit_binding_cpp = true,
          .emit_reflection_ir = true,
          .reflection_ir_format = astralix::SerializationFormat::Json,
      });

  if (!result.ok()) {
    for (const auto &e : result.errors)
      std::cerr << e << '\n';
    return 1;
  }

  if (opts.verbose) {
    std::cout << "Compilation successful. Stages: " << result.stages.size()
              << '\n';
  }

  fs::path out_dir =
      opts.output_dir.empty() ? fs::current_path() : fs::path(opts.output_dir);
  std::vector<fs::path> written_paths;
  std::string output_error;
  if (!write_outputs(result, opts.input_file, out_dir, &output_error,
                     &written_paths)) {
    std::cerr << "error: " << output_error << '\n';
    return 1;
  }

  for (const auto &path : written_paths) {
    std::cout << "wrote " << path.string() << '\n';
  }

  return 0;
}
