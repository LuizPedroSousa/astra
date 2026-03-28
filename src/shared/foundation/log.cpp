#include "log.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace astralix {

std::string Logger::timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t current_time = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
#if defined(_WIN32)
  localtime_s(&local_time, &current_time);
#else
  localtime_r(&current_time, &local_time);
#endif

  std::ostringstream timestamp_stream;
  timestamp_stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
  return timestamp_stream.str();
}

void Logger::set_max_entries(size_t max_entries) {
  m_max_entries = std::max<size_t>(1u, max_entries);
  trim_to_capacity();
}

void Logger::clear() { m_logs.clear(); }

Logger::SinkID Logger::add_sink(std::function<void(const Log &)> sink) {
  const SinkID sink_id = m_next_sink_id++;
  m_sinks.insert_or_assign(sink_id, std::move(sink));
  return sink_id;
}

void Logger::remove_sink(SinkID sink_id) { m_sinks.erase(sink_id); }

void Logger::push_log(Log log) {
  trim_to_capacity();
  m_logs.push_back(log);
  render_terminal_log(m_logs.back());

  for (const auto &[_, sink] : m_sinks) {
    if (sink) {
      sink(m_logs.back());
    }
  }

  trim_to_capacity();
}

void Logger::trim_to_capacity() {
  while (m_logs.size() > m_max_entries) {
    m_logs.pop_front();
  }
}

void Logger::render_terminal_log(const Log &log) const {
  if (!m_terminal_output_enabled) {
    return;
  }

  std::ostringstream log_stream;
  log_stream << BOLD << "[" << log.timestamp << "] :: ";

  switch (log.level) {
  case LogLevel::INFO:
    log_stream << CYAN << "[INFO] ";
    break;
  case LogLevel::WARNING:
    log_stream << YELLOW << "[WARNING] ";
    break;
  case LogLevel::ERROR:
    log_stream << RED << "[ERROR] ";
    break;
  case LogLevel::DEBUG:
    log_stream << CYAN << "[DEBUG] ";
    break;
  }

  log_stream << RESET << BOLD << "[" << log.file << "::" << log.line << "]";
  log_stream << " [" << log.caller << "]";
  log_stream << RESET << " :: " << log.message << "\n";

#ifdef LOG_TO_CONSOLE
  std::cout << log_stream.str();
#endif
}

std::string Logger::extract_filename(const char *file) {
  if (file == nullptr) {
    return {};
  }

  const std::string file_path(file);
  const size_t pos = file_path.find_last_of("/\\");
  return pos != std::string::npos ? file_path.substr(pos + 1u) : file_path;
}

} // namespace astralix
