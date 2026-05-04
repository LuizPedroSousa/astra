#include "job-system.hpp"

#include "assert.hpp"
#include "log.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astralix {

namespace {

constexpr std::array<JobPriority, 4> k_priority_order = {
    JobPriority::Critical,
    JobPriority::High,
    JobPriority::Normal,
    JobPriority::Low,
};

const char *queue_name(JobQueue queue) {
  switch (queue) {
    case JobQueue::Worker:
      return "Worker";
    case JobQueue::Main:
      return "Main";
    case JobQueue::Background:
      return "Background";
  }

  return "Unknown";
}

const char *priority_name(JobPriority priority) {
  switch (priority) {
    case JobPriority::Low:
      return "Low";
    case JobPriority::Normal:
      return "Normal";
    case JobPriority::High:
      return "High";
    case JobPriority::Critical:
      return "Critical";
  }

  return "Unknown";
}

size_t priority_index(JobPriority priority) {
  switch (priority) {
    case JobPriority::Low:
      return 0u;
    case JobPriority::Normal:
      return 1u;
    case JobPriority::High:
      return 2u;
    case JobPriority::Critical:
      return 3u;
  }

  return 1u;
}

class JobSystemImpl {
public:
  explicit JobSystemImpl(JobSystem::Config config)
      : m_main_thread_id(std::this_thread::get_id()) {
    uint32_t worker_count = config.worker_count;
    if (worker_count == 0u) {
      const uint32_t hardware_threads = std::thread::hardware_concurrency();
      worker_count = hardware_threads > 1u ? hardware_threads - 1u : 1u;
    }

    m_worker_threads.reserve(worker_count);
    for (uint32_t index = 0; index < worker_count; ++index) {
      m_worker_threads.emplace_back([this]() { worker_loop(); });
    }

    LOG_INFO("[JobSystem] initialized", "workers=", worker_count);
  }

  ~JobSystemImpl() = default;

  JobHandle submit(
      JobCallable work,
      JobQueue queue,
      JobPriority priority
  ) {
    return submit_locked({}, JobBarrier{}, std::move(work), queue, priority);
  }

  JobHandle submit_after(
      std::span<const JobHandle> dependencies,
      JobCallable work,
      JobQueue queue,
      JobPriority priority
  ) {
    return submit_locked(
        dependencies, JobBarrier{}, std::move(work), queue, priority
    );
  }

  JobHandle submit_after_barrier(
      JobBarrier barrier,
      JobCallable work,
      JobQueue queue,
      JobPriority priority
  ) {
    return submit_locked({}, barrier, std::move(work), queue, priority);
  }

  JobBarrier create_barrier(uint32_t expected_count) {
    std::lock_guard lock(m_mutex);

    JobBarrier barrier{.id = m_next_barrier_id++};
    auto &record = m_barriers[barrier.id];
    record.remaining = expected_count;
    record.fired = expected_count == 0u;
    LOG_DEBUG(
        "[JobSystem] created barrier",
        "barrier_id=", barrier.id,
        "expected_count=", expected_count
    );
    return barrier;
  }

  void attach_to_barrier(JobHandle handle, JobBarrier barrier) {
    if (!handle.is_valid() || !barrier.is_valid()) {
      return;
    }

    std::lock_guard lock(m_mutex);

    auto job_it = m_jobs.find(handle.id);
    ASTRA_ENSURE(job_it == m_jobs.end(), "Unknown job handle");

    auto barrier_it = m_barriers.find(barrier.id);
    ASTRA_ENSURE(barrier_it == m_barriers.end(), "Unknown job barrier");

    if (barrier_it->second.fired) {
      return;
    }

    auto &job = job_it->second;
    if (job.complete) {
      complete_barrier_attachment_locked(barrier.id);
      return;
    }

    if (std::find(
            job.attached_barriers.begin(),
            job.attached_barriers.end(),
            barrier.id
        ) != job.attached_barriers.end()) {
      return;
    }

    job.attached_barriers.push_back(barrier.id);
    LOG_DEBUG(
        "[JobSystem] attached job to barrier",
        "job_id=", handle.id,
        "barrier_id=", barrier.id
    );
  }

  bool is_complete(JobHandle handle) const {
    if (!handle.is_valid()) {
      return true;
    }

    std::lock_guard lock(m_mutex);

    auto it = m_jobs.find(handle.id);
    ASTRA_ENSURE(it == m_jobs.end(), "Unknown job handle");
    return it->second.complete;
  }

