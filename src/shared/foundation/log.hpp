#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"

#ifdef LOG_TO_CONSOLE
#include <iostream>
#endif

namespace astralix {

enum class LogLevel { INFO = 0, WARNING = 1, ERROR = 2, DEBUG = 3 };

class Logger {
public:
  using SinkID = uint64_t;

  static Logger &get() {
    static Logger instance;
    return instance;
  }

  struct Log {
    std::string message;
    std::string timestamp;
    LogLevel level;
    std::string caller;
    std::string file;
    int line;
  };

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  template <typename... Args>
  void log(LogLevel level, const char *caller, const char *file, int line,
           Args &&...args) {
#ifdef ENABLE_LOGS
    push_log(Log{
        .message = build_message(std::forward<Args>(args)...),
        .timestamp = timestamp_now(),
        .level = level,
        .caller = caller != nullptr ? caller : "",
        .file = extract_filename(file),
        .line = line,
    });
#endif
  }

  const std::deque<Log> &logs() const { return m_logs; }
  size_t max_entries() const { return m_max_entries; }
  void set_max_entries(size_t max_entries);
  void clear();

  SinkID add_sink(std::function<void(const Log &)> sink);
  void remove_sink(SinkID sink_id);

  void set_terminal_output_enabled(bool enabled) {
    m_terminal_output_enabled = enabled;
  }

  static std::string timestamp_now();

  template <typename... Args> std::string build_message(Args &&...args) {
    std::ostringstream message_stream;
    ((message_stream << std::forward<Args>(args) << " "), ...);
    std::string result = message_stream.str();

    if (!result.empty()) {
      result.pop_back(); // Remove the trailing space
    }

    return result;
  }

private:
  Logger() = default;
  ~Logger() = default;

  void push_log(Log log);
  void trim_to_capacity();
  void render_terminal_log(const Log &log) const;
  static std::string extract_filename(const char *file);

  std::deque<Log> m_logs;
  size_t m_max_entries = 500u;
  SinkID m_next_sink_id = 1u;
  bool m_terminal_output_enabled = true;
  std::unordered_map<SinkID, std::function<void(const Log &)>> m_sinks;
};

#ifdef ENABLE_LOGS
#define LOG_INFO(...)                                                          \
  Logger::get().log(LogLevel::INFO, __FUNCTION__, __FILE__, __LINE__,          \
                    __VA_ARGS__)
#define LOG_WARN(...)                                                          \
  Logger::get().log(LogLevel::WARNING, __FUNCTION__, __FILE__, __LINE__,       \
                    __VA_ARGS__)
#define LOG_ERROR(...)                                                         \
  Logger::get().log(LogLevel::ERROR, __FUNCTION__, __FILE__, __LINE__,         \
                    __VA_ARGS__)
#define LOG_DEBUG(...)                                                         \
  Logger::get().log(LogLevel::DEBUG, __FUNCTION__, __FILE__, __LINE__,         \
                    __VA_ARGS__)
#else
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERROR(...)
#define LOG_DEBUG(...)
#endif

} // namespace astralix
