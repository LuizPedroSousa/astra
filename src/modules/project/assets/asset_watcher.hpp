#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace astralix {

class AssetWatcher {
public:
  struct Config {
    std::chrono::milliseconds poll_interval{500};
  };

  explicit AssetWatcher(Config config);
  ~AssetWatcher();

  AssetWatcher(const AssetWatcher &) = delete;
  AssetWatcher &operator=(const AssetWatcher &) = delete;

  void start();
  void stop();
  void clear();

  void register_file(
      const std::string &asset_key,
      const std::filesystem::path &absolute_path
  );

  std::vector<std::string> poll_changed();

private:
  void watch_loop();

  struct WatchedFile {
    std::filesystem::path path;
    std::filesystem::file_time_type last_mtime;
    std::string asset_key;
  };

  Config m_config;
  std::mutex m_mutex;
  std::unordered_map<std::string, WatchedFile> m_watched_files;
  std::vector<std::string> m_pending_reloads;
  std::thread m_thread;
  std::atomic<bool> m_running{false};
};

} // namespace astralix