  void wait(JobHandle handle) {
    if (!handle.is_valid()) {
      return;
    }

    for (;;) {
      {
        std::lock_guard lock(m_mutex);
        auto it = m_jobs.find(handle.id);
        ASTRA_ENSURE(it == m_jobs.end(), "Unknown job handle");
        if (it->second.complete) {
          if (it->second.exception != nullptr) {
            std::rethrow_exception(it->second.exception);
          }
          return;
        }
      }

      if (is_main_thread() && drain_main_queue(1u) > 0u) {
        continue;
      }

      std::unique_lock lock(m_mutex);
      m_completion_cv.wait_for(lock, std::chrono::milliseconds(1));
    }
  }

  void wait_all(std::span<const JobHandle> handles) {
    for (const auto &handle : handles) {
      wait(handle);
    }
  }

  void wait_barrier(JobBarrier barrier) {
    if (!barrier.is_valid()) {
      return;
    }

    for (;;) {
      {
        std::lock_guard lock(m_mutex);
        auto it = m_barriers.find(barrier.id);
        ASTRA_ENSURE(it == m_barriers.end(), "Unknown job barrier");
        if (it->second.fired) {
          return;
        }
      }

      if (is_main_thread() && drain_main_queue(1u) > 0u) {
        continue;
      }

      std::unique_lock lock(m_mutex);
      m_completion_cv.wait_for(lock, std::chrono::milliseconds(1));
    }
  }

  size_t drain_main_queue(size_t max_jobs) {
    ASTRA_ENSURE(
        !is_main_thread(),
        "Main queue jobs must be drained from the initializing thread"
    );

    size_t drained = 0u;

    while (drained < max_jobs) {
      JobCallable work;
      uint64_t job_id = 0u;

      {
        std::lock_guard lock(m_mutex);
        job_id = pop_ready_job_locked(m_main_ready);
        if (job_id == 0u) {
          break;
        }

        auto &job = m_jobs.at(job_id);
        job.queued = false;
        job.running = true;
        ++m_running_jobs;
        work = std::move(job.work);
      }

      execute_job(job_id, std::move(work));
      ++drained;
    }

    if (drained > 0u) {
      LOG_DEBUG("[JobSystem] drained main queue", "jobs=", drained);
    }

    return drained;
  }

  bool has_pending_main_work() const {
    std::lock_guard lock(m_mutex);
    return !m_main_ready.empty();
  }

  void shutdown() {
    wait_for_quiescence();

    {
      std::lock_guard lock(m_mutex);
      m_stopping = true;
    }

    m_ready_cv.notify_all();

    for (auto &worker : m_worker_threads) {
      if (worker.joinable()) {
        worker.join();
      }
    }

    LOG_INFO("[JobSystem] shutdown complete");
  }

private:
  struct ReadyQueues {
    std::array<std::deque<uint64_t>, 4> by_priority;

    bool empty() const {
      for (const auto &queue : by_priority) {
        if (!queue.empty()) {
          return false;
        }
      }
      return true;
    }
  };

  struct JobRecord {
    JobQueue queue = JobQueue::Worker;
    JobPriority priority = JobPriority::Normal;
    JobCallable work;
    std::vector<uint64_t> dependents;
    std::vector<uint64_t> attached_barriers;
    uint32_t remaining_job_dependencies = 0u;
    uint32_t remaining_barrier_dependencies = 0u;
    bool queued = false;
    bool running = false;
    bool complete = false;
    std::exception_ptr exception;
  };

  struct BarrierRecord {
    uint32_t remaining = 0u;
    bool fired = false;
    std::vector<uint64_t> waiting_jobs;
  };

  JobHandle submit_locked(
      std::span<const JobHandle> dependencies,
      JobBarrier barrier,
      JobCallable work,
      JobQueue queue,
      JobPriority priority
  ) {
    ASTRA_ENSURE(!work, "Cannot submit an empty job");

    std::lock_guard lock(m_mutex);
    ASTRA_ENSURE(m_stopping, "JobSystem is shutting down");

    JobHandle handle{.id = m_next_job_id++};
    auto &job = m_jobs[handle.id];
    job.queue = queue;
    job.priority = priority;
    job.work = std::move(work);

    for (const auto &dependency : dependencies) {
      if (!dependency.is_valid()) {
        continue;
      }

      auto dep_it = m_jobs.find(dependency.id);
      ASTRA_ENSURE(dep_it == m_jobs.end(), "Unknown dependency job handle");

      if (dep_it->second.complete) {
        continue;
      }

      dep_it->second.dependents.push_back(handle.id);
      ++job.remaining_job_dependencies;
    }

    if (barrier.is_valid()) {
      auto barrier_it = m_barriers.find(barrier.id);
      ASTRA_ENSURE(barrier_it == m_barriers.end(), "Unknown job barrier");

      if (!barrier_it->second.fired) {
        barrier_it->second.waiting_jobs.push_back(handle.id);
        ++job.remaining_barrier_dependencies;
      }
    }

    ++m_incomplete_jobs;

    LOG_DEBUG(
        "[JobSystem] submitted job",
        "job_id=", handle.id,
        "queue=", queue_name(queue),
        "priority=", priority_name(priority),
        "job_dependencies=", job.remaining_job_dependencies,
        "barrier_dependencies=", job.remaining_barrier_dependencies
    );

    if (job.remaining_job_dependencies == 0u &&
        job.remaining_barrier_dependencies == 0u) {
      enqueue_ready_locked(handle.id, job.queue, job.priority);
      m_ready_cv.notify_one();
    }

    return handle;
  }

