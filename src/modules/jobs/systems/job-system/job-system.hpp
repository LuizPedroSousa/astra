#pragma once

#include "job-callable.hpp"
#include "systems/system.hpp"

#include <cstdint>
#include <limits>
#include <span>

namespace astralix {

enum class JobPriority : uint8_t {
  Low,
  Normal,
  High,
  Critical,
};

enum class JobQueue : uint8_t {
  Worker,
  Main,
  Background,
};

struct JobHandle {
  uint64_t id = 0;

  bool is_valid() const noexcept { return id != 0; }
};

struct JobBarrier {
  uint64_t id = 0;

  bool is_valid() const noexcept { return id != 0; }
};

class JobSystem : public System<JobSystem> {
public:
  struct Config {
    uint32_t worker_count = 0;
  };

  JobSystem();
  explicit JobSystem(Config config);
  ~JobSystem() override;

  static JobSystem *get();

  void start() override;
  void end() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

  JobHandle submit(
      JobCallable work,
      JobQueue queue = JobQueue::Worker,
      JobPriority priority = JobPriority::Normal
  );

  JobHandle submit_after(
      std::span<const JobHandle> dependencies,
      JobCallable work,
      JobQueue queue = JobQueue::Worker,
      JobPriority priority = JobPriority::Normal
  );

  JobBarrier create_barrier(uint32_t expected_count);
  void attach_to_barrier(JobHandle handle, JobBarrier barrier);

  JobHandle submit_after_barrier(
      JobBarrier barrier,
      JobCallable work,
      JobQueue queue = JobQueue::Worker,
      JobPriority priority = JobPriority::Normal
  );

  bool is_complete(JobHandle handle) const;
  void wait(JobHandle handle);
  void wait_all(std::span<const JobHandle> handles);
  void wait_barrier(JobBarrier barrier);

  size_t
  drain_main_queue(size_t max_jobs = std::numeric_limits<size_t>::max());
  bool has_pending_main_work() const;

private:
  JobSystem(const JobSystem &) = delete;
  JobSystem &operator=(const JobSystem &) = delete;

  Config m_config;
  static JobSystem *m_instance;
};

} // namespace astralix
