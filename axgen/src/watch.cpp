#include "watch.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <thread>

namespace axgen {

namespace {

struct WatchedFileState {
  bool exists = false;
  std::filesystem::file_time_type write_time{};

  bool operator==(const WatchedFileState &) const = default;
};

using WatchSnapshot = std::map<std::filesystem::path, WatchedFileState>;

WatchSnapshot
capture_watch_snapshot(const std::vector<std::filesystem::path> &paths) {
  WatchSnapshot snapshot;
  for (const auto &path : paths) {
    const auto normalized = path.lexically_normal();
    WatchedFileState state;
    state.exists = std::filesystem::exists(normalized);
    if (state.exists) {
      state.write_time = std::filesystem::last_write_time(normalized);
    }
    snapshot.emplace(normalized, state);
  }
  return snapshot;
}

} // namespace

int run_watch_loop(const Options &options, SyncRunner run_once) {
  std::cout << "axgen sync-shaders: watching for changes\n";

  auto context = run_once(options);
  auto snapshot = capture_watch_snapshot(context.watch_paths);

  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    if (context.watch_paths.empty()) {
      context = run_once(options);
      snapshot = capture_watch_snapshot(context.watch_paths);
      continue;
    }

    auto current_snapshot = capture_watch_snapshot(context.watch_paths);
    if (current_snapshot == snapshot) {
      continue;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    context = run_once(options);
    snapshot = capture_watch_snapshot(context.watch_paths);
  }
}

} // namespace axgen