  void worker_loop() {
    for (;;) {
      JobCallable work;
      uint64_t job_id = 0u;

      {
        std::unique_lock lock(m_mutex);
        m_ready_cv.wait(lock, [this]() {
          return m_stopping || has_worker_ready_jobs_locked();
        });

        if (m_stopping && !has_worker_ready_jobs_locked()) {
          return;
        }

        job_id = pop_ready_job_locked(m_worker_ready);
        if (job_id == 0u) {
          job_id = pop_ready_job_locked(m_background_ready);
        }

        if (job_id == 0u) {
          continue;
        }

        auto &job = m_jobs.at(job_id);
        job.queued = false;
        job.running = true;
        ++m_running_jobs;
        work = std::move(job.work);
      }

      execute_job(job_id, std::move(work));
    }
  }

  void execute_job(uint64_t job_id, JobCallable work) {
    std::exception_ptr exception;

    try {
      if (work) {
        work();
      }
    } catch (...) {
      exception = std::current_exception();
    }

    std::lock_guard lock(m_mutex);
    complete_job_locked(job_id, exception);
  }

  void complete_job_locked(uint64_t job_id, std::exception_ptr exception) {
    auto job_it = m_jobs.find(job_id);
    if (job_it == m_jobs.end() || job_it->second.complete) {
      return;
    }

    auto &job = job_it->second;
    job.complete = true;
    job.running = false;
    job.exception = exception;

    if (exception != nullptr) {
      LOG_ERROR(
          "[JobSystem] job failed",
          "job_id=", job_id,
          "queue=", queue_name(job.queue),
          "priority=", priority_name(job.priority)
      );
    }

    if (m_running_jobs > 0u) {
      --m_running_jobs;
    }
    if (m_incomplete_jobs > 0u) {
      --m_incomplete_jobs;
    }

    auto dependents = std::move(job.dependents);
    auto barriers = std::move(job.attached_barriers);
    job.dependents.clear();
    job.attached_barriers.clear();

    for (uint64_t barrier_id : barriers) {
      complete_barrier_attachment_locked(barrier_id);
    }

    for (uint64_t dependent_id : dependents) {
      auto dependent_it = m_jobs.find(dependent_id);
      if (dependent_it == m_jobs.end() || dependent_it->second.complete) {
        continue;
      }

      auto &dependent = dependent_it->second;
      if (dependent.remaining_job_dependencies > 0u) {
        --dependent.remaining_job_dependencies;
      }

      if (dependent.remaining_job_dependencies == 0u &&
          dependent.remaining_barrier_dependencies == 0u &&
          !dependent.queued && !dependent.running) {
        enqueue_ready_locked(
            dependent_id, dependent.queue, dependent.priority
        );
      }
    }

    m_completion_cv.notify_all();
    m_ready_cv.notify_all();
  }

  void complete_barrier_attachment_locked(uint64_t barrier_id) {
    auto barrier_it = m_barriers.find(barrier_id);
    if (barrier_it == m_barriers.end() || barrier_it->second.fired) {
      return;
    }

    auto &barrier = barrier_it->second;
    if (barrier.remaining > 0u) {
      --barrier.remaining;
    }

    if (barrier.remaining != 0u) {
      return;
    }

    barrier.fired = true;
    auto waiting_jobs = std::move(barrier.waiting_jobs);
    barrier.waiting_jobs.clear();

    for (uint64_t waiting_job_id : waiting_jobs) {
      auto job_it = m_jobs.find(waiting_job_id);
      if (job_it == m_jobs.end() || job_it->second.complete) {
        continue;
      }

      auto &job = job_it->second;
      if (job.remaining_barrier_dependencies > 0u) {
        --job.remaining_barrier_dependencies;
      }

      if (job.remaining_job_dependencies == 0u &&
          job.remaining_barrier_dependencies == 0u &&
          !job.queued && !job.running) {
        enqueue_ready_locked(waiting_job_id, job.queue, job.priority);
      }
    }
  }

  void enqueue_ready_locked(
      uint64_t job_id,
      JobQueue queue,
      JobPriority priority
  ) {
    auto &ready = ready_queue_locked(queue);
    ready.by_priority[priority_index(priority)].push_back(job_id);
    m_jobs.at(job_id).queued = true;
  }

