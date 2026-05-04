#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace astralix {

struct BuildLogLine {
  std::string text;
  std::chrono::steady_clock::time_point timestamp;
};

enum class BuildStatus : uint8_t {
  Idle,
  Building,
  Succeeded,
  Failed,
};

class BuildLogStore {
public:
  static BuildLogStore &get() {
    static BuildLogStore instance;
    return instance;
  }

  void begin_build(const std::string &target) {
    std::lock_guard lock(m_mutex);
    m_lines.clear();
    m_status = BuildStatus::Building;
    m_target = target;
    m_revision++;
  }

  void append_line(const std::string &line) {
    std::lock_guard lock(m_mutex);
    m_lines.push_back({
        .text = line,
        .timestamp = std::chrono::steady_clock::now(),
    });
    m_revision++;
  }

  void finish_build(bool success) {
    std::lock_guard lock(m_mutex);
    m_status = success ? BuildStatus::Succeeded : BuildStatus::Failed;
    m_finished_at = std::chrono::steady_clock::now();
    m_revision++;
  }

  struct Snapshot {
    std::vector<BuildLogLine> lines;
    BuildStatus status = BuildStatus::Idle;
    std::string target;
    std::chrono::steady_clock::time_point finished_at;
    uint64_t revision = 0;
  };

  Snapshot snapshot() const {
    std::lock_guard lock(m_mutex);
    return {
        .lines = m_lines,
        .status = m_status,
        .target = m_target,
        .finished_at = m_finished_at,
        .revision = m_revision,
    };
  }

  uint64_t revision() const {
    std::lock_guard lock(m_mutex);
    return m_revision;
  }

private:
  BuildLogStore() = default;

  mutable std::mutex m_mutex;
  std::vector<BuildLogLine> m_lines;
  BuildStatus m_status = BuildStatus::Idle;
  std::string m_target;
  std::chrono::steady_clock::time_point m_finished_at;
  uint64_t m_revision = 0;
};

} // namespace astralix
