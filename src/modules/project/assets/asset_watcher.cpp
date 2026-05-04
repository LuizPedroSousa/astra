#include "assets/asset_watcher.hpp"

#include "log.hpp"

#include <algorithm>

namespace astralix {

AssetWatcher::AssetWatcher(Config config) : m_config(std::move(config)) {}

AssetWatcher::~AssetWatcher() { stop(); }

void AssetWatcher::start() {
  if (m_running.load()) return;
  m_running.store(true);
  m_thread = std::thread([this]() { watch_loop(); });
  LOG_INFO("AssetWatcher: started");
}

void AssetWatcher::stop() {
  if (!m_running.load()) return;
  m_running.store(false);
  if (m_thread.joinable()) {
    m_thread.join();
  }
  LOG_INFO("AssetWatcher: stopped");
}

void AssetWatcher::clear() {
  std::lock_guard lock(m_mutex);
  m_watched_files.clear();
  m_pending_reloads.clear();
  LOG_INFO("AssetWatcher: cleared all watched files");
}

void AssetWatcher::register_file(
    const std::string &asset_key,
    const std::filesystem::path &absolute_path
) {
  std::lock_guard lock(m_mutex);

  auto path_key = absolute_path.string();
  if (m_watched_files.contains(path_key)) {
    return;
  }

  std::error_code error_code;
  auto mtime = std::filesystem::last_write_time(absolute_path, error_code);
  if (error_code) {
    mtime = std::filesystem::file_time_type::min();
  }

  m_watched_files.emplace(path_key, WatchedFile{
      .path = absolute_path,
      .last_mtime = mtime,
      .asset_key = asset_key,
  });

  LOG_INFO("AssetWatcher: registered", asset_key, "->", absolute_path.string());
}

std::vector<std::string> AssetWatcher::poll_changed() {
  std::lock_guard lock(m_mutex);
  auto result = std::move(m_pending_reloads);
  m_pending_reloads.clear();
  return result;
}

void AssetWatcher::watch_loop() {
  while (m_running.load()) {
    std::this_thread::sleep_for(m_config.poll_interval);
    if (!m_running.load()) break;

    std::lock_guard lock(m_mutex);

    for (auto &[path_key, watched] : m_watched_files) {
      std::error_code error_code;
      auto current_mtime =
          std::filesystem::last_write_time(watched.path, error_code);
      if (error_code) continue;
      if (current_mtime <= watched.last_mtime) continue;

      watched.last_mtime = current_mtime;

      if (std::find(
              m_pending_reloads.begin(),
              m_pending_reloads.end(),
              watched.asset_key
          ) == m_pending_reloads.end()) {
        LOG_INFO("AssetWatcher: change detected in", watched.asset_key);
        m_pending_reloads.push_back(watched.asset_key);
      }
    }
  }
}

} // namespace astralix