  ReadyQueues &ready_queue_locked(JobQueue queue) {
    switch (queue) {
      case JobQueue::Worker:
        return m_worker_ready;
      case JobQueue::Main:
        return m_main_ready;
      case JobQueue::Background:
        return m_background_ready;
    }

    return m_worker_ready;
  }

  uint64_t pop_ready_job_locked(ReadyQueues &ready) {
    for (JobPriority priority : k_priority_order) {
      auto &bucket = ready.by_priority[priority_index(priority)];
      if (bucket.empty()) {
        continue;
      }

      uint64_t job_id = bucket.front();
      bucket.pop_front();
      return job_id;
    }

    return 0u;
  }

  bool has_worker_ready_jobs_locked() const {
    return !m_worker_ready.empty() || !m_background_ready.empty();
  }

  void wait_for_quiescence() {
    for (;;) {
      {
        std::lock_guard lock(m_mutex);
        if (m_incomplete_jobs == 0u) {
          return;
        }
      }

      if (is_main_thread() && drain_main_queue(1u) > 0u) {
        continue;
      }

      std::unique_lock lock(m_mutex);
      m_completion_cv.wait_for(lock, std::chrono::milliseconds(1));
    }
  }

  bool is_main_thread() const noexcept {
    return std::this_thread::get_id() == m_main_thread_id;
  }

  std::thread::id m_main_thread_id;
  mutable std::mutex m_mutex;
  std::condition_variable m_ready_cv;
  std::condition_variable m_completion_cv;
  std::vector<std::thread> m_worker_threads;
  std::unordered_map<uint64_t, JobRecord> m_jobs;
  std::unordered_map<uint64_t, BarrierRecord> m_barriers;
  ReadyQueues m_worker_ready;
  ReadyQueues m_main_ready;
  ReadyQueues m_background_ready;
  uint64_t m_next_job_id = 1u;
  uint64_t m_next_barrier_id = 1u;
  size_t m_incomplete_jobs = 0u;
  size_t m_running_jobs = 0u;
  bool m_stopping = false;
};

} // namespace

JobSystem *JobSystem::m_instance = nullptr;
static JobSystemImpl *g_job_system_impl = nullptr;

JobSystem::JobSystem() = default;

JobSystem::JobSystem(Config config) : m_config(config) {}

JobSystem::~JobSystem() = default;

JobSystem *JobSystem::get() { return m_instance; }

void JobSystem::start() {
  if (m_instance == nullptr) {
    m_instance = this;
  }

  if (g_job_system_impl == nullptr) {
    g_job_system_impl = new JobSystemImpl(m_config);
  }
}

void JobSystem::end() {
  if (g_job_system_impl != nullptr) {
    g_job_system_impl->shutdown();
    delete g_job_system_impl;
    g_job_system_impl = nullptr;
  }

  if (m_instance == this) {
    m_instance = nullptr;
  }
}

void JobSystem::fixed_update(double fixed_dt) { (void)fixed_dt; }

void JobSystem::pre_update(double dt) { (void)dt; }

void JobSystem::update(double dt) { (void)dt; }

JobHandle JobSystem::submit(
    JobCallable work,
    JobQueue queue,
    JobPriority priority
) {
  return g_job_system_impl->submit(std::move(work), queue, priority);
}

JobHandle JobSystem::submit_after(
    std::span<const JobHandle> dependencies,
    JobCallable work,
    JobQueue queue,
    JobPriority priority
) {
  return g_job_system_impl->submit_after(
      dependencies, std::move(work), queue, priority
  );
}

JobBarrier JobSystem::create_barrier(uint32_t expected_count) {
  return g_job_system_impl->create_barrier(expected_count);
}

void JobSystem::attach_to_barrier(JobHandle handle, JobBarrier barrier) {
  g_job_system_impl->attach_to_barrier(handle, barrier);
}

JobHandle JobSystem::submit_after_barrier(
    JobBarrier barrier,
    JobCallable work,
    JobQueue queue,
    JobPriority priority
) {
  return g_job_system_impl->submit_after_barrier(
      barrier, std::move(work), queue, priority
  );
}

bool JobSystem::is_complete(JobHandle handle) const {
  return g_job_system_impl->is_complete(handle);
}

void JobSystem::wait(JobHandle handle) { g_job_system_impl->wait(handle); }

void JobSystem::wait_all(std::span<const JobHandle> handles) {
  g_job_system_impl->wait_all(handles);
}

void JobSystem::wait_barrier(JobBarrier barrier) {
  g_job_system_impl->wait_barrier(barrier);
}

size_t JobSystem::drain_main_queue(size_t max_jobs) {
  return g_job_system_impl->drain_main_queue(max_jobs);
}

bool JobSystem::has_pending_main_work() const {
  return g_job_system_impl->has_pending_main_work();
}

} // namespace astralix
