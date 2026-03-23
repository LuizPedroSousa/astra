#pragma once

#include "shader-lang/compiler.hpp"
#include <filesystem>
#include <string>
#include <vector>

bool write_outputs(const astralix::CompileResult &result,
                   const std::filesystem::path &input_file,
                   const std::filesystem::path &output_dir,
                   std::string *error = nullptr,
                   std::vector<std::filesystem::path> *written_paths = nullptr);
