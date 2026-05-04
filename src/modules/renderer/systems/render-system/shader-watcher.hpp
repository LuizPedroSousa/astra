#pragma once

#include "guid.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace astralix {

class ShaderWatcher {
public:
  struct Config {
    std::vector<std::filesystem::path> watch_directories;
    std::chrono::milliseconds poll_interval{500};
  };

  explicit ShaderWatcher(Config config);
  ~ShaderWatcher();

  ShaderWatcher(const ShaderWatcher &) = delete;
  ShaderWatcher &operator=(const ShaderWatcher &) = delete;

  void start();
  void stop();

  void register_source(
      const ResourceDescriptorID &descriptor_id,
      const std::filesystem::path &resolved_path
  );

  std::vector<ResourceDescriptorID> poll_changed();

private:
  void watch_loop();

  struct WatchedFile {
    std::filesystem::path path;
    std::filesystem::file_time_type last_mtime;
    std::vector<ResourceDescriptorID> descriptor_ids;
  };

  Config m_config;
  std::mutex m_mutex;
  std::unordered_map<std::string, WatchedFile> m_watched_files;
  std::vector<ResourceDescriptorID> m_pending_reloads;
  std::thread m_thread;
  std::atomic<bool> m_running{false};
};

} // namespace astralix
