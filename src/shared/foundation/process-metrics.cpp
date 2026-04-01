#include "process-metrics.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace astralix {
namespace {

struct ProcStatFields {
  unsigned long long utime = 0u;
  unsigned long long stime = 0u;
  unsigned long minflt = 0u;
  unsigned long majflt = 0u;
  long num_threads = 0;
  bool valid = false;
};

ProcStatFields read_proc_self_stat() {
  ProcStatFields fields;

  FILE *file = std::fopen("/proc/self/stat", "r");
  if (file == nullptr) {
    return fields;
  }

  int pid = 0;
  char comm[256] = {};
  char state = ' ';
  int ppid = 0;
  int pgrp = 0;
  int session = 0;
  int tty_nr = 0;
  int tpgid = 0;
  unsigned int flags = 0u;
  unsigned long minflt = 0u;
  unsigned long cminflt = 0u;
  unsigned long majflt = 0u;
  unsigned long cmajflt = 0u;
  unsigned long long utime_ticks = 0u;
  unsigned long long stime_ticks = 0u;
  long long cutime = 0;
  long long cstime = 0;
  long priority = 0;
  long nice_value = 0;
  long num_threads = 0;

  int matched = std::fscanf(
      file,
      "%d %255s %c %d %d %d %d %d %u %lu %lu %lu %lu %llu %llu %lld %lld %ld %ld %ld",
      &pid,
      comm,
      &state,
      &ppid,
      &pgrp,
      &session,
      &tty_nr,
      &tpgid,
      &flags,
      &minflt,
      &cminflt,
      &majflt,
      &cmajflt,
      &utime_ticks,
      &stime_ticks,
      &cutime,
      &cstime,
      &priority,
      &nice_value,
      &num_threads
  );

  std::fclose(file);

  if (matched >= 20) {
    fields.utime = utime_ticks;
    fields.stime = stime_ticks;
    fields.minflt = minflt;
    fields.majflt = majflt;
    fields.num_threads = num_threads;
    fields.valid = true;
  }

  return fields;
}

struct ProcMemFields {
  unsigned long virtual_pages = 0u;
  unsigned long resident_pages = 0u;
  bool valid = false;
};

ProcMemFields read_proc_self_statm() {
  ProcMemFields fields;

  FILE *file = std::fopen("/proc/self/statm", "r");
  if (file == nullptr) {
    return fields;
  }

  unsigned long virtual_pages = 0u;
  unsigned long resident_pages = 0u;

  int matched = std::fscanf(file, "%lu %lu", &virtual_pages, &resident_pages);
  std::fclose(file);

  if (matched >= 2) {
    fields.virtual_pages = virtual_pages;
    fields.resident_pages = resident_pages;
    fields.valid = true;
  }

  return fields;
}

struct ProcStatusFields {
  size_t voluntary_context_switches = 0u;
  size_t involuntary_context_switches = 0u;
  bool valid = false;
};

ProcStatusFields read_proc_self_status() {
  ProcStatusFields fields;

  FILE *file = std::fopen("/proc/self/status", "r");
  if (file == nullptr) {
    return fields;
  }

  char line[256];
  while (std::fgets(line, sizeof(line), file) != nullptr) {
    size_t value = 0u;
    if (std::sscanf(line, "voluntary_ctxt_switches: %zu", &value) == 1) {
      fields.voluntary_context_switches = value;
      fields.valid = true;
    } else if (std::sscanf(line, "nonvoluntary_ctxt_switches: %zu", &value) == 1) {
      fields.involuntary_context_switches = value;
    }
  }

  std::fclose(file);
  return fields;
}

struct ProcIoFields {
  unsigned long long read_bytes = 0u;
  unsigned long long write_bytes = 0u;
  bool valid = false;
};

ProcIoFields read_proc_self_io() {
  ProcIoFields fields;

  FILE *file = std::fopen("/proc/self/io", "r");
  if (file == nullptr) {
    return fields;
  }

  char line[256];
  while (std::fgets(line, sizeof(line), file) != nullptr) {
    unsigned long long value = 0u;
    if (std::sscanf(line, "read_bytes: %llu", &value) == 1) {
      fields.read_bytes = value;
      fields.valid = true;
    } else if (std::sscanf(line, "write_bytes: %llu", &value) == 1) {
      fields.write_bytes = value;
    }
  }

  std::fclose(file);
  return fields;
}

size_t count_proc_self_fd() {
  DIR *directory = opendir("/proc/self/fd");
  if (directory == nullptr) {
    return 0u;
  }

  size_t count = 0u;
  while (readdir(directory) != nullptr) {
    count++;
  }
  closedir(directory);

  if (count >= 3u) {
    count -= 3u;
  }

  return count;
}

double wall_time_seconds() {
  using Clock = std::chrono::steady_clock;
  const auto now = Clock::now().time_since_epoch();
  return std::chrono::duration<double>(now).count();
}

} // namespace

ProcessMetrics ProcessMetricsSampler::sample() {
  ProcessMetrics metrics;

  const ProcMemFields memory = read_proc_self_statm();
  if (memory.valid) {
    const long page_size = sysconf(_SC_PAGESIZE);
    metrics.memory_rss_mb =
        static_cast<float>(memory.resident_pages) *
        static_cast<float>(page_size) / (1024.0f * 1024.0f);
    metrics.memory_virtual_mb =
        static_cast<float>(memory.virtual_pages) *
        static_cast<float>(page_size) / (1024.0f * 1024.0f);
  }

  const ProcStatFields cpu = read_proc_self_stat();
  const double current_wall = wall_time_seconds();

  if (cpu.valid) {
    metrics.minor_page_faults = static_cast<size_t>(cpu.minflt);
    metrics.major_page_faults = static_cast<size_t>(cpu.majflt);
    metrics.thread_count = static_cast<size_t>(cpu.num_threads);

    if (m_has_previous_sample) {
      const double wall_delta = current_wall - m_previous_wall_seconds;
      if (wall_delta > 0.0) {
        const long ticks_per_second = sysconf(_SC_CLK_TCK);
        const unsigned long long tick_delta =
            (cpu.utime + cpu.stime) - (m_previous_utime + m_previous_stime);
        const double cpu_seconds =
            static_cast<double>(tick_delta) /
            static_cast<double>(ticks_per_second);
        metrics.cpu_usage_percent =
            static_cast<float>((cpu_seconds / wall_delta) * 100.0);
      }
    }

    m_previous_utime = cpu.utime;
    m_previous_stime = cpu.stime;
    m_previous_wall_seconds = current_wall;
    m_has_previous_sample = true;
  }

  metrics.open_fd_count = count_proc_self_fd();

  const ProcStatusFields status = read_proc_self_status();
  if (status.valid) {
    metrics.voluntary_context_switches = status.voluntary_context_switches;
    metrics.involuntary_context_switches = status.involuntary_context_switches;
  }

  const ProcIoFields io = read_proc_self_io();
  if (io.valid) {
    metrics.disk_read_bytes = static_cast<float>(io.read_bytes);
    metrics.disk_write_bytes = static_cast<float>(io.write_bytes);
  }

  if (!m_has_binary_size) {
    struct stat file_stat {};
    if (::stat("/proc/self/exe", &file_stat) == 0) {
      m_cached_binary_size_mb =
          static_cast<float>(file_stat.st_size) / (1024.0f * 1024.0f);
      m_has_binary_size = true;
    }
  }
  metrics.binary_size_mb = m_cached_binary_size_mb;

  return metrics;
}

} // namespace astralix
