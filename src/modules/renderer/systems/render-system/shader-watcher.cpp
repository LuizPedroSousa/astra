#include "shader-watcher.hpp"
#include "log.hpp"

#include <algorithm>
#include <unordered_set>

namespace astralix {

ShaderWatcher::ShaderWatcher(Config config) : m_config(std::move(config)) {}

ShaderWatcher::~ShaderWatcher() { stop(); }

void ShaderWatcher::start() {
  if (m_running.load()) return;
  m_running.store(true);
  m_thread = std::thread([this]() { watch_loop(); });
}

void ShaderWatcher::stop() {
  m_running.store(false);
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void ShaderWatcher::register_source(
    const ResourceDescriptorID &descriptor_id,
    const std::filesystem::path &resolved_path
) {
  std::lock_guard lock(m_mutex);

  auto key = resolved_path.string();
  auto it = m_watched_files.find(key);

  if (it != m_watched_files.end()) {
    auto &ids = it->second.descriptor_ids;
    if (std::find(ids.begin(), ids.end(), descriptor_id) == ids.end()) {
      ids.push_back(descriptor_id);
    }
    return;
  }

  std::error_code error_code;
  auto mtime = std::filesystem::last_write_time(resolved_path, error_code);
  if (error_code) {
    mtime = std::filesystem::file_time_type::min();
  }

  m_watched_files.emplace(key, WatchedFile{
      .path = resolved_path,
      .last_mtime = mtime,
      .descriptor_ids = {descriptor_id},
  });
}

std::vector<ResourceDescriptorID> ShaderWatcher::poll_changed() {
  std::lock_guard lock(m_mutex);
  auto result = std::move(m_pending_reloads);
  m_pending_reloads.clear();
  return result;
}

void ShaderWatcher::watch_loop() {
  while (m_running.load()) {
    std::this_thread::sleep_for(m_config.poll_interval);
    if (!m_running.load()) break;

    std::unordered_set<ResourceDescriptorID> changed_descriptors;

    {
      std::lock_guard lock(m_mutex);

      for (auto &[key, watched] : m_watched_files) {
        std::error_code error_code;
        auto current_mtime =
            std::filesystem::last_write_time(watched.path, error_code);
        if (error_code) continue;
        if (current_mtime <= watched.last_mtime) continue;

        watched.last_mtime = current_mtime;

        for (const auto &descriptor_id : watched.descriptor_ids) {
          changed_descriptors.insert(descriptor_id);
        }
      }

      if (!changed_descriptors.empty()) {
        for (const auto &descriptor_id : changed_descriptors) {
          if (std::find(m_pending_reloads.begin(), m_pending_reloads.end(),
                        descriptor_id) == m_pending_reloads.end()) {
            m_pending_reloads.push_back(descriptor_id);
          }
        }
      }
    }
  }
}

} // namespace astralix
