#pragma once

#include "args.hpp"

#include <filesystem>
#include <vector>

namespace axgen {

struct RunContext {
  bool ok = false;
  std::vector<std::filesystem::path> watch_paths;
};

using SyncRunner = RunContext (*)(const Options &options);

int run_watch_loop(const Options &options, SyncRunner run_once);

} // namespace axgen
