#pragma once

#include "module-api.h"

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace astralix {

class ModuleHandle {
public:
  struct Config {
    std::filesystem::path module_path;
    std::filesystem::path source_dir;
    std::filesystem::path build_dir;
    std::string build_target;
    bool auto_rebuild = true;
  };

  explicit ModuleHandle(Config config);
  ~ModuleHandle();

  ModuleHandle(const ModuleHandle &) = delete;
  ModuleHandle &operator=(const ModuleHandle &) = delete;

  bool load();
  void unload();
  bool poll_changed();
  const AstraModuleAPI *api() const;
  bool is_loaded() const;
  uint64_t generation() const;

private:
  std::filesystem::path copy_to_temp();
  void start_watcher();
  void stop_watcher();
  void cleanup_temp_files();

  Config m_config;
  void *m_handle = nullptr;
  const AstraModuleAPI *m_api = nullptr;
  uint64_t m_generation = 0;
  std::filesystem::file_time_type m_last_module_mtime{};
  std::filesystem::file_time_type m_last_source_mtime{};
  std::thread m_watcher_thread;
  std::atomic<bool> m_watcher_running{false};
  std::atomic<bool> m_build_in_progress{false};
  std::vector<std::filesystem::path> m_temp_files;
};

} // namespace astralix
